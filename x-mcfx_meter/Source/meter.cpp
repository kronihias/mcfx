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

//[Headers] You can add your own extra header files here...
//[/Headers]

#include "meter.h"
#include <cmath>

#define LOGTEN 2.302585092994

inline float rmstodb(float rms)
{
    // return 20.f/(float)LOGTEN * log2f(rms);
    return 20.f * log10f(rms);
}

//[MiscUserDefs] You can add your own user definitions and misc code here...

float iec_scale(float dB)
{
    
        float fScale = 1.0f;
        
        if (dB < -70.0f)
            fScale = 0.0f;
        else if (dB < -60.0f)
            fScale = (dB + 70.0f) * 0.0025f;
        else if (dB < -50.0f)
            fScale = (dB + 60.0f) * 0.005f + 0.025f;
        else if (dB < -40.0)
            fScale = (dB + 50.0f) * 0.0075f + 0.075f;
        else if (dB < -30.0f)
            fScale = (dB + 40.0f) * 0.015f + 0.15f;
        else if (dB < -20.0f)
            fScale = (dB + 30.0f) * 0.02f + 0.3f;
        else if (dB < -0.001f || dB > 0.001f)  /* if (dB < 0.0f) */
            fScale = (dB + 20.0f) * 0.025f + 0.5f;
        
        return fScale;
    
    
}
//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
MeterComponent::MeterComponent ()
: _peak_hold(false),
    dpk_hold_db(-199.f),
    dpk_db(-199.f),
    rms_db(-199.f),
    _offset(0)
    
{
    cachedImage_meter_gradient_png = ImageCache::getFromMemory (meter_gradient_png, meter_gradient_pngSize);
    
    cachedImage_meter_gradient_off_png = ImageCache::getFromMemory (meter_gradient_off_png, meter_gradient_off_pngSize);
    //[UserPreSize]
    //[/UserPreSize]

    setSize (8, 163);
    dpk_db = 0.0f;
    rms_db = 0.0f;

    reset();
    //[Constructor] You can add your own custom stuff here..
    //[/Constructor]
}

MeterComponent::~MeterComponent()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    //[/Destructor_pre]



    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void MeterComponent::paint (Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    g.fillAll (Colours::white);

    g.setTiledImageFill (cachedImage_meter_gradient_off_png,
                         0, 0,
                         1.0000f);
    g.fillRect (0, 0, 8, 163);

    g.setTiledImageFill (cachedImage_meter_gradient_png,
                         0, 0,
                         1.0000f);
    
    // 163 max height
	int size_rms = (int)floorf(iec_scale(rms_db - _offset)*163.f);

    int y_rms = 163-size_rms;
    
    g.fillRect (0, y_rms, 8, size_rms);
    
    // peak value
	int y_dpk = (int)floorf(163 - iec_scale(dpk_db - _offset)*163.f);
    
    if (dpk_db > 0.f)
        g.setColour (Colours::red);
    else
        g.setColour (Colours::white);
    
    g.fillRect (0, y_dpk, 8, 2);
    
    if (_peak_hold)
    {
        // hold peak value
		int y_dpk_hold = (int)floorf(163 - iec_scale(dpk_hold_db - _offset)*163.f);
        
        if (y_dpk_hold < 0)
        {
            y_dpk_hold = 0;
        }
        
        if (dpk_hold_db > 0.f)
        {
            g.setColour (Colours::red);
        } else {
            g.setColour (Colours::yellow);
        }
        
        
        g.fillRect (0, y_dpk_hold, 8, 2);
    }
    
    // g.fillRect (0, 0, 8, 163);
    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void MeterComponent::resized()
{
    //[UserResized] Add your own custom resize handling here..
    //[/UserResized]
}

void MeterComponent::mouseUp (const MouseEvent& e)
{
    reset();
}

void MeterComponent::reset ()
{
    rms_db=-199.f;
    dpk_db=-199.f;
    dpk_hold_db=-199.f;
    repaint();
}

void MeterComponent::offset(int offset)
{
    _offset = offset;
}

void MeterComponent::setValue(float rms, float dpk, float dpk_hold)
{
    rms_db = rmstodb(rms);
    dpk_db = rmstodb(dpk);
    
    dpk_hold_db = rmstodb(dpk_hold);
    
    // rms_db = iec_scale(rmstodb(rms) - _offset);
    // dpk_db = iec_scale(rmstodb(dpk) - _offset);
    
    // dpk_hold_db = iec_scale(rmstodb(dpk_hold) - _offset);
    
    if (rms || dpk)
        repaint();
    
}


//==============================================================================
#if 0
/*  -- Jucer information section --

    This is where the Jucer puts all of its metadata, so don't change anything in here!

BEGIN_JUCER_METADATA

<JUCER_COMPONENT documentType="Component" className="MeterComponent" componentName=""
                 parentClasses="public Component" constructorParams="" variableInitialisers=""
                 snapPixels="8" snapActive="1" snapShown="1" overlayOpacity="0.330000013"
                 fixedSize="0" initialWidth="8" initialHeight="163">
  <METHODS>
    <METHOD name="mouseUp (const MouseEvent&amp; e)"/>
  </METHODS>
  <BACKGROUND backgroundColour="ffffffff">
    <RECT pos="0 0 8 163" fill="image: meter0_png, 1, 0 0" hasStroke="0"/>
    <RECT pos="0 0 8 163" fill="image: meter1_png, 1, 0 0" hasStroke="0"/>
  </BACKGROUND>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif

//==============================================================================
// Binary resources - be careful not to edit any of these sections!

// JUCER_RESOURCE: meter_gradient_png, 309, "meter_gradient.png"
static const unsigned char resource_MeterComponent_meter_gradient_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,8,0,0,0,163,8,6,0,0,0,183,212,95,166,0,0,0,252,73,68,65,84,88,195,237,87,
    209,13,131,32,16,61,26,22,120,179,184,147,43,116,40,87,232,10,93,193,17,250,70,184,254,168,32,34,210,212,40,181,144,24,32,92,240,238,221,227,113,24,213,187,74,162,221,100,163,85,131,92,3,122,19,122,253,
    248,89,161,91,112,70,152,230,214,159,148,26,230,1,78,90,110,250,192,143,126,129,156,40,80,89,125,154,1,60,252,195,241,108,7,68,18,134,29,124,48,170,154,212,73,251,236,31,243,163,141,97,140,53,78,114,222,
    95,42,155,144,101,143,49,23,177,197,74,251,34,197,156,130,144,200,75,165,13,136,92,145,140,34,201,200,125,194,212,14,252,193,48,183,117,178,235,250,189,145,172,148,171,64,93,4,73,172,22,156,137,186,26,
    163,65,170,10,58,14,168,73,216,134,1,32,66,191,200,97,160,112,62,186,155,2,98,218,246,165,149,81,133,60,105,25,191,249,233,72,235,210,27,51,170,201,218,235,98,53,77,147,121,46,152,19,38,207,121,218,255,
    75,178,0,253,78,196,140,136,22,47,131,54,237,98,25,81,188,1,92,94,99,76,218,106,35,254,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* MeterComponent::meter_gradient_png = (const char*) resource_MeterComponent_meter_gradient_png;
const int MeterComponent::meter_gradient_pngSize = 309;

// JUCER_RESOURCE: meter_gradient_off_png, 250, "meter_gradient_off.png"
static const unsigned char resource_MeterComponent_meter_gradient_off_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,8,0,0,0,163,8,6,0,0,0,183,212,95,166,0,0,0,193,73,68,65,84,88,195,237,
    87,65,14,131,32,16,28,12,137,156,252,0,63,240,228,209,27,255,255,209,226,133,30,84,160,73,161,38,85,67,219,217,203,110,96,136,113,118,119,96,149,115,54,160,98,29,222,24,1,103,1,244,235,101,147,3,204,135,
    159,240,123,228,1,244,155,199,22,3,208,113,1,217,102,22,127,11,147,61,43,138,128,243,0,42,132,80,213,73,53,205,211,10,144,172,111,37,245,175,142,27,187,201,179,39,213,4,52,40,164,230,31,126,115,89,152,
    238,155,116,114,28,231,230,31,156,218,123,102,147,128,84,15,34,228,161,21,1,177,246,114,1,137,215,65,193,107,0,168,129,152,205,27,164,120,29,95,11,179,94,154,107,187,242,233,159,234,139,97,112,199,250,
    66,142,168,156,176,38,175,76,22,224,2,121,104,130,135,7,196,152,69,160,249,241,138,3,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* MeterComponent::meter_gradient_off_png = (const char*) resource_MeterComponent_meter_gradient_off_png;
const int MeterComponent::meter_gradient_off_pngSize = 250;
