#include "NodePropertiesPanel.h"
#include "GraphEditorComponent.h"
#include "../PluginProcessor.h"
#include "../Graph/GraphController.h"
#include "../Graph/SubgraphNode.h"
#include "../NativeNodes/GainNode.h"
#include "../NativeNodes/MutePhaseNode.h"
#include "../NativeNodes/MatrixMixerNode.h"
#include "../NativeNodes/DelayNode.h"

namespace
{
    constexpr int kRowHeight    = 28;
    constexpr int kHeaderHeight = 24;
    constexpr int kLabelWidth   = 48;

    juce::Slider::SliderStyle horizontalSlider() { return juce::Slider::LinearHorizontal; }

    /** Slider preconfigured for dB-style display. Also accepts linear-gain
        input — type a number followed by "x" or "lin" (e.g. "0.5 x", "1.5 lin")
        and it's converted to dB. Plain numbers and "X dB" stay in dB. */
    void styleAsDb (juce::Slider& s)
    {
        s.setSliderStyle (horizontalSlider());
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
        s.setRange (-120.0, 40.0, 0.1);
        s.setNumDecimalPlacesToDisplay (1);
        // Skew so the visual midpoint maps to 0 dB — keeps unity at the
        // centre with equal slider real-estate for boost and cut, while
        // still letting the lower stretch reach down to silence.
        s.setSkewFactorFromMidPoint (0.0);
        // Double-click resets to unity gain (0 dB).
        s.setDoubleClickReturnValue (true, 0.0);

        s.textFromValueFunction = [] (double db)
        {
            return juce::String (db, 1) + " dB";
        };
        s.valueFromTextFunction = [] (const juce::String& text) -> double
        {
            const auto trimmed = text.trim();
            const auto lower   = trimmed.toLowerCase();
            const double v     = trimmed.getDoubleValue();

            // Linear suffix → convert to dB. Recognize "x", "lin", "linear".
            const bool isLinear = lower.endsWith ("x")
                               || lower.endsWith ("lin")
                               || lower.endsWith ("linear");
            if (isLinear)
                return juce::Decibels::gainToDecibels (v, -120.0);

            // Otherwise treat as dB (with or without explicit "dB" suffix).
            return v;
        };
    }

    /** Same slider, but its value lives in *linear* gain. Range [0, 100]
        covers silence to +40 dB; midpoint = unity. Type "X dB" to enter a
        dB value and it's converted to linear. */
    void styleAsLinearGain (juce::Slider& s)
    {
        s.setSliderStyle (horizontalSlider());
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
        s.setRange (0.0, 100.0, 0.001);
        s.setNumDecimalPlacesToDisplay (3);
        s.setSkewFactorFromMidPoint (1.0);
        // Double-click resets to unity gain (1.0 linear == 0 dB).
        s.setDoubleClickReturnValue (true, 1.0);

        s.textFromValueFunction = [] (double v)
        {
            return juce::String (v, 3);
        };
        s.valueFromTextFunction = [] (const juce::String& text) -> double
        {
            const auto trimmed = text.trim();
            const auto lower   = trimmed.toLowerCase();
            const double v     = trimmed.getDoubleValue();

            // "X dB" suffix → convert to linear gain.
            if (lower.endsWith ("db"))
                return juce::Decibels::decibelsToGain (v, -120.0);
            return v;
        };
    }

    /** Generic horizontal slider for raw-numeric uses (samples, etc.). */
    void styleAsHSlider (juce::Slider& s)
    {
        s.setSliderStyle (horizontalSlider());
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 22);
    }

    //==========================================================================
    /** The "Automation parameters" list: one row per parameter of a node's
        processor, each with a button that binds it to one of mcfx_graph's
        forwarding slots. Shared by every node kind that has parameters —
        hosted plug-ins and the native gain / delay / mute-phase nodes. */
    class ParameterExposureList : public juce::Component
    {
    public:
        ParameterExposureList (Mcfx_graphAudioProcessor& outerProc, GraphNode& gn)
            : outerProc_ (outerProc), node_ (gn)
        {
            if (node_.processor == nullptr) return;

            paramsHeader_.setText ("Automation parameters", juce::dontSendNotification);
            paramsHeader_.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
            paramsHeader_.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
            addAndMakeVisible (paramsHeader_);

            slotsLabel_.setFont (juce::Font (juce::FontOptions (10.0f)));
            slotsLabel_.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.5f));
            addAndMakeVisible (slotsLabel_);

            const auto& params = node_.processor->getParameters();
            paramRows_.reserve ((size_t) params.size());

            for (int i = 0; i < params.size(); ++i)
            {
                auto* p = params.getUnchecked (i);
                if (p == nullptr) continue;

                auto row = std::make_unique<ParamRow>();
                row->paramIndex = i;

                auto name = p->getName (40);
                if (name.isEmpty()) name = "Param " + juce::String (i + 1);
                row->nameLabel.setText (name, juce::dontSendNotification);
                row->nameLabel.setColour (juce::Label::textColourId,
                                          juce::Colours::white.withAlpha (0.85f));
                row->nameLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
                addAndMakeVisible (row->nameLabel);

                auto& btn = row->exposeButton;
                btn.setClickingTogglesState (false);
                btn.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff333344));
                btn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3e6aa8));
                btn.setTooltip ("Expose this parameter to the DAW so it can be automated. "
                                "When bound, the button shows the slot index (e.g. \"P3\"). "
                                "Click again to free the slot. mcfx_graph has 256 slots total.");
                btn.onClick = [this, idx = i] { onExposeClicked (idx); };
                addAndMakeVisible (btn);

                paramRows_.push_back (std::move (row));
            }

            refreshExposureLabels();
        }

        bool isEmpty() const noexcept { return paramRows_.empty(); }

        /** Height this list needs; 0 when the node has no parameters. */
        int getPreferredHeight() const
        {
            return paramRows_.empty() ? 0
                                      : 18 + 14 + 4 + (int) paramRows_.size() * kRowHeight;
        }

        void resized() override
        {
            if (paramRows_.empty()) return;

            auto area = getLocalBounds();
            paramsHeader_.setBounds (area.removeFromTop (18));
            slotsLabel_.setBounds   (area.removeFromTop (14));
            area.removeFromTop (4);

            for (auto& row : paramRows_)
            {
                auto r = area.removeFromTop (kRowHeight);
                row->exposeButton.setBounds (r.removeFromRight (44).reduced (1));
                row->nameLabel  .setBounds (r);
            }
        }

    private:
        static constexpr int kRowHeight = 22;

        struct ParamRow
        {
            int              paramIndex = -1;
            juce::Label      nameLabel;
            juce::TextButton exposeButton;
        };

        void onExposeClicked (int paramIndex)
        {
            if (outerProc_.getExposedSlotFor (node_.uuid, paramIndex) >= 0)
                outerProc_.unexposeParameter (node_.uuid, paramIndex);
            else
                outerProc_.exposeParameter   (node_.uuid, paramIndex);

            refreshExposureLabels();
        }

        void refreshExposureLabels()
        {
            int boundCount = 0;
            for (auto& row : paramRows_)
            {
                const int slot = outerProc_.getExposedSlotFor (node_.uuid, row->paramIndex);
                if (slot >= 0)
                {
                    row->exposeButton.setButtonText ("P" + juce::String (slot));
                    row->exposeButton.setToggleState (true, juce::dontSendNotification);
                    ++boundCount;
                }
                else
                {
                    row->exposeButton.setButtonText ("P");
                    row->exposeButton.setToggleState (false, juce::dontSendNotification);
                }
            }

            int totalBound = 0;
            const int total = outerProc_.getForwardingParameterCount();
            for (int i = 0; i < total; ++i)
                if (auto* fp = outerProc_.getForwardingParameter (i))
                    if (fp->isBound()) ++totalBound;

            slotsLabel_.setText ("This node: " + juce::String (boundCount)
                                  + "    Total used: " + juce::String (totalBound)
                                  + " / " + juce::String (total),
                                 juce::dontSendNotification);
        }

        Mcfx_graphAudioProcessor& outerProc_;
        GraphNode& node_;
        juce::Label paramsHeader_, slotsLabel_;
        std::vector<std::unique_ptr<ParamRow>> paramRows_;
    };

    //==========================================================================
    // Per-kind subpanels. Each builds its own children in the constructor and
    // lays them out in resized().

    class GainPropertiesPanel : public juce::Component,
                                private juce::Slider::Listener,
                                private juce::Timer
    {
    public:
        GainPropertiesPanel (GainNode& g, Mcfx_graphAudioProcessor& proc, GraphNode& gn)
            : gain_ (g), proc_ (proc)
        {
            linkedToggle_.setButtonText ("Linked");
            linkedToggle_.setToggleState (gain_.isLinked(), juce::dontSendNotification);
            linkedToggle_.onClick = [this]
            {
                gain_.setLinked (linkedToggle_.getToggleState());
                if (gain_.isLinked())
                    for (auto& s : sliders_) s->setValue (sliders_[0]->getValue(), juce::dontSendNotification);
                proc_.commitHistorySnapshot();
            };
            addAndMakeVisible (linkedToggle_);

            // Display mode: dB (default) ↔ Linear. The underlying gain is
            // always stored in dB on the node — only the slider's range,
            // skew and text formatting change between modes.
            modeToggle_.setButtonText ("Linear");
            modeToggle_.setToggleState (linearMode_, juce::dontSendNotification);
            modeToggle_.setTooltip ("Show gain values in linear units (1.0 = unity) "
                                    "instead of dB. Type \"X dB\" in linear mode "
                                    "or \"X x\" in dB mode to enter the other form.");
            modeToggle_.onClick = [this]
            {
                linearMode_ = modeToggle_.getToggleState();
                applyMode();
            };
            addAndMakeVisible (modeToggle_);

            for (int c = 0; c < gain_.getNumChannels(); ++c)
            {
                auto* lab = labels_.add (new juce::Label ({}, "Ch " + juce::String (c + 1)));
                lab->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.8f));
                addAndMakeVisible (lab);

                auto* s = sliders_.add (new ResetCommitSlider());
                s->onResetCommit = [this] { proc_.commitHistorySnapshot(); };
                s->addListener (this);
                // Commit on drag-end so a single drag is one undo step rather
                // than tens of slider-tick events.
                s->onDragEnd = [this] { proc_.commitHistorySnapshot(); };
                addAndMakeVisible (s);
            }

            applyMode();

            exposure_ = std::make_unique<ParameterExposureList> (proc_, gn);
            addAndMakeVisible (*exposure_);

            setSize (260, kHeaderHeight + (gain_.getNumChannels() + 2) * kRowHeight
                          + exposure_->getPreferredHeight());

            // The node's value can change without going through this panel —
            // host automation of an exposed parameter writes it straight into
            // the node — so poll it back into the sliders.
            startTimerHz (15);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (4);
            auto headerRow = area.removeFromTop (kRowHeight - 2);
            linkedToggle_.setBounds (headerRow.removeFromLeft (headerRow.getWidth() / 2));
            modeToggle_  .setBounds (headerRow);

            for (int c = 0; c < sliders_.size(); ++c)
            {
                auto row = area.removeFromTop (kRowHeight);
                labels_[c] ->setBounds (row.removeFromLeft (kLabelWidth));
                sliders_[c]->setBounds (row);
            }
            if (exposure_ != nullptr && exposure_->getPreferredHeight() > 0)
                exposure_->setBounds (area.removeFromTop (exposure_->getPreferredHeight()));

        }

    private:
        // Slider subclass that fires a callback on double-click reset, so the
        // panel can commit an undo snapshot. juce::Slider's setDoubleClickReturnValue
        // performs the value change but doesn't go through onDragEnd.
        struct ResetCommitSlider : juce::Slider
        {
            std::function<void()> onResetCommit;
            void mouseDoubleClick (const juce::MouseEvent& e) override
            {
                juce::Slider::mouseDoubleClick (e);
                if (onResetCommit) onResetCommit();
            }
        };

        void timerCallback() override
        {
            // Skip while the user is dragging, or the poll would fight them.
            for (auto* sl : sliders_)
                if (sl->isMouseButtonDown()) return;

            for (int c = 0; c < sliders_.size(); ++c)
            {
                const float db = gain_.getGainDb (c);
                const double shown = linearMode_
                        ? (double) juce::Decibels::decibelsToGain (db, -120.0f)
                        : (double) db;
                if (std::abs (sliders_[c]->getValue() - shown) > 1.0e-4)
                    sliders_[c]->setValue (shown, juce::dontSendNotification);
            }
            linkedToggle_.setToggleState (gain_.isLinked(), juce::dontSendNotification);
        }

        void applyMode()
        {
            for (int c = 0; c < sliders_.size(); ++c)
            {
                auto* s = sliders_[c];
                const float db = gain_.getGainDb (c);

                // Reconfigure without re-firing change callbacks.
                s->removeListener (this);
                if (linearMode_)
                {
                    styleAsLinearGain (*s);
                    s->setValue (juce::Decibels::decibelsToGain (db, -120.0f),
                                  juce::dontSendNotification);
                }
                else
                {
                    styleAsDb (*s);
                    s->setValue (db, juce::dontSendNotification);
                }
                // Force a text refresh — Slider::setValue is a no-op (text
                // included) when the value already matches, which leaves the
                // box showing pre-styled formatting (no "dB" suffix) on a
                // freshly-constructed slider whose value is already 0.0.
                s->updateText();
                s->addListener (this);
            }
        }

        void sliderValueChanged (juce::Slider* s) override
        {
            for (int c = 0; c < sliders_.size(); ++c)
            {
                if (sliders_[c] == s)
                {
                    const float db = linearMode_
                        ? juce::Decibels::gainToDecibels ((float) s->getValue(), -120.0f)
                        : (float) s->getValue();

                    gain_.setGainDb (c, db);
                    if (gain_.isLinked())
                        for (auto* other : sliders_)
                            if (other != s)
                                other->setValue (s->getValue(), juce::dontSendNotification);
                    break;
                }
            }
        }

        GainNode& gain_;
        Mcfx_graphAudioProcessor& proc_;
        bool linearMode_ = false;
        juce::ToggleButton linkedToggle_;
        juce::ToggleButton modeToggle_;
        juce::OwnedArray<juce::Label>            labels_;
        juce::OwnedArray<ResetCommitSlider>      sliders_;
        std::unique_ptr<ParameterExposureList> exposure_;
    };

    //==========================================================================
    class MutePhasePropertiesPanel : public juce::Component,
                                     private juce::Timer
    {
    public:
        MutePhasePropertiesPanel (MutePhaseNode& m, Mcfx_graphAudioProcessor& proc, GraphNode& gn)
            : mp_ (m), proc_ (proc)
        {
            linkedToggle_.setButtonText ("Linked");
            linkedToggle_.setToggleState (mp_.isLinked(), juce::dontSendNotification);
            linkedToggle_.onClick = [this]
            {
                mp_.setLinked (linkedToggle_.getToggleState());
                if (mp_.isLinked())
                {
                    // Mirror channel-0 state to every UI button.
                    const bool m0 = muteButtons_[0]->getToggleState();
                    const bool i0 = invButtons_ [0]->getToggleState();
                    for (auto* b : muteButtons_) b->setToggleState (m0, juce::dontSendNotification);
                    for (auto* b : invButtons_)  b->setToggleState (i0, juce::dontSendNotification);
                }
                proc_.commitHistorySnapshot();
            };
            addAndMakeVisible (linkedToggle_);

            for (int c = 0; c < mp_.getNumChannels(); ++c)
            {
                auto* lab = labels_.add (new juce::Label ({}, "Ch " + juce::String (c + 1)));
                lab->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.8f));
                addAndMakeVisible (lab);

                auto* mute = muteButtons_.add (new juce::ToggleButton ("M"));
                mute->setToggleState (mp_.getMute (c), juce::dontSendNotification);
                mute->onClick = [this, c, mute]
                {
                    mp_.setMute (c, mute->getToggleState());
                    if (mp_.isLinked())
                        for (auto* other : muteButtons_)
                            if (other != mute)
                                other->setToggleState (mute->getToggleState(),
                                                        juce::dontSendNotification);
                    proc_.commitHistorySnapshot();
                };
                addAndMakeVisible (mute);

                auto* inv = invButtons_.add (new juce::ToggleButton ("Inv"));
                inv->setToggleState (mp_.getInvert (c), juce::dontSendNotification);
                inv->onClick = [this, c, inv]
                {
                    mp_.setInvert (c, inv->getToggleState());
                    if (mp_.isLinked())
                        for (auto* other : invButtons_)
                            if (other != inv)
                                other->setToggleState (inv->getToggleState(),
                                                        juce::dontSendNotification);
                    proc_.commitHistorySnapshot();
                };
                addAndMakeVisible (inv);
            }

            exposure_ = std::make_unique<ParameterExposureList> (proc_, gn);
            addAndMakeVisible (*exposure_);

            setSize (260, kHeaderHeight + (mp_.getNumChannels() + 1) * kRowHeight
                          + exposure_->getPreferredHeight());

            // Poll the node: host automation of an exposed parameter changes
            // it without passing through this panel.
            startTimerHz (15);
        }

        void timerCallback() override
        {
            for (int c = 0; c < muteButtons_.size(); ++c)
            {
                muteButtons_[c]->setToggleState (mp_.getMute   (c), juce::dontSendNotification);
                invButtons_ [c]->setToggleState (mp_.getInvert (c), juce::dontSendNotification);
            }
            linkedToggle_.setToggleState (mp_.isLinked(), juce::dontSendNotification);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (4);
            linkedToggle_.setBounds (area.removeFromTop (kRowHeight - 2));

            for (int c = 0; c < labels_.size(); ++c)
            {
                auto row = area.removeFromTop (kRowHeight);
                labels_[c]->setBounds      (row.removeFromLeft (kLabelWidth));
                muteButtons_[c]->setBounds (row.removeFromLeft (60));
                invButtons_[c]->setBounds  (row.removeFromLeft (60));
            }

            if (exposure_ != nullptr && exposure_->getPreferredHeight() > 0)
                exposure_->setBounds (area.removeFromTop (exposure_->getPreferredHeight()));
        }

    private:
        MutePhaseNode& mp_;
        Mcfx_graphAudioProcessor& proc_;
        juce::ToggleButton                   linkedToggle_;
        juce::OwnedArray<juce::Label>        labels_;
        juce::OwnedArray<juce::ToggleButton> muteButtons_;
        juce::OwnedArray<juce::ToggleButton> invButtons_;
        std::unique_ptr<ParameterExposureList> exposure_;
    };

    //==========================================================================
    class MatrixMixerPropertiesPanel : public juce::Component
    {
    public:
        // The grid is painted, not built from one TextEditor per cell: a
        // 256 x 256 matrix would be 65,536 components, which froze the editor
        // for minutes. Only the cells inside the clip are drawn, and a single
        // shared TextEditor is overlaid on the cell being edited.
        MatrixMixerPropertiesPanel (MatrixMixerNode& r, Mcfx_graphAudioProcessor& proc)
            : mixer_ (r), proc_ (proc),
              nIn_ (mixer_.getNumIn()), nOut_ (mixer_.getNumOut())
        {
            copyButton_.setButtonText  ("Copy");
            copyButton_.setTooltip ("Copy matrix to clipboard (TSV: rows = inputs, cols = outputs). "
                                    "Always copied as raw linear gain regardless of the dB toggle.");
            copyButton_.onClick = [this] { copyMatrixToClipboard(); };
            addAndMakeVisible (copyButton_);

            pasteButton_.setButtonText ("Paste");
            pasteButton_.setTooltip ("Paste TSV / CSV / whitespace-separated matrix from clipboard. "
                                    "Cells beyond the matrix dimensions are ignored; missing ones stay 0. "
                                    "Always parsed as raw linear gain regardless of the dB toggle.");
            pasteButton_.onClick = [this] { pasteMatrixFromClipboard(); };
            addAndMakeVisible (pasteButton_);

            dbToggle_.setButtonText ("dB");
            dbToggle_.setTooltip ("Show / enter cell values in dB instead of linear gain. "
                                   "0.0 linear corresponds to -120.0 dB.");
            dbToggle_.onClick = [this] { endEdit (false); repaint(); };
            addAndMakeVisible (dbToggle_);

            cellEditor_.owner = this;
            cellEditor_.setMultiLine (false);
            cellEditor_.setJustification (juce::Justification::centred);
            cellEditor_.onReturnKey = [this] { endEdit (true); };
            cellEditor_.onEscapeKey = [this] { endEdit (false); };
            cellEditor_.onFocusLost = [this] { endEdit (true); };
            addChildComponent (cellEditor_);

            setSize (juce::jmax (260, kPad * 2 + kRowLabelW + nOut_ * kMinCellW),
                     gridTop() + (nIn_ + 1) * kCellH + kPad);
        }

        void resized() override
        {
            auto toolbar = getLocalBounds().reduced (kPad).removeFromTop (kRowHeight);
            copyButton_ .setBounds (toolbar.removeFromLeft (60));
            toolbar.removeFromLeft (4);
            pasteButton_.setBounds (toolbar.removeFromLeft (60));
            toolbar.removeFromLeft (10);
            dbToggle_   .setBounds (toolbar.removeFromLeft (50));

            // Stretch the cells when the panel is wider than the matrix needs.
            cellW_ = nOut_ > 0 ? juce::jmax (kMinCellW, (getWidth() - kPad * 2 - kRowLabelW) / nOut_)
                               : kMinCellW;
            endEdit (true);
        }

        // The hosting Viewport scrolls by moving this component. Repaint so the
        // sticky headers follow, and close the editor rather than letting it
        // slide under them.
        void moved() override
        {
            endEdit (true);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            if (nIn_ == 0 || nOut_ == 0) return;

            const auto clip  = g.getClipBounds();
            const auto view  = visibleArea();
            const int  hdrY  = juce::jmax (gridTop(), view.getY());   // sticky column header
            const int  hdrX  = juce::jmax (kPad, view.getX());        // sticky row header
            const bool dB    = dbMode();

            const int o0 = juce::jlimit (0, nOut_ - 1, (clip.getX() - gridLeft()) / cellW_);
            const int o1 = juce::jlimit (0, nOut_ - 1, (clip.getRight() - gridLeft()) / cellW_);
            const int i0 = juce::jlimit (0, nIn_ - 1, (clip.getY() - cellsTop()) / kCellH);
            const int i1 = juce::jlimit (0, nIn_ - 1, (clip.getBottom() - cellsTop()) / kCellH);

            g.setFont (juce::FontOptions (13.0f));

            for (int i = i0; i <= i1; ++i)
            {
                for (int o = o0; o <= o1; ++o)
                {
                    const auto r = cellBounds (i, o);
                    const float v = mixer_.getMatrix (o, i);
                    const bool active = (i == activeIn_ && o == activeOut_);

                    g.setColour (active ? juce::Colour (0xff3a3a20)
                                        : (v != 0.0f ? juce::Colour (0xff2c3440)
                                                     : juce::Colour (0xff23232b)));
                    g.fillRect (r.reduced (1));

                    if (! (editing_ && active))
                    {
                        g.setColour (v != 0.0f ? juce::Colours::white
                                               : juce::Colours::white.withAlpha (0.35f));
                        g.drawText (formatCell (v, dB), r, juce::Justification::centred, false);
                    }
                }
            }

            // Column header (outputs) pinned to the top of the visible area.
            const auto bg = juce::Colour (0xff1c1c24);
            g.setColour (bg);
            g.fillRect (clip.getX(), hdrY, clip.getWidth(), kCellH);
            for (int o = o0; o <= o1; ++o)
            {
                g.setColour (o == activeOut_ ? juce::Colours::yellow : juce::Colours::orange);
                g.drawText ("O" + juce::String (o + 1),
                            gridLeft() + o * cellW_, hdrY, cellW_, kCellH,
                            juce::Justification::centred, false);
            }

            // Row header (inputs) pinned to the left of the visible area.
            g.setColour (bg);
            g.fillRect (hdrX, clip.getY(), kRowLabelW, clip.getHeight());
            for (int i = i0; i <= i1; ++i)
            {
                const int y = cellsTop() + i * kCellH;
                if (y + kCellH <= hdrY + kCellH) continue;   // hidden behind the column header
                g.setColour (i == activeIn_ ? juce::Colours::yellow : juce::Colours::lightblue);
                g.drawText ("In " + juce::String (i + 1), hdrX, y, kRowLabelW - 6, kCellH,
                            juce::Justification::centredRight, false);
            }

            // Corner where the two headers meet.
            g.setColour (bg);
            g.fillRect (hdrX, hdrY, kRowLabelW, kCellH);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const auto view = visibleArea();
            const int  hdrY = juce::jmax (gridTop(), view.getY());
            const int  hdrX = juce::jmax (kPad, view.getX());

            // Clicks on the pinned headers don't reach the cells beneath them.
            if (e.y < hdrY + kCellH || e.x < hdrX + kRowLabelW)
            {
                endEdit (true);
                return;
            }

            const int o = (e.x - gridLeft()) / cellW_;
            const int i = (e.y - cellsTop()) / kCellH;
            if (juce::isPositiveAndBelow (o, nOut_) && juce::isPositiveAndBelow (i, nIn_))
                beginEdit (i, o);
            else
                endEdit (true);
        }

    private:
        static constexpr int kPad       = 4;
        static constexpr int kRowLabelW = 48;
        static constexpr int kCellH     = 22;
        // Min cell width fits "-120.0 dB" with a bit of padding so the dB
        // display mode never clips.
        static constexpr int kMinCellW  = 72;

        // Shared overlay editor. Tab / Shift+Tab and the arrow keys move to
        // the neighbouring cell, like a spreadsheet.
        struct CellEditor : juce::TextEditor
        {
            MatrixMixerPropertiesPanel* owner = nullptr;

            bool keyPressed (const juce::KeyPress& k) override
            {
                if (owner != nullptr)
                {
                    const auto code = k.getKeyCode();
                    if (code == juce::KeyPress::tabKey)
                        return owner->moveEdit (0, k.getModifiers().isShiftDown() ? -1 : 1, true);
                    if (code == juce::KeyPress::upKey)   return owner->moveEdit (-1, 0, false);
                    if (code == juce::KeyPress::downKey) return owner->moveEdit ( 1, 0, false);
                }
                return juce::TextEditor::keyPressed (k);
            }
        };

        int gridTop()  const { return kPad + kRowHeight + 4; }       // column header row
        int cellsTop() const { return gridTop() + kCellH; }
        int gridLeft() const { return kPad + kRowLabelW; }

        juce::Rectangle<int> cellBounds (int i, int o) const
        {
            return { gridLeft() + o * cellW_, cellsTop() + i * kCellH, cellW_, kCellH };
        }

        // The part of this component the hosting Viewport currently shows
        // (all of it when not in a Viewport).
        juce::Rectangle<int> visibleArea() const
        {
            if (auto* vp = findParentComponentOfClass<juce::Viewport>())
                if (vp->getViewedComponent() == this)
                    return vp->getViewArea();
            return getLocalBounds();
        }

        void beginEdit (int i, int o)
        {
            endEdit (true);
            scrollCellIntoView (i, o);

            activeIn_  = i;
            activeOut_ = o;
            editing_   = true;

            cellEditor_.setBounds (cellBounds (i, o).reduced (1));
            cellEditor_.setText (formatCell (mixer_.getMatrix (o, i), dbMode()),
                                 juce::dontSendNotification);
            cellEditor_.setVisible (true);
            cellEditor_.grabKeyboardFocus();
            cellEditor_.selectAll();
            repaint();
        }

        void endEdit (bool commit)
        {
            if (! editing_) return;
            editing_ = false;   // first: hiding the editor fires onFocusLost again

            if (commit)
            {
                const float v = parseCell (cellEditor_.getText());
                if (v != mixer_.getMatrix (activeOut_, activeIn_))
                {
                    mixer_.setMatrix (activeOut_, activeIn_, v);
                    proc_.commitHistorySnapshot();
                }
            }

            cellEditor_.setVisible (false);
            activeIn_  = -1;
            activeOut_ = -1;
            repaint();
        }

        // Move the edit by (di, do_). With wrap, running off a row's end
        // continues on the next / previous row (Tab order).
        bool moveEdit (int di, int dout, bool wrap)
        {
            int i = activeIn_ + di, o = activeOut_ + dout;
            if (wrap)
            {
                if (o >= nOut_) { o = 0;         ++i; }
                if (o < 0)      { o = nOut_ - 1; --i; }
            }
            if (! juce::isPositiveAndBelow (i, nIn_) || ! juce::isPositiveAndBelow (o, nOut_))
                return true;   // at the edge: swallow the key
            beginEdit (i, o);
            return true;
        }

        // Scroll the hosting Viewport so the cell is visible below / right of
        // the sticky headers.
        void scrollCellIntoView (int i, int o)
        {
            auto* vp = findParentComponentOfClass<juce::Viewport>();
            if (vp == nullptr || vp->getViewedComponent() != this) return;

            const auto view = vp->getViewArea();
            const auto cell = cellBounds (i, o);
            int x = view.getX(), y = view.getY();

            if (cell.getX() - kRowLabelW < x)                 x = cell.getX() - gridLeft();
            else if (cell.getRight() > view.getRight())       x = cell.getRight() - view.getWidth();
            if (cell.getY() - kCellH < y)                     y = cell.getY() - cellsTop();
            else if (cell.getBottom() > view.getBottom())     y = cell.getBottom() - view.getHeight();

            // Called before editing_ is set, so the moved() this triggers
            // doesn't end the new edit.
            if (x != view.getX() || y != view.getY())
                vp->setViewPosition (juce::jmax (0, x), juce::jmax (0, y));
        }

        bool dbMode() const { return dbToggle_.getToggleState(); }

        // Render a linear-gain matrix cell using the current display mode.
        // dB mode: signed dB, with -120 dB representing 0.0 linear (clamp floor).
        static juce::String formatCell (float v, bool dB)
        {
            if (! dB) return juce::String (v, 2);
            const float mag = std::abs (v);
            const float db  = juce::Decibels::gainToDecibels (mag, -120.0f);
            const juce::String s = juce::String (db, 1) + " dB";
            return v < 0.0f ? "-" + s : s;
        }

        // Parse what the user typed into a cell back to linear gain. Honours
        // the current display mode but also accepts the OTHER mode's syntax
        // (a trailing "dB" in linear mode → dB; otherwise raw number in dB
        // mode is interpreted as dB).
        float parseCell (const juce::String& text) const
        {
            const auto trimmed = text.trim();
            const auto lower   = trimmed.toLowerCase();
            const bool hasDb   = lower.endsWith ("db");
            const bool asDb    = dbMode() || hasDb;

            if (! asDb)
                return (float) trimmed.getDoubleValue();

            // dB → linear. In dB mode a leading "-" denotes "below 0 dB" —
            // not polarity invert.
            const double v = trimmed.getDoubleValue();
            const double mag = std::abs (v);
            const float lin = (float) juce::Decibels::decibelsToGain (mag, -120.0);
            return (! dbMode() && v < 0.0) ? -lin : lin;
        }

        /** Format: rows separated by '\n', cells in each row by '\t'.
            Rows are inputs, cols are outputs (matches the on-screen grid).
            Always linear values regardless of the dB display toggle so a
            round-trip through a spreadsheet preserves precision. */
        void copyMatrixToClipboard()
        {
            endEdit (true);
            juce::String s;
            for (int i = 0; i < nIn_; ++i)
            {
                for (int o = 0; o < nOut_; ++o)
                {
                    if (o > 0) s += "\t";
                    s += juce::String (mixer_.getMatrix (o, i), 6);
                }
                s += "\n";
            }
            juce::SystemClipboard::copyTextToClipboard (s);
        }

        /** Accepts TSV / CSV / whitespace-separated values, with row breaks
            on '\n' or ';'. Cells outside the matrix dimensions are silently
            ignored; missing cells stay at their current value. Always linear
            (matches copyMatrixToClipboard). */
        void pasteMatrixFromClipboard()
        {
            endEdit (true);
            const auto text = juce::SystemClipboard::getTextFromClipboard();
            if (text.isEmpty()) return;

            juce::StringArray rows;
            rows.addTokens (text, "\n;", "");
            rows.removeEmptyStrings();

            for (int i = 0; i < juce::jmin (nIn_, rows.size()); ++i)
            {
                juce::StringArray cells;
                cells.addTokens (rows[i], "\t,; ", "\"");
                cells.removeEmptyStrings();
                for (int o = 0; o < juce::jmin (nOut_, cells.size()); ++o)
                    mixer_.setMatrix (o, i, (float) cells[o].getDoubleValue());
            }
            proc_.commitHistorySnapshot();
            repaint();
        }

        MatrixMixerNode& mixer_;
        Mcfx_graphAudioProcessor& proc_;
        const int nIn_, nOut_;
        int cellW_ = kMinCellW;
        juce::TextButton   copyButton_, pasteButton_;
        juce::ToggleButton dbToggle_;
        CellEditor         cellEditor_;
        bool editing_  = false;
        int activeIn_  = -1;
        int activeOut_ = -1;
    };

    //==========================================================================
    class DelayPropertiesPanel : public juce::Component,
                                 private juce::Slider::Listener,
                                 private juce::Timer
    {
    public:
        DelayPropertiesPanel (DelayNode& d, double sampleRate, Mcfx_graphAudioProcessor& proc, GraphNode& gn)
            : delay_ (d), sr_ (sampleRate), proc_ (proc)
        {
            linkedToggle_.setButtonText ("Linked");
            linkedToggle_.setToggleState (delay_.isLinked(), juce::dontSendNotification);
            linkedToggle_.onClick = [this]
            {
                delay_.setLinked (linkedToggle_.getToggleState());
                if (delay_.isLinked())
                    for (auto& s : sliders_)
                        s->setValue (sliders_[0]->getValue(), juce::dontSendNotification);
                proc_.commitHistorySnapshot();
            };
            addAndMakeVisible (linkedToggle_);

            unitToggle_.setButtonText ("ms");
            unitToggle_.onClick = [this] { repaintValues(); };
            addAndMakeVisible (unitToggle_);

            for (int c = 0; c < delay_.getNumChannels(); ++c)
            {
                auto* lab = labels_.add (new juce::Label ({}, "Ch " + juce::String (c + 1)));
                lab->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.8f));
                addAndMakeVisible (lab);

                auto* s = sliders_.add (new juce::Slider());
                styleAsHSlider (*s);
                s->setRange (0.0, (double) delay_.getMaxDelaySamples(), 1.0);
                s->setTextValueSuffix (" smp");
                s->setValue (delay_.getDelaySamples (c), juce::dontSendNotification);
                s->addListener (this);
                s->onDragEnd = [this] { proc_.commitHistorySnapshot(); };
                addAndMakeVisible (s);
            }

            exposure_ = std::make_unique<ParameterExposureList> (proc_, gn);
            addAndMakeVisible (*exposure_);

            setSize (260, kHeaderHeight + (delay_.getNumChannels() + 1) * kRowHeight
                          + exposure_->getPreferredHeight());

            // Poll the node: host automation of an exposed parameter changes
            // it without passing through this panel.
            startTimerHz (15);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (4);
            auto headerRow = area.removeFromTop (kRowHeight - 2);
            linkedToggle_.setBounds (headerRow.removeFromLeft (headerRow.getWidth() / 2));
            unitToggle_  .setBounds (headerRow);

            for (int c = 0; c < sliders_.size(); ++c)
            {
                auto row = area.removeFromTop (kRowHeight);
                labels_[c] ->setBounds (row.removeFromLeft (kLabelWidth));
                sliders_[c]->setBounds (row);
            }
            if (exposure_ != nullptr && exposure_->getPreferredHeight() > 0)
                exposure_->setBounds (area.removeFromTop (exposure_->getPreferredHeight()));

        }

    private:
        void timerCallback() override
        {
            for (auto* sl : sliders_)
                if (sl->isMouseButtonDown()) return;

            const bool ms = unitToggle_.getToggleState();
            for (int c = 0; c < sliders_.size(); ++c)
            {
                const double shown = (ms && sr_ > 0.0)
                        ? (double) delay_.getDelaySamples (c) * 1000.0 / sr_
                        : (double) delay_.getDelaySamples (c);
                if (std::abs (sliders_[c]->getValue() - shown) > 1.0e-4)
                    sliders_[c]->setValue (shown, juce::dontSendNotification);
            }
            linkedToggle_.setToggleState (delay_.isLinked(), juce::dontSendNotification);
        }

        void repaintValues()
        {
            const bool ms = unitToggle_.getToggleState();
            for (int c = 0; c < sliders_.size(); ++c)
            {
                if (ms && sr_ > 0.0)
                {
                    sliders_[c]->setRange (0.0, (double) delay_.getMaxDelaySamples() * 1000.0 / sr_, 0.01);
                    sliders_[c]->setTextValueSuffix (" ms");
                    sliders_[c]->setValue (delay_.getDelaySamples (c) * 1000.0 / sr_,
                                           juce::dontSendNotification);
                }
                else
                {
                    sliders_[c]->setRange (0.0, (double) delay_.getMaxDelaySamples(), 1.0);
                    sliders_[c]->setTextValueSuffix (" smp");
                    sliders_[c]->setValue (delay_.getDelaySamples (c), juce::dontSendNotification);
                }
            }
        }

        void sliderValueChanged (juce::Slider* s) override
        {
            const bool ms = unitToggle_.getToggleState();
            for (int c = 0; c < sliders_.size(); ++c)
            {
                if (sliders_[c] == s)
                {
                    const float samples = ms && sr_ > 0.0
                                              ? (float) (s->getValue() * sr_ / 1000.0)
                                              : (float) s->getValue();
                    delay_.setDelaySamples (c, samples);
                    if (delay_.isLinked())
                        for (auto* other : sliders_)
                            if (other != s)
                                other->setValue (s->getValue(), juce::dontSendNotification);
                    break;
                }
            }
        }

        DelayNode& delay_;
        double sr_;
        Mcfx_graphAudioProcessor& proc_;
        juce::ToggleButton linkedToggle_;
        juce::ToggleButton unitToggle_;
        juce::OwnedArray<juce::Label>  labels_;
        juce::OwnedArray<juce::Slider> sliders_;
        std::unique_ptr<ParameterExposureList> exposure_;
    };

    //==========================================================================
    class SubgraphPropertiesPanel : public juce::Component
    {
    public:
        SubgraphPropertiesPanel (GraphEditorComponent& editor, GraphNode& gn)
            : editor_ (editor), node_ (gn)
        {
            descend_.setButtonText ("Edit subgraph contents");
            descend_.onClick = [this] { editor_.descendInto (node_.uuid); };
            addAndMakeVisible (descend_);

            info_.setText ("In: " + juce::String (gn.channelCountIn)
                           + "    Out: " + juce::String (gn.channelCountOut),
                           juce::dontSendNotification);
            info_.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.8f));
            addAndMakeVisible (info_);

            setSize (260, 80);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (8);
            info_.setBounds (area.removeFromTop (24));
            area.removeFromTop (4);
            descend_.setBounds (area.removeFromTop (32));
        }

    private:
        GraphEditorComponent& editor_;
        GraphNode& node_;
        juce::TextButton descend_;
        juce::Label info_;
    };

    //==========================================================================
    class PluginPropertiesPanel : public juce::Component
    {
    public:
        PluginPropertiesPanel (Mcfx_graphAudioProcessor& outerProc, GraphNode& gn)
            : outerProc_ (outerProc), node_ (gn)
        {
            // Name and channel counts are in the panel's subtitle already.
            juce::String infoText;
            if (gn.pluginDescription != nullptr)
            {
                infoText = gn.pluginDescription->pluginFormatName;
                if (gn.pluginDescription->manufacturerName.isNotEmpty())
                    infoText += (infoText.isNotEmpty() ? "    by " : "by ")
                              + gn.pluginDescription->manufacturerName;
            }
            info_.setText (infoText, juce::dontSendNotification);
            info_.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.8f));
            addAndMakeVisible (info_);

            // Any plug-in node can open an editor: its own GUI if it has one,
            // otherwise a generic parameter editor (see openEditor()).
            if (node_.processor != nullptr)
            {
                openEditor_.setButtonText (node_.processor->hasEditor() ? "Open plugin editor"
                                                                        : "Open generic editor");
                openEditor_.onClick = [this] { openEditor(); };
                addAndMakeVisible (openEditor_);
            }
            else
            {
                openEditor_.setVisible (false);
            }

            // Parameter list. The "P" toggle on each row exposes the parameter
            // to the host as one of mcfx_graph's 256 forwarding parameters.
            exposure_ = std::make_unique<ParameterExposureList> (outerProc_, node_);
            addAndMakeVisible (*exposure_);
            relayout();
        }

        void resized() override { relayout(); }

    private:
        struct AutoDeleteWindow : juce::DocumentWindow
        {
            using juce::DocumentWindow::DocumentWindow;
            void closeButtonPressed() override { delete this; }
        };

        void relayout()
        {
            auto area = getLocalBounds().reduced (8);
            info_.setBounds (area.removeFromTop (24));
            area.removeFromTop (4);

            if (openEditor_.isVisible())
            {
                openEditor_.setBounds (area.removeFromTop (32));
                area.removeFromTop (8);
            }

            const int listH = exposure_ != nullptr ? exposure_->getPreferredHeight() : 0;
            if (listH > 0)
                exposure_->setBounds (area.removeFromTop (listH));

            // Keep the component tall enough for the viewport to scroll.
            const int neededHeight = 8 + 24 + 4
                                   + (openEditor_.isVisible() ? 32 + 8 : 0)
                                   + listH
                                   + 8;
            if (getHeight() != neededHeight)
                setSize (juce::jmax (260, getWidth()), neededHeight);
        }

        // Tracking variant of AutoDeleteWindow — registers itself with the
        // outer plug-in's window registry so the processor can close it
        // before destroying the inner plug-in instance (see
        // PluginProcessor::registerPluginWindow).
        struct TrackedPluginWindow : juce::DocumentWindow
        {
            TrackedPluginWindow (Mcfx_graphAudioProcessor& outer,
                                  juce::Uuid nodeUuid,
                                  const juce::String& name,
                                  juce::Colour bg,
                                  int requiredButtons,
                                  bool addToDesktop)
                : juce::DocumentWindow (name, bg, requiredButtons, addToDesktop),
                  outer_ (outer),
                  nodeUuid_ (nodeUuid)
            {
                outer_.registerPluginWindow (nodeUuid_, this);
            }

            ~TrackedPluginWindow() override
            {
                outer_.unregisterPluginWindow (this);
            }

            void closeButtonPressed() override { delete this; }

        private:
            Mcfx_graphAudioProcessor& outer_;
            juce::Uuid nodeUuid_;
        };

        void openEditor()
        {
            auto* p = node_.processor;
            if (p == nullptr) return;

            // Prefer the plug-in's own GUI; fall back to a generic editor built
            // from its parameters when it has none (e.g. LADSPA, or LV2/VST3
            // without an embeddable UI for this platform).
            juce::AudioProcessorEditor* ed = p->hasEditor() ? p->createEditorAndMakeActive() : nullptr;
            const bool isGeneric = (ed == nullptr);
            if (isGeneric)
                ed = new juce::GenericAudioProcessorEditor (*p);

            const auto title = node_.displayName.isNotEmpty() ? node_.displayName : p->getName();
            auto* dw = new TrackedPluginWindow (outerProc_, node_.uuid,
                                                 title, juce::Colours::darkgrey,
                                                 juce::DocumentWindow::closeButton, true);
            dw->setUsingNativeTitleBar (true);
            dw->setContentOwned (ed, true);
            dw->setResizable (isGeneric, false);
            dw->setAlwaysOnTop (true);
            dw->centreWithSize (ed->getWidth(), ed->getHeight());
            dw->setVisible (true);
            dw->toFront (true);
        }

        Mcfx_graphAudioProcessor& outerProc_;
        GraphNode& node_;
        juce::Label  info_;
        juce::TextButton openEditor_;
        std::unique_ptr<ParameterExposureList> exposure_;
    };
}

//==============================================================================

NodePropertiesPanel::NodePropertiesPanel (GraphEditorComponent& editor)
    : editor_ (editor)
{
    titleLabel_.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    titleLabel_.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel_.setTooltip ("Double-click to rename this node");
    titleLabel_.onTextChange = [this] { commitRename(); };
    addAndMakeVisible (titleLabel_);

    subtitleLabel_.setFont (juce::Font (juce::FontOptions (11.0f)));
    subtitleLabel_.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    addAndMakeVisible (subtitleLabel_);

    viewport_ = std::make_unique<juce::Viewport>();
    // Horizontal scroll is enabled because the matrix-mixer body width grows
    // linearly with output count and routinely exceeds the panel; other
    // parameter views have a fixed natural width and won't trigger the bar.
    viewport_->setScrollBarsShown (true, true);
    addAndMakeVisible (*viewport_);

    rebuildContent();
}

NodePropertiesPanel::~NodePropertiesPanel() = default;

void NodePropertiesPanel::setSelectedNode (GraphNode* node)
{
    selectedNode_ = node;
    rebuildContent();
}

void NodePropertiesPanel::commitRename()
{
    if (selectedNode_ == nullptr) return;
    const auto isTerminal = selectedNode_->kind == NodeKind::InputTerminal
                         || selectedNode_->kind == NodeKind::OutputTerminal;
    if (isTerminal) return;

    const auto uuid = selectedNode_->uuid;
    editor_.getActiveController().setNodeDisplayName (uuid, titleLabel_.getText());

    // Reflect the trimmed/normalized name (and the kind-string fallback when
    // the user cleared the field) back into the label without retriggering.
    titleLabel_.setText (selectedNode_->displayName.isNotEmpty()
                            ? selectedNode_->displayName
                            : juce::String (nodeKindToString (selectedNode_->kind)),
                         juce::dontSendNotification);
}

void NodePropertiesPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181820));
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawVerticalLine (0, 0.0f, (float) getHeight());
}

void NodePropertiesPanel::resized()
{
    auto area = getLocalBounds().reduced (8);
    titleLabel_.setBounds    (area.removeFromTop (24));
    subtitleLabel_.setBounds (area.removeFromTop (16));
    area.removeFromTop (6);
    viewport_->setBounds (area);

    if (body_ != nullptr)
        body_->setSize (juce::jmax (body_->getWidth(),
                                    viewport_->getWidth() - viewport_->getScrollBarThickness() - 4),
                        body_->getHeight());
}

void NodePropertiesPanel::rebuildContent()
{
    body_.reset();
    viewport_->setViewedComponent (nullptr, false);

    const auto selSize = editor_.getSelection().size();

    if (selectedNode_ == nullptr)
    {
        titleLabel_.setEditable (false, false, false);
        titleLabel_.setText ("(no selection)", juce::dontSendNotification);
        subtitleLabel_.setText ("Click a node to edit its properties.",
                                juce::dontSendNotification);
        resized();
        return;
    }

    if (selSize > 1)
    {
        titleLabel_.setEditable (false, false, false);
        titleLabel_.setText ("(" + juce::String ((int) selSize) + " nodes selected)",
                             juce::dontSendNotification);
        subtitleLabel_.setText ("Press C to chain-connect, Delete to remove all, "
                                "or click a single node to edit its properties.",
                                juce::dontSendNotification);
        resized();
        return;
    }

    const bool isTerminal = selectedNode_->kind == NodeKind::InputTerminal
                         || selectedNode_->kind == NodeKind::OutputTerminal;

    // Single-click does nothing (preserves selection click); double-click enters
    // edit mode; loseFocusOnEnter=false so clicks elsewhere commit, not discard.
    titleLabel_.setEditable (false, ! isTerminal, false);

    titleLabel_.setText (selectedNode_->displayName.isNotEmpty()
                            ? selectedNode_->displayName
                            : juce::String (nodeKindToString (selectedNode_->kind)),
                         juce::dontSendNotification);
    // A plug-in node is named by its plug-in, a native node by its type.
    auto kindText = juce::String (nodeKindToString (selectedNode_->kind));
    if (selectedNode_->kind == NodeKind::Plugin)
    {
        if (selectedNode_->pluginDescription != nullptr && selectedNode_->pluginDescription->name.isNotEmpty())
            kindText = selectedNode_->pluginDescription->name;
        else if (selectedNode_->processor != nullptr)
            kindText = selectedNode_->processor->getName();
    }
    subtitleLabel_.setText (kindText
                                + "  •  in " + juce::String (selectedNode_->channelCountIn)
                                + " / out " + juce::String (selectedNode_->channelCountOut),
                            juce::dontSendNotification);

    body_ = createBodyForNode (editor_, *selectedNode_);

    if (body_ != nullptr)
        viewport_->setViewedComponent (body_.get(), false);

    resized();
}

std::unique_ptr<juce::Component>
NodePropertiesPanel::createBodyForNode (GraphEditorComponent& editor, GraphNode& node)
{
    auto& proc = editor.getProcessor();
    switch (node.kind)
    {
        case NodeKind::Gain:
            if (auto* g = dynamic_cast<GainNode*> (node.processor))
                return std::make_unique<GainPropertiesPanel> (*g, proc, node);
            return nullptr;
        case NodeKind::MutePhase:
            if (auto* m = dynamic_cast<MutePhaseNode*> (node.processor))
                return std::make_unique<MutePhasePropertiesPanel> (*m, proc, node);
            return nullptr;
        case NodeKind::MatrixMixer:
            if (auto* r = dynamic_cast<MatrixMixerNode*> (node.processor))
                return std::make_unique<MatrixMixerPropertiesPanel> (*r, proc);
            return nullptr;
        case NodeKind::Delay:
            if (auto* d = dynamic_cast<DelayNode*> (node.processor))
                return std::make_unique<DelayPropertiesPanel> (
                            *d, editor.getActiveController().getInputChannelCount() > 0
                                  ? d->getSampleRate()
                                  : 48000.0,
                            proc, node);
            return nullptr;
        case NodeKind::Subgraph:
            return std::make_unique<SubgraphPropertiesPanel> (editor, node);
        case NodeKind::Plugin:
            return std::make_unique<PluginPropertiesPanel> (editor.getProcessor(), node);
        case NodeKind::FeedbackSend:
        case NodeKind::FeedbackReturn:
            // Width is changed from the node's own channel-count popup and the
            // pairing by dragging the link pin, so there is nothing extra to
            // edit here. The shared header still shows In/Out.
            return nullptr;
        case NodeKind::InputTerminal:
        case NodeKind::OutputTerminal:
            // Terminals have no editable properties — the channel count comes
            // from the host bus.
            return nullptr;
    }
    return nullptr;
}
