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
Mcfx_delayAudioProcessor::Mcfx_delayAudioProcessor() : _delay_ms(0.f),
    _delay_smpls(0),
    _delay_buffer(2,256),
    _buf_write_pos(0),
    _buf_read_pos(0),
    _buf_size(256)
{
}

Mcfx_delayAudioProcessor::~Mcfx_delayAudioProcessor()
{
}

//==============================================================================
const String Mcfx_delayAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int Mcfx_delayAudioProcessor::getNumParameters()
{
    return totalNumParams;
}

float Mcfx_delayAudioProcessor::getParameter (int index)
{
    return _delay_ms;
}

void Mcfx_delayAudioProcessor::setParameter (int index, float newValue)
{
    _delay_ms = newValue;
    _delay_smpls = (int)(_delay_ms*MAX_DELAYTIME_S*getSampleRate());
}

const String Mcfx_delayAudioProcessor::getParameterName (int index)
{
    return "Delay in ms";
}

const String Mcfx_delayAudioProcessor::getParameterText (int index)
{
    String text;
    text = String(_delay_ms*MAX_DELAYTIME_S*1000);
    text << " ms";
    return text;
}

const String Mcfx_delayAudioProcessor::getInputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

const String Mcfx_delayAudioProcessor::getOutputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

bool Mcfx_delayAudioProcessor::isInputChannelStereoPair (int index) const
{
    return true;
}

bool Mcfx_delayAudioProcessor::isOutputChannelStereoPair (int index) const
{
    return true;
}

bool Mcfx_delayAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool Mcfx_delayAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool Mcfx_delayAudioProcessor::silenceInProducesSilenceOut() const
{
    return false;
}

double Mcfx_delayAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int Mcfx_delayAudioProcessor::getNumPrograms()
{
    return 0;
}

int Mcfx_delayAudioProcessor::getCurrentProgram()
{
    return 0;
}

void Mcfx_delayAudioProcessor::setCurrentProgram (int index)
{
}

const String Mcfx_delayAudioProcessor::getProgramName (int index)
{
    return String::empty;
}

void Mcfx_delayAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void Mcfx_delayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    _buf_size = (int)(MAX_DELAYTIME_S * sampleRate + samplesPerBlock + 1); // MAX_DELAYTIME_S maximum
    _delay_buffer.clear();
    _delay_smpls = (int)(_delay_ms*MAX_DELAYTIME_S*sampleRate);
}

void Mcfx_delayAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void Mcfx_delayAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    
    // compute read position
    _buf_read_pos = _buf_write_pos - _delay_smpls;
    if (_buf_read_pos < 0)
        _buf_read_pos = _buf_size + _buf_read_pos - 1;
    
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
    if (_buf_read_pos + buffer.getNumSamples() < _buf_size)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ch++)
        {
            buffer.copyFrom(ch, 0, _delay_buffer, ch, _buf_read_pos, buffer.getNumSamples());
        }
        // update read position
        _buf_read_pos += buffer.getNumSamples();
    } else {
        
        int samples_to_read1 = _buf_size - _buf_read_pos;
        int samples_to_read2 = buffer.getNumSamples() - samples_to_read1;
        
        for (int ch = 0; ch < buffer.getNumChannels(); ch++)
        {
            
            // copy until end
            buffer.copyFrom(ch, 0, _delay_buffer, ch, _buf_read_pos, samples_to_read1);
            
            // start copy from front
            buffer.copyFrom(ch, samples_to_read1, _delay_buffer, ch, 0, samples_to_read2);
        }
        // update write position
        _buf_read_pos = samples_to_read2;
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
bool Mcfx_delayAudioProcessor::hasEditor() const
{
    return false; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* Mcfx_delayAudioProcessor::createEditor()
{
    //return new Mcfx_delayAudioProcessorEditor (this);
    return nullptr;
}

//==============================================================================
void Mcfx_delayAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void Mcfx_delayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Mcfx_delayAudioProcessor();
}
