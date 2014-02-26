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

#ifndef PLUGINPROCESSOR_H_INCLUDED
#define PLUGINPROCESSOR_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"


//==============================================================================
/**
*/
class LowhighpassAudioProcessor  : public AudioProcessor
{
public:
    //==============================================================================
    LowhighpassAudioProcessor();
    ~LowhighpassAudioProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock);
    void releaseResources();

    void processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages);

    void checkFilters();
    
    //==============================================================================
    AudioProcessorEditor* createEditor();
    bool hasEditor() const;

    //==============================================================================
    const String getName() const;

    int getNumParameters();

    float getParameter (int index);
    void setParameter (int index, float newValue);

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

  enum Parameters
	{
    LCOnParam,
    LCfreqParam,
    LCorderParam,
    
    HCOnParam,
    HCfreqParam,
    HCorderParam,
    
    PF1GainParam,
    PF1freqParam,
    PF1QParam,
    
    PF2GainParam,
    PF2freqParam,
    PF2QParam,
    
    LSGainParam,
    LSfreqParam,
    LSQParam,
    
    HSGainParam,
    HSfreqParam,
    HSQParam,
    
		totalNumParams
	};
  
private:
  
    float _lc_on_param, // 1 = on
    _lc_freq_param, // 0...20kHz log
    _lc_order_param,
    _lc_freq_param_; // 1... 2x2nd order butterworth cascaded, 0... 1x2nd order butterworth
    float _hc_on_param, _hc_freq_param, _hc_order_param,_hc_freq_param_;
    
    float _pf1_gain_param, _pf1_freq_param, _pf1_q_param;
    float _pf2_gain_param, _pf2_freq_param, _pf2_q_param;
  
    float _ls_gain_param, _ls_freq_param, _ls_q_param;
    float _hs_gain_param, _hs_freq_param, _hs_q_param;
  
    float _pf1_gain_param_, _pf1_freq_param_, _pf1_q_param_; // buffer
    float _pf2_gain_param_, _pf2_freq_param_, _pf2_q_param_;
  
  
    float _ls_gain_param_, _ls_freq_param_, _ls_q_param_;
    float _hs_gain_param_, _hs_freq_param_, _hs_q_param_;
  
    OwnedArray<IIRFilter> _LC_IIR_1;
    OwnedArray<IIRFilter> _LC_IIR_2;
    OwnedArray<IIRFilter> _HC_IIR_1;
    OwnedArray<IIRFilter> _HC_IIR_2;
    
    
    IIRCoefficients _IIR_LC_Coeff;
    IIRCoefficients _IIR_HC_Coeff;
    
    // Peak/Notch Filters
    IIRCoefficients _IIR_PF_Coeff_1;
    OwnedArray<IIRFilter> _PF_IIR_1;
    
    
    IIRCoefficients _IIR_PF_Coeff_2;
    OwnedArray<IIRFilter> _PF_IIR_2;
  
  
    // High/Low Shelf
  
    IIRCoefficients _IIR_LS_Coeff;
    OwnedArray<IIRFilter> _LS_IIR;
  
  
    IIRCoefficients _IIR_HS_Coeff;
    OwnedArray<IIRFilter> _HS_IIR;
  
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LowhighpassAudioProcessor)
};

#endif  // PLUGINPROCESSOR_H_INCLUDED
