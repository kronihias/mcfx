/*
  ==============================================================================

  This is an automatically generated GUI class created by the Introjucer!

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Created with Introjucer version: 3.1.1

  ------------------------------------------------------------------------------

  The Introjucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-13 by Raw Material Software Ltd.

  ==============================================================================
*/

#ifndef __JUCE_HEADER_8D34C71B7702159A__
#define __JUCE_HEADER_8D34C71B7702159A__

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
    ScopedPointer<Label> label;
    ScopedPointer<Slider> sld_hold;
    ScopedPointer<Slider> sld_fall;
    ScopedPointer<Label> label2;
    ScopedPointer<Label> label3;
    ScopedPointer<ToggleButton> tgl_pkhold;
    ScopedPointer<Slider> sld_offset;
    Image cachedImage_meter_scale_png;
    Image cachedImage_meter_scale_png;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Ambix_kmeterAudioProcessorEditor)
};

//[EndFile] You can add extra defines here...
//[/EndFile]

#endif   // __JUCE_HEADER_8D34C71B7702159A__
