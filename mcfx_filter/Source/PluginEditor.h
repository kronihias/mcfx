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

#ifndef __JUCE_HEADER_F47B898C4BCD30D0__
#define __JUCE_HEADER_F47B898C4BCD30D0__

//[Headers]     -- You can add your own extra header files here --
#include "JuceHeader.h"
#include "PluginProcessor.h"
#include "FilterGraph.h"
//[/Headers]



//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Jucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
class LowhighpassAudioProcessorEditor  : public AudioProcessorEditor,
                                         public Button::Listener,
                                         public Slider::Listener,
                                         public ChangeListener,
                                         public Timer
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
    
    void changeListenerCallback (ChangeBroadcaster *source);
    
    void timerCallback();
    
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


private:
    //[UserVariables]   -- You can add your own custom variables in this section.
    //[/UserVariables]
    
    void updateSliders();
    
    //==============================================================================
    Label lbl_gd;
    ImageButton btn_lc_on;
    Slider sld_lc_f;
    ImageButton btn_lc_order;
    ImageButton btn_hc_on;
    Slider sld_hc_f;
    ImageButton btn_hc_order;
    Slider sld_hs_f;
    Slider sld_hs_g;
    Slider sld_hs_q;
    Slider sld_p1_f;
    Slider sld_p1_g;
    Slider sld_p1_q;
    Slider sld_p2_f;
    Slider sld_p2_g;
    Slider sld_p2_q;
    Slider sld_ls_f;
    Slider sld_ls_g;
    Slider sld_ls_q;
    std::unique_ptr<FilterGraph> filtergraph;
    ImageButton btn_analyzer;

    LookAndFeel_V3 MyLookAndFeel;

    bool changed_;
    
    LowhighpassAudioProcessor* getProcessor() const
    {
        return static_cast <LowhighpassAudioProcessor*> (getAudioProcessor());
    }
    
    TooltipWindow tooltipWindow;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LowhighpassAudioProcessorEditor)
};

//[EndFile] You can add extra defines here...
//[/EndFile]

#endif   // __JUCE_HEADER_F47B898C4BCD30D0__
