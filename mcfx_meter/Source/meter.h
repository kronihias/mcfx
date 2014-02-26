/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  16 Nov 2012 7:03:13pm

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.12

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_METERCOMPONENT_METER_8182604B__
#define __JUCER_HEADER_METERCOMPONENT_METER_8182604B__

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
class MeterComponent  : public Component
{
public:
    //==============================================================================
    MeterComponent ();
    ~MeterComponent();

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    //[/UserMethods]

    void paint (Graphics& g);
    void resized();
    void mouseUp (const MouseEvent& e);
    void setValue(float rms, float dpk, float dpk_hold);
    void reset();
    
    // Binary resources:
    static const char* meter_gradient_png;
    static const int meter_gradient_pngSize;
    static const char* meter_gradient_off_png;
    static const int meter_gradient_off_pngSize;
    
    
    
    bool _peak_hold;
private:
    //[UserVariables]   -- You can add your own custom variables in this section.
    //[/UserVariables]

    //==============================================================================
    
    Image cachedImage_meter_gradient_png;
    
    Image cachedImage_meter_gradient_off_png;
    
    float dpk_hold_db;
    float dpk_db;
    float rms_db;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterComponent);
};


#endif   // __JUCER_HEADER_METERCOMPONENT_METER_8182604B__
