#include "PluginEditor.h"

namespace
{
    constexpr int kToolbarHeight    = 32;
    constexpr int kBreadcrumbHeight = 28;

    constexpr int kPropPanelMin     = 200;
    constexpr int kPropPanelMax     = 1200;
    constexpr int kPropPanelDefault = 280;

    constexpr int kResizerWidth     = 6;

    // Help popup — read-only multiline TextEditor listing every shortcut and
    // mouse interaction. Lives in a CallOutBox anchored at the help button.
    class HelpPopup : public juce::Component
    {
    public:
        HelpPopup()
        {
            text_.setMultiLine (true);
            text_.setReadOnly (true);
            text_.setCaretVisible (false);
            text_.setScrollbarsShown (true);
            text_.setColour (juce::TextEditor::backgroundColourId,
                             juce::Colour (0xff1c1c24));
            text_.setColour (juce::TextEditor::textColourId,
                             juce::Colours::white.withAlpha (0.92f));
            text_.setColour (juce::TextEditor::outlineColourId,
                             juce::Colours::transparentBlack);
            text_.setColour (juce::TextEditor::focusedOutlineColourId,
                             juce::Colours::transparentBlack);
            text_.setFont (juce::Font (juce::FontOptions (
                              juce::Font::getDefaultMonospacedFontName(),
                              12.5f, juce::Font::plain)));
            text_.setText (helpBody(), false);
            addAndMakeVisible (text_);

            setSize (560, 480);
        }

        void resized() override { text_.setBounds (getLocalBounds().reduced (8)); }

    private:
        juce::TextEditor text_;

        static juce::String helpBody()
        {
            // Two-space columns; lined up by eye in the monospace font.
            return juce::String::fromUTF8 (
                "mcfx_graph keyboard shortcuts\n"
                "==============================\n"
                "\n"
                "Editing\n"
                "  Cmd / Ctrl + Z          Undo\n"
                "  Cmd / Ctrl + Shift + Z  Redo\n"
                "  Cmd / Ctrl + Y          Redo (alternative)\n"
                "  Backspace / Delete      Delete selected nodes / wires\n"
                "  Esc                     Clear selection\n"
                "\n"
                "On the selected node(s)\n"
                "  B                       Toggle bypass\n"
                "  M                       Toggle mute\n"
                "  C                       Chain-connect output of one selected\n"
                "                          node to inputs of another (1:1 by\n"
                "                          channel index)\n"
                "\n"
                "Mouse on the canvas\n"
                "  Left-drag empty area    Marquee select nodes + wires\n"
                "  Shift / Cmd + click     Add to / toggle selection\n"
                "  Drag from output pin    Make a wire to another input pin\n"
                "  Shift + drag a wire     Chain-connect: each subsequent output\n"
                "                          pin auto-wires to the matching input\n"
                "                          pin (out_i → in_i, until the next\n"
                "                          row falls off either side).\n"
                "                          If the target has only one input pin,\n"
                "                          all source outputs are summed into it.\n"
                "                          If the source has only one output pin,\n"
                "                          it is broadcast to every input pin.\n"
                "  Right-click canvas      Add Node menu (native + plug-ins)\n"
                "  Right-click node        Per-node context menu\n"
                "  Right-click wire        Delete wire\n"
                "  Double-click plug-in    Open plug-in's GUI\n"
                "  Double-click subgraph   Descend into subgraph\n"
                "  Double-click native     Open native-node properties window\n"
                "  Cmd + scroll on canvas  Zoom around cursor\n"
                "\n"
                "Toolbar\n"
                "  Save / Load JSON        Export / import the entire graph\n"
                "  Scan plug-ins           Refresh the VST3 / VST / AU list\n"
                "  -, 1:1, +               Zoom out, reset, zoom in\n"
                "  P button on a parameter Expose to the DAW as one of 256\n"
                "                          automatable forwarding parameters\n");
        }
    };
}

Mcfx_graphAudioProcessorEditor::Mcfx_graphAudioProcessorEditor (Mcfx_graphAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor_ (p),
      canvas_ (p, p.getGraph()),
      breadcrumbBar_ (canvas_),
      propertiesPanel_ (canvas_)
{
    setResizable (true, true);
    setResizeLimits (760, 420, 4000, 3000);

    addAndMakeVisible (saveButton_);
    addAndMakeVisible (loadButton_);
    addAndMakeVisible (scanButton_);
    addAndMakeVisible (undoButton_);
    addAndMakeVisible (redoButton_);
    addAndMakeVisible (zoomOutButton_);
    addAndMakeVisible (resetZoomButton_);
    addAndMakeVisible (zoomInButton_);
    addAndMakeVisible (helpButton_);
    addAndMakeVisible (statusLabel_);
    addAndMakeVisible (titleLabel_);
    addAndMakeVisible (versionLabel_);
    addAndMakeVisible (breadcrumbBar_);
    addAndMakeVisible (propertiesPanel_);

    canvasViewport_.setViewedComponent (&canvas_, false);     // we own canvas_
    canvasViewport_.setScrollBarsShown (true, true);
    addAndMakeVisible (canvasViewport_);

    // Tooltips on every toolbar button so a hover explains what they do.
    saveButton_      .setTooltip ("Save the entire graph (topology, plug-in states, parameter bindings) to a JSON file.");
    loadButton_      .setTooltip ("Load a graph from a JSON file (replaces the current graph).");
    scanButton_      .setTooltip ("Scan the system for VST3 / VST / AU plug-ins and rebuild the picker list.");
    undoButton_      .setTooltip ("Undo the last graph change (Cmd+Z).");
    redoButton_      .setTooltip ("Redo (Cmd+Shift+Z).");
    zoomOutButton_   .setTooltip ("Zoom out the canvas. Cmd+scroll on the canvas zooms around the cursor.");
    resetZoomButton_ .setTooltip ("Reset zoom to 1:1 (canvas at native size, scroll position reset).");
    zoomInButton_    .setTooltip ("Zoom in the canvas. Cmd+scroll on the canvas zooms around the cursor.");
    helpButton_      .setTooltip ("Show keyboard shortcuts and mouse interactions.");

    titleLabel_.setText ("mcfx_graph", juce::dontSendNotification);
    titleLabel_.setColour (juce::Label::textColourId, juce::Colours::aquamarine);
    titleLabel_.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));

    versionLabel_.setText (juce::String ("v") + JucePlugin_VersionString, juce::dontSendNotification);
    versionLabel_.setColour (juce::Label::textColourId, juce::Colours::grey);
    versionLabel_.setFont (juce::Font (juce::FontOptions (10.0f)));
    versionLabel_.setJustificationType (juce::Justification::centredLeft);

    // Drag handle between canvas (item 0) and properties panel (item 2).
    // Set up BEFORE setSize() so the first resized() — which setSize triggers
    // synchronously — has a valid layout to work with. Otherwise the canvas
    // and panel get zero-size bounds and the GUI shows up blank until the
    // user nudges the host window.
    horizontalLayout_.setItemLayout (0, 200, -1.0, -0.65);                  // canvas
    horizontalLayout_.setItemLayout (1, kResizerWidth, kResizerWidth, kResizerWidth);
    horizontalLayout_.setItemLayout (2, kPropPanelMin, kPropPanelMax, kPropPanelDefault);

    resizerBar_ = std::make_unique<juce::StretchableLayoutResizerBar> (
                      &horizontalLayout_, /*itemIndex=*/1, /*isVertical=*/true);
    addAndMakeVisible (*resizerBar_);

    setSize (1100, 680);

    // Restore the user's previously chosen panel width if any. Done after
    // setSize so we can reference the actual width.
    const int restoredWidth = processor_.getPropertyPanelWidth();
    if (restoredWidth > 0 && getWidth() > restoredWidth + kResizerWidth + 200)
    {
        horizontalLayout_.setItemPosition (1,
            getWidth() - restoredWidth - kResizerWidth);
        resized();
    }

    saveButton_.onClick = [this] { onSaveClicked(); };
    loadButton_.onClick = [this] { onLoadClicked(); };
    scanButton_.onClick = [this] { onScanClicked(); };
    undoButton_.onClick = [this] { onUndoClicked(); };
    redoButton_.onClick = [this] { onRedoClicked(); };
    resetZoomButton_.onClick = [this] { canvas_.resetZoom(); };
    zoomInButton_   .onClick = [this]
    {
        const auto vp = canvasViewport_.getViewArea().getCentre();
        canvas_.setZoom (canvas_.getZoom() * 1.25f, vp);
    };
    zoomOutButton_  .onClick = [this]
    {
        const auto vp = canvasViewport_.getViewArea().getCentre();
        canvas_.setZoom (canvas_.getZoom() / 1.25f, vp);
    };
    helpButton_     .onClick = [this] { onHelpClicked(); };

    statusLabel_.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    statusLabel_.setText ("", juce::dontSendNotification);

    canvas_.setSelectionListener ([this] (juce::Uuid)
    {
        propertiesPanel_.setSelectedNode (canvas_.getSelectedNode());
    });
    canvas_.setBreadcrumbListener ([this] { breadcrumbBar_.rebuild(); });

    setWantsKeyboardFocus (true);

    processor_.addChangeListener (this);
}

Mcfx_graphAudioProcessorEditor::~Mcfx_graphAudioProcessorEditor()
{
    processor_.removeChangeListener (this);
}

void Mcfx_graphAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181820));
}

void Mcfx_graphAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    auto top  = area.removeFromTop (kToolbarHeight).reduced (4);
    saveButton_.setBounds (top.removeFromLeft (90)); top.removeFromLeft (4);
    loadButton_.setBounds (top.removeFromLeft (90)); top.removeFromLeft (4);
    scanButton_.setBounds (top.removeFromLeft (110)); top.removeFromLeft (10);
    undoButton_.setBounds (top.removeFromLeft (60)); top.removeFromLeft (4);
    redoButton_.setBounds (top.removeFromLeft (60)); top.removeFromLeft (10);
    zoomOutButton_  .setBounds (top.removeFromLeft (28));
    resetZoomButton_.setBounds (top.removeFromLeft (44));
    zoomInButton_   .setBounds (top.removeFromLeft (28));
    top.removeFromLeft (10);
    helpButton_     .setBounds (top.removeFromLeft (28));
    top.removeFromLeft (10);

    // Right-aligned: name + version. Status label uses what's left in between.
    const int versionW = 60;
    const int titleW   = 96;
    versionLabel_.setBounds (top.removeFromRight (versionW));
    titleLabel_  .setBounds (top.removeFromRight (titleW));
    statusLabel_ .setBounds (top);

    breadcrumbBar_.setBounds (area.removeFromTop (kBreadcrumbHeight));

    // Lay out canvasViewport | resizer | propertiesPanel horizontally.
    juce::Component* comps[] = { &canvasViewport_, resizerBar_.get(), &propertiesPanel_ };
    horizontalLayout_.layOutComponents (
        comps, 3,
        area.getX(), area.getY(), area.getWidth(), area.getHeight(),
        /*vertically=*/false, /*resizeOtherDimension=*/true);

    // Persist the chosen panel width (in pixels) so it survives reloads.
    processor_.setPropertyPanelWidth (propertiesPanel_.getWidth());
}

bool Mcfx_graphAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    const auto cmd = juce::ModifierKeys::commandModifier;

    if (key == juce::KeyPress ('z', cmd, 0))                                 { onUndoClicked(); return true; }
    if (key == juce::KeyPress ('z', cmd | juce::ModifierKeys::shiftModifier, 0)) { onRedoClicked(); return true; }
    if (key == juce::KeyPress ('y', cmd, 0))                                 { onRedoClicked(); return true; }
    if (key == juce::KeyPress (juce::KeyPress::backspaceKey)
        || key == juce::KeyPress (juce::KeyPress::deleteKey))
    {
        // Snapshot both selections before mutating — removing a node also
        // notifies the topology listener which clears selection state.
        const auto nodeVictims = canvas_.getSelection();
        const auto wireVictims = canvas_.getSelectedConnections();

        if (nodeVictims.empty() && wireVictims.empty())
            return false;

        // Remove wires first — if a node is also being removed, the wires
        // attached to it disappear automatically. Removing them here first
        // is harmless either way.
        for (const auto& c : wireVictims)
            canvas_.getActiveController().removeConnection (c.fromUuid, c.fromCh,
                                                            c.toUuid,   c.toCh);
        for (const auto& uuid : nodeVictims)
            canvas_.getActiveController().removeNode (uuid);

        canvas_.clearSelection();
        return true;
    }
    return false;
}

//==============================================================================

bool Mcfx_graphAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".json"))
            return true;
    return false;
}

void Mcfx_graphAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& f : files)
    {
        if (! f.endsWithIgnoreCase (".json")) continue;

        juce::String err;
        if (! processor_.loadFromJsonFile (juce::File (f), &err))
            statusLabel_.setText ("Load failed: " + err, juce::dontSendNotification);
        else
            statusLabel_.setText ("Loaded " + juce::File (f).getFileName(),
                                  juce::dontSendNotification);
        return;
    }
}

//==============================================================================

void Mcfx_graphAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    canvas_.rebuildAll();
    breadcrumbBar_.rebuild();
    propertiesPanel_.setSelectedNode (canvas_.getSelectedNode());
}

void Mcfx_graphAudioProcessorEditor::onSaveClicked()
{
    chooser_ = std::make_unique<juce::FileChooser> (
                    "Save mcfx_graph as JSON",
                    juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                        .getChildFile ("mcfx_graph.json"),
                    "*.json");

    auto flags = juce::FileBrowserComponent::saveMode
               | juce::FileBrowserComponent::canSelectFiles
               | juce::FileBrowserComponent::warnAboutOverwriting;

    chooser_->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file == juce::File()) return;

        juce::String err;
        if (! processor_.saveToJsonFile (file, &err))
            statusLabel_.setText ("Save failed: " + err, juce::dontSendNotification);
        else
            statusLabel_.setText ("Saved " + file.getFileName(), juce::dontSendNotification);
    });
}

void Mcfx_graphAudioProcessorEditor::onLoadClicked()
{
    chooser_ = std::make_unique<juce::FileChooser> (
                    "Load mcfx_graph JSON",
                    juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                    "*.json");

    auto flags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectFiles;

    chooser_->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file == juce::File()) return;

        juce::String err;
        if (! processor_.loadFromJsonFile (file, &err))
            statusLabel_.setText ("Load failed: " + err, juce::dontSendNotification);
        else
            statusLabel_.setText ("Loaded " + file.getFileName(), juce::dontSendNotification);
    });
}

void Mcfx_graphAudioProcessorEditor::onScanClicked()
{
    auto& mgr = processor_.getPluginList();
    if (mgr.isScanning())
    {
        mgr.cancelRescan();
        statusLabel_.setText ("Scan cancelled", juce::dontSendNotification);
        return;
    }

    juce::Component::SafePointer<juce::Label> label (&statusLabel_);
    mgr.startRescan ([label] (float progress, juce::String name, int found)
    {
        if (auto* l = label.getComponent())
        {
            const int pct = juce::roundToInt (progress * 100.0f);
            l->setText (juce::String (pct) + "%  " + name + "  ("
                            + juce::String (found) + " plugins)",
                        juce::dontSendNotification);
        }
    });

    statusLabel_.setText ("Scanning...", juce::dontSendNotification);
}

void Mcfx_graphAudioProcessorEditor::onUndoClicked()
{
    if (processor_.undo())
        statusLabel_.setText ("Undo", juce::dontSendNotification);
    else
        statusLabel_.setText ("Nothing to undo", juce::dontSendNotification);
}

void Mcfx_graphAudioProcessorEditor::onRedoClicked()
{
    if (processor_.redo())
        statusLabel_.setText ("Redo", juce::dontSendNotification);
    else
        statusLabel_.setText ("Nothing to redo", juce::dontSendNotification);
}

void Mcfx_graphAudioProcessorEditor::onHelpClicked()
{
    auto popup = std::make_unique<HelpPopup>();
    const auto anchor = helpButton_.localAreaToGlobal (helpButton_.getLocalBounds());
    juce::CallOutBox::launchAsynchronously (std::move (popup), anchor, nullptr);
}

