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

#ifndef __PLUGINPROCESSOR_H_43F61266__
#define __PLUGINPROCESSOR_H_43F61266__

#include "../JuceLibraryCode/JuceHeader.h"

#include "MySignalGenerator.h"
#include "ChannelStepper.h"

// number of parameters per channel
#define PARAMS_PER_CH 6

//==============================================================================
/**
*/
class Mcfx_gain_delayAudioProcessor  : public AudioProcessor,
                                       public ChangeBroadcaster
{
public:
    //==============================================================================
    Mcfx_gain_delayAudioProcessor();
    ~Mcfx_gain_delayAudioProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock);
    void releaseResources();

    void processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages);

    //==============================================================================
    AudioProcessorEditor* createEditor();
    bool hasEditor() const;

    //==============================================================================
    const String getName() const;

    int getNumParameters();

    float getParameter (int index);
    void setParameter (int index, float newValue);

    void setSiggenForAllChannels(bool active); // activate/deactivate the signal generator for all channels
  
    const String getParameterName (int index);
    const String getParameterText (int index);

    const String getInputChannelName (int channelIndex) const;
    const String getOutputChannelName (int channelIndex) const;
    bool isInputChannelStereoPair (int index) const;
    bool isOutputChannelStereoPair (int index) const;

    bool acceptsMidi() const;
    bool producesMidi() const;
    bool silenceInProducesSilenceOut() const;
    double getTailLengthSeconds() const;

    //==============================================================================
    int getNumPrograms();
    int getCurrentProgram();
    void setCurrentProgram (int index);
    const String getProgramName (int index);
    void changeProgramName (int index, const String& newName);

    //==============================================================================
    void getStateInformation (MemoryBlock& destData);
    void setStateInformation (const void* data, int sizeInBytes);

private:
  
  void calcGainFact(int ch);
  
  Array<float> _delay_ms;
  Array<int> _delay_smpls;
  AudioSampleBuffer _delay_buffer;
  
  Array<float> _gain_param;
  Array<float> _gain_factor;
  Array<float> _gain_factor_; // for ramp
  
  Array<float> _phase;
  Array<bool> _mute;
  Array<bool> _solo;
  Array<bool> _siggen_flag;

  int _solocount;
  
  int _buf_write_pos;
  int _buf_size;
  Array<int> _buf_read_pos;
  
  MySignalGenerator _siggen;
  
  float _stepspeed; // hold time for each channel while iterating
  bool _stepper_on; // step through channels with testsignal on/off
  ChannelStepper _channelstepper; // helper object to iterate testsignal over channels
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Mcfx_gain_delayAudioProcessor)
};

#endif  // __PLUGINPROCESSOR_H_43F61266__
