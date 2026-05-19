#include "NodeComponent.h"
#include "GraphEditorComponent.h"
#include "NodePropertiesPanel.h"
#include "../Graph/GraphController.h"
#include "../Graph/SubgraphNode.h"
#include "../NativeNodes/GainNode.h"
#include "../NativeNodes/MutePhaseNode.h"
#include "../NativeNodes/MatrixMixerNode.h"
#include "../NativeNodes/DelayNode.h"
#include "../Hosting/PluginInstanceFactory.h"
#include "../Graph/GraphClipboard.h"
#include "../PluginProcessor.h"

namespace
{
    constexpr int kPinSize    = 12;
    constexpr int kPinSpacing = 16;
    constexpr int kHeaderH    = 24;
    constexpr int kPaddingX   = 16;
    constexpr int kMinWidth   = 140;
}

NodeComponent::NodeComponent (GraphEditorComponent& editor, GraphNode& node)
    : editor_ (editor), node_ (node)
{
    rebuildPins();
    setSize (kMinWidth, kHeaderH + 16 + juce::jmax (1,
                       juce::jmax (node.channelCountIn, node.channelCountOut)) * kPinSpacing);
    setTopLeftPosition (node.editorPosition);
    setInterceptsMouseClicks (true, true);
}

void NodeComponent::rebuildPins()
{
    inputPins_.clear();
    outputPins_.clear();

    for (int c = 0; c < node_.channelCountIn; ++c)
        addAndMakeVisible (inputPins_.add (
            new PinComponent (editor_, node_.uuid, PinComponent::Direction::Input, c)));

    for (int c = 0; c < node_.channelCountOut; ++c)
        addAndMakeVisible (outputPins_.add (
            new PinComponent (editor_, node_.uuid, PinComponent::Direction::Output, c)));

    const int rows = juce::jmax (1, juce::jmax (node_.channelCountIn, node_.channelCountOut));
    setSize (kMinWidth, kHeaderH + 16 + rows * kPinSpacing);
    resized();
}

void NodeComponent::resized()
{
    const int w = getWidth();
    const int firstY = kHeaderH + 8;

    for (int i = 0; i < inputPins_.size(); ++i)
        inputPins_[i]->setBounds (2, firstY + i * kPinSpacing, kPinSize, kPinSize);

    for (int i = 0; i < outputPins_.size(); ++i)
        outputPins_[i]->setBounds (w - kPinSize - 2, firstY + i * kPinSpacing, kPinSize, kPinSize);
}

void NodeComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    juce::Colour fill;
    switch (node_.kind)
    {
        case NodeKind::Plugin:         fill = juce::Colour (0xff305078); break;
        case NodeKind::Gain:           fill = juce::Colour (0xff446644); break;
        case NodeKind::MutePhase:      fill = juce::Colour (0xff666644); break;
        case NodeKind::MatrixMixer:         fill = juce::Colour (0xff5a3a6a); break;
        case NodeKind::Delay:          fill = juce::Colour (0xff6a4a3a); break;
        case NodeKind::Subgraph:       fill = juce::Colour (0xff444466); break;
        case NodeKind::InputTerminal:  fill = juce::Colour (0xff223344); break;
        case NodeKind::OutputTerminal: fill = juce::Colour (0xff332233); break;
    }

    g.setColour (fill);
    g.fillRoundedRectangle (area, 6.0f);

    const bool isSelected = editor_.isSelected (node_.uuid);
    if (isSelected)
    {
        g.setColour (juce::Colours::yellow);
        g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 2.5f);
    }
    else
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.0f);
    }

    // Right-aligned format chip for plugin nodes (VST / VST3 / AudioUnit / ...).
    // Painted first so the title text below knows how much horizontal room
    // is left for it.
    float titleRightPadding = 8.0f;
    if (node_.kind == NodeKind::Plugin && node_.pluginDescription != nullptr
        && node_.pluginDescription->pluginFormatName.isNotEmpty())
    {
        const auto fmt = node_.pluginDescription->pluginFormatName;
        const auto chipFont = juce::Font (juce::FontOptions (9.5f, juce::Font::bold));
        const int  textW = juce::GlyphArrangement::getStringWidthInt (chipFont, fmt);
        const float chipW = (float) (textW + 10);
        const float chipH = 14.0f;

        auto chipRect = juce::Rectangle<float> (
            area.getRight() - chipW - 6.0f,
            area.getY() + (kHeaderH - chipH) * 0.5f,
            chipW, chipH);

        // Format-specific tint so VST / VST3 / AU are visually distinct.
        auto chipColour = juce::Colour (0xff556080);
        if      (fmt == "VST3")           chipColour = juce::Colour (0xff5077a8);
        else if (fmt == "VST")            chipColour = juce::Colour (0xff7a5a3a);
        else if (fmt == "AudioUnit")      chipColour = juce::Colour (0xff5a8060);
        else if (fmt == "LV2")            chipColour = juce::Colour (0xff805a80);

        g.setColour (chipColour);
        g.fillRoundedRectangle (chipRect, 3.0f);
        g.setColour (juce::Colours::white);
        g.setFont (chipFont);
        g.drawText (fmt, chipRect, juce::Justification::centred, false);

        titleRightPadding = chipW + 14.0f;
    }

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));

    auto title = node_.displayName.isNotEmpty()
                 ? node_.displayName
                 : juce::String (node_.processor != nullptr
                                 ? node_.processor->getName()
                                 : juce::String ("Node"));

    auto titleArea = area.withHeight ((float) kHeaderH);
    titleArea.removeFromLeft  (8.0f);
    titleArea.removeFromRight (titleRightPadding);
    g.drawText (title, titleArea, juce::Justification::centredLeft, true);

    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.setColour (juce::Colours::white.withAlpha (0.6f));

    auto detail = juce::String (node_.channelCountIn) + " in / "
                + juce::String (node_.channelCountOut) + " out";
    g.drawText (detail,
                area.withTrimmedTop ((float) kHeaderH - 6.0f).withHeight (16.0f).reduced (8.0f, 2.0f),
                juce::Justification::centredLeft, true);

    // Channel labels next to pins
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    const int firstY = kHeaderH + 8;

    for (int i = 0; i < node_.channelCountIn; ++i)
        g.drawText (juce::String (i + 1),
                    juce::Rectangle<int> (kPinSize + 4, firstY + i * kPinSpacing, 24, kPinSize),
                    juce::Justification::centredLeft, false);

    for (int i = 0; i < node_.channelCountOut; ++i)
        g.drawText (juce::String (i + 1),
                    juce::Rectangle<int> (getWidth() - kPinSize - 28, firstY + i * kPinSpacing,
                                          24, kPinSize),
                    juce::Justification::centredRight, false);

    // Bypass / Mute visual indicators. Mute takes visual precedence (a muted
    // node is also bypassed internally).
    if (node_.muted || node_.bypassed)
    {
        const auto label = node_.muted ? juce::String ("MUTE") : juce::String ("BYPASS");
        const auto chipColour = node_.muted ? juce::Colour (0xffaa3030)
                                            : juce::Colour (0xff707030);

        // Diagonal hash overlay so the node looks visibly de-activated.
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (area, 6.0f);

        // Center label
        const auto chipFont = juce::Font (juce::FontOptions (11.0f, juce::Font::bold));
        const int  textW = juce::GlyphArrangement::getStringWidthInt (chipFont, label);
        const float chipW = (float) (textW + 14);
        const float chipH = 18.0f;
        auto chipRect = juce::Rectangle<float> (
            area.getCentreX() - chipW * 0.5f,
            area.getBottom() - chipH - 4.0f,
            chipW, chipH);

        g.setColour (chipColour);
        g.fillRoundedRectangle (chipRect, 4.0f);
        g.setColour (juce::Colours::white);
        g.setFont (chipFont);
        g.drawText (label, chipRect, juce::Justification::centred, false);
    }
}

PinComponent* NodeComponent::findPin (PinComponent::Direction dir, int channelIndex) const
{
    auto& list = (dir == PinComponent::Direction::Input) ? inputPins_ : outputPins_;
    for (auto* p : list)
        if (p->getChannelIndex() == channelIndex)
            return p;
    return nullptr;
}

void NodeComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showContextMenu();
        return;
    }

    // Selection rules:
    //  - Shift/Cmd-click toggles this node in/out of the selection.
    //  - Plain click on an unselected node makes it the only selection.
    //  - Plain click on an already-selected node leaves the selection alone
    //    (so the user can drag the whole group without losing it).
    if (e.mods.isShiftDown() || e.mods.isCommandDown())
    {
        editor_.toggleSelection (node_.uuid);
    }
    else if (! editor_.isSelected (node_.uuid))
    {
        editor_.setSelectedNode (node_.uuid);
    }

    dragStartPosition_ = node_.editorPosition;
    didDrag_ = false;
    dragger_.startDraggingComponent (this, e);
}

void NodeComponent::mouseDrag (const juce::MouseEvent& e)
{
    const auto worldBefore = node_.editorPosition;

    dragger_.dragComponent (this, e, nullptr);

    // Translate the canvas-local position (post-zoom) back to world coords
    // on the GraphNode. Pan is handled by the surrounding Viewport so it
    // doesn't enter the equation here.
    const float zoom = editor_.getZoom();
    const auto  cur  = getPosition();
    node_.editorPosition.x = juce::roundToInt (cur.x / zoom);
    node_.editorPosition.y = juce::roundToInt (cur.y / zoom);

    // If multiple nodes are selected and this one is among them, move every
    // other selected node by the same world-space delta — group drag.
    const auto delta = node_.editorPosition - worldBefore;
    const auto& sel  = editor_.getSelection();
    if (delta != juce::Point<int>{} && sel.size() > 1 && editor_.isSelected (node_.uuid))
    {
        for (const auto& uuid : sel)
        {
            if (uuid == node_.uuid) continue;
            if (auto* gn = editor_.getController().getNode (uuid))
            {
                gn->editorPosition += delta;
                editor_.getController().setNodePosition (uuid, gn->editorPosition);
            }
        }
    }

    if (node_.editorPosition != worldBefore)
        didDrag_ = true;

    editor_.onNodeMoved (*this);
}

void NodeComponent::mouseUp (const juce::MouseEvent&)
{
    // Drags don't fire notifyTopologyChanged (move is metadata-only), so we
    // explicitly commit a snapshot at drag-end. Without this, node moves
    // weren't undoable. Skip the commit if the click didn't actually move
    // anything — otherwise plain selection clicks would pollute history.
    if (didDrag_ && node_.editorPosition != dragStartPosition_)
        editor_.getProcessor().commitHistorySnapshot();
    didDrag_ = false;
}

void NodeComponent::mouseDoubleClick (const juce::MouseEvent&)
{
    switch (node_.kind)
    {
        case NodeKind::Plugin:
            openPluginEditor();
            break;
        case NodeKind::Subgraph:
            descendIntoSubgraph();
            break;
        case NodeKind::Gain:
        case NodeKind::MutePhase:
        case NodeKind::MatrixMixer:
        case NodeKind::Delay:
            openNativePropertiesWindow();
            break;
        case NodeKind::InputTerminal:
        case NodeKind::OutputTerminal:
            break;
    }
}

namespace
{
    struct AutoDeleteWindow : juce::DocumentWindow
    {
        using juce::DocumentWindow::DocumentWindow;
        void closeButtonPressed() override { delete this; }
    };

    // Plugin GUI window that registers itself with the outer plug-in's window
    // registry on construction and removes itself on destruction, so the
    // processor knows to close it BEFORE destroying the inner plug-in
    // instance (some plug-ins crash if their editor is still mounted when
    // the AudioProcessor is freed). Used only for hosted-plugin GUIs —
    // native-node property windows are owned by the editor lifetime and
    // don't outlive the node.
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
}

void NodeComponent::openPluginEditor()
{
    auto* p = node_.processor;
    if (p == nullptr || ! p->hasEditor()) return;

    auto* ed = p->createEditorAndMakeActive();
    if (ed == nullptr) return;

    const auto title = node_.displayName.isNotEmpty() ? node_.displayName : p->getName();

    auto* dw = new TrackedPluginWindow (editor_.getProcessor(), node_.uuid,
                                         title, juce::Colours::darkgrey,
                                         juce::DocumentWindow::closeButton, true);
    dw->setUsingNativeTitleBar (true);
    dw->setContentOwned (ed, true);
    dw->setResizable (false, false);
    // Always-on-top so the plugin GUI doesn't get covered by the DAW window
    // or the mcfx_graph editor that owns the host context. Without this,
    // some hosts (Reaper) keep the plugin GUI behind the FX chain window.
    dw->setAlwaysOnTop (true);
    dw->centreWithSize (ed->getWidth(), ed->getHeight());
    dw->setVisible (true);
    dw->toFront (true);
}

void NodeComponent::descendIntoSubgraph()
{
    if (dynamic_cast<SubgraphNode*> (node_.processor) != nullptr)
        editor_.descendInto (node_.uuid);
}

namespace
{
    // Floating-window content: title + body wrapped in a Viewport so the body
    // can keep its natural size (matrix-mixer bodies grow linearly with output
    // count and easily exceed the screen) while the window itself is bounded
    // to fit and scrollbars handle overflow.
    class FloatingNodeEditor : public juce::Component
    {
    public:
        FloatingNodeEditor (GraphEditorComponent& editor, GraphNode& node)
        {
            title_.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
            title_.setColour (juce::Label::textColourId, juce::Colours::white);
            title_.setText (node.displayName.isNotEmpty() ? node.displayName
                                                          : juce::String (nodeKindToString (node.kind)),
                            juce::dontSendNotification);
            addAndMakeVisible (title_);

            subtitle_.setFont (juce::Font (juce::FontOptions (10.0f)));
            subtitle_.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
            subtitle_.setText (juce::String (nodeKindToString (node.kind))
                                  + "  in " + juce::String (node.channelCountIn)
                                  + " / out " + juce::String (node.channelCountOut),
                               juce::dontSendNotification);
            addAndMakeVisible (subtitle_);

            body_ = NodePropertiesPanel::createBodyForNode (editor, node);

            viewport_.setScrollBarsShown (true, true);
            if (body_ != nullptr)
                viewport_.setViewedComponent (body_.get(), false);
            addAndMakeVisible (viewport_);

            // Initial window size: prefer the body's natural size, but cap at
            // the available display so wide matrix mixers don't spawn windows
            // larger than the screen. Scrollbars handle the overflow.
            const int bodyW = body_ ? juce::jmax (body_->getWidth(),  280) : 280;
            const int bodyH = body_ ? juce::jmax (body_->getHeight(),  80) :  60;

            const auto userArea = juce::Desktop::getInstance().getDisplays()
                                    .getPrimaryDisplay() != nullptr
                                ? juce::Desktop::getInstance().getDisplays()
                                      .getPrimaryDisplay()->userArea
                                : juce::Rectangle<int> { 1280, 800 };

            // Leave some margin off each edge for the title bar / dock and add
            // ~20 px for the scrollbar thicknesses.
            const int maxW = juce::jmax (320, userArea.getWidth()  - 80);
            const int maxH = juce::jmax (240, userArea.getHeight() - 120);

            const int chrome = 8 + 18 + 14 + 4 + 8; // padding + title + subtitle + gap + padding
            const int wantedW = juce::jmin (bodyW + 16 + 20, maxW);
            const int wantedH = juce::jmin (bodyH + chrome + 20, maxH);

            setSize (wantedW, wantedH);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xff1c1c24));
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (8);
            title_.setBounds    (area.removeFromTop (18));
            subtitle_.setBounds (area.removeFromTop (14));
            area.removeFromTop (4);
            viewport_.setBounds (area);
            // body_ keeps its natural size; the Viewport scrolls when needed.
            // Grow the body to fill the viewport when it would otherwise be
            // smaller (so narrow bodies still look right at any window size).
            if (body_ != nullptr)
            {
                const int sbThick = viewport_.getScrollBarThickness();
                const int innerW = juce::jmax (body_->getWidth(),
                                               viewport_.getWidth()  - sbThick - 4);
                const int innerH = juce::jmax (body_->getHeight(),
                                               viewport_.getHeight() - sbThick - 4);
                body_->setSize (innerW, innerH);
            }
        }

    private:
        juce::Label title_, subtitle_;
        juce::Viewport viewport_;
        std::unique_ptr<juce::Component> body_;
    };
}

void NodeComponent::openNativePropertiesWindow()
{
    auto content = std::make_unique<FloatingNodeEditor> (editor_, node_);

    const auto title = node_.displayName.isNotEmpty()
                          ? node_.displayName
                          : juce::String (nodeKindToString (node_.kind));

    auto* dw = new AutoDeleteWindow (title, juce::Colours::darkgrey,
                                     juce::DocumentWindow::closeButton, true);
    dw->setUsingNativeTitleBar (true);
    dw->setContentOwned (content.release(), true);
    dw->setResizable (true, false);
    dw->setAlwaysOnTop (true);
    dw->centreWithSize (dw->getWidth(), dw->getHeight());
    dw->setVisible (true);
    dw->toFront (true);
}

void NodeComponent::showContextMenu()
{
    juce::PopupMenu m;
    const bool isTerminal = node_.kind == NodeKind::InputTerminal
                         || node_.kind == NodeKind::OutputTerminal;

    // Copy / Duplicate target: the existing selection if this node is part of
    // it (so the user can right-click a member of a multi-select), otherwise
    // just this node.
    const auto& currentSel = editor_.getSelection();
    std::vector<juce::Uuid> targetSel;
    if (std::find (currentSel.begin(), currentSel.end(), node_.uuid) != currentSel.end())
        targetSel = currentSel;
    else
        targetSel.push_back (node_.uuid);

    m.addItem ("Copy", ! isTerminal, false,
               [this, targetSel]
               {
                   GraphClipboard::copySelection (editor_.getController(), targetSel);
               });

    m.addItem ("Duplicate", ! isTerminal, false,
               [this, targetSel]
               {
                   if (! GraphClipboard::copySelection (editor_.getController(), targetSel))
                       return;

                   const auto newUuids = GraphClipboard::pasteFromClipboard (
                       editor_.getController(),
                       editor_.getProcessor().getPluginList().getFormatManager(),
                       &editor_.getProcessor().getPluginList().getKnownPluginList(),
                       std::nullopt /* use +30/+30 default offset */);

                   if (newUuids.empty()) return;

                   editor_.clearSelection();
                   for (const auto& u : newUuids)
                       editor_.addToSelection (u);
                   editor_.getProcessor().commitHistorySnapshot();
               });

    m.addSeparator();

    m.addItem ("Rename...", ! isTerminal, false,
               [this] { showRenamePopup(); });

    m.addItem ("Delete node", ! isTerminal, false,
               [this]
               {
                   editor_.getController().removeNode (node_.uuid);
                   editor_.rebuildAll();
               });

    m.addItem ("Open plugin editor", node_.kind == NodeKind::Plugin
                                     && node_.processor != nullptr
                                     && node_.processor->hasEditor(),
               false,
               [this] { openPluginEditor(); });

    if (! isTerminal)
    {
        m.addSeparator();
        m.addItem ("Bypass", true, node_.bypassed,
                   [this]
                   {
                       editor_.getController().setNodeBypassed (node_.uuid, ! node_.bypassed);
                       repaint();
                   });
        m.addItem ("Mute", true, node_.muted,
                   [this]
                   {
                       editor_.getController().setNodeMuted (node_.uuid, ! node_.muted);
                       repaint();
                   });

        m.addSeparator();
        m.addItem ("Change channel count...", true, false,
                   [this] { showChangeChannelCountPopup(); });
    }

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

//==============================================================================

namespace
{
    /** Modal popup that lets the user pick new In and Out channel counts in
        one shot. Two combo boxes (Inputs / Outputs) plus Apply / Cancel. */
    class ChannelCountPopup : public juce::Component
    {
    public:
        ChannelCountPopup (int currentIn, int currentOut,
                           const std::vector<int>& allowedIn,
                           const std::vector<int>& allowedOut,
                           bool linked,
                           juce::String headline,
                           std::function<void (int newIn, int newOut)> onApply)
            : linked_ (linked), onApply_ (std::move (onApply))
        {
            title_.setText (headline, juce::dontSendNotification);
            title_.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
            title_.setColour (juce::Label::textColourId, juce::Colours::white);
            addAndMakeVisible (title_);

            inLabel_.setText (linked_ ? "Channels:" : "Inputs:", juce::dontSendNotification);
            inLabel_.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
            addAndMakeVisible (inLabel_);

            outLabel_.setText ("Outputs:", juce::dontSendNotification);
            outLabel_.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));

            populateCombo (inCombo_,  allowedIn,  currentIn);
            inCombo_.onChange = [this]
            {
                if (linked_) outCombo_.setSelectedId (inCombo_.getSelectedId(), juce::dontSendNotification);
            };
            addAndMakeVisible (inCombo_);

            populateCombo (outCombo_, allowedOut, currentOut);

            // In linked mode we keep the outputs row hidden — there's nothing
            // independent to set for Plugin / Gain / MutePhase / Delay nodes.
            if (! linked_)
            {
                addAndMakeVisible (outLabel_);
                addAndMakeVisible (outCombo_);
            }

            applyButton_.setButtonText ("Apply");
            applyButton_.onClick = [this]
            {
                const int n  = inCombo_.getSelectedId() - 1;
                const int m  = linked_ ? n : (outCombo_.getSelectedId() - 1);
                if (n > 0 && m > 0)
                {
                    auto cb = onApply_;
                    if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                        box->dismiss();
                    if (cb) cb (n, m);
                }
            };
            addAndMakeVisible (applyButton_);

            cancelButton_.setButtonText ("Cancel");
            cancelButton_.onClick = [this]
            {
                if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                    box->dismiss();
            };
            addAndMakeVisible (cancelButton_);

            setSize (260, linked_ ? 110 : 150);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xff2a2a2a));
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (10);
            title_.setBounds (area.removeFromTop (20));
            area.removeFromTop (6);

            const int rowH    = 26;
            const int labelW  = 80;

            auto inRow = area.removeFromTop (rowH);
            inLabel_.setBounds (inRow.removeFromLeft (labelW));
            inCombo_ .setBounds (inRow);

            if (! linked_)
            {
                area.removeFromTop (4);
                auto outRow = area.removeFromTop (rowH);
                outLabel_.setBounds (outRow.removeFromLeft (labelW));
                outCombo_.setBounds (outRow);
            }

            area.removeFromTop (8);
            auto buttonRow = area.removeFromTop (28);
            applyButton_.setBounds (buttonRow.removeFromRight (80));
            buttonRow.removeFromRight (6);
            cancelButton_.setBounds (buttonRow.removeFromRight (80));
        }

    private:
        static void populateCombo (juce::ComboBox& cb, const std::vector<int>& values, int current)
        {
            // ComboBox itemId == n + 1 (item 0 means "no selection"). We store
            // the integer channel count + 1 in the id so we can recover it
            // unambiguously without an items->value lookup.
            for (int n : values)
                cb.addItem (juce::String (n), n + 1);
            cb.setSelectedId (current + 1, juce::dontSendNotification);
        }

        bool linked_;
        std::function<void (int, int)> onApply_;

        juce::Label      title_;
        juce::Label      inLabel_, outLabel_;
        juce::ComboBox   inCombo_, outCombo_;
        juce::TextButton applyButton_, cancelButton_;
    };

    /** Modal popup to rename a node. Single text editor + Apply / Cancel.
        Apply (or Return) commits the trimmed value via onApply; an empty
        value clears the custom name and reverts the tile to the
        processor-derived label. */
    class RenamePopup : public juce::Component,
                        private juce::TextEditor::Listener
    {
    public:
        RenamePopup (const juce::String& currentName,
                     const juce::String& fallbackName,
                     std::function<void (juce::String)> onApply)
            : onApply_ (std::move (onApply))
        {
            title_.setText ("Rename node", juce::dontSendNotification);
            title_.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
            title_.setColour (juce::Label::textColourId, juce::Colours::white);
            addAndMakeVisible (title_);

            editor_.setText (currentName, juce::dontSendNotification);
            editor_.setTextToShowWhenEmpty (fallbackName,
                                            juce::Colours::white.withAlpha (0.4f));
            editor_.setSelectAllWhenFocused (true);
            editor_.addListener (this);
            addAndMakeVisible (editor_);

            applyButton_.setButtonText ("Apply");
            applyButton_.onClick = [this] { commit(); };
            addAndMakeVisible (applyButton_);

            cancelButton_.setButtonText ("Cancel");
            cancelButton_.onClick = [this]
            {
                if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                    box->dismiss();
            };
            addAndMakeVisible (cancelButton_);

            setSize (300, 110);

            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<RenamePopup> (this)]
            {
                if (safe != nullptr) safe->editor_.grabKeyboardFocus();
            });
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xff2a2a2a));
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (10);
            title_.setBounds  (area.removeFromTop (20));
            area.removeFromTop (6);
            editor_.setBounds (area.removeFromTop (26));
            area.removeFromTop (8);
            auto buttonRow = area.removeFromTop (28);
            applyButton_ .setBounds (buttonRow.removeFromRight (80));
            buttonRow.removeFromRight (6);
            cancelButton_.setBounds (buttonRow.removeFromRight (80));
        }

    private:
        void textEditorReturnKeyPressed (juce::TextEditor&) override { commit(); }
        void textEditorEscapeKeyPressed (juce::TextEditor&) override
        {
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        }

        void commit()
        {
            auto cb = onApply_;
            const auto value = editor_.getText();
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
            if (cb) cb (value);
        }

        std::function<void (juce::String)> onApply_;

        juce::Label      title_;
        juce::TextEditor editor_;
        juce::TextButton applyButton_, cancelButton_;
    };
} // namespace

void NodeComponent::showRenamePopup()
{
    const bool isTerminal = node_.kind == NodeKind::InputTerminal
                         || node_.kind == NodeKind::OutputTerminal;
    if (isTerminal) return;

    const auto fallback = node_.processor != nullptr
                            ? juce::String (node_.processor->getName())
                            : juce::String (nodeKindToString (node_.kind));

    auto uuid = node_.uuid;
    auto popup = std::make_unique<RenamePopup> (
        node_.displayName, fallback,
        [this, uuid] (juce::String newName)
        {
            editor_.getController().setNodeDisplayName (uuid, newName);
        });

    juce::CallOutBox::launchAsynchronously (
        std::move (popup),
        getScreenBounds(),
        nullptr);
}

void NodeComponent::showChangeChannelCountPopup()
{
    // Full 1..64 range. Plugin nodes are restricted by what the plugin probes
    // as supported on its main bus.
    std::vector<int> all;
    all.reserve (64);
    for (int i = 1; i <= 64; ++i) all.push_back (i);

    std::vector<int> allowedIn  = all;
    std::vector<int> allowedOut = all;
    bool             linked     = true;
    juce::String     headline   = "Change channel count";

    if (node_.kind == NodeKind::Plugin)
    {
        std::vector<int> probed;
        if (node_.processor != nullptr)
        {
            if (auto* inst = dynamic_cast<juce::AudioPluginInstance*> (node_.processor))
            {
                // The probe destructively flips the plugin's layout on each
                // candidate to verify it actually takes (Waves and similar
                // plug-ins lie via isBusesLayoutSupported; only the round-trip
                // through setBusesLayout + readback is trustworthy). Suspend
                // the outer graph's processing for the duration so the audio
                // thread doesn't race the temporary releaseResources / restore
                // cycle. Causes a brief audio dropout (< 1 s typical), which
                // is the price of an accurate channel-count list.
                GraphController::ScopedSuspend suspend (editor_.getProcessor());
                probed = PluginInstanceFactory::probeAvailableMainBusSizes (*inst, all);
            }
        }

        if (probed.empty())
        {
            juce::NativeMessageBox::showAsync (
                juce::MessageBoxOptions()
                    .withIconType (juce::MessageBoxIconType::InfoIcon)
                    .withTitle ("Plugin channel count")
                    .withMessage ("This plugin doesn't expose alternative main-bus sizes."),
                nullptr);
            return;
        }
        allowedIn  = probed;
        allowedOut = probed;
        linked     = true; // plugins set both main-bus sides together
        headline   = "Change plugin channel count";
    }
    else if (node_.kind == NodeKind::MatrixMixer || node_.kind == NodeKind::Subgraph)
    {
        linked = false;    // independent in / out
    }
    else
    {
        linked = true;     // Gain / MutePhase / Delay are always in == out
    }

    auto popup = std::make_unique<ChannelCountPopup> (
        node_.channelCountIn, node_.channelCountOut,
        allowedIn, allowedOut, linked, headline,
        [this] (int newIn, int newOut)
        {
            if (node_.kind == NodeKind::Plugin)
                changePluginNodeMainBusChannels (newIn);
            else
                changeNativeNodeChannelCount (newIn, newOut);
        });

    juce::CallOutBox::launchAsynchronously (
        std::move (popup),
        getScreenBounds(),
        nullptr);
}

void NodeComponent::changeNativeNodeChannelCount (int newChIn, int newChOut)
{
    if (newChIn <= 0 || newChOut <= 0) return;

    std::unique_ptr<juce::AudioProcessor> newProc;
    auto newKind = node_.kind;

    switch (node_.kind)
    {
        case NodeKind::Gain:
        {
            auto p = std::make_unique<GainNode> (newChIn);
            // Preserve current state by serializing the old one.
            if (auto* old = dynamic_cast<GainNode*> (node_.processor))
                p->fromVar (old->toVar());
            newProc = std::move (p);
            break;
        }
        case NodeKind::MutePhase:
        {
            auto p = std::make_unique<MutePhaseNode> (newChIn);
            if (auto* old = dynamic_cast<MutePhaseNode*> (node_.processor))
                p->fromVar (old->toVar());
            newProc = std::move (p);
            break;
        }
        case NodeKind::MatrixMixer:
        {
            auto p = std::make_unique<MatrixMixerNode> (newChIn, newChOut);
            // Don't carry over the old matrix — its dimensions don't match.
            // The new mixer starts with a unity diagonal.
            newProc = std::move (p);
            break;
        }
        case NodeKind::Delay:
        {
            auto p = std::make_unique<DelayNode> (newChIn);
            if (auto* old = dynamic_cast<DelayNode*> (node_.processor))
                p->fromVar (old->toVar());
            newProc = std::move (p);
            break;
        }
        case NodeKind::Subgraph:
        {
            // Replacing a subgraph with new I/O sizes drops its inner graph —
            // the old inner-graph state can't be reconciled with a different
            // outer bus size.
            newProc = std::make_unique<SubgraphNode> (newChIn, newChOut);
            break;
        }
        default:
            return;
    }

    editor_.getController().replaceNodeProcessor (
        node_.uuid, std::move (newProc), newKind, node_.displayName,
        newChIn, newChOut);
}

void NodeComponent::changePluginNodeMainBusChannels (int newMainCh)
{
    if (newMainCh <= 0) return;
    if (node_.pluginDescription == nullptr) return;

    auto& proc = editor_.getProcessor();
    juce::String err;
    auto inst = PluginInstanceFactory::create (
                    proc.getPluginList().getFormatManager(),
                    *node_.pluginDescription,
                    proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0,
                    proc.getBlockSize()  > 0   ? proc.getBlockSize()  : 512,
                    newMainCh,
                    err);
    if (inst == nullptr)
    {
        juce::NativeMessageBox::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::WarningIcon)
                .withTitle ("Plugin channel-count change failed")
                .withMessage (err.isEmpty() ? "Unknown error" : err),
            nullptr);
        return;
    }

    // Preserve the plugin's automation state by copying it forward. The state
    // typically encodes the previous bus layout, so re-applying the requested
    // channel count AFTER restoring is what actually resizes the bus —
    // otherwise mcfx-style multichannel plugins read the saved 2-channel
    // layout out of their state and silently revert.
    if (node_.processor != nullptr)
    {
        juce::MemoryBlock state;
        node_.processor->getStateInformation (state);
        if (state.getSize() > 0)
            inst->setStateInformation (state.getData(), (int) state.getSize());

        // Re-apply the requested channel count. Channel-count is a structural
        // change; if the plugin can't honour the new size after state restore,
        // we leave whatever channel count it ended up with.
        PluginInstanceFactory::trySetMainBusChannels (*inst, newMainCh);
    }

    const int chIn  = inst->getMainBusNumInputChannels();
    const int chOut = inst->getMainBusNumOutputChannels();

    // The probe should have screened this out, but plug-ins sometimes lie.
    // If the plugin ended up at a different channel count than requested,
    // tell the user instead of silently leaving them with a node that doesn't
    // match the popup choice.
    if (chIn != newMainCh || chOut != newMainCh)
    {
        juce::NativeMessageBox::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::WarningIcon)
                .withTitle ("Channel-count change rejected")
                .withMessage ("The plugin didn't accept "
                              + juce::String (newMainCh) + " channels and stayed at "
                              + juce::String (chIn) + " in / " + juce::String (chOut) + " out. "
                              "This often happens with VST3 plug-ins whose host "
                              "wrapper has no SpeakerArrangement that fits the "
                              "requested channel count, or with plug-ins that "
                              "advertise broader bus support than they actually "
                              "honour."),
            nullptr);
    }

    auto descCopy = std::make_unique<juce::PluginDescription> (*node_.pluginDescription);

    editor_.getController().replaceNodeProcessor (
        node_.uuid, std::move (inst), NodeKind::Plugin,
        node_.displayName, chIn, chOut, std::move (descCopy));
}
