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
                                            public Timer,
                                            public TextEditor::Listener
{
public:
    Mcfx_convolverAudioProcessorEditor (Mcfx_convolverAudioProcessor&);

    ~Mcfx_convolverAudioProcessorEditor();

    //==============================================================================
    // This is just a standard Juce paint method...
    void paint (Graphics& g);
    
    void resized();
    
    void timerCallback();
    
    void changeListenerCallback (ChangeBroadcaster *source);
    
    void UpdateText();
    
    void UpdateFiltersMenu();
    
    void buttonClicked (Button* buttonThatWasClicked);
    
    static void righClickButtonCallback (int result, Mcfx_convolverAudioProcessorEditor* demoComponent);
    
//    static void menuItemChosenCallback (int result, Mcfx_convolverAudioProcessorEditor* demoComponent);
    void menuItemChosenCallback (int result);
    
    void chooseExportFile();
    
    void comboBoxChanged (ComboBox* comboBoxThatHasChanged);
    
    int getInputChannelFromDialog();

private:
    View view;

    TooltipWindow tooltipWindow;
    
    PopupMenu setNewGeneralPath;

    PopupMenu filterMenu;
    OwnedArray<PopupMenu> filterSubmenus;
    
    File exportFile;
    CriticalSection exportFileMutex;
    
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> masterGainAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> MinPSComboAttachment;
    
    Mcfx_convolverAudioProcessor& processor;
};


#endif  // PLUGINEDITOR_H_INCLUDED
