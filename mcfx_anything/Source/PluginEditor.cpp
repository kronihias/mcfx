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
    getProcessor()->removeChangeListener (this);

    closeAllInspectorWindows();

    if (_hostedEditor)
        _hostedEditor->removeComponentListener (this);
    _hostedEditor.reset();
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
    const int inspectWidth = 60;
    const int unloadWidth = 28;
    const int infoWidth = 120;

    int x = margin;
    _settingsButton.setBounds (x, margin, settingsWidth, buttonHeight);
    x += settingsWidth + margin;

    int rightWidth = genericWidth + margin + inspectWidth + margin + unloadWidth + margin + infoWidth + margin;
    _pluginSelectorButton.setBounds (x, margin, getWidth() - x - rightWidth, buttonHeight);

    _instanceInfoLabel.setBounds (getWidth() - margin - unloadWidth - margin - inspectWidth - margin - genericWidth - margin - infoWidth,
                                  margin, infoWidth, buttonHeight);
    _genericToggleButton.setBounds (getWidth() - margin - unloadWidth - margin - inspectWidth - margin - genericWidth,
                                    margin, genericWidth, buttonHeight);
    _inspectButton.setBounds (getWidth() - margin - unloadWidth - margin - inspectWidth, margin, inspectWidth, buttonHeight);
    _unloadButton.setBounds (getWidth() - margin - unloadWidth, margin, unloadWidth, buttonHeight);

    if (_hostedEditor)
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
    if (&component == _hostedEditor.get() && wasResized)
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

    PopupMenu menu;
    knownPlugins.addToMenu (menu, types, KnownPluginList::sortByManufacturer);

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&_pluginSelectorButton),
        [this, types] (int result)
        {
            if (result > 0)
            {
                int index = KnownPluginList::getIndexChosenByMenu (types, result);
                if (index >= 0 && index < types.size())
                    getProcessor()->loadPlugin (types.getReference (index));
            }
        });
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
                    // Default to generic editor when the main window shows a custom GUI,
                    // since some plugins (e.g. Waves) don't support multiple native
                    // editor instances and show a white/blank GUI for the second one.
                    bool masterCustomOpen = _hostedEditor != nullptr && ! _useGenericEditor;

                    auto* window = new InstanceInspectorWindow (instance, index, chPerInst, masterCustomOpen,
                        [this] (InstanceInspectorWindow* w) { _openInspectorWindows.removeFirstMatchingValue (w); });

                    _openInspectorWindows.add (window);
                }
            }
        });
}

void Mcfx_anythingAudioProcessorEditor::toggleMainEditorType()
{
    _useGenericEditor = ! _useGenericEditor;
    updateHostedEditor();
}

void Mcfx_anythingAudioProcessorEditor::updateHostedEditor()
{
    // Remove old editor
    if (_hostedEditor)
    {
        _hostedEditor->removeComponentListener (this);
        removeChildComponent (_hostedEditor.get());
        _hostedEditor.reset();
    }

    auto* master = getProcessor()->getMasterInstance();
    if (master)
    {
        if (_useGenericEditor || ! master->hasEditor())
            _hostedEditor.reset (new GenericAudioProcessorEditor (*master));
        else
            _hostedEditor.reset (master->createEditor());

        if (_hostedEditor)
        {
            addAndMakeVisible (_hostedEditor.get());
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
}

void Mcfx_anythingAudioProcessorEditor::updateSelectorButtonText()
{
    auto name = getProcessor()->getLoadedPluginName();
    if (name.isNotEmpty())
        _pluginSelectorButton.setButtonText (name);
    else
        _pluginSelectorButton.setButtonText ("Select Plugin...");
}
