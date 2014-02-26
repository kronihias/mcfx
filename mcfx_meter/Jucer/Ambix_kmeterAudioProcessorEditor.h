/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  18 Apr 2013 12:51:24pm

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.12

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_AMBIXKMETERAUDIOPROCESSOREDITOR_AMBIXKMETERAUDIOPROCESSOREDITOR_A849FF45__
#define __JUCER_HEADER_AMBIXKMETERAUDIOPROCESSOREDITOR_AMBIXKMETERAUDIOPROCESSOREDITOR_A849FF45__

//[Headers]     -- You can add your own extra header files here --
#include "JuceHeader.h"
//[/Headers]



//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Jucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
class Ambix_kmeterAudioProcessorEditor  : public AudioProcessorEditor,
                                          public SliderListener,
                                          public ButtonListener
{
public:
    //==============================================================================
    Ambix_kmeterAudioProcessorEditor (Ambix_kmeterAudioProcessor* ownerFilter);
    ~Ambix_kmeterAudioProcessorEditor();

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    //[/UserMethods]

    void paint (Graphics& g);
    void resized();
    void sliderValueChanged (Slider* sliderThatWasMoved);
    void buttonClicked (Button* buttonThatWasClicked);

    // Binary resources:
    static const char* meter_scale_png;
    static const int meter_scale_pngSize;


private:
    //[UserVariables]   -- You can add your own custom variables in this section.
    //[/UserVariables]

    //==============================================================================
    Label* label;
    Slider* sld_hold;
    Slider* sld_fall;
    Label* label2;
    Label* label3;
    ToggleButton* tgl_pkhold;
    Image cachedImage_meter_scale_png;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Ambix_kmeterAudioProcessorEditor)
};

//[EndFile] You can add extra defines here...
//[/EndFile]

#endif   // __JUCER_HEADER_AMBIXKMETERAUDIOPROCESSOREDITOR_AMBIXKMETERAUDIOPROCESSOREDITOR_A849FF45__
