/*
 ==============================================================================
 
 This file is part of the mcfx (Multichannel Effects) plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com
 
 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)
 
 Details of these licenses can be found at: www.gnu.org/licenses
 
 ambix is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 
 ==============================================================================
 */

#ifndef __JUCE_HEADER_2351554F350E59C5__
#define __JUCE_HEADER_2351554F350E59C5__

//[Headers]     -- You can add your own extra header files here --
#include "JuceHeader.h"
#include "PluginProcessor.h"
//[/Headers]



//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Jucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
class Mcfx_gain_delayAudioProcessorEditor  : public AudioProcessorEditor,
                                             public SliderListener,
                                             public ButtonListener,
                                             public ChangeListener,
                                             public ComboBoxListener
{
public:
    //==============================================================================
    Mcfx_gain_delayAudioProcessorEditor (Mcfx_gain_delayAudioProcessor* ownerFilter);
    ~Mcfx_gain_delayAudioProcessorEditor();

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    //[/UserMethods]

    void paint (Graphics& g);
    void resized();
    void sliderValueChanged (Slider* sliderThatWasMoved);
    void buttonClicked (Button* buttonThatWasClicked);
    void comboBoxChanged (ComboBox* comboBoxThatHasChanged);
    
    void changeListenerCallback (ChangeBroadcaster *source);
    
    // Binary resources:
    static const char* clipboard35_png;
    static const int clipboard35_pngSize;
    static const char* clipboard35_grey_png;
    static const int clipboard35_grey_pngSize;
    static const char* phase_symbol_png;
    static const int phase_symbol_pngSize;
    static const char* phase_symbol_inv_png;
    static const int phase_symbol_inv_pngSize;
    static const char* phase_symbol_over_png;
    static const int phase_symbol_over_pngSize;
    static const char* solo_symbol_png;
    static const int solo_symbol_pngSize;
    static const char* solo_symbol_over_png;
    static const int solo_symbol_over_pngSize;
    static const char* solo_symbol_act_png;
    static const int solo_symbol_act_pngSize;
    static const char* mute_symbol_png;
    static const int mute_symbol_pngSize;
    static const char* mute_symbol_over_png;
    static const int mute_symbol_over_pngSize;
    static const char* mute_symbol_act_png;
    static const int mute_symbol_act_pngSize;
    static const char* sig_symbol_png;
    static const int sig_symbol_pngSize;
    static const char* sig_symbol_over_png;
    static const int sig_symbol_over_pngSize;
    static const char* sig_symbol_act_png;
    static const int sig_symbol_act_pngSize;

private:
    //[UserVariables]   -- You can add your own custom variables in this section.
    //[/UserVariables]

    //==============================================================================
    ScopedPointer<Label> lbl_gd;
    ScopedPointer<Label> label2;
    ScopedPointer<Label> label3;
    ScopedPointer<ImageButton> btn_paste_gain;
    ScopedPointer<ImageButton> btn_paste_gain2;
  
  
    ScopedPointer<DrawableButton> btn_phase_reset;
    ScopedPointer<DrawableButton> btn_mute_reset;
    ScopedPointer<DrawableButton> btn_solo_reset;
    ScopedPointer<DrawableButton> btn_sig_reset;
    
    OwnedArray<Slider> sld_del;
    OwnedArray<Slider> sld_gain;
    OwnedArray<Label> lbl_ch;
    OwnedArray<DrawableButton> btn_phase;
    OwnedArray<DrawableButton> btn_mute;
    OwnedArray<DrawableButton> btn_solo;
    OwnedArray<DrawableButton> btn_sig;
    
    ScopedPointer<Slider> sld_siggain;
    ScopedPointer<ComboBox> box_signal;
    ScopedPointer<ComboBox> box_sigtime;
    ScopedPointer<Slider> sld_sigfreq;
    ScopedPointer<Slider> sld_sigstepinterval;
    ScopedPointer<ToggleButton> tgl_sigstep;
    ScopedPointer<Label> label4;
    ScopedPointer<Label> label6;
    ScopedPointer<Label> label7;
    
    Mcfx_gain_delayAudioProcessor* getProcessor() const
    {
        return static_cast <Mcfx_gain_delayAudioProcessor*> (getAudioProcessor());
    }
    
    TooltipWindow tooltipWindow;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Mcfx_gain_delayAudioProcessorEditor)
};

//[EndFile] You can add extra defines here...
//[/EndFile]

#endif   // __JUCE_HEADER_2351554F350E59C5__
