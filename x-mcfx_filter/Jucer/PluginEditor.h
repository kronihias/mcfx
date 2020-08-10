/*
  ==============================================================================

  This is an automatically generated GUI class created by the Introjucer!

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Created with Introjucer version: 3.2.0

  ------------------------------------------------------------------------------

  The Introjucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright (c) 2015 - ROLI Ltd.

  ==============================================================================
*/

#ifndef __JUCE_HEADER_F47B898C4BCD30D0__
#define __JUCE_HEADER_F47B898C4BCD30D0__

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
class LowhighpassAudioProcessorEditor  : public AudioProcessorEditor,
                                         public ButtonListener,
                                         public SliderListener
{
public:
    //==============================================================================
    LowhighpassAudioProcessorEditor (LowhighpassAudioProcessor* ownerFilter);
    ~LowhighpassAudioProcessorEditor();

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    //[/UserMethods]

    void paint (Graphics& g);
    void resized();
    void buttonClicked (Button* buttonThatWasClicked);
    void sliderValueChanged (Slider* sliderThatWasMoved);

    // Binary resources:
    static const char* lc_off_png;
    static const int lc_off_pngSize;
    static const char* _2nd_png;
    static const int _2nd_pngSize;
    static const char* hc_off_png;
    static const int hc_off_pngSize;
    static const char* _4th_png;
    static const int _4th_pngSize;
    static const char* hc_over_png;
    static const int hc_over_pngSize;
    static const char* hc_on_png;
    static const int hc_on_pngSize;
    static const char* lc_over_png;
    static const int lc_over_pngSize;
    static const char* lc_on_png;
    static const int lc_on_pngSize;
    static const char* analyzer_off_png;
    static const int analyzer_off_pngSize;
    static const char* analyzer_over_png;
    static const int analyzer_over_pngSize;
    static const char* analyzer_on_png;
    static const int analyzer_on_pngSize;
    static const char* drag_off_png;
    static const int drag_off_pngSize;
    static const char* drag_on_png;
    static const int drag_on_pngSize;
    static const char* drag_over_png;
    static const int drag_over_pngSize;


private:
    //[UserVariables]   -- You can add your own custom variables in this section.
    //[/UserVariables]

    //==============================================================================
    ScopedPointer<Label> lbl_gd;
    ScopedPointer<ImageButton> btn_lc_on;
    ScopedPointer<Slider> sld_lc_f;
    ScopedPointer<ImageButton> btn_lc_order;
    ScopedPointer<ImageButton> btn_hc_on;
    ScopedPointer<Slider> sld_hc_f;
    ScopedPointer<ImageButton> btn_hc_order;
    ScopedPointer<Slider> sld_hs_f;
    ScopedPointer<Slider> sld_hs_g;
    ScopedPointer<Slider> sld_hs_q;
    ScopedPointer<Slider> sld_p1_f;
    ScopedPointer<Slider> sld_p1_g;
    ScopedPointer<Slider> sld_p1_q;
    ScopedPointer<Slider> sld_p2_f;
    ScopedPointer<Slider> sld_p2_g;
    ScopedPointer<Slider> sld_p2_q;
    ScopedPointer<Slider> sld_ls_f;
    ScopedPointer<Slider> sld_ls_g;
    ScopedPointer<Slider> sld_ls_q;
    ScopedPointer<Component> filtergraph;
    ScopedPointer<ImageButton> btn_analyzer;
    ScopedPointer<ImageButton> btn_drag_lc;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LowhighpassAudioProcessorEditor)
};

//[EndFile] You can add extra defines here...
//[/EndFile]

#endif   // __JUCE_HEADER_F47B898C4BCD30D0__
