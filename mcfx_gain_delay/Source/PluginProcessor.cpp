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

float param2db(float param)
{
  return param*18*2-18;
}

#define LOGTEN 2.302585092994
float dbtorms(float db)
{
  return exp(((float)LOGTEN * 0.05f) * db);
}

float param2gain (float param)
{
  return dbtorms(param2db(param));
}

//==============================================================================
Mcfx_gain_delayAudioProcessor::Mcfx_gain_delayAudioProcessor() :
  _delay_buffer(2,256),
  _buf_write_pos(0),
  _buf_size(256)
{
  _delay_ms.resize(NUM_CHANNELS);
  _delay_smpls.resize(NUM_CHANNELS);
  _buf_read_pos.resize(NUM_CHANNELS);
  _gain_param.resize(NUM_CHANNELS);
  _gain_factor.resize(NUM_CHANNELS);
  _gain_factor_.resize(NUM_CHANNELS);
  _phase.resize(NUM_CHANNELS);
  _mute.resize(NUM_CHANNELS);
  _solo.resize(NUM_CHANNELS);
  
  _solocount = 0;
  
  for (int i = 0; i < NUM_CHANNELS; i++) {
    _delay_ms.set(i, 0.f);
    _delay_smpls.set(i, 0);
    
    _buf_read_pos.set(i, 0);
    
    _gain_param.set(i, 0.5f);
    _gain_factor.set(i, 1.f);
    _gain_factor_.set(i, 1.f);
      
    _phase.set(i, 1.f); // 1... not inverted
    _mute.set(i, false); // false.. not muted
    _solo.set(i, false);
  }
  
}

Mcfx_gain_delayAudioProcessor::~Mcfx_gain_delayAudioProcessor()
{
}

//==============================================================================
const String Mcfx_gain_delayAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int Mcfx_gain_delayAudioProcessor::getNumParameters()
{
    return NUM_CHANNELS*5; // gain, delay, phase, mute, solo for each channel
}

float Mcfx_gain_delayAudioProcessor::getParameter (int index)
{
  int ch = index/5;
  int param = index%5;
  
  switch (param) {
    case 0: // gain
      return _gain_param.getUnchecked(ch);
      break;
      
    case 1: // delay
      return _delay_ms.getUnchecked(ch);
      break;
          
    case 2: // phase
      if (_phase.getUnchecked(ch) < 0.f)
        return 1.f; // invert phase!
      else
        return 0.f;
      break;
      
    case 3: // mute
      if (_mute.getUnchecked(ch))
        return 1.0f;
      else
        return 0.0f;
      break;
      
    case 4: // solo
      if (_solo.getUnchecked(ch))
        return 1.0f;
      else
        return 0.0f;
      break;
      
    default:
      return 0.f;
  }
}

void Mcfx_gain_delayAudioProcessor::setParameter (int index, float newValue)
{
  int ch = index/5;
  int param = index%5;
  
  bool alt_all_ch = false;
  
  switch (param) {
    case 0: // gain
      _gain_param.set(ch, newValue);
      break;
      
    case 1: // delay
      _delay_ms.set(ch, newValue);
      _delay_smpls.set(ch, (int)(newValue*MAX_DELAYTIME_S*getSampleRate()));
      break;
      
    case 2: // phase
      if (newValue > 0.5f)
        _phase.set(ch, -1.f);
      else
        _phase.set(ch, 1.f);
      break;
      
    case 3: // mute
      if (newValue > 0.5f)
        _mute.set(ch, true);
      else
        _mute.set(ch, false);
      break;
      
    case 4: // solo
      if (newValue > 0.5f)
      {
        if (!_solo.getUnchecked(ch))
        {
          if (++_solocount == 1) {
            alt_all_ch = true;
          }
        }
        
        _solo.set(ch, true);
      }
      else
      {
        if (_solo.getUnchecked(ch))
        {
          if (--_solocount == 0) {
            alt_all_ch = true;
          }
        }
        
        _solo.set(ch, false);
      }
      break;
      
    default:
      break;
  }
  
  //////////////
  // compute new gain value for the channel(s)
  
  if (alt_all_ch)
  {
    for (int i=0; i < NUM_CHANNELS; i++) {
      calcGainFact(i);
    }
  }
  else
  { // change only the actual channel...
    
    calcGainFact(ch);
  }

  sendChangeMessage();

}


void Mcfx_gain_delayAudioProcessor::calcGainFact(int ch)
{
  // solo mode is active
  if (_solocount > 0)
  {
    if (_solo.getUnchecked(ch)) {
      // channel is soloed, apply normal gain and phase
      _gain_factor.set(ch, param2gain(_gain_param.getUnchecked(ch))*_phase.getUnchecked(ch) );
    }
    else
    {
      // this channel is not soloed -> mute it
      _gain_factor.set(ch, 0.f);
    }
  }
  // no channel is soloed
  else
  {
    if (_mute.getUnchecked(ch)) {
      // channel is muted
      _gain_factor.set(ch, 0.f);
    }
    else
    {
      // channel is not muted and no channel is soloed... process normally
      _gain_factor.set(ch, param2gain(_gain_param.getUnchecked(ch))*_phase.getUnchecked(ch) );
    }
  }
}


const String Mcfx_gain_delayAudioProcessor::getParameterName (int index)
{
  String name = "";
  
  int ch = index/5;
  int param = index%5;
  
  name << "Ch " << ch+1 << " "; // start with channel 1
  
  switch (param) {
    case 0: // gain
      name << "gain";
      break;
      
    case 1: // delay
      name << "delaytime";
      break;
      
    case 2: // phase
      name << "phase";
      break;
      
    case 3: // mute
      name << "mute";
      break;
      
    case 4: // solo
      name << "solo";
      break;
      
    default:
      break;
  }
  return name;
}

const String Mcfx_gain_delayAudioProcessor::getParameterText (int index)
{
  String text = "";
  
  int ch = index/5;
  int param = index%5;
  
  switch (param) {
    case 0: // gain
      text << String(param2db(_gain_param.getUnchecked(ch)));
      text << " dB";
      break;
      
    case 1: // delay
      text << String(_delay_ms.getUnchecked(ch)*MAX_DELAYTIME_S*1000).substring(0, 5);
      text << " ms";
      break;
      
    case 2: // phase
      if (_phase.getUnchecked(ch) > 0.f)
        text << "";
      else
        text << "invert";
      break;
      
    case 3: // mute
      if (_mute.getUnchecked(ch))
        text << "Muted";
      else
        text << "Not muted";
      break;
      
    case 4: // solo
      if (_solo.getUnchecked(ch))
        text << "Soloed";
      else
        text << "Not soloed";
      break;
      
    default:
      break;
  }
  
  return text;
}

const String Mcfx_gain_delayAudioProcessor::getInputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

const String Mcfx_gain_delayAudioProcessor::getOutputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

bool Mcfx_gain_delayAudioProcessor::isInputChannelStereoPair (int index) const
{
    return true;
}

bool Mcfx_gain_delayAudioProcessor::isOutputChannelStereoPair (int index) const
{
    return true;
}

bool Mcfx_gain_delayAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool Mcfx_gain_delayAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool Mcfx_gain_delayAudioProcessor::silenceInProducesSilenceOut() const
{
    return false;
}

double Mcfx_gain_delayAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int Mcfx_gain_delayAudioProcessor::getNumPrograms()
{
    return 0;
}

int Mcfx_gain_delayAudioProcessor::getCurrentProgram()
{
    return 0;
}

void Mcfx_gain_delayAudioProcessor::setCurrentProgram (int index)
{
}

const String Mcfx_gain_delayAudioProcessor::getProgramName (int index)
{
    return String::empty;
}

void Mcfx_gain_delayAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void Mcfx_gain_delayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
  _buf_size = (int)(MAX_DELAYTIME_S * sampleRate + samplesPerBlock + 1); // MAX_DELAYTIME_S maximum
  _delay_buffer.clear();
  
  for (int i = 0; i < NUM_CHANNELS; i++) {
    _delay_smpls.set(i, (int)(_delay_ms.getUnchecked(i)*MAX_DELAYTIME_S*sampleRate));
  }
  
}

void Mcfx_gain_delayAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void Mcfx_gain_delayAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
  
  int numSamples = buffer.getNumSamples();
  
  
  // compute read position
  for (int i=0; i < NUM_CHANNELS; i++) {
    _buf_read_pos.set(i, _buf_write_pos - _delay_smpls.getUnchecked(i));
    if (_buf_read_pos.getUnchecked(i) < 0)
      _buf_read_pos.set(i, _buf_size + _buf_read_pos.getUnchecked(i) - 1);
  }
  
  
  // std::cout << "size : " << _buf_size << " read pos: " << _buf_read_pos << std::endl;
  // resize buffer if necessary
  if (_delay_buffer.getNumChannels() < buffer.getNumChannels() || _delay_buffer.getNumSamples() < _buf_size) {
    // resize buffer
    _delay_buffer.setSize(buffer.getNumChannels(), _buf_size, true, true, false);
  }
  
  // write to the buffer
  
  if (_buf_write_pos + buffer.getNumSamples() < _buf_size)
  {
    for (int ch = 0; ch < buffer.getNumChannels(); ch++)
    {
      // copy straight into buffer
      _delay_buffer.copyFrom(ch, _buf_write_pos, buffer, ch, 0, buffer.getNumSamples());
    }
    // update write position
    _buf_write_pos += buffer.getNumSamples();
    
  } else { // if buffer reaches end
    
    int samples_to_write1 = _buf_size - _buf_write_pos;
    int samples_to_write2 = buffer.getNumSamples() - samples_to_write1;
    
    // std::cout << "spl_write1: " << samples_to_write1 << " spl_write2: " << samples_to_write2 << std::endl;
    
    for (int ch = 0; ch < buffer.getNumChannels(); ch++)
    {
      
      // copy until end
      _delay_buffer.copyFrom(ch, _buf_write_pos, buffer, ch, 0, samples_to_write1);
      
      
      // start copy to front
      _delay_buffer.copyFrom(ch, 0, buffer, ch, samples_to_write1, samples_to_write2);
    }
    // update write position
    _buf_write_pos = samples_to_write2;
  }
  
  
  // read from buffer
  for (int i = 0; i<NUM_CHANNELS; i++) {
    
    if (_buf_read_pos.getUnchecked(i) + buffer.getNumSamples() < _buf_size)
    {
      
      buffer.copyFrom(i, 0, _delay_buffer, i, _buf_read_pos.getUnchecked(i), buffer.getNumSamples());
      // update read position
      _buf_read_pos.set(i, _buf_read_pos.getUnchecked(i) + buffer.getNumSamples());
      
    } else {
      
      int samples_to_read1 = _buf_size - _buf_read_pos.getUnchecked(i);
      int samples_to_read2 = buffer.getNumSamples() - samples_to_read1;
      
      // copy until end
      buffer.copyFrom(i, 0, _delay_buffer, i, _buf_read_pos.getUnchecked(i), samples_to_read1);
        
      // start copy from front
      buffer.copyFrom(i, samples_to_read1, _delay_buffer, i, 0, samples_to_read2);
      
      // update write position
      _buf_read_pos.set(i, samples_to_read2);
    }
    
    // apply gain!
    
    if (_gain_factor_.getUnchecked(i) != _gain_factor.getUnchecked(i))
    {
      buffer.applyGainRamp(i, 0, numSamples, _gain_factor_.getUnchecked(i), _gain_factor.getUnchecked(i));
    }
    else if (_gain_factor.getUnchecked(i) != 1.f)
    {
      buffer.applyGain(i, 0, numSamples, _gain_factor.getUnchecked(i));
    }
    
  }
  
  _gain_factor_ = _gain_factor; // store old values
  
  
  
  // In case we have more outputs than inputs, we'll clear any output
  // channels that didn't contain input data, (because these aren't
  // guaranteed to be empty - they may contain garbage).
  for (int i = getNumInputChannels(); i < getNumOutputChannels(); ++i)
  {
    buffer.clear (i, 0, buffer.getNumSamples());
  }
}

//==============================================================================
bool Mcfx_gain_delayAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* Mcfx_gain_delayAudioProcessor::createEditor()
{
    return new Mcfx_gain_delayAudioProcessorEditor (this);
    //return nullptr;
}

//==============================================================================
void Mcfx_gain_delayAudioProcessor::getStateInformation (MemoryBlock& destData)
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

void Mcfx_gain_delayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    
    ScopedPointer<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    
    if (xmlState != nullptr)
    {
        // make sure that it's actually our type of XML object..
        if (xmlState->hasTagName ("MYPLUGINSETTINGS"))
        {
            int numattr = xmlState->getNumAttributes();
            // old version compatibility with only 2 parameters per channel (gain, delay)
            if (numattr < 3*NUM_CHANNELS)
            {
                
                for (int i=0; i < numattr; i++) {
                    int ch = i/2;
                    int par = i%2;
                    setParameter(5*ch+par, xmlState->getDoubleAttribute(String(i)));
                }
                
            } else if (numattr < 5*NUM_CHANNELS) { // saved with 3 param version (gain, delay, phase)
                for (int i=0; i < numattr; i++) {
                    int ch = i/3;
                    int par = i%3;
                    setParameter(5*ch+par, xmlState->getDoubleAttribute(String(i)));
                }
              
            } else { // saved with 5 param version (gain, delay, phase, mute, solo)
                for (int i=0; i < jmin(getNumParameters(),numattr); i++) {
                    setParameter(i, xmlState->getDoubleAttribute(String(i)));
                }
              
            }
        }
        
    }
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Mcfx_gain_delayAudioProcessor();
}
