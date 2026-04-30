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
#include "InspectorClickBlocker.h"

//==============================================================================
// Transparent overlay placed on top of the inspector's plugin editor to make
// it read-only. We never want a user to drive parameters through a slave
// instance — the master→slave sync is one-way, so any change made on a
// slave's GUI desyncs that slave from everything else (see
// Mcfx_anythingAudioProcessor::syncParametersFromMaster).
//
// The overlay has two layers, both invisible to rendering and opaque to input:
//   1. JUCE-level: setInterceptsMouseClicks(true, false) catches every event
//      that travels through JUCE's component tree.
//   2. macOS-native: an internal NSViewComponent wraps a custom NSView
//      whose hitTest: returns self for any point in its frame and whose
//      mouse/keyboard handlers are no-ops. This is required for AU and VST3
//      plugin UIs whose events flow NSWindow → NSView hitTest: directly to
//      the native view, bypassing JUCE entirely. The overlay's NSView is a
//      sibling of the plugin's NSView in the JUCE peer's view, and since
//      it's added later it sits on top in NSWindow's reverse-order
//      hit-testing — see InspectorContentComponent::rebuildEditor.
class LockedEditorOverlay : public Component
{
public:
    LockedEditorOverlay()
    {
        // Eat every JUCE-level mouse event that lands on us, and don't pass
        // through to siblings underneath. Keyboard focus stays off so we
        // never become a tab stop.
        setInterceptsMouseClicks (true, false);
        setWantsKeyboardFocus (false);

       #if JUCE_MAC
        if (auto* nsView = mcfx::createInspectorClickBlockerNSView())
            _nsViewBlocker.setView (nsView);
        addAndMakeVisible (_nsViewBlocker);
       #endif
    }

    void paint (Graphics& g) override
    {
        // Small badge in the top-right corner so the lock state is obvious
        // without obscuring the plugin's UI.
        auto badgeText = String (CharPointer_UTF8 ("\xf0\x9f\x94\x92")) + " read-only";
        Font f (FontOptions (11.0f));
        const int padX = 6;
        const int padY = 3;
        const int textW = GlyphArrangement::getStringWidthInt (f, badgeText);
        Rectangle<int> badge (getWidth() - textW - 2 * padX - 4, 4,
                              textW + 2 * padX, f.getHeight() + 2 * padY);

        g.setColour (Colour (0xcc202020));
        g.fillRoundedRectangle (badge.toFloat(), 4.0f);
        g.setColour (Colour (0xffe06060));
        g.drawRoundedRectangle (badge.toFloat(), 4.0f, 1.0f);
        g.setColour (Colours::white);
        g.setFont (f);
        g.drawText (badgeText, badge, Justification::centred, false);
    }

    void resized() override
    {
       #if JUCE_MAC
        _nsViewBlocker.setBounds (getLocalBounds());
       #endif
    }

private:
   #if JUCE_MAC
    NSViewComponent _nsViewBlocker;
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LockedEditorOverlay)
};

//==============================================================================
// Popup window that shows a specific plugin instance's GUI for inspection.
// The window is always read-only — interaction is blocked via LockedEditorOverlay
// so the user can verify master→slave sync without being able to desync a
// slave by editing its GUI directly.
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

        Rectangle<int> editorArea (0, toolbar, getWidth(), getHeight() - toolbar);

        if (_pluginEditor)
            _pluginEditor->setBounds (editorArea);

        // Overlay tracks the editor area exactly. Sized AFTER the editor so
        // the JUCE-level mouse interception sits over the editor's content.
        _overlay.setBounds (editorArea);
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
        // Drop the overlay first so the editor swap doesn't briefly leave a
        // stale overlay parented above a freshly-created editor.
        removeChildComponent (&_overlay);

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

            // CRITICAL: add the overlay AFTER the editor. Its child
            // NSViewComponent is then added to the JUCE peer's NSView AFTER
            // the plugin editor's NSView, which puts our blocker NSView on
            // top in NSWindow's hit-test order. Reversing this order would
            // make the AU/VST3 native UI receive every event again.
            addAndMakeVisible (_overlay);

            // Disable JUCE-side input on the editor too — for plugin editors
            // implemented in pure JUCE, this propagates to all slider/button
            // children. This is belt-and-braces with the overlay; the
            // overlay alone is sufficient, but disabled widgets give the
            // user a clearer visual cue that the controls are inert.
            _pluginEditor->setEnabled (false);

            int toolbar = _hasCustomEditor ? toolbarHeight : 0;
            int w = jmax (200, _pluginEditor->getWidth());
            int h = _pluginEditor->getHeight() + toolbar;

            _pluginEditor->setBounds (0, toolbar, w, _pluginEditor->getHeight());
            _overlay.setBounds (0, toolbar, w, _pluginEditor->getHeight());
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
    LockedEditorOverlay _overlay;

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
                          + "-" + String ((instanceIndex + 1) * channelsPerInstance) + ")"
                          + String (CharPointer_UTF8 (" \xe2\x80\x94 read-only")),
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
