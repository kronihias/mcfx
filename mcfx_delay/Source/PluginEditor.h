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

#ifndef __JUCE_HEADER_8C7FEFDEFBB3D5CF__
#define __JUCE_HEADER_8C7FEFDEFBB3D5CF__

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
class Mcfx_delayAudioProcessorEditor  : public AudioProcessorEditor,
                                        public Slider::Listener,
                                        public ChangeListener
{
public:
    //==============================================================================
    Mcfx_delayAudioProcessorEditor (Mcfx_delayAudioProcessor* ownerFilter);
    ~Mcfx_delayAudioProcessorEditor();

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    //[/UserMethods]

    void paint (Graphics& g);
    void resized();
    void sliderValueChanged (Slider* sliderThatWasMoved);

    void changeListenerCallback (ChangeBroadcaster *source);

private:
    //[UserVariables]   -- You can add your own custom variables in this section.
    //[/UserVariables]

    //==============================================================================
    Label lbl_g;
    Slider sld_del_ms;
    Label label3;
    Label label2;
    Slider sld_del_smpl;
    Label label4;

    TooltipWindow tooltipWindow;

    LookAndFeel_V3 MyLookAndFeel;

    Mcfx_delayAudioProcessor* getProcessor() const
    {
        return static_cast <Mcfx_delayAudioProcessor*> (getAudioProcessor());
    }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Mcfx_delayAudioProcessorEditor)
};

//[EndFile] You can add extra defines here...
//[/EndFile]

#endif   // __JUCE_HEADER_8C7FEFDEFBB3D5CF__
