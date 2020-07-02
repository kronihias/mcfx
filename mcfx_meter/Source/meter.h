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
    
    void offset(int offset);
    
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
    
    int _offset;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterComponent);
};


#endif   // __JUCER_HEADER_METERCOMPONENT_METER_8182604B__
