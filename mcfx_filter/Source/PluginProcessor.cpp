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

#include "PluginProcessor.h"
#include "PluginEditor.h"


//==============================================================================
LowhighpassAudioProcessor::LowhighpassAudioProcessor() :
_lc_on_param(0.f),
_lc_freq_param(0.1f),
_lc_order_param(1.f),
_hc_on_param(0.f),
_hc_freq_param(0.9f),
_hc_order_param(1.f),
_pf1_gain_param(0.5f),
_pf1_freq_param(0.3f),
_pf1_q_param(0.27f), // Q=0.7
_pf2_gain_param(0.5f),
_pf2_freq_param(0.7f),
_pf2_q_param(0.27f), // Q=0.7
_ls_gain_param(0.5f),
_ls_freq_param(0.2f),
_ls_q_param(0.27f), // Q=0.7
_hs_gain_param(0.5f),
_hs_freq_param(0.7f),
_hs_q_param(0.27f) // Q=0.7
{
    
}

LowhighpassAudioProcessor::~LowhighpassAudioProcessor()
{
    
}

//==============================================================================
const String LowhighpassAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int LowhighpassAudioProcessor::getNumParameters()
{
    return totalNumParams;
}

float LowhighpassAudioProcessor::getParameter (int index)
{
    switch (index)
	{
		case LCOnParam:    return _lc_on_param;
		case LCfreqParam:  return _lc_freq_param;
		case LCorderParam: return _lc_order_param;
            
		case HCOnParam:      return _hc_on_param;
		case HCfreqParam:    return _hc_freq_param;
		case HCorderParam:   return _hc_order_param;
            
    case PF1GainParam:  return _pf1_gain_param;
    case PF1freqParam:  return _pf1_freq_param;
    case PF1QParam:     return _pf1_q_param;
            
    case PF2GainParam:  return _pf2_gain_param;
    case PF2freqParam:  return _pf2_freq_param;
    case PF2QParam:     return _pf2_q_param;
      
    case LSGainParam:  return _ls_gain_param;
    case LSfreqParam:  return _ls_freq_param;
    case LSQParam:     return _ls_q_param;
      
    case HSGainParam:  return _hs_gain_param;
    case HSfreqParam:  return _hs_freq_param;
    case HSQParam:     return _hs_q_param;
      
		default:            return 0.0f;
	}
}

void LowhighpassAudioProcessor::setParameter (int index, float newValue)
{
    switch (index)
	{
		case LCOnParam:
            _lc_on_param = newValue;
            break;
		case LCfreqParam:
            _lc_freq_param = newValue;
            break;
		case LCorderParam:
            _lc_order_param = newValue;
            break;
            
		case HCOnParam:
            _hc_on_param = newValue;
            break;
		case HCfreqParam:
            _hc_freq_param = newValue;
            break;
		case HCorderParam:
            _hc_order_param = newValue;
            break;
            
        case PF1GainParam:
            _pf1_gain_param = newValue;
            break;
        case PF1freqParam:
            _pf1_freq_param = newValue;
            break;
        case PF1QParam:
            _pf1_q_param = newValue;
            break;
            
        case PF2GainParam:
            _pf2_gain_param = newValue;
            break;
        case PF2freqParam:
            _pf2_freq_param = newValue;
            break;
        case PF2QParam:
            _pf2_q_param = newValue;
            break;
      
    case LSGainParam:
      _ls_gain_param = newValue;
      break;
    case LSfreqParam:
      _ls_freq_param = newValue;
      break;
    case LSQParam:
      _ls_q_param = newValue;
      break;
      
    case HSGainParam:
      _hs_gain_param = newValue;
      break;
    case HSfreqParam:
      _hs_freq_param = newValue;
      break;
    case HSQParam:
      _hs_q_param = newValue;
      break;
      
      
		default:
            break;
	}
}

const String LowhighpassAudioProcessor::getParameterName (int index)
{
    switch (index)
	{
		case LCOnParam:    return "LowCut On";
		case LCfreqParam:    return "LowCut Freq";
		case LCorderParam:    return "LowCut Order";
            
		case HCOnParam:    return "HighCut On";
		case HCfreqParam:    return "HighCut Freq";
		case HCorderParam:    return "HighCut Order";
            
    case PF1GainParam:    return "Peak 1 Gain";
    case PF1freqParam:    return "Peak 1 Freq";
    case PF1QParam:    return "Peak 1 Q";
            
    case PF2GainParam:    return "Peak 2 Gain";
    case PF2freqParam:    return "Peak 2 Freq";
    case PF2QParam:    return "Peak 2 Q";
      
    case LSGainParam:    return "LowShelf Gain";
    case LSfreqParam:    return "LowShelf Freq";
    case LSQParam:    return "LowShelf Q";
      
    case HSGainParam:    return "HighShelf Gain";
    case HSfreqParam:    return "HighShelf Freq";
    case HSQParam:    return "HighShelf Q";
      
		default:            return "";
	}
}

// convert param 0...1 to freq 24Hz ... 21618Hz
float param2freq(float param)
{
    return powf(2.f, param*9.8f + 4.6f);
}

// convert parameter 0...1 to Q 0.2 .... 20
float param2q(float param)
{
    return powf(2.f, param*6.6439f)*0.2f;
}

float param2db(float param)
{
    return param*18*2-18;
}

#define LOGTEN 2.302585092994
float dbtorms(float db)
{
    return expf(((float)LOGTEN * 0.05f) * db);
}

float param2gain (float param)
{
    return dbtorms(param2db(param));
}

const String LowhighpassAudioProcessor::getParameterText (int index)
{
    String text;
    
    switch (index)
	{
        case LCOnParam:
            if (_lc_on_param <= 0.5f)
                text = "Off";
            else
                text = "On";
            break;
            
        case LCfreqParam:
            text = String(param2freq(_lc_freq_param)).substring(0, 6);
            text << " Hz";
            break;
            
        case LCorderParam:
            if (_lc_order_param <= 0.5f)
                text = "2nd";
            else
                text = "4th";
            break;
            
            
        case HCOnParam:
            if (_hc_on_param <= 0.5f)
                text = "Off";
            else
                text = "On";
            break;
            
        case HCfreqParam:
            text = String(param2freq(_hc_freq_param)).substring(0, 6);
            text << " Hz";
            break;
            
        case HCorderParam:
            if (_hc_order_param <= 0.5f)
                text = "2nd";
            else
                text = "4th";
            break;
            
        case PF1GainParam:
            text = String(param2db(_pf1_gain_param)).substring(0, 4);
            text << " dB";
            break;
            
        case PF1freqParam:
            text = String(param2freq(_pf1_freq_param)).substring(0, 6);
            text << " Hz";
            break;
            
        case PF1QParam:
            text = String(param2q(_pf1_q_param)).substring(0, 5);
            break;
            
            
            
        case PF2GainParam:
            text = String(param2db(_pf2_gain_param)).substring(0, 4);
            text << " dB";
            break;
            
        case PF2freqParam:
            text = String(param2freq(_pf2_freq_param)).substring(0, 6);
            text << " Hz";
            break;
            
        case PF2QParam:
            text = String(param2q(_pf2_q_param)).substring(0, 5);
            break;
      
      
    case LSGainParam:
      text = String(param2db(_ls_gain_param)).substring(0, 4);
      text << " dB";
      break;
      
    case LSfreqParam:
      text = String(param2freq(_ls_freq_param)).substring(0, 6);
      text << " Hz";
      break;
      
    case LSQParam:
      text = String(param2q(_ls_q_param)).substring(0, 5);
      break;
      
    
    case HSGainParam:
      text = String(param2db(_hs_gain_param)).substring(0, 4);
      text << " dB";
      break;
      
    case HSfreqParam:
      text = String(param2freq(_hs_freq_param)).substring(0, 6);
      text << " Hz";
      break;
      
    case HSQParam:
      text = String(param2q(_hs_q_param)).substring(0, 5);
      break;
      
      
		default:
            break;
	}
  
	return text;
}

const String LowhighpassAudioProcessor::getInputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

const String LowhighpassAudioProcessor::getOutputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

bool LowhighpassAudioProcessor::isInputChannelStereoPair (int index) const
{
    return true;
}

bool LowhighpassAudioProcessor::isOutputChannelStereoPair (int index) const
{
    return true;
}

bool LowhighpassAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool LowhighpassAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool LowhighpassAudioProcessor::silenceInProducesSilenceOut() const
{
    return false;
}

double LowhighpassAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int LowhighpassAudioProcessor::getNumPrograms()
{
    return 0;
}

int LowhighpassAudioProcessor::getCurrentProgram()
{
    return 0;
}

void LowhighpassAudioProcessor::setCurrentProgram (int index)
{
}

const String LowhighpassAudioProcessor::getProgramName (int index)
{
    return String::empty;
}

void LowhighpassAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void LowhighpassAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // std::cout << "Processing channels: " << getNumInputChannels() << std::endl;
    checkFilters();
}

void LowhighpassAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void LowhighpassAudioProcessor::checkFilters()
{
    
    ///////////////////////
    // low cut parameters and filters
    if (_lc_freq_param != _lc_freq_param_)
    {
        _IIR_LC_Coeff = _IIR_LC_Coeff.makeHighPass(getSampleRate(), param2freq(_lc_freq_param));
        
        for (int i = 0; i < _LC_IIR_1.size(); i++) {
            _LC_IIR_1.getUnchecked(i)->setCoefficients(_IIR_LC_Coeff);
            _LC_IIR_2.getUnchecked(i)->setCoefficients(_IIR_LC_Coeff);
        }
        _lc_freq_param_ = _lc_freq_param; // set old value
    }
    // add filters if necessary
    if (_LC_IIR_1.size() < getNumInputChannels()) {
        while (getNumInputChannels() - _LC_IIR_1.size() > 0) {
            _LC_IIR_1.add(new IIRFilter());
            _LC_IIR_1.getLast()->setCoefficients(_IIR_LC_Coeff);
            _LC_IIR_2.add(new IIRFilter());
            _LC_IIR_2.getLast()->setCoefficients(_IIR_LC_Coeff);
        }
    }
    
    ///////////////////////
    // high cut parameters and filters
    if (_hc_freq_param != _hc_freq_param_)
    {
        _IIR_HC_Coeff = _IIR_HC_Coeff.makeLowPass(getSampleRate(), param2freq(_hc_freq_param));
        
        for (int i = 0; i < _HC_IIR_1.size(); i++) {
            _HC_IIR_1.getUnchecked(i)->setCoefficients(_IIR_HC_Coeff);
            _HC_IIR_2.getUnchecked(i)->setCoefficients(_IIR_HC_Coeff);
        }
        _hc_freq_param_ = _hc_freq_param; // set old value
    }
    // add filters if necessary
    if (_HC_IIR_1.size() < getNumInputChannels()) {
        while (getNumInputChannels() - _HC_IIR_1.size() > 0) {
            _HC_IIR_1.add(new IIRFilter());
            _HC_IIR_1.getLast()->setCoefficients(_IIR_HC_Coeff);
            _HC_IIR_2.add(new IIRFilter());
            _HC_IIR_2.getLast()->setCoefficients(_IIR_HC_Coeff);
        }
    }
    
    ///////////////////
    // Peak Filter 1
    if (_pf1_freq_param != _pf1_freq_param_ || _pf1_gain_param != _pf1_gain_param_ || _pf1_q_param != _pf1_q_param_)
    {
        // get new coefficients
        _IIR_PF_Coeff_1 = _IIR_PF_Coeff_1.makePeakFilter(getSampleRate(), param2freq(_pf1_freq_param), param2q(_pf1_q_param), param2gain(_pf1_gain_param));
       // update coefficients for filters
        for (int i = 0; i < _PF_IIR_1.size(); i++) {
            _PF_IIR_1.getUnchecked(i)->setCoefficients(_IIR_PF_Coeff_1);
        }
        
        // save "old" values
        _pf1_freq_param_ = _pf1_freq_param;
        _pf1_gain_param_ = _pf1_gain_param;
        _pf1_q_param_ = _pf1_q_param;
        
    }
    // add filters if necessary
    if (_PF_IIR_1.size() < getNumInputChannels()) {
        while (getNumInputChannels() - _PF_IIR_1.size() > 0) {
            _PF_IIR_1.add(new IIRFilter());
            _PF_IIR_1.getLast()->setCoefficients(_IIR_PF_Coeff_1);
            _PF_IIR_1.add(new IIRFilter());
            _PF_IIR_1.getLast()->setCoefficients(_IIR_PF_Coeff_1);
        }
    }
    
    ///////////////////
    // Peak Filter 2
    if (_pf2_freq_param != _pf2_freq_param_ || _pf2_gain_param != _pf2_gain_param_ || _pf2_q_param != _pf2_q_param_)
    {
        // get new coefficients
        _IIR_PF_Coeff_2 = _IIR_PF_Coeff_2.makePeakFilter(getSampleRate(), param2freq(_pf2_freq_param), param2q(_pf2_q_param), param2gain(_pf2_gain_param));
        // update coefficients for filters
        for (int i = 0; i < _PF_IIR_2.size(); i++) {
            _PF_IIR_2.getUnchecked(i)->setCoefficients(_IIR_PF_Coeff_2);
        }
        
        // save "old" values
        _pf2_freq_param_ = _pf2_freq_param;
        _pf2_gain_param_ = _pf2_gain_param;
        _pf2_q_param_ = _pf2_q_param;
        
    }
    // add filters if necessary
    if (_PF_IIR_2.size() < getNumInputChannels()) {
        while (getNumInputChannels() - _PF_IIR_2.size() > 0) {
            _PF_IIR_2.add(new IIRFilter());
            _PF_IIR_2.getLast()->setCoefficients(_IIR_PF_Coeff_2);
            _PF_IIR_2.add(new IIRFilter());
            _PF_IIR_2.getLast()->setCoefficients(_IIR_PF_Coeff_2);
        }
    }
  
    ///////////////////
    // Low Shelf Filter
    if (_ls_freq_param != _ls_freq_param_ || _ls_gain_param != _ls_gain_param_ || _ls_q_param != _ls_q_param_)
    {
        // get new coefficients
        _IIR_LS_Coeff = _IIR_LS_Coeff.makeLowShelf(getSampleRate(), param2freq(_ls_freq_param), param2q(_ls_q_param), param2gain(_ls_gain_param));
        // update coefficients for filters
        for (int i = 0; i < _LS_IIR.size(); i++) {
            _LS_IIR.getUnchecked(i)->setCoefficients(_IIR_LS_Coeff);
        }
        
        // save "old" values
        _ls_freq_param_ = _ls_freq_param;
        _ls_gain_param_ = _ls_gain_param;
        _ls_q_param_ = _ls_q_param;
        
    }
    // add filters if necessary
    if (_LS_IIR.size() < getNumInputChannels()) {
        while (getNumInputChannels() - _LS_IIR.size() > 0) {
            _LS_IIR.add(new IIRFilter());
            _LS_IIR.getLast()->setCoefficients(_IIR_LS_Coeff);
            _LS_IIR.add(new IIRFilter());
            _LS_IIR.getLast()->setCoefficients(_IIR_LS_Coeff);
        }
    }
    
    ///////////////////
    // High Shelf Filter
    if (_hs_freq_param != _hs_freq_param_ || _hs_gain_param != _hs_gain_param_ || _hs_q_param != _hs_q_param_)
    {
        // get new coefficients
        _IIR_HS_Coeff = _IIR_HS_Coeff.makeHighShelf(getSampleRate(), param2freq(_hs_freq_param), param2q(_hs_q_param), param2gain(_hs_gain_param));
        // update coefficients for filters
        for (int i = 0; i < _HS_IIR.size(); i++) {
            _HS_IIR.getUnchecked(i)->setCoefficients(_IIR_HS_Coeff);
        }
        
        // save "old" values
        _hs_freq_param_ = _hs_freq_param;
        _hs_gain_param_ = _hs_gain_param;
        _hs_q_param_ = _hs_q_param;
        
    }
    // add filters if necessary
    if (_HS_IIR.size() < getNumInputChannels()) {
        while (getNumInputChannels() - _HS_IIR.size() > 0) {
            _HS_IIR.add(new IIRFilter());
            _HS_IIR.getLast()->setCoefficients(_IIR_HS_Coeff);
            _HS_IIR.add(new IIRFilter());
            _HS_IIR.getLast()->setCoefficients(_IIR_HS_Coeff);
        }
    }
    
}

void LowhighpassAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    checkFilters();
    
    // std::cout << "Processing channels: " << getNumInputChannels() << std::endl;
    // std::cout << "Buffer channels: " << buffer.getNumChannels() << std::endl;
    
    int numSamples = buffer.getNumSamples();
    
    for (int channel = 0; channel < getNumInputChannels(); channel++)
    {
        // LOW CUT
        if (_lc_on_param > 0.5f) {
            
            _LC_IIR_1.getUnchecked(channel)->processSamples(buffer.getWritePointer(channel), numSamples);
            
            // second stage -> 4th order butterworth
            if (_lc_order_param > 0.5f)
                _LC_IIR_2.getUnchecked(channel)->processSamples(buffer.getWritePointer(channel), numSamples);
        }
        
        
        // HIGH CUT
        if (_hc_on_param > 0.5f) {
            
            _HC_IIR_1.getUnchecked(channel)->processSamples(buffer.getWritePointer(channel), numSamples);
            
            // second stage -> 4th order butterworth
            if (_hc_order_param > 0.5f)
                _HC_IIR_2.getUnchecked(channel)->processSamples(buffer.getWritePointer(channel), numSamples);
        }
        
        // PF1
        if (_pf1_gain_param != 0.5f) {
            
            _PF_IIR_1.getUnchecked(channel)->processSamples(buffer.getWritePointer(channel), numSamples);
            
        }
        
        // PF2
        if (_pf2_gain_param != 0.5f) {
            
            _PF_IIR_2.getUnchecked(channel)->processSamples(buffer.getWritePointer(channel), numSamples);
            
        }
        
        // LS
        if (_ls_gain_param != 0.5f) {
            
            _LS_IIR.getUnchecked(channel)->processSamples(buffer.getWritePointer(channel), numSamples);
            
        }
        
        // HS
        if (_hs_gain_param != 0.5f) {
            
            _HS_IIR.getUnchecked(channel)->processSamples(buffer.getWritePointer(channel), numSamples);
            
        }
        
    }
    // In case we have more outputs than inputs, we'll clear any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    for (int i = getNumInputChannels(); i < getNumOutputChannels(); ++i)
    {
        buffer.clear (i, 0, buffer.getNumSamples());
    }
}

//==============================================================================
bool LowhighpassAudioProcessor::hasEditor() const
{
    return false; // (change this to false if you choose to not supply an editor)
}


AudioProcessorEditor* LowhighpassAudioProcessor::createEditor()
{
    //return new LowhighpassAudioProcessorEditor (this);
    return nullptr;
}

//==============================================================================
void LowhighpassAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    // Create an outer XML element..
    
    XmlElement xml ("MYPLUGINSETTINGS");
    
    // add some attributes to it..
    for (int i=0; i < getNumParameters(); i++)
    {
        xml.setAttribute (String(i), getParameter(i));
    }
    
    // then use this helper function to stuff it into the binary blob and return it..
    copyXmlToBinary (xml, destData);
}

void LowhighpassAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    
    ScopedPointer<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    
    if (xmlState != nullptr)
    {
        // make sure that it's actually our type of XML object..
        if (xmlState->hasTagName ("MYPLUGINSETTINGS"))
        {
            for (int i=0; i < getNumParameters(); i++) {
                setParameter(i, xmlState->getDoubleAttribute(String(i)));
            }
        }
        
    }
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LowhighpassAudioProcessor();
}
