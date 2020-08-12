/*
<<<<<<< HEAD
=======
 ==============================================================================
 
>>>>>>> 71e302b0336e90e59dc9e41f60ffb3479dd7bf05
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

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginProcessor.h"

#include "PluginView.h"


/*==============================================================================*/
//============================== MAIN EDITOR CLASS ============================== 

class Mcfx_convolverAudioProcessorEditor  : public AudioProcessorEditor,
                                            public Button::Listener,
                                            public ChangeListener,
                                            public ComboBox::Listener,
                                            public Slider::Listener,
                                            public Timer,
                                            public TextEditor::Listener,
                                            public KeyListener
{
public:
    //Mcfx_convolverAudioProcessorEditor (Mcfx_convolverAudioProcessor* ownerFilter);
    Mcfx_convolverAudioProcessorEditor (Mcfx_convolverAudioProcessor&);

    ~Mcfx_convolverAudioProcessorEditor();

    //==============================================================================
    // This is just a standard Juce paint method...
    void paint (Graphics& g);
    
    void resized();
    void buttonClicked (Button* buttonThatWasClicked);
    
    void changeListenerCallback (ChangeBroadcaster *source);
    
    void comboBoxChanged (ComboBox* comboBoxThatHasChanged);
    
    void textEditorFocusLost(TextEditor& ed);
    
    void textEditorReturnKeyPressed(TextEditor& ed);

    static void menuItemChosenCallback (int result, Mcfx_convolverAudioProcessorEditor* demoComponent);
    
    static void righClickButtonCallback (int result, Mcfx_convolverAudioProcessorEditor* demoComponent);
    
    void UpdateFiltersMenu();
    
    void UpdateText();
    
    void timerCallback();
    
    int getInputChannelFromDialog();
    
    void sliderValueChanged(Slider *slider);
    
    bool keyPressed (const KeyPress& key,
    Component* originatingComponent);
    
    bool keyStateChanged (bool isKeyDown, Component* originatingComponent);

private:
//    Mcfx_convolverAudioProcessor* getProcessor() const
//    {
//        return static_cast <Mcfx_convolverAudioProcessor*> (getAudioProcessor());
//    }

    Mcfx_convolverAudioProcessor& processor;
    
    View view;

    TooltipWindow tooltipWindow;

    PopupMenu filterMenu;
    OwnedArray<PopupMenu> filterSubmenus;
    
    PopupMenu setNewGeneralPath;
    
//    LookAndFeel_V4 MyLookAndFeel;
};


#endif  // PLUGINEDITOR_H_INCLUDED
