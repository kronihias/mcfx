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
#include "CircularMeter.h"
#include "WaterfallComponent.h"
#include "DotMatrixComponent.h"


#define METER_WIDTH 16
#define GROUP_CHANNELS 4
// NB: no trailing semicolon — it used to have one, which only survived because
// both uses happened to be statement-final.
#define METER_GROUP_SPACE 10


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
                                        public ComboBox::Listener,
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

    void comboBoxChanged (ComboBox* comboBoxThatHasChanged);

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

    OwnedArray<MeterScaleComponent> _scales;

    // Alternate view: all channels around a ring. Constructed once and shown or
    // hidden, rather than rebuilt on every mode change.
    CircularMeter _circular;

    // Alternate view: per-channel spectra receding into depth.
    WaterfallComponent _waterfall;

    // Alternate view: one colour-mapped dot per channel — the compact one.
    DotMatrixComponent _dots;

    // Refill the waterfall's channel selector for the current channel count and
    // show the processor's remembered choice.
    void rebuildChannelSelector();

    // Clear peak/peak-hold on every channel. Shared by the bar view's
    // click-anywhere gesture and the ring's click-off-the-wedges one.
    void resetAllMeters();

    // Show only the components the current view uses.
    void applyModeVisibility();

    // Rebuild / (re)layout the per-channel meter strips, scales and the
    // editor's overall size to match the host's currently-negotiated
    // channel count. Called from the constructor and from
    // changeListenerCallback() when the host re-negotiates the layout.
    void rebuildChannelStrips();

    // Window size and resize limits for the current view. Split out of
    // rebuildChannelStrips() so a host re-negotiating the channel count cannot
    // stomp a size the user chose in one of the resizable views.
    void applyModeSizing();

    // The control strip is laid out at absolute pixel positions: the peak-hold
    // toggle ends at x=473 and the view selector runs 481..573, so anything
    // narrower than this hides the selector entirely — with no other way to get
    // back to the bar view. The bar view alone is narrower than this below ~32
    // channels, which is why the strip already clipped before the selector
    // existed.
    static constexpr int kMinEditorWidth = 581;

    // How many channels the band analyser transforms per timer tick. Bounds the
    // per-tick cost so it does not grow with the channel count.
    static constexpr int kWaterfallChannelsPerTick = 32;

    // Counts ticks so the waterfall repaints once per analyser sweep.
    int _wf_tick = 0;

    int _cachedNumCh = 0;

    int _width;
    int _height;

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
    ComboBox cb_view;
    ComboBox cb_wf_channel;
    Slider sld_offset;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Ambix_meterAudioProcessorEditor)
};

//[EndFile] You can add extra defines here...
//[/EndFile]

#endif   // __JUCER_HEADER_AMBIXmeterAUDIOPROCESSOREDITOR_PLUGINEDITOR_A2379E92__
