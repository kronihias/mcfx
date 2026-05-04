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
    // Per-kind subpanels. Each builds its own children in the constructor and
    // lays them out in resized().

    class GainPropertiesPanel : public juce::Component, private juce::Slider::Listener
    {
    public:
        GainPropertiesPanel (GainNode& g, Mcfx_graphAudioProcessor& proc)
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

            setSize (260, kHeaderHeight + (gain_.getNumChannels() + 2) * kRowHeight);
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
    };

    //==========================================================================
    class MutePhasePropertiesPanel : public juce::Component
    {
    public:
        MutePhasePropertiesPanel (MutePhaseNode& m, Mcfx_graphAudioProcessor& proc)
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

            setSize (260, kHeaderHeight + (mp_.getNumChannels() + 1) * kRowHeight);
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
        }

    private:
        MutePhaseNode& mp_;
        Mcfx_graphAudioProcessor& proc_;
        juce::ToggleButton                   linkedToggle_;
        juce::OwnedArray<juce::Label>        labels_;
        juce::OwnedArray<juce::ToggleButton> muteButtons_;
        juce::OwnedArray<juce::ToggleButton> invButtons_;
    };

    //==========================================================================
    class MatrixMixerPropertiesPanel : public juce::Component
    {
    public:
        MatrixMixerPropertiesPanel (MatrixMixerNode& r, Mcfx_graphAudioProcessor& proc)
            : mixer_ (r), proc_ (proc)
        {
            const int nIn  = mixer_.getNumIn();
            const int nOut = mixer_.getNumOut();

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
            dbToggle_.onClick = [this] { repaintCells(); };
            addAndMakeVisible (dbToggle_);

            // Header row: empty cell + "Out 1".."Out N"
            for (int o = 0; o < nOut; ++o)
            {
                auto* h = colHeaders_.add (new juce::Label ({}, "O" + juce::String (o + 1)));
                h->setJustificationType (juce::Justification::centred);
                h->setColour (juce::Label::textColourId, juce::Colours::orange);
                addAndMakeVisible (h);
            }

            for (int i = 0; i < nIn; ++i)
            {
                auto* rh = rowHeaders_.add (new juce::Label ({}, "In " + juce::String (i + 1)));
                rh->setJustificationType (juce::Justification::centredRight);
                rh->setColour (juce::Label::textColourId, juce::Colours::lightblue);
                addAndMakeVisible (rh);

                for (int o = 0; o < nOut; ++o)
                {
                    auto* ed = cellEditors_.add (new MatrixCellEditor());
                    ed->setMultiLine (false);
                    ed->setText (formatCell (mixer_.getMatrix (o, i)),
                                 juce::dontSendNotification);
                    ed->setJustification (juce::Justification::centred);
                    ed->onReturnKey  = [this, ed, i, o] { commitCell (ed, i, o); };
                    ed->onFocusLost  = [this, ed, i, o]
                    {
                        commitCell (ed, i, o);
                        clearHeaderHighlight (i, o);
                    };
                    ed->onFocusGained = [this, i, o] { highlightHeaders (i, o); };
                    addAndMakeVisible (ed);
                }
            }

            setSize (juce::jmax (260, 48 + nOut * 76),
                     kHeaderHeight + (nIn + 2) * kRowHeight + 8);
        }

        void resized() override
        {
            const int nIn  = mixer_.getNumIn();
            const int nOut = mixer_.getNumOut();
            if (nIn == 0 || nOut == 0) return;

            auto area = getLocalBounds().reduced (4);

            // Toolbar: Copy / Paste / dB
            auto toolbar = area.removeFromTop (kRowHeight);
            copyButton_ .setBounds (toolbar.removeFromLeft (60));
            toolbar.removeFromLeft (4);
            pasteButton_.setBounds (toolbar.removeFromLeft (60));
            toolbar.removeFromLeft (10);
            dbToggle_   .setBounds (toolbar.removeFromLeft (50));
            area.removeFromTop (4);

            const int rowLabelW = 48;
            // Min cell width fits "-120.0 dB" with a bit of padding so the
            // dB display mode never clips.
            const int cellW     = juce::jmax (72, (area.getWidth() - rowLabelW) / nOut);

            // Column headers
            auto headerRow = area.removeFromTop (kRowHeight);
            headerRow.removeFromLeft (rowLabelW);
            for (int o = 0; o < nOut; ++o)
                colHeaders_[o]->setBounds (headerRow.removeFromLeft (cellW));

            // Rows
            for (int i = 0; i < nIn; ++i)
            {
                auto row = area.removeFromTop (kRowHeight);
                rowHeaders_[i]->setBounds (row.removeFromLeft (rowLabelW));
                for (int o = 0; o < nOut; ++o)
                    cellEditors_[i * nOut + o]->setBounds (row.removeFromLeft (cellW).reduced (1));
            }
        }

    private:
        // TextEditor subclass that exposes focusGained as a std::function,
        // matching the style of the built-in onFocusLost. Used so the matrix
        // panel can highlight the row / column header for the focused cell.
        struct MatrixCellEditor : juce::TextEditor
        {
            std::function<void()> onFocusGained;
            void focusGained (FocusChangeType t) override
            {
                juce::TextEditor::focusGained (t);
                if (onFocusGained) onFocusGained();
            }
        };

        static juce::Colour rowDefaultColour() { return juce::Colours::lightblue; }
        static juce::Colour colDefaultColour() { return juce::Colours::orange; }
        static juce::Colour rowFocusedColour() { return juce::Colours::yellow; }
        static juce::Colour colFocusedColour() { return juce::Colours::yellow; }

        void highlightHeaders (int inCh, int outCh)
        {
            activeIn_  = inCh;
            activeOut_ = outCh;
            applyHeaderColours();
        }

        // Only revert if no other cell has stolen focus in the meantime —
        // when tabbing between cells, focusLost fires on the old cell BEFORE
        // focusGained on the new one, so a blanket clear here would briefly
        // reset the state the new cell is about to set.
        void clearHeaderHighlight (int inCh, int outCh)
        {
            if (activeIn_ == inCh && activeOut_ == outCh)
            {
                activeIn_  = -1;
                activeOut_ = -1;
                applyHeaderColours();
            }
        }

        void applyHeaderColours()
        {
            for (int i = 0; i < rowHeaders_.size(); ++i)
                rowHeaders_[i]->setColour (juce::Label::textColourId,
                                            i == activeIn_ ? rowFocusedColour()
                                                            : rowDefaultColour());
            for (int o = 0; o < colHeaders_.size(); ++o)
                colHeaders_[o]->setColour (juce::Label::textColourId,
                                            o == activeOut_ ? colFocusedColour()
                                                             : colDefaultColour());
        }

        bool dbMode() const { return dbToggle_.getToggleState(); }

        // Render a linear-gain matrix cell using the current display mode.
        // dB mode: signed dB, with -120 dB representing 0.0 linear (clamp floor).
        juce::String formatCell (float v) const
        {
            if (! dbMode()) return juce::String (v, 2);
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

            // dB → linear, preserving sign.
            const double v = trimmed.getDoubleValue();
            const double mag = std::abs (v);
            // Preserve sign written in front of an explicit "dB" suffix only
            // when the user typed a negative magnitude into linear-mode (we
            // shouldn't get here in that path anyway). In dB mode a leading
            // "-" denotes "below 0 dB" — not polarity invert.
            const float lin = (float) juce::Decibels::decibelsToGain (mag, -120.0);
            return (! dbMode() && v < 0.0) ? -lin : lin;
        }

        void repaintCells()
        {
            const int nIn  = mixer_.getNumIn();
            const int nOut = mixer_.getNumOut();
            for (int i = 0; i < nIn; ++i)
                for (int o = 0; o < nOut; ++o)
                    if (auto* ed = cellEditors_[i * nOut + o])
                        ed->setText (formatCell (mixer_.getMatrix (o, i)),
                                     juce::dontSendNotification);
        }

        void commitCell (juce::TextEditor* ed, int inCh, int outCh)
        {
            const float v = parseCell (ed->getText());
            mixer_.setMatrix (outCh, inCh, v);
            ed->setText (formatCell (v), juce::dontSendNotification);
            proc_.commitHistorySnapshot();
        }

        /** Format: rows separated by '\n', cells in each row by '\t'.
            Rows are inputs, cols are outputs (matches the on-screen grid).
            Always linear values regardless of the dB display toggle so a
            round-trip through a spreadsheet preserves precision. */
        void copyMatrixToClipboard()
        {
            const int nIn  = mixer_.getNumIn();
            const int nOut = mixer_.getNumOut();
            juce::String s;
            for (int i = 0; i < nIn; ++i)
            {
                for (int o = 0; o < nOut; ++o)
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
            const auto text = juce::SystemClipboard::getTextFromClipboard();
            if (text.isEmpty()) return;

            const int nIn  = mixer_.getNumIn();
            const int nOut = mixer_.getNumOut();

            juce::StringArray rows;
            rows.addTokens (text, "\n;", "");
            rows.removeEmptyStrings();

            for (int i = 0; i < juce::jmin (nIn, rows.size()); ++i)
            {
                juce::StringArray cells;
                cells.addTokens (rows[i], "\t,; ", "\"");
                cells.removeEmptyStrings();
                for (int o = 0; o < juce::jmin (nOut, cells.size()); ++o)
                {
                    const float v = (float) cells[o].getDoubleValue();
                    mixer_.setMatrix (o, i, v);
                    if (auto* ed = cellEditors_[i * nOut + o])
                        ed->setText (formatCell (v), juce::dontSendNotification);
                }
            }
            proc_.commitHistorySnapshot();
        }

        MatrixMixerNode& mixer_;
        Mcfx_graphAudioProcessor& proc_;
        juce::TextButton                       copyButton_, pasteButton_;
        juce::ToggleButton                     dbToggle_;
        juce::OwnedArray<juce::Label>          colHeaders_;
        juce::OwnedArray<juce::Label>          rowHeaders_;
        juce::OwnedArray<MatrixCellEditor>     cellEditors_;
        int activeIn_  = -1;
        int activeOut_ = -1;
    };

    //==========================================================================
    class DelayPropertiesPanel : public juce::Component, private juce::Slider::Listener
    {
    public:
        DelayPropertiesPanel (DelayNode& d, double sampleRate, Mcfx_graphAudioProcessor& proc)
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

            setSize (260, kHeaderHeight + (delay_.getNumChannels() + 1) * kRowHeight);
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
        }

    private:
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
            juce::String infoText = "In: " + juce::String (gn.channelCountIn)
                                  + "    Out: " + juce::String (gn.channelCountOut);
            if (gn.pluginDescription != nullptr
                && gn.pluginDescription->pluginFormatName.isNotEmpty())
            {
                infoText += "    Format: " + gn.pluginDescription->pluginFormatName;
                if (gn.pluginDescription->manufacturerName.isNotEmpty())
                    infoText += "    by " + gn.pluginDescription->manufacturerName;
            }
            info_.setText (infoText, juce::dontSendNotification);
            info_.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.8f));
            addAndMakeVisible (info_);

            const bool canOpen = (node_.processor != nullptr && node_.processor->hasEditor());
            if (canOpen)
            {
                openEditor_.setButtonText ("Open plugin editor");
                openEditor_.onClick = [this] { openEditor(); };
                addAndMakeVisible (openEditor_);
            }
            else
            {
                openEditor_.setVisible (false);
            }

            // Parameter list. The "P" toggle on each row exposes the parameter
            // to the host as one of mcfx_graph's 256 forwarding parameters.
            buildParameterList();
            relayout();
        }

        void resized() override { relayout(); }

    private:
        struct AutoDeleteWindow : juce::DocumentWindow
        {
            using juce::DocumentWindow::DocumentWindow;
            void closeButtonPressed() override { delete this; }
        };

        struct ParamRow
        {
            int                    paramIndex = -1;
            juce::Label            nameLabel;
            juce::TextButton       exposeButton;
        };

        void buildParameterList()
        {
            paramRows_.clear();

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

        void onExposeClicked (int paramIndex)
        {
            const int currentSlot = outerProc_.getExposedSlotFor (node_.uuid, paramIndex);
            if (currentSlot >= 0)
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

            if (! paramRows_.empty())
            {
                paramsHeader_.setBounds (area.removeFromTop (18));
                slotsLabel_.setBounds   (area.removeFromTop (14));
                area.removeFromTop (4);

                constexpr int rowH = 22;
                for (auto& row : paramRows_)
                {
                    auto r = area.removeFromTop (rowH);
                    row->exposeButton.setBounds (r.removeFromRight (44).reduced (1));
                    row->nameLabel  .setBounds (r);
                }
            }

            // Keep the component tall enough for the viewport to scroll.
            const int neededHeight = 8 + 24 + 4
                                   + (openEditor_.isVisible() ? 32 + 8 : 0)
                                   + (paramRows_.empty() ? 0 : 18 + 14 + 4 + (int) paramRows_.size() * 22)
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
            if (p == nullptr || ! p->hasEditor()) return;
            auto* ed = p->createEditor();
            if (ed == nullptr) return;

            const auto title = node_.displayName.isNotEmpty() ? node_.displayName : p->getName();
            auto* dw = new TrackedPluginWindow (outerProc_, node_.uuid,
                                                 title, juce::Colours::darkgrey,
                                                 juce::DocumentWindow::closeButton, true);
            dw->setUsingNativeTitleBar (true);
            dw->setContentOwned (ed, true);
            dw->setResizable (false, false);
            dw->setAlwaysOnTop (true);
            dw->centreWithSize (ed->getWidth(), ed->getHeight());
            dw->setVisible (true);
            dw->toFront (true);
        }

        Mcfx_graphAudioProcessor& outerProc_;
        GraphNode& node_;
        juce::Label  info_;
        juce::Label  paramsHeader_;
        juce::Label  slotsLabel_;
        juce::TextButton openEditor_;
        std::vector<std::unique_ptr<ParamRow>> paramRows_;
    };
}

//==============================================================================

NodePropertiesPanel::NodePropertiesPanel (GraphEditorComponent& editor)
    : editor_ (editor)
{
    titleLabel_.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    titleLabel_.setColour (juce::Label::textColourId, juce::Colours::white);
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
        titleLabel_.setText ("(no selection)", juce::dontSendNotification);
        subtitleLabel_.setText ("Click a node to edit its properties.",
                                juce::dontSendNotification);
        resized();
        return;
    }

    if (selSize > 1)
    {
        titleLabel_.setText ("(" + juce::String ((int) selSize) + " nodes selected)",
                             juce::dontSendNotification);
        subtitleLabel_.setText ("Press C to chain-connect, Delete to remove all, "
                                "or click a single node to edit its properties.",
                                juce::dontSendNotification);
        resized();
        return;
    }

    titleLabel_.setText (selectedNode_->displayName.isNotEmpty()
                            ? selectedNode_->displayName
                            : juce::String (nodeKindToString (selectedNode_->kind)),
                         juce::dontSendNotification);
    subtitleLabel_.setText (juce::String (nodeKindToString (selectedNode_->kind))
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
                return std::make_unique<GainPropertiesPanel> (*g, proc);
            return nullptr;
        case NodeKind::MutePhase:
            if (auto* m = dynamic_cast<MutePhaseNode*> (node.processor))
                return std::make_unique<MutePhasePropertiesPanel> (*m, proc);
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
                            proc);
            return nullptr;
        case NodeKind::Subgraph:
            return std::make_unique<SubgraphPropertiesPanel> (editor, node);
        case NodeKind::Plugin:
            return std::make_unique<PluginPropertiesPanel> (editor.getProcessor(), node);
        case NodeKind::InputTerminal:
        case NodeKind::OutputTerminal:
            // Terminals have no editable properties — the channel count comes
            // from the host bus.
            return nullptr;
    }
    return nullptr;
}
