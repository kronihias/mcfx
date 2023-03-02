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

#ifndef __JUCER_HEADER_AMBIXmeterAUDIOPROCESSOREDITOR_PLUGINEDITOR_A2379E92__
#define __JUCER_HEADER_AMBIXmeterAUDIOPROCESSOREDITOR_PLUGINEDITOR_A2379E92__


#include "JuceHeader.h"
#include "PluginProcessor.h"
#include "meter.h"
#include "MeterScale.h"


#define METER_WIDTH 16
#define GROUP_CHANNELS 4
#define METER_GROUP_SPACE 10;


//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Jucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
class Ambix_meterAudioProcessorEditor  : public AudioProcessorEditor,
                                        public Slider::Listener,
                                        public Button::Listener,
                                        public Timer,
                                        public ChangeListener
{
public:
    //==============================================================================
    Ambix_meterAudioProcessorEditor (Ambix_meterAudioProcessor* ownerFilter);
    ~Ambix_meterAudioProcessorEditor();

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    //[/UserMethods]

    void paint (Graphics& g);
    void resized();

    void sliderValueChanged (Slider* sliderThatWasMoved);
    void buttonClicked (Button* buttonThatWasClicked);

    void mouseDown (const MouseEvent& e);

    void timerCallback();

    void changeListenerCallback (ChangeBroadcaster *source);

    // Binary resources:
    static const char* meter_scale_png;
    static const int meter_scale_pngSize;


private:
    //[UserVariables]   -- You can add your own custom variables in this section.
    OwnedArray<MeterComponent> _meters;
    OwnedArray<Label> _labels;

    MeterScaleComponent _scale_left;
    MeterScaleComponent _scale_right;

    int _width;

    TooltipWindow tooltipWindow;

    LookAndFeel_V3 MyLookAndFeel;

    Ambix_meterAudioProcessor* getProcessor() const
	{
		return static_cast <Ambix_meterAudioProcessor*> (getAudioProcessor());
	}
    //[/UserVariables]

    //==============================================================================
    Label label;
    Slider sld_hold;
    Slider sld_fall;
    Label label2;
    Label label3;
    ToggleButton tgl_pkhold;
    Slider sld_offset;
    Image cachedImage_meter_scale_png;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Ambix_meterAudioProcessorEditor)
};

//[EndFile] You can add extra defines here...
//[/EndFile]

#endif   // __JUCER_HEADER_AMBIXmeterAUDIOPROCESSOREDITOR_PLUGINEDITOR_A2379E92__
