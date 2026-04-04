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

#ifndef PLUGINEDITOR_H_INCLUDED
#define PLUGINEDITOR_H_INCLUDED

#include "JuceHeader.h"
#include "PluginProcessor.h"

//==============================================================================
// Popup window that shows a specific plugin instance's GUI for inspection
// Wrapper component that holds a toolbar + the plugin editor inside the inspector
class InspectorContentComponent : public Component
{
public:
    InspectorContentComponent (AudioPluginInstance* instance, bool hasCustomEditor, bool startGeneric)
        : _instance (instance), _hasCustomEditor (hasCustomEditor), _useGenericEditor (startGeneric)
    {
        if (_hasCustomEditor)
        {
            _toggleButton.setButtonText (_useGenericEditor ? "Custom UI" : "Generic UI");
            _toggleButton.setTooltip ("Switch between custom and generic editor");
            _toggleButton.onClick = [this]() { toggleEditorType(); };
            addAndMakeVisible (_toggleButton);
        }

        rebuildEditor();
    }

    void resized() override
    {
        int toolbar = _hasCustomEditor ? toolbarHeight : 0;
        _toggleButton.setBounds (4, 2, 80, toolbarHeight - 4);

        if (_pluginEditor)
            _pluginEditor->setBounds (0, toolbar, getWidth(), getHeight() - toolbar);
    }

private:
    static constexpr int toolbarHeight = 28;

    void toggleEditorType()
    {
        _useGenericEditor = ! _useGenericEditor;
        rebuildEditor();
    }

    void rebuildEditor()
    {
        _pluginEditor.reset();

        if (_useGenericEditor || ! _hasCustomEditor)
            _pluginEditor.reset (new GenericAudioProcessorEditor (*_instance));
        else
            _pluginEditor.reset (_instance->createEditor());

        if (_pluginEditor)
        {
            addAndMakeVisible (_pluginEditor.get());

            int toolbar = _hasCustomEditor ? toolbarHeight : 0;
            int w = jmax (200, _pluginEditor->getWidth());
            int h = _pluginEditor->getHeight() + toolbar;

            _pluginEditor->setBounds (0, toolbar, w, _pluginEditor->getHeight());
            _toggleButton.setBounds (4, 2, 80, toolbarHeight - 4);
            _toggleButton.setButtonText (_useGenericEditor ? "Custom UI" : "Generic UI");

            setSize (w, h);
        }
    }

    AudioPluginInstance* _instance;
    bool _hasCustomEditor;
    bool _useGenericEditor;
    std::unique_ptr<Component> _pluginEditor;
    TextButton _toggleButton;
};

class InstanceInspectorWindow : public DocumentWindow
{
public:
    InstanceInspectorWindow (AudioPluginInstance* instance, int instanceIndex, int channelsPerInstance,
                             bool masterEditorOpen = false,
                             std::function<void(InstanceInspectorWindow*)> onClose = nullptr)
        : DocumentWindow ("Instance #" + String (instanceIndex + 1)
                          + " (ch " + String (instanceIndex * channelsPerInstance + 1)
                          + "-" + String ((instanceIndex + 1) * channelsPerInstance) + ")",
                          Colour (0xff2a2a2a), DocumentWindow::closeButton),
          _onClose (std::move (onClose))
    {
        if (instance)
        {
            bool hasCustom = instance->hasEditor();
            auto* content = new InspectorContentComponent (instance, hasCustom, masterEditorOpen);
            setContentOwned (content, true);
            setResizable (true, false);
            setUsingNativeTitleBar (true);
            setAlwaysOnTop (true);
            centreWithSize (getContentComponent()->getWidth(),
                            getContentComponent()->getHeight());
            setVisible (true);
            toFront (true);
        }
    }

    void closeButtonPressed() override
    {
        if (_onClose) _onClose (this);
        delete this;
    }

    bool keyPressed (const KeyPress& key) override
    {
        if (key == KeyPress::escapeKey) { closeButtonPressed(); return true; }
        return DocumentWindow::keyPressed (key);
    }

private:
    std::function<void(InstanceInspectorWindow*)> _onClose;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstanceInspectorWindow)
};

//==============================================================================
class Mcfx_anythingAudioProcessorEditor  : public AudioProcessorEditor,
                                              public ChangeListener,
                                              public ComponentListener
{
public:
    Mcfx_anythingAudioProcessorEditor (Mcfx_anythingAudioProcessor* ownerFilter);
    ~Mcfx_anythingAudioProcessorEditor();

    void paint (Graphics& g) override;
    void resized() override;

    void changeListenerCallback (ChangeBroadcaster* source) override;
    void componentMovedOrResized (Component& component, bool wasMoved, bool wasResized) override;

private:
    static constexpr int topBarHeight = 40;
    static constexpr int minWidth = 500;
    static constexpr int minHeight = 200;

    void showPluginMenu();
    void showSettings();
    void showInstanceInspectorMenu();
    void closeAllInspectorWindows();
    void updateHostedEditor();
    void updateSelectorButtonText();
    void toggleMainEditorType();

    TextButton _pluginSelectorButton;
    TextButton _settingsButton;
    TextButton _unloadButton;
    TextButton _inspectButton;
    TextButton _genericToggleButton;
    Label _instanceInfoLabel;

    std::unique_ptr<Component> _hostedEditor;
    bool _useGenericEditor = false;

    Array<InstanceInspectorWindow*> _openInspectorWindows;

    TooltipWindow _tooltipWindow;
    LookAndFeel_V3 _lookAndFeel;

    Mcfx_anythingAudioProcessor* getProcessor() const
    {
        return static_cast<Mcfx_anythingAudioProcessor*> (getAudioProcessor());
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Mcfx_anythingAudioProcessorEditor)
};

#endif  // PLUGINEDITOR_H_INCLUDED
