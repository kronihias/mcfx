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

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginProcessor.h"


//==============================================================================
/**
*/
class Mcfx_convolverAudioProcessorEditor  : public AudioProcessorEditor,
                                            public ButtonListener,
                                            public ChangeListener
{
public:
    Mcfx_convolverAudioProcessorEditor (Mcfx_convolverAudioProcessor* ownerFilter);
    ~Mcfx_convolverAudioProcessorEditor();

    //==============================================================================
    // This is just a standard Juce paint method...
    void paint (Graphics& g);
    
    void resized();
    void buttonClicked (Button* buttonThatWasClicked);
    
    void changeListenerCallback (ChangeBroadcaster *source);
    
    static void menuItemChosenCallback (int result, Mcfx_convolverAudioProcessorEditor* demoComponent);
    
    void UpdatePresets();
    
    void UpdateText();
    
private:
    Mcfx_convolverAudioProcessor* getProcessor() const
    {
        return static_cast <Mcfx_convolverAudioProcessor*> (getAudioProcessor());
    }
    TooltipWindow tooltipWindow;
    
    ScopedPointer<Label> label;
    ScopedPointer<TextEditor> txt_preset;
    PopupMenu popup_presets;
    OwnedArray<PopupMenu> popup_submenu;
    ScopedPointer<Label> label5;
    ScopedPointer<TextEditor> txt_debug;
    ScopedPointer<TextButton> btn_open;
    ScopedPointer<Label> label2;
    ScopedPointer<Label> label3;
    ScopedPointer<Label> label4;
    ScopedPointer<Label> num_ch;
    ScopedPointer<Label> num_spk;
    ScopedPointer<Label> num_hrtf;
    ScopedPointer<TextButton> btn_preset_folder;
    
};


#endif  // PLUGINEDITOR_H_INCLUDED
