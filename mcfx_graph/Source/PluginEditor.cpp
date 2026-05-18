#include "PluginEditor.h"
#include "Graph/GraphClipboard.h"

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
                "  Cmd / Ctrl + C          Copy selected nodes (with internal\n"
                "                          connections preserved). Some hosts\n"
                "                          (e.g. Reaper) intercept Cmd+C — use\n"
                "                          right-click → Copy / Duplicate instead.\n"
                "  Cmd / Ctrl + V          Paste at the cursor (or offset from\n"
                "                          the originals if cursor is off-canvas).\n"
                "                          Right-click empty canvas → Paste also\n"
                "                          works in hosts that intercept Cmd+V.\n"
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
                "  Presets                 Save / load named presets, or import\n"
                "                          / export a JSON file anywhere on disk.\n"
                "                          Named presets live in:\n"
                "                            ~/Library/Application Support/\n"
                "                              mcfx_graph/presets   (macOS)\n"
                "                            %APPDATA%\\mcfx_graph\\presets (Win)\n"
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

    addAndMakeVisible (presetsButton_);
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
    presetsButton_   .setTooltip ("Save / load named graph presets (stored under the user's app-data folder), or import / export a JSON file anywhere on disk.");
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

    presetsButton_.onClick = [this] { onPresetsClicked(); };
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
    scanButton_.setBounds (top.removeFromLeft (110)); top.removeFromLeft (10);
    zoomOutButton_  .setBounds (top.removeFromLeft (28));
    resetZoomButton_.setBounds (top.removeFromLeft (44));
    zoomInButton_   .setBounds (top.removeFromLeft (28));
    top.removeFromLeft (10);
    helpButton_     .setBounds (top.removeFromLeft (28));
    top.removeFromLeft (10);

    // Right-aligned group (mirrors mcfx_mimoeq's toolbar):
    //   …status…  Undo Redo  Presets  mcfx_graph  v0.8.3
    // Undo / Redo / Presets cluster together at the right so the title +
    // version read as the anchor, with the editing-history + preset actions
    // immediately adjacent to them rather than scattered across the bar.
    const int versionW = 60;
    const int titleW   = 96;
    const int presetsW = 90;
    const int undoW    = 60;
    const int redoW    = 60;
    versionLabel_ .setBounds (top.removeFromRight (versionW));
    titleLabel_   .setBounds (top.removeFromRight (titleW));
    top.removeFromRight (6); // small gap between title and Presets
    presetsButton_.setBounds (top.removeFromRight (presetsW));
    top.removeFromRight (10);
    redoButton_   .setBounds (top.removeFromRight (redoW));
    top.removeFromRight (4);
    undoButton_   .setBounds (top.removeFromRight (undoW));
    top.removeFromRight (10);
    statusLabel_  .setBounds (top);

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

    if (key == juce::KeyPress ('c', cmd, 0))
    {
        GraphClipboard::copySelection (canvas_.getActiveController(),
                                       canvas_.getSelection());
        return true;
    }

    if (key == juce::KeyPress ('v', cmd, 0))
    {
        // Mouse-driven paste position only when the cursor is over the visible
        // canvas viewport. World coords = canvas-local / zoom (node positions
        // are in world coords; node-component placement multiplies by zoom).
        std::optional<juce::Point<int>> origin;
        const auto mouseScreen = juce::Desktop::getMousePosition();
        const auto vpLocal     = canvasViewport_.getLocalPoint (nullptr, mouseScreen);
        if (canvasViewport_.getLocalBounds().contains (vpLocal))
        {
            const auto canvasLocal = canvas_.getLocalPoint (nullptr, mouseScreen);
            const float zoom       = juce::jmax (0.001f, canvas_.getZoom());
            origin = juce::Point<int> { (int) std::round (canvasLocal.x / zoom),
                                        (int) std::round (canvasLocal.y / zoom) };
        }

        const auto newUuids = GraphClipboard::pasteFromClipboard (
            canvas_.getActiveController(),
            processor_.getPluginList().getFormatManager(),
            &processor_.getPluginList().getKnownPluginList(),
            origin);

        if (! newUuids.empty())
        {
            canvas_.clearSelection();
            for (const auto& u : newUuids)
                canvas_.addToSelection (u);
            processor_.commitHistorySnapshot();
        }
        return true;
    }

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

//==============================================================================
// Presets menu
//
// One toolbar button opens this menu (replaces the old Save JSON / Load JSON
// pair). Named presets live as JSON files under
//   userApplicationDataDirectory/mcfx_graph/presets
// — same convention as PluginListManager's pluginList.xml cache. The popup is
// rebuilt from disk every open, so external edits to the folder show up
// without restart.

void Mcfx_graphAudioProcessorEditor::savePresetFile (const juce::File& file)
{
    juce::String err;
    if (! processor_.saveToJsonFile (file, &err))
        statusLabel_.setText ("Save failed: " + err, juce::dontSendNotification);
    else
        statusLabel_.setText ("Saved " + file.getFileName(), juce::dontSendNotification);
}

void Mcfx_graphAudioProcessorEditor::loadPresetFile (const juce::File& file)
{
    juce::String err;
    if (! processor_.loadFromJsonFile (file, &err))
        statusLabel_.setText ("Load failed: " + err, juce::dontSendNotification);
    else
        statusLabel_.setText ("Loaded " + file.getFileName(), juce::dontSendNotification);
}

namespace
{
    // Pick a sensible start directory for the file pickers: the user's preset
    // folder when it exists (puts the chooser right where named presets live),
    // otherwise ~/Documents (the legacy default).
    juce::File pickStartDir (const PresetManager& pm)
    {
        auto dir = pm.getPresetDir();
        if (dir.isDirectory()) return dir;
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    }

    // After an AlertWindow with a text-editor field enters its modal state,
    // focus that text editor and select its contents so the user can start
    // typing immediately. Has to be posted via callAsync because
    // grabKeyboardFocus() only takes effect once the AlertWindow is on
    // screen, and the modal-state machinery may still be settling when
    // enterModalState() returns. SafePointer protects against the user
    // dismissing the popup before the async callback runs.
    void focusAlertTextEditor (juce::AlertWindow* aw, const juce::String& editorName)
    {
        juce::Component::SafePointer<juce::AlertWindow> safe (aw);
        juce::MessageManager::callAsync ([safe, editorName]
        {
            if (auto* w = safe.getComponent())
                if (auto* ed = w->getTextEditor (editorName))
                {
                    ed->grabKeyboardFocus();
                    ed->selectAll();
                }
        });
    }
}

void Mcfx_graphAudioProcessorEditor::onPresetsClicked()
{
    juce::PopupMenu m;

    // ---- File-picker entries (the old Save JSON / Load JSON behaviour) ----

    m.addItem ("Load from file...", [this]
    {
        chooser_ = std::make_unique<juce::FileChooser> (
                        "Load mcfx_graph JSON",
                        pickStartDir (presets_),
                        "*.json");

        auto flags = juce::FileBrowserComponent::openMode
                   | juce::FileBrowserComponent::canSelectFiles;

        chooser_->launchAsync (flags, [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File()) return;
            loadPresetFile (file);
        });
    });

    m.addItem ("Save to file...", [this]
    {
        chooser_ = std::make_unique<juce::FileChooser> (
                        "Save mcfx_graph as JSON",
                        pickStartDir (presets_).getChildFile ("mcfx_graph.json"),
                        "*.json");

        auto flags = juce::FileBrowserComponent::saveMode
                   | juce::FileBrowserComponent::canSelectFiles
                   | juce::FileBrowserComponent::warnAboutOverwriting;

        chooser_->launchAsync (flags, [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File()) return;
            savePresetFile (file);
        });
    });

    m.addSeparator();

    // ---- Named-preset save + list ----

    m.addItem ("Save as named preset...", [this] { promptSaveAsNamedPreset(); });

    m.addSeparator();

    const auto entries = presets_.listPresets();
    if (entries.empty())
    {
        m.addItem ("(no presets yet)", false, false, [] {});
    }
    else
    {
        for (const auto& e : entries)
        {
            auto file = e.file;
            m.addItem (e.name, [this, file] { loadPresetFile (file); });
        }
    }

    m.addSeparator();

    // ---- Manage submenus ----

    juce::PopupMenu renameMenu;
    juce::PopupMenu deleteMenu;
    for (const auto& e : entries)
    {
        auto file = e.file;
        renameMenu.addItem (e.name, [this, file] { promptRenamePreset (file); });
        deleteMenu.addItem (e.name, [this, file] { confirmDeletePreset (file); });
    }
    m.addSubMenu ("Rename preset",  renameMenu, ! entries.empty());
    m.addSubMenu ("Delete preset",  deleteMenu, ! entries.empty());

    m.addSeparator();

    m.addItem ("Reveal preset folder", [this]
    {
        // Create the folder so reveal does something meaningful even before the
        // first save — Finder/Explorer just opens the new empty directory.
        presets_.ensurePresetDirExists();
        presets_.getPresetDir().revealToUser();
    });

    m.showMenuAsync (juce::PopupMenu::Options()
                         .withTargetComponent (&presetsButton_));
}

void Mcfx_graphAudioProcessorEditor::promptSaveAsNamedPreset()
{
    alertWindow_ = std::make_unique<juce::AlertWindow> (
                       "Save preset",
                       "Enter a name for this preset:",
                       juce::MessageBoxIconType::QuestionIcon);

    alertWindow_->addTextEditor ("name", "", {}, false);
    alertWindow_->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    alertWindow_->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    alertWindow_->enterModalState (true,
        juce::ModalCallbackFunction::create ([this] (int result)
        {
            if (result == 0 || alertWindow_ == nullptr) { alertWindow_.reset(); return; }

            const auto raw   = alertWindow_->getTextEditorContents ("name");
            const auto clean = PresetManager::sanitise (raw);
            alertWindow_.reset();

            if (clean.isEmpty())
            {
                statusLabel_.setText ("Preset name is empty", juce::dontSendNotification);
                return;
            }

            if (! presets_.ensurePresetDirExists())
            {
                statusLabel_.setText ("Could not create preset folder", juce::dontSendNotification);
                return;
            }

            const auto file = presets_.fileForName (clean);

            // Confirm overwrite if a preset with the same name already exists.
            if (file.existsAsFile())
            {
                alertWindow_ = std::make_unique<juce::AlertWindow> (
                                   "Overwrite preset?",
                                   "A preset named \"" + clean + "\" already exists.\n"
                                   "Overwrite it?",
                                   juce::MessageBoxIconType::WarningIcon);
                alertWindow_->addButton ("Overwrite", 1, juce::KeyPress (juce::KeyPress::returnKey));
                alertWindow_->addButton ("Cancel",    0, juce::KeyPress (juce::KeyPress::escapeKey));
                alertWindow_->enterModalState (true,
                    juce::ModalCallbackFunction::create ([this, file] (int r)
                    {
                        alertWindow_.reset();
                        if (r != 1) return;
                        savePresetFile (file);
                    }), false);
                return;
            }

            savePresetFile (file);
        }), false);

    focusAlertTextEditor (alertWindow_.get(), "name");
}

void Mcfx_graphAudioProcessorEditor::promptRenamePreset (const juce::File& file)
{
    const auto oldName = file.getFileNameWithoutExtension();

    alertWindow_ = std::make_unique<juce::AlertWindow> (
                       "Rename preset",
                       "Enter a new name for \"" + oldName + "\":",
                       juce::MessageBoxIconType::QuestionIcon);

    alertWindow_->addTextEditor ("name", oldName, {}, false);
    alertWindow_->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alertWindow_->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    alertWindow_->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, file, oldName] (int result)
        {
            if (result == 0 || alertWindow_ == nullptr) { alertWindow_.reset(); return; }

            const auto raw   = alertWindow_->getTextEditorContents ("name");
            const auto clean = PresetManager::sanitise (raw);
            alertWindow_.reset();

            if (clean.isEmpty() || clean == oldName) return;

            const auto target = presets_.fileForName (clean);
            if (target.existsAsFile())
            {
                statusLabel_.setText ("A preset named \"" + clean + "\" already exists",
                                      juce::dontSendNotification);
                return;
            }

            if (file.moveFileTo (target))
                statusLabel_.setText ("Renamed to " + target.getFileName(),
                                      juce::dontSendNotification);
            else
                statusLabel_.setText ("Rename failed", juce::dontSendNotification);
        }), false);

    focusAlertTextEditor (alertWindow_.get(), "name");
}

void Mcfx_graphAudioProcessorEditor::confirmDeletePreset (const juce::File& file)
{
    alertWindow_ = std::make_unique<juce::AlertWindow> (
                       "Delete preset?",
                       "Delete preset \"" + file.getFileNameWithoutExtension() + "\"?\n"
                       "This cannot be undone.",
                       juce::MessageBoxIconType::WarningIcon);
    alertWindow_->addButton ("Delete", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alertWindow_->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    alertWindow_->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, file] (int result)
        {
            alertWindow_.reset();
            if (result != 1) return;

            if (file.deleteFile())
                statusLabel_.setText ("Deleted " + file.getFileName(),
                                      juce::dontSendNotification);
            else
                statusLabel_.setText ("Delete failed", juce::dontSendNotification);
        }), false);
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

