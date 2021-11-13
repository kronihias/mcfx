//==============================================================================
/**
 * OVERRIDE StandaloneFilterWindow for native OS standalone window
 *  
*/

#ifndef JUCE_STANDALONE_FILTER_WINDOW_H_INCLUDED
#define JUCE_STANDALONE_FILTER_WINDOW_H_INCLUDED

#include "JuceHeader.h"
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace juce
{
//==============================================================================
/**
    A class that can be used to run a simple standalone application containing your filter.

    Just create one of these objects in your JUCEApplicationBase::initialise() method, and
    let it do its work. It will create your filter object using the same createPluginFilter() function
    that the other plugin wrappers use.

    @tags{Audio}
*/
class CustomStandaloneFilterWindow    : public DocumentWindow,
                                        public MenuBarModel,
                                        private Button::Listener
{
public:
    //==============================================================================
    typedef StandalonePluginHolder::PluginInOuts PluginInOuts;

    //==============================================================================
    /** Creates a window with a given title and colour.
        The settings object can be a PropertySet that the class should use to
        store its settings (it can also be null). If takeOwnershipOfSettings is
        true, then the settings object will be owned and deleted by this object.
    */
    CustomStandaloneFilterWindow (const String& title,
                                  Colour backgroundColour,
                                  PropertySet* settingsToUse,
                                  bool takeOwnershipOfSettings,
                                  const String& preferredDefaultDeviceName = String(),
                                  const AudioDeviceManager::AudioDeviceSetup* preferredSetupOptions = nullptr,
                                  const Array<PluginInOuts>& constrainToConfiguration = {},
                                #if JUCE_ANDROID || JUCE_IOS
                                  bool autoOpenMidiDevices = true
                                #else
                                  bool autoOpenMidiDevices = false
                                #endif
                                ) : DocumentWindow (title,
                                                    backgroundColour,
                                                    DocumentWindow::minimiseButton | DocumentWindow::closeButton),
                                    optionsButton ("Options"),
                                    
                                    titleBarModelChange (false)
    {
       #if JUCE_IOS || JUCE_ANDROID
        setTitleBarHeight (0);
       #else
        Component::addAndMakeVisible (optionsButton);
        optionsButton.addListener (this);
        optionsButton.setTriggeredOnMouseDown (true);
       #if JUCE_WINDOWS
        setTitleBarButtonsRequired (DocumentWindow::minimiseButton | DocumentWindow::closeButton, false);
        #else
        setTitleBarButtonsRequired (DocumentWindow::minimiseButton | DocumentWindow::closeButton, true);
        #endif
       #endif
//        setTitleBarHeight (0);
//        clearContentComponent();
        
        pluginHolder.reset (new StandalonePluginHolder (settingsToUse, takeOwnershipOfSettings,
                                                        preferredDefaultDeviceName, preferredSetupOptions,
                                                        constrainToConfiguration, autoOpenMidiDevices));
        
        /*modified code*/
        if (auto* props = pluginHolder->settings.get())
        {
            const int   x = props->getIntValue ("windowX", -100);
            const int   y = props->getIntValue ("windowY", -100);
            const bool  native = props->getBoolValue("nativeTitleBar", true);

            setUsingNativeTitleBar (native);

            if (!native)
            {
                setDropShadowEnabled (false);
                optionsButton.setVisible(!native);
            }

            if (x != -100 && y != -100)
                setBoundsConstrained ({ x, y, getWidth(), getHeight() });
            else
                centreWithSize (getWidth(), getHeight());
        }
        else
        {
            setUsingNativeTitleBar (true);
            centreWithSize (getWidth(), getHeight());
        }
        #if JUCE_MAC
            setMacMainMenu(this);
        #else
            setMenuBar (this);
        #endif
        
        #if JUCE_IOS || JUCE_ANDROID
         setFullScreen (true);
         setContentOwned (new MainContentComponent (*this), false);
        #else
         setContentOwned (new MainContentComponent (*this), true);
        
        // addToDesktop();
        //end modified code
       #endif
    }


    ~CustomStandaloneFilterWindow() override
    {
       #if (! JUCE_IOS) && (! JUCE_ANDROID)
        if (auto* props = pluginHolder->settings.get())
        {
            props->setValue ("windowX", getX());
            props->setValue ("windowY", getY());

            //added lines
             if (!titleBarModelChange)
                props->setValue("nativeTitleBar", isUsingNativeTitleBar());
        }
       #endif

        #if JUCE_MAC
            MenuBarModel::setMacMainMenu (0);
        #else
            setMenuBar (0);
        #endif

        pluginHolder->stopPlaying();
        clearContentComponent();
        pluginHolder = nullptr;
    }

    //==============================================================================
    StringArray getMenuBarNames() override
    {
        const char* const names[] = { "File", "Presets", "Options", nullptr };

        return StringArray (names);
    }

    PopupMenu getMenuForIndex (int topLevelMenuIndex, const String& /*menuName*/) override
    {
        PopupMenu menu;

        if (topLevelMenuIndex == 0)
        {
            menu.addItem (1, TRANS("Load a saved state..."));
            menu.addItem (2, TRANS("Save current state"));
            menu.addItem (3, TRANS("Save current state as..."));
            menu.addSeparator();
            menu.addItem (4, TRANS("Reset to default state"));
            menu.addSeparator();
            menu.addItem (5, TRANS("Quit"));
        }
        else if (topLevelMenuIndex == 1)
        {
            if (getAudioProcessor() != nullptr && getAudioProcessor()->getNumPrograms() > 0)
            {
                for (int i=0; i < getAudioProcessor()->getNumPrograms(); i++)
                    menu.addItem (11+i, getAudioProcessor()->getProgramName(i));
            }
            else
                menu.addItem (10, TRANS("(none)"), false);
        }
        else if (topLevelMenuIndex == 2)
        {
            menu.addItem (6, TRANS("Audio Settings..."));
            menu.addSeparator();
            if (isUsingNativeTitleBar())
                menu.addItem (7, TRANS("Use JUCE Titlebar"));
            else
                menu.addItem (7, TRANS("Use Native Titlebar"));
        }
        return menu;
    }

    void menuItemSelected (int menuItemID, int /*topLevelMenuIndex*/) override
    {
        if (menuItemID == 1)
            pluginHolder->askUserToLoadState();
        else if (menuItemID == 2)
            pluginHolder->savePluginState();
        else if (menuItemID == 3)
            pluginHolder->askUserToLoadState();
        else if (menuItemID == 4)
            resetToDefaultState();
        else if (menuItemID == 5)
            closeButtonPressed();
        else if (menuItemID == 6)
            pluginHolder->showAudioSettingsDialog();
        else if (menuItemID == 7)
        {
            if (auto* props = pluginHolder->settings.get())
            {
                titleBarModelChange = true;
                props->setValue("nativeTitleBar", !isUsingNativeTitleBar());
                AlertWindow::showMessageBoxAsync(AlertWindow::InfoIcon, "Need restart", "You need to restart this plugin to apply the changes", TRANS("Ok"), this);
            }
        }
        else if (menuItemID >= 11)
          getAudioProcessor()->setCurrentProgram(menuItemID-11);
    }

    //==============================================================================
    AudioProcessor* getAudioProcessor() const noexcept      { return pluginHolder->processor.get(); }
    AudioDeviceManager& getDeviceManager() const noexcept   { return pluginHolder->deviceManager; }

    /** Deletes and re-creates the plugin, resetting it to its default state. */
    void resetToDefaultState()
    {
        pluginHolder->stopPlaying();
        clearContentComponent();
        pluginHolder->deletePlugin();

        if (auto* props = pluginHolder->settings.get())
            props->removeValue ("filterState");

        pluginHolder->createPlugin();
        setContentOwned (new MainContentComponent (*this), true);
        pluginHolder->startPlaying();

        resized();
    }

    //==============================================================================
    void closeButtonPressed() override
    {
        pluginHolder->savePluginState();

        JUCEApplicationBase::quit();
    }

    void handleMenuResult (int result)
    {
        switch (result)
        {
            case 1:  pluginHolder->showAudioSettingsDialog(); break;
            case 2:  pluginHolder->askUserToSaveState(); break;
            case 3:  pluginHolder->askUserToLoadState(); break;
            case 4:  resetToDefaultState(); break;
            default: break;
        }
    }

    static void menuCallback (int result, CustomStandaloneFilterWindow* button)
    {
        if (button != nullptr && result != 0)
            button->handleMenuResult (result);
    }

    void resized() override
    {
        DocumentWindow::resized();
        #if JUCE_WINDOWS
        optionsButton.setBounds (8, 6, 60, getTitleBarHeight() - 8);
        #else
        optionsButton.setBounds (getWidth()-68, 6, 60, getTitleBarHeight() - 8);
        #endif
    }

    virtual StandalonePluginHolder* getPluginHolder()    { return pluginHolder.get(); }

    std::unique_ptr<StandalonePluginHolder> pluginHolder;

private:
    void buttonClicked (Button*) override
    {
        PopupMenu m;
        m.addItem (1, TRANS("Audio/MIDI Settings..."));
        m.addSeparator();
        m.addItem (2, TRANS("Save current state..."));
        m.addItem (3, TRANS("Load a saved state..."));
        m.addSeparator();
        m.addItem (4, TRANS("Reset to default state"));

        m.showMenuAsync (PopupMenu::Options(),
                         ModalCallbackFunction::forComponent (menuCallback, this));
    }

    //==============================================================================
    class MainContentComponent  : public Component,
                                  private Value::Listener,
                                  private Button::Listener,
                                  private ComponentListener
    {
    public:
        MainContentComponent (CustomStandaloneFilterWindow& filterWindow)
            : owner (filterWindow), notification (this),
              editor (owner.getAudioProcessor()->hasEditor() ? owner.getAudioProcessor()->createEditorIfNeeded()
                                                             : new GenericAudioProcessorEditor (*owner.getAudioProcessor()))
        {
            Value& inputMutedValue = owner.pluginHolder->getMuteInputValue();

            if (editor != nullptr)
            {
                editor->addComponentListener (this);
                componentMovedOrResized (*editor, false, true);

                addAndMakeVisible (editor.get());
            }

            addChildComponent (notification);

            if (owner.pluginHolder->getProcessorHasPotentialFeedbackLoop())
            {
                inputMutedValue.addListener (this);
                shouldShowNotification = inputMutedValue.getValue();
            }

            inputMutedChanged (shouldShowNotification);
        }

        ~MainContentComponent() override
        {
            if (editor != nullptr)
            {
                editor->removeComponentListener (this);
                owner.pluginHolder->processor->editorBeingDeleted (editor.get());
                editor = nullptr;
            }
        }

        void resized() override
        {
            auto r = getLocalBounds();

            if (shouldShowNotification)
                notification.setBounds (r.removeFromTop (NotificationArea::height));

            if (editor != nullptr)
                editor->setBounds (editor->getLocalArea (this, r)
                                          .withPosition (r.getTopLeft().transformedBy (editor->getTransform().inverted())));
        }

    private:
        //==============================================================================
        class NotificationArea : public Component
        {
        public:
            enum { height = 30 };

            NotificationArea (Button::Listener* settingsButtonListener)
                : notification ("notification", "Audio input is muted to avoid feedback loop"),
                 #if JUCE_IOS || JUCE_ANDROID
                  settingsButton ("Unmute Input")
                 #else
                  settingsButton ("Settings...")
                 #endif
            {
                setOpaque (true);

                notification.setColour (Label::textColourId, Colours::black);

                settingsButton.addListener (settingsButtonListener);

                addAndMakeVisible (notification);
                addAndMakeVisible (settingsButton);
            }

            void paint (Graphics& g) override
            {
                auto r = getLocalBounds();

                g.setColour (Colours::darkgoldenrod);
                g.fillRect (r.removeFromBottom (1));

                g.setColour (Colours::lightgoldenrodyellow);
                g.fillRect (r);
            }

            void resized() override
            {
                auto r = getLocalBounds().reduced (5);

                settingsButton.setBounds (r.removeFromRight (70));
                notification.setBounds (r);
            }
        private:
            Label notification;
            TextButton settingsButton;
        };

        //==============================================================================
        void inputMutedChanged (bool newInputMutedValue)
        {
            shouldShowNotification = newInputMutedValue;
            notification.setVisible (shouldShowNotification);

           #if JUCE_IOS || JUCE_ANDROID
            resized();
           #else
            if (editor != nullptr)
            {
                auto rect = getSizeToContainEditor();

                setSize (rect.getWidth(),
                         rect.getHeight() + (shouldShowNotification ? NotificationArea::height : 0));
            }
           #endif
        }

        void valueChanged (Value& value) override     { inputMutedChanged (value.getValue()); }
        void buttonClicked (Button*) override
        {
           #if JUCE_IOS || JUCE_ANDROID
            owner.pluginHolder->getMuteInputValue().setValue (false);
           #else
            owner.pluginHolder->showAudioSettingsDialog();
           #endif
        }

        //==============================================================================
        void componentMovedOrResized (Component&, bool, bool) override
        {
            if (editor != nullptr)
            {
                auto rect = getSizeToContainEditor();

                setSize (rect.getWidth(),
                         rect.getHeight() + (shouldShowNotification ? NotificationArea::height : 0));
            }
        }

        Rectangle<int> getSizeToContainEditor() const
        {
            if (editor != nullptr)
                return getLocalArea (editor.get(), editor->getLocalBounds());

            return {};
        }

        //==============================================================================
        CustomStandaloneFilterWindow& owner;
        NotificationArea notification;
        std::unique_ptr<AudioProcessorEditor> editor;
        bool shouldShowNotification = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
    };

    //==============================================================================
    TextButton optionsButton;
    bool titleBarModelChange;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomStandaloneFilterWindow)
};

} // namespace juce

#endif // JUCE_STANDALONE_FILTER_WINDOW_H_INCLUDED
