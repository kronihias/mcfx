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

    // Defers creation of the native plugin editor until the inspector window
    // is actually on-screen. Two problems drive this:
    //  - Some plugins (notably Waves) render a blank/white custom GUI when
    //    IPlugView::attached() happens before the host NSView is on screen:
    //    they bind their GL/Metal context to a zero-sized parent and never
    //    recover.
    //  - Some Apple AUs (e.g. AUHipassFilter) throw an NSException out of
    //    -[CAAppleEQGraphView setFrameSize:] during the first autolayout
    //    pass that runs inside -[NSWindow makeKeyAndOrderFront:], which
    //    crashes the host if their native view already exists at show time.
    // rebuildEditor() therefore creates a GenericAudioProcessorEditor as a
    // placeholder until _didPostShowRebuild flips, at which point it builds
    // the real custom editor.
    void parentHierarchyChanged() override
    {
        if (_didPostShowRebuild) return;
        if (! (_hasCustomEditor && ! _useGenericEditor)) return;
        if (! isShowing() || getPeer() == nullptr) return;

        _didPostShowRebuild = true;

        WeakReference<Component> safeThis (this);
        MessageManager::callAsync ([safeThis]()
        {
            if (auto* c = dynamic_cast<InspectorContentComponent*> (safeThis.get()))
                c->rebuildEditor();
        });
    }

private:
    static constexpr int toolbarHeight = 28;

    void toggleEditorType()
    {
        _useGenericEditor = ! _useGenericEditor;

        // Defer the rebuild until the current mouse event has fully unwound.
        // Some plugins (e.g. TDR Kotelnikov) crash inside IPlugView::removed()
        // when torn down from a nested event dispatch stack.
        _toggleButton.setEnabled (false);
        WeakReference<Component> safeThis (this);
        MessageManager::callAsync ([safeThis]()
        {
            if (auto* c = dynamic_cast<InspectorContentComponent*> (safeThis.get()))
            {
                c->rebuildEditor();
                c->_toggleButton.setEnabled (true);
            }
        });
    }

    void rebuildEditor()
    {
        _pluginEditor.reset();

        // Only create the native plugin editor once the inspector window has
        // been shown — see parentHierarchyChanged() for why.
        const bool canMakeCustom = _hasCustomEditor && ! _useGenericEditor && _didPostShowRebuild;

        if (canMakeCustom)
            _pluginEditor.reset (_instance->createEditor());
        else
            _pluginEditor.reset (new GenericAudioProcessorEditor (*_instance));

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
    bool _didPostShowRebuild = false;
    std::unique_ptr<Component> _pluginEditor;
    TextButton _toggleButton;

    JUCE_DECLARE_WEAK_REFERENCEABLE (InspectorContentComponent)
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

    // Called by the processor BEFORE tearing down plugin instances, so the
    // hosted plugin editor (and all inspector popups) are destroyed while
    // the plugin instances are still fully alive and operational.
public:
    void destroyHostedEditorsForUnload();

private:
    void showPluginMenu();
    void showSettings();
    void showInstanceInspectorMenu();
    void showSidechainRoutingMenu();
    void closeAllInspectorWindows();
    void updateHostedEditor();
    void updateSelectorButtonText();
    void updateSidechainButton();
    void toggleMainEditorType();

    TextButton _pluginSelectorButton;
    TextButton _settingsButton;
    TextButton _unloadButton;
    TextButton _inspectButton;
    TextButton _genericToggleButton;
    TextButton _sidechainButton;
    Label _instanceInfoLabel;

    // The currently visible editor. Raw pointer into either _hostedCustomEditor
    // or _hostedGenericEditor — does NOT own.
    Component* _hostedEditor = nullptr;

    // The plugin's custom editor is cached and kept alive across Generic<->Custom
    // toggles. Some VST3 plugins (e.g. TDR Kotelnikov) crash inside their own
    // IPlugView::removed() when destroyed in a process that has multiple
    // instances of the same plugin alive. Destroying it only on real
    // plugin unload/reload (while all instances are still alive) avoids the
    // crash for toggle operations, which are the common case.
    std::unique_ptr<Component> _hostedCustomEditor;
    std::unique_ptr<Component> _hostedGenericEditor;

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
