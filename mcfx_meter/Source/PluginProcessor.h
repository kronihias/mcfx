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

#ifndef __PLUGINPROCESSOR_H_421B8C00__
#define __PLUGINPROCESSOR_H_421B8C00__

#include "JuceHeader.h"
#include "mcfx_buses.h"
#include "MyMeterDsp.h"
#include "MultiBandAnalyser.h"

//==============================================================================
/**
*/
class Ambix_meterAudioProcessor  : public AudioProcessor,
                                   public ChangeBroadcaster
{
public:
    //==============================================================================
    Ambix_meterAudioProcessor();
    ~Ambix_meterAudioProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

   #if MCFX_MULTICHANNEL_BUILD
    MCFX_MULTICHANEL_APPLY_BUS_LAYOUTS_OVERRIDE
   #endif

    void processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override;

    // Notify the editor (if any) when the host re-negotiates the bus layout
    // so it can rebuild its per-channel meter strips without the user having
    // to close and reopen the GUI.
    void numChannelsChanged() override { sendChangeMessage(); }

    //==============================================================================
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const String getName() const override;

    int getNumParameters() override;

    float getParameter (int index) override;
    void setParameter (int index, float newValue) override;

    const String getParameterName (int index) override;
    const String getParameterText (int index) override;

    const String getInputChannelName (int channelIndex) const override;
    const String getOutputChannelName (int channelIndex) const override;
    bool isInputChannelStereoPair (int index) const override;
    bool isOutputChannelStereoPair (int index) const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool silenceInProducesSilenceOut() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    OwnedArray<MyMeterDsp> _my_meter_dsp;

    /** Which visualisation the editor shows. Deliberately NOT a plug-in
        parameter: it is a view setting, so it has no business being automatable,
        and adding a fifth parameter would change getNumParameters() — which is
        the bound of the state loop below and of the host's parameter list. */
    enum class ViewMode { Bars = 0, Circle = 1, Waterfall = 2 };
    ViewMode _view_mode = ViewMode::Bars;

    // Editor size remembered per view, so switching back and forth does not
    // forget how the user sized each one. Bars derives its size from the
    // channel count and ignores these.
    int _size_circle_w = 640, _size_circle_h = 700;
    int _size_wf_w     = 900, _size_wf_h     = 560;

    /** Per-channel band levels for the waterfall view. Idle — and unallocated —
        until the editor switches that view on. */
    MultiBandAnalyser _band_analyser;

    float _hold; // peak hold time, seconds
    float _fall; // peak fallback rate, dB/s

    bool _pk_hold;
    float _offset;

    // JUCE 8 removed AudioProcessor::setParameterNotifyingHost.
    void setParameterNotifyingHost(int parameterIndex, float newValue)
    {
        setParameter(parameterIndex, newValue);
    }

    enum Parameters
	{
		HoldParam,
        FallParam,
        PkHoldParam,
        OffsetParam,
		totalNumParams
	};

private:

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Ambix_meterAudioProcessor)
};

#endif  // __PLUGINPROCESSOR_H_421B8C00__
