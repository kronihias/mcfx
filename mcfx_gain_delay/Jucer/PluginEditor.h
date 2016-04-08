/*
  ==============================================================================

  This is an automatically generated GUI class created by the Introjucer!

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Created with Introjucer version: 4.1.0

  ------------------------------------------------------------------------------

  The Introjucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright (c) 2015 - ROLI Ltd.

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
                                             public ComboBoxListener
{
public:
    //==============================================================================
    Mcfx_gain_delayAudioProcessorEditor (Mcfx_gain_delayAudioProcessor* ownerFilter);
    ~Mcfx_gain_delayAudioProcessorEditor();

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    //[/UserMethods]

    void paint (Graphics& g) override;
    void resized() override;
    void sliderValueChanged (Slider* sliderThatWasMoved) override;
    void buttonClicked (Button* buttonThatWasClicked) override;
    void comboBoxChanged (ComboBox* comboBoxThatHasChanged) override;

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
    ScopedPointer<Slider> sld_del;
    ScopedPointer<Slider> sld_gain;
    ScopedPointer<Label> label2;
    ScopedPointer<Label> label3;
    ScopedPointer<Label> lbl_ch;
    ScopedPointer<ImageButton> btn_paste_gain;
    ScopedPointer<ImageButton> btn_paste_gain2;
    ScopedPointer<ImageButton> btn_phase;
    ScopedPointer<ImageButton> btn_solo;
    ScopedPointer<ImageButton> btn_mute;
    ScopedPointer<ImageButton> btn_mute_reset;
    ScopedPointer<ImageButton> btn_solo_reset;
    ScopedPointer<ImageButton> btn_sig;
    ScopedPointer<ImageButton> btn_phase_reset;
    ScopedPointer<Slider> sld_siggain;
    ScopedPointer<ComboBox> box_signal;
    ScopedPointer<ComboBox> box_sigtime;
    ScopedPointer<Slider> sld_sigfreq;
    ScopedPointer<Slider> sld_sigstepinterval;
    ScopedPointer<ToggleButton> tgl_sigstep;
    ScopedPointer<ImageButton> btn_sig_reset;
    ScopedPointer<Label> label4;
    ScopedPointer<Label> label6;
    ScopedPointer<Label> label7;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Mcfx_gain_delayAudioProcessorEditor)
};

//[EndFile] You can add extra defines here...
//[/EndFile]

#endif   // __JUCE_HEADER_2351554F350E59C5__
