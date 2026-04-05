/*
 ==============================================================================

 This file is part of the mcfx (Multichannel Effects) plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com

 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)

 Details of these licenses can be found at: www.gnu.org/licenses

 mcfx is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 ==============================================================================
 */

#include "PluginEditor.h"

#define Q(x) #x
#define QUOTE(x) Q(x)

//==============================================================================
// Searchable plugin selector popup: a TextEditor on top and a ListBox below,
// filtering the plugin list in real time as the user types.
namespace
{
class PluginSearchPopup : public Component,
                          private TextEditor::Listener,
                          private ListBoxModel
{
public:
    PluginSearchPopup (Array<PluginDescription> allTypes,
                       std::function<void(int)> onPicked)
        : _all (std::move (allTypes)), _onPicked (std::move (onPicked))
    {
        _search.setTextToShowWhenEmpty ("Type to search...", Colours::grey);
        _search.addListener (this);
        _search.setWantsKeyboardFocus (true);
        addAndMakeVisible (_search);

        // Collect unique formats (VST, VST3, AudioUnit, ...) in sorted order
        StringArray formats;
        for (const auto& d : _all)
            if (d.pluginFormatName.isNotEmpty() && ! formats.contains (d.pluginFormatName))
                formats.add (d.pluginFormatName);
        formats.sort (true);

        for (const auto& fmt : formats)
        {
            auto* b = new TextButton (fmt);
            b->setClickingTogglesState (true);
            b->setToggleState (true, dontSendNotification);
            b->setColour (TextButton::buttonColourId,   Colour (0xff3a3a3a));
            b->setColour (TextButton::buttonOnColourId, Colour (0xff3e6aa8));
            b->onClick = [this]() { rebuildFiltered(); };
            addAndMakeVisible (b);
            _formatButtons.add (b);
        }

        _list.setModel (this);
        _list.setRowHeight (20);
        _list.setColour (ListBox::backgroundColourId, Colour (0xff1e1e1e));
        addAndMakeVisible (_list);

        rebuildFiltered();
        setSize (400, 440);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4);
        _search.setBounds (r.removeFromTop (24));
        r.removeFromTop (4);

        if (! _formatButtons.isEmpty())
        {
            auto row = r.removeFromTop (22);
            int n = _formatButtons.size();
            int gap = 4;
            int bw = (row.getWidth() - gap * (n - 1)) / n;
            for (int i = 0; i < n; ++i)
            {
                _formatButtons[i]->setBounds (row.removeFromLeft (bw));
                if (i < n - 1) row.removeFromLeft (gap);
            }
            r.removeFromTop (4);
        }

        _list.setBounds (r);
    }

    void paint (Graphics& g) override
    {
        g.fillAll (Colour (0xff2a2a2a));
    }

    void visibilityChanged() override
    {
        if (isShowing())
            _search.grabKeyboardFocus();
    }

    bool keyPressed (const KeyPress& key) override
    {
        if (key == KeyPress::downKey || key == KeyPress::upKey
            || key == KeyPress::pageDownKey || key == KeyPress::pageUpKey)
        {
            _list.keyPressed (key);
            return true;
        }
        if (key == KeyPress::returnKey)
        {
            pickRow (_list.getSelectedRow());
            return true;
        }
        return false;
    }

private:
    // TextEditor::Listener
    void textEditorTextChanged (TextEditor&) override { rebuildFiltered(); }
    void textEditorReturnKeyPressed (TextEditor&) override
    {
        pickRow (_list.getSelectedRow() >= 0 ? _list.getSelectedRow() : 0);
    }
    void textEditorEscapeKeyPressed (TextEditor&) override
    {
        if (auto* box = findParentComponentOfClass<CallOutBox>())
            box->dismiss();
    }

    // ListBoxModel
    int getNumRows() override { return _filtered.size(); }

    void paintListBoxItem (int row, Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || row >= _filtered.size()) return;
        if (selected)
            g.fillAll (Colour (0xff3e6aa8));

        const auto& d = _all.getReference (_filtered[row]);
        g.setFont (Font (FontOptions (14.0f)));

        // Right-aligned format tag
        auto fmt = d.pluginFormatName;
        int fmtW = 0;
        if (fmt.isNotEmpty())
        {
            fmtW = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), fmt) + 12;
            g.setColour (Colour (0xffa0a0a0));
            g.drawText (fmt, w - fmtW, 0, fmtW - 6, h, Justification::centredRight, true);
        }

        g.setColour (Colours::white);
        String text = d.name;
        if (d.manufacturerName.isNotEmpty())
            text += "  (" + d.manufacturerName + ")";
        g.drawText (text, 6, 0, w - 12 - fmtW, h, Justification::centredLeft, true);
    }

    void listBoxItemClicked (int row, const MouseEvent&) override
    {
        pickRow (row);
    }

    void returnKeyPressed (int row) override { pickRow (row); }

    void pickRow (int row)
    {
        if (row < 0 || row >= _filtered.size()) return;
        int idx = _filtered[row];
        auto cb = _onPicked;
        if (auto* box = findParentComponentOfClass<CallOutBox>())
            box->dismiss();
        if (cb) cb (idx);
    }

    void rebuildFiltered()
    {
        _filtered.clearQuick();
        auto q = _search.getText().trim();
        StringArray tokens;
        tokens.addTokens (q, " ", "");
        tokens.removeEmptyStrings();

        // Collect enabled formats. If none are enabled, show all (avoids empty list).
        StringArray enabledFormats;
        for (auto* b : _formatButtons)
            if (b->getToggleState())
                enabledFormats.add (b->getButtonText());
        bool filterByFormat = ! enabledFormats.isEmpty() && enabledFormats.size() < _formatButtons.size();
        bool anyEnabled = ! enabledFormats.isEmpty();

        for (int i = 0; i < _all.size(); ++i)
        {
            const auto& d = _all.getReference (i);
            if (anyEnabled && ! enabledFormats.contains (d.pluginFormatName))
                continue;
            (void) filterByFormat;
            String hay = (d.name + " " + d.manufacturerName + " " + d.pluginFormatName).toLowerCase();
            bool ok = true;
            for (auto& t : tokens)
                if (! hay.contains (t.toLowerCase())) { ok = false; break; }
            if (ok) _filtered.add (i);
        }
        _list.updateContent();
        if (_filtered.size() > 0)
            _list.selectRow (0);
    }

    Array<PluginDescription> _all;
    Array<int> _filtered;
    TextEditor _search;
    OwnedArray<TextButton> _formatButtons;
    ListBox _list;
    std::function<void(int)> _onPicked;
};
} // namespace

//==============================================================================
Mcfx_anythingAudioProcessorEditor::Mcfx_anythingAudioProcessorEditor (Mcfx_anythingAudioProcessor* ownerFilter)
    : AudioProcessorEditor (ownerFilter)
{
    setLookAndFeel (&_lookAndFeel);

    // Plugin selector button
    _pluginSelectorButton.setButtonText ("Select Plugin...");
    _pluginSelectorButton.setTooltip ("Choose a plugin to load - multiple instances will be created to cover all " + String (NUM_CHANNELS) + " channels");
    _pluginSelectorButton.onClick = [this]() { showPluginMenu(); };
    addAndMakeVisible (_pluginSelectorButton);

    // Settings button
    _settingsButton.setButtonText ("Settings");
    _settingsButton.setTooltip ("Configure plugin search paths, scan for new plugins, and manage the plugin list");
    _settingsButton.onClick = [this]() { showSettings(); };
    addAndMakeVisible (_settingsButton);

    // Unload button
    _unloadButton.setButtonText ("X");
    _unloadButton.setTooltip ("Unload the current plugin and silence all outputs");
    _unloadButton.onClick = [this]() { getProcessor()->unloadPlugin(); };
    addAndMakeVisible (_unloadButton);

    // Inspect button
    _inspectButton.setButtonText ("Inspect");
    _inspectButton.setTooltip ("Open the GUI of a specific plugin instance in a popup window to verify parameter sync");
    _inspectButton.onClick = [this]() { showInstanceInspectorMenu(); };
    addAndMakeVisible (_inspectButton);

    // Generic/Custom GUI toggle button
    _genericToggleButton.setButtonText ("Generic");
    _genericToggleButton.setTooltip ("Switch between the plugin's custom GUI and a generic parameter editor");
    _genericToggleButton.onClick = [this]() { toggleMainEditorType(); };
    addAndMakeVisible (_genericToggleButton);

    // Sidechain routing button
    _sidechainButton.setButtonText ("SC");
    _sidechainButton.setTooltip ("Route an input channel to the sidechain input of each plugin instance");
    _sidechainButton.onClick = [this]() { showSidechainRoutingMenu(); };
    addAndMakeVisible (_sidechainButton);

    // Instance info label
    _instanceInfoLabel.setFont (FontOptions (12.0f));
    _instanceInfoLabel.setColour (Label::textColourId, Colours::lightgrey);
    _instanceInfoLabel.setJustificationType (Justification::centredRight);
    addAndMakeVisible (_instanceInfoLabel);

    // Set initial size
    setSize (minWidth, minHeight);

    // If a plugin is already loaded (editor recreated), show it
    updateHostedEditor();
    updateSelectorButtonText();

    // Listen for changes from the processor
    ownerFilter->addChangeListener (this);
}

Mcfx_anythingAudioProcessorEditor::~Mcfx_anythingAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
    auto* proc = getProcessor();
    proc->removeChangeListener (this);

    // Protect editor teardown from concurrent audio processing
    const ScopedLock audioLock (proc->getPluginLock());

    closeAllInspectorWindows();

    if (_hostedEditor)
    {
        _hostedEditor->removeComponentListener (this);
        _hostedEditor = nullptr;
    }
    _hostedCustomEditor.reset();
    _hostedGenericEditor.reset();
}

//==============================================================================

void Mcfx_anythingAudioProcessorEditor::paint (Graphics& g)
{
    // Background
    g.fillAll (Colour (0xff1a1a1a));

    // Top bar background
    g.setColour (Colour (0xff2a2a2a));
    g.fillRect (0, 0, getWidth(), topBarHeight);

    // Top bar border
    g.setColour (Colour (0xff3a3a3a));
    g.drawLine (0.0f, (float) topBarHeight, (float) getWidth(), (float) topBarHeight);

    // Plugin name (bottom-left, aquamarine like other mcfx plugins)
    g.setColour (Colours::aquamarine);
    g.setFont (FontOptions (15.0f));
    g.drawText ("mcfx_anything",
                8, getHeight() - 20, 150, 16,
                Justification::centredLeft, false);

    // Version (bottom-right, grey like other mcfx plugins)
    g.setColour (Colours::grey);
    g.setFont (FontOptions (10.0f));
    String version;
    version << "v" << QUOTE(VERSION);
    g.drawText (version,
                getWidth() - 58, getHeight() - 15, 50, 10,
                Justification::bottomRight, true);

    // "No plugin loaded" message
    if (! getProcessor()->isPluginLoaded())
    {
        int midY = topBarHeight + (getHeight() - topBarHeight) / 2;

        g.setColour (Colours::lightgrey);
        g.setFont (FontOptions (16.0f));
        g.drawText ("Use any audio plugin for any number of channels.",
                    20, midY - 60, getWidth() - 40, 20,
                    Justification::centred, false);
        g.drawText ("That's all.",
                    20, midY - 38, getWidth() - 40, 20,
                    Justification::centred, false);

        g.setColour (Colours::grey);
        g.setFont (FontOptions (16.0f));
        g.drawText ("No plugin loaded",
                    0, midY, getWidth(), 24,
                    Justification::centred, false);

        g.setFont (FontOptions (12.0f));
        g.drawText ("Use Settings to scan for plugins, then select one from the menu",
                    20, midY + 30, getWidth() - 40, 20,
                    Justification::centred, false);
    }
}

void Mcfx_anythingAudioProcessorEditor::resized()
{
    const int margin = 4;
    const int buttonHeight = topBarHeight - 2 * margin;
    const int settingsWidth = 70;
    const int genericWidth = 80;
    const int scWidth = 60;
    const int inspectWidth = 60;
    const int unloadWidth = 28;
    const int infoWidth = 120;

    int x = margin;
    _settingsButton.setBounds (x, margin, settingsWidth, buttonHeight);
    x += settingsWidth + margin;

    // Right-side buttons: info | generic | SC | inspect | X
    // Only reserve space for buttons that are actually visible, so the
    // plugin selector button gets maximum width (especially when no plugin
    // is loaded and only the X button is shown on the right).
    const bool loaded = getProcessor()->isPluginLoaded();
    const bool showCustomToggle = loaded && _genericToggleButton.isVisible();
    const bool hasSC = loaded && getProcessor()->hasSidechain();

    int rightWidth = unloadWidth + margin;
    if (loaded)
    {
        rightWidth += infoWidth + margin + inspectWidth + margin;
        if (showCustomToggle) rightWidth += genericWidth + margin;
        if (hasSC)            rightWidth += scWidth + margin;
    }

    _pluginSelectorButton.setBounds (x, margin, getWidth() - x - rightWidth, buttonHeight);

    int rx = getWidth() - margin - unloadWidth;
    _unloadButton.setBounds (rx, margin, unloadWidth, buttonHeight);
    if (loaded)
    {
        rx -= margin + inspectWidth;
        _inspectButton.setBounds (rx, margin, inspectWidth, buttonHeight);
        if (hasSC)
        {
            rx -= margin + scWidth;
            _sidechainButton.setBounds (rx, margin, scWidth, buttonHeight);
        }
        if (showCustomToggle)
        {
            rx -= margin + genericWidth;
            _genericToggleButton.setBounds (rx, margin, genericWidth, buttonHeight);
        }
        rx -= margin + infoWidth;
        _instanceInfoLabel.setBounds (rx, margin, infoWidth, buttonHeight);
    }

    if (_hostedEditor != nullptr)
        _hostedEditor->setTopLeftPosition (0, topBarHeight);
}

//==============================================================================

void Mcfx_anythingAudioProcessorEditor::changeListenerCallback (ChangeBroadcaster* source)
{
    // Close all inspector popups FIRST — their editors hold references
    // to plugin instances that may be about to be destroyed.
    closeAllInspectorWindows();

    _useGenericEditor = false; // reset to custom GUI when plugin changes
    updateHostedEditor();
    updateSelectorButtonText();
    repaint();
}

void Mcfx_anythingAudioProcessorEditor::componentMovedOrResized (Component& component, bool wasMoved, bool wasResized)
{
    if (&component == _hostedEditor && wasResized)
    {
        int w = jmax (minWidth, _hostedEditor->getWidth());
        int h = _hostedEditor->getHeight() + topBarHeight;
        setSize (w, h);
    }
}

//==============================================================================

void Mcfx_anythingAudioProcessorEditor::showPluginMenu()
{
    auto& knownPlugins = getProcessor()->getKnownPluginList();
    auto types = knownPlugins.getTypes();

    if (types.isEmpty())
    {
        PopupMenu menu;
        menu.addItem (1, "No plugins found - open Settings to scan", false);
        menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&_pluginSelectorButton));
        return;
    }

    // Sort by manufacturer then name for a stable, readable list
    auto sorted = types;
    std::sort (sorted.begin(), sorted.end(),
               [] (const PluginDescription& a, const PluginDescription& b)
               {
                   int m = a.manufacturerName.compareIgnoreCase (b.manufacturerName);
                   if (m != 0) return m < 0;
                   return a.name.compareIgnoreCase (b.name) < 0;
               });

    auto content = std::make_unique<PluginSearchPopup> (sorted,
        [this, sorted] (int index)
        {
            if (index >= 0 && index < sorted.size())
                getProcessor()->loadPlugin (sorted.getReference (index));
        });

    auto buttonArea = _pluginSelectorButton.getScreenBounds();
    CallOutBox::launchAsynchronously (std::move (content), buttonArea, nullptr);
}

void Mcfx_anythingAudioProcessorEditor::showSettings()
{
    auto deadMansPedalFile = File::getSpecialLocation (File::userApplicationDataDirectory)
                                .getChildFile ("mcfx_anything")
                                .getChildFile ("deadMansPedal.txt");

    // Ensure directory exists
    deadMansPedalFile.getParentDirectory().createDirectory();

    auto* content = new PluginListComponent (getProcessor()->getFormatManager(),
                                             getProcessor()->getKnownPluginList(),
                                             deadMansPedalFile,
                                             nullptr,
                                             true);
    content->setSize (600, 500);

    DialogWindow::LaunchOptions options;
    options.content.setOwned (content);
    options.dialogTitle = "mcfx_anything - Plugin Settings";
    options.dialogBackgroundColour = Colour (0xff2a2a2a);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.launchAsync();
}

void Mcfx_anythingAudioProcessorEditor::destroyHostedEditorsForUnload()
{
    auto* proc = getProcessor();

    // Take the audio lock while destroying editors, so the audio thread can't
    // be inside processAudio() while we tear down VST3PluginWindow/etc.
    const ScopedLock audioLock (proc->getPluginLock());

    // Destroy inspector popups first (they hold editors of slave instances).
    closeAllInspectorWindows();

    // Destroy the main hosted editor(s). Must happen while plugin instances
    // are still fully alive so the editor's destructor (VST3PluginWindow::~,
    // etc.) can safely call back into the plugin (e.g. IPlugView::removed()).
    if (_hostedEditor)
    {
        _hostedEditor->removeComponentListener (this);
        removeChildComponent (_hostedEditor);
        _hostedEditor = nullptr;
    }
    _hostedCustomEditor.reset();
    _hostedGenericEditor.reset();
}

void Mcfx_anythingAudioProcessorEditor::closeAllInspectorWindows()
{
    // Delete all tracked inspector windows. Each window's onClose callback
    // removes it from the array, so we iterate by always deleting the first.
    // However, since delete triggers closeButtonPressed which calls onClose
    // which modifies _openInspectorWindows, we take a copy first.
    auto windowsCopy = _openInspectorWindows;
    _openInspectorWindows.clear();
    for (auto* w : windowsCopy)
        delete w;
}

void Mcfx_anythingAudioProcessorEditor::showInstanceInspectorMenu()
{
    auto* proc = getProcessor();
    int numInstances = proc->getNumInstances();
    int chPerInst = proc->getChannelsPerInstance();

    if (numInstances <= 0)
    {
        PopupMenu menu;
        menu.addItem (1, "No plugin loaded", false);
        menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&_inspectButton));
        return;
    }

    PopupMenu menu;
    for (int i = 0; i < numInstances; ++i)
    {
        int startCh = i * chPerInst + 1;
        int endCh = (i + 1) * chPerInst;
        String label = "Instance #" + String (i + 1) + "  (ch " + String (startCh) + "-" + String (endCh) + ")";
        if (i == 0)
            label += "  [master]";
        menu.addItem (i + 1, label);
    }

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&_inspectButton),
        [this, chPerInst] (int result)
        {
            if (result > 0)
            {
                int index = result - 1;
                auto* instance = getProcessor()->getInstance (index);
                if (instance)
                {
                    // Default to the custom UI for inspectors. If the plugin doesn't
                    // support multiple native editor instances (e.g. some Waves plugins),
                    // the user can toggle to the generic UI via the toolbar button.
                    auto* window = new InstanceInspectorWindow (instance, index, chPerInst, /*startGeneric*/ false,
                        [this] (InstanceInspectorWindow* w) { _openInspectorWindows.removeFirstMatchingValue (w); });

                    _openInspectorWindows.add (window);
                }
            }
        });
}

void Mcfx_anythingAudioProcessorEditor::showSidechainRoutingMenu()
{
    auto* proc = getProcessor();
    if (! proc->hasSidechain())
        return;

    int currentSc = proc->getSidechainSourceChannel();
    int scNumCh = proc->getSidechainNumChannels();

    PopupMenu menu;

    // Header info
    String scInfo = "Sidechain: " + String (scNumCh) + " ch";
    menu.addSectionHeader (scInfo);
    menu.addSeparator();

    // "Off" option
    menu.addItem (1, "Off", true, currentSc < 0);

    menu.addSeparator();

    // Channel list
    for (int ch = 0; ch < NUM_CHANNELS; ++ch)
    {
        String label = "Channel " + String (ch + 1);
        menu.addItem (ch + 2, label, true, currentSc == ch);
    }

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&_sidechainButton),
        [this] (int result)
        {
            if (result == 1)
            {
                getProcessor()->setSidechainSourceChannel (-1);
                updateSidechainButton();
            }
            else if (result >= 2)
            {
                getProcessor()->setSidechainSourceChannel (result - 2);
                updateSidechainButton();
            }
        });
}

void Mcfx_anythingAudioProcessorEditor::updateSidechainButton()
{
    auto* proc = getProcessor();
    bool hasSc = proc->hasSidechain() && proc->isPluginLoaded();
    _sidechainButton.setVisible (hasSc);

    if (hasSc)
    {
        int scCh = proc->getSidechainSourceChannel();
        if (scCh >= 0)
            _sidechainButton.setButtonText ("SC:" + String (scCh + 1));
        else
            _sidechainButton.setButtonText ("SC");
    }
}

void Mcfx_anythingAudioProcessorEditor::toggleMainEditorType()
{
    _useGenericEditor = ! _useGenericEditor;

    // Defer the actual editor swap until the current mouse/click event has
    // fully unwound. Some VST3 plugins (e.g. TDR Kotelnikov) crash inside
    // their IPlugView::removed() teardown when it runs from a nested event
    // dispatch stack. Disable the button briefly so users can't spam-click.
    _genericToggleButton.setEnabled (false);

    Component::SafePointer<Mcfx_anythingAudioProcessorEditor> safeThis (this);
    MessageManager::callAsync ([safeThis]()
    {
        if (auto* self = safeThis.getComponent())
        {
            self->updateHostedEditor();
            self->_genericToggleButton.setEnabled (true);
        }
    });
}

void Mcfx_anythingAudioProcessorEditor::updateHostedEditor()
{
    auto* proc = getProcessor();

    // Pause audio processing while we destroy/create the hosted editor.
    // Some plugins (e.g. TDR Kotelnikov) crash if their editor is torn down
    // while their processAudio is concurrently running on the audio thread.
    // We also take the plugin lock to ensure the audio thread's tryLock in
    // processBlock will fail and the buffer will be silenced until we're done.
    const bool wasReady = proc->isPluginReady();
    proc->setPluginReady (false);
    const ScopedLock audioLock (proc->getPluginLock());

    auto* master = proc->getMasterInstance();

    // If the master instance has changed (e.g. a different plugin was loaded),
    // or there is no master anymore, drop any cached editors that belong to
    // the old plugin. This must happen here rather than in updateHostedEditor's
    // normal toggle path, because cached editors from an old plugin must not
    // outlive their plugin instance.
    auto editorBelongsToMaster = [master] (Component* ed) -> bool
    {
        if (ed == nullptr || master == nullptr) return false;
        if (auto* ape = dynamic_cast<AudioProcessorEditor*> (ed))
            return ape->getAudioProcessor() == master;
        return false;
    };

    if (_hostedCustomEditor != nullptr && ! editorBelongsToMaster (_hostedCustomEditor.get()))
    {
        if (_hostedEditor == _hostedCustomEditor.get())
        {
            _hostedEditor->removeComponentListener (this);
            removeChildComponent (_hostedEditor);
            _hostedEditor = nullptr;
        }
        _hostedCustomEditor.reset();
    }
    if (_hostedGenericEditor != nullptr && ! editorBelongsToMaster (_hostedGenericEditor.get()))
    {
        if (_hostedEditor == _hostedGenericEditor.get())
        {
            _hostedEditor->removeComponentListener (this);
            removeChildComponent (_hostedEditor);
            _hostedEditor = nullptr;
        }
        _hostedGenericEditor.reset();
    }

    // Detach the currently visible editor (we'll re-attach the chosen one below).
    if (_hostedEditor)
    {
        _hostedEditor->removeComponentListener (this);
        removeChildComponent (_hostedEditor);
        _hostedEditor = nullptr;
    }

    if (master)
    {
        const bool wantGeneric = _useGenericEditor || ! master->hasEditor();

        if (wantGeneric)
        {
            if (! _hostedGenericEditor)
                _hostedGenericEditor.reset (new GenericAudioProcessorEditor (*master));
            _hostedEditor = _hostedGenericEditor.get();
        }
        else
        {
            if (! _hostedCustomEditor)
                _hostedCustomEditor.reset (master->createEditor());
            _hostedEditor = _hostedCustomEditor.get();
        }

        if (_hostedEditor)
        {
            addAndMakeVisible (_hostedEditor);
            _hostedEditor->setTopLeftPosition (0, topBarHeight);
            _hostedEditor->addComponentListener (this);

            int w = jmax (minWidth, _hostedEditor->getWidth());
            int h = _hostedEditor->getHeight() + topBarHeight;
            setSize (w, h);
        }
    }
    else
    {
        setSize (minWidth, minHeight);
    }

    // Resume audio processing if it was previously enabled
    proc->setPluginReady (wasReady);

    // Update button text to reflect current mode
    bool hasCustomEditor = master != nullptr && master->hasEditor();
    _genericToggleButton.setButtonText (_useGenericEditor ? "Custom UI" : "Generic UI");
    _genericToggleButton.setTooltip (_useGenericEditor
        ? "Switch to the plugin's custom GUI"
        : "Switch to a generic parameter editor");

    // Update instance info and button visibility
    bool loaded = getProcessor()->isPluginLoaded();
    _inspectButton.setVisible (loaded);
    _genericToggleButton.setVisible (loaded && hasCustomEditor);
    updateSidechainButton();

    if (loaded)
    {
        int mainCh = getProcessor()->getChannelsPerInstance();
        int totalCh = getProcessor()->getTotalInstanceChannels();
        String info = String (getProcessor()->getNumInstances()) + "x " + String (mainCh) + "ch";
        if (totalCh != mainCh)
            info += " (" + String (totalCh) + " total)";
        _instanceInfoLabel.setText (info, dontSendNotification);
    }
    else
    {
        _instanceInfoLabel.setText ("", dontSendNotification);
    }

    // Re-layout the top bar: button visibility changed, so the plugin
    // selector button may need to expand/shrink to fill the new free space.
    resized();
}

void Mcfx_anythingAudioProcessorEditor::updateSelectorButtonText()
{
    auto name = getProcessor()->getLoadedPluginName();
    if (name.isNotEmpty())
        _pluginSelectorButton.setButtonText (name);
    else
        _pluginSelectorButton.setButtonText ("Select Plugin...");
}
