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
Ambix_meterAudioProcessor::Ambix_meterAudioProcessor() :
_hold(0.5f),
_fall(15.0f)
{
    for (int i=0; i<NUM_CHANNELS; i++)
    {
        _my_meter_dsp.add(new MyMeterDsp());
        
    }
}

Ambix_meterAudioProcessor::~Ambix_meterAudioProcessor()
{
}

//==============================================================================
const String Ambix_meterAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int Ambix_meterAudioProcessor::getNumParameters()
{
    return 0;
}

float Ambix_meterAudioProcessor::getParameter (int index)
{
    return 0.0f;
}

void Ambix_meterAudioProcessor::setParameter (int index, float newValue)
{
}

const String Ambix_meterAudioProcessor::getParameterName (int index)
{
    return String::empty;
}

const String Ambix_meterAudioProcessor::getParameterText (int index)
{
    return String::empty;
}

const String Ambix_meterAudioProcessor::getInputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

const String Ambix_meterAudioProcessor::getOutputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

bool Ambix_meterAudioProcessor::isInputChannelStereoPair (int index) const
{
    return true;
}

bool Ambix_meterAudioProcessor::isOutputChannelStereoPair (int index) const
{
    return true;
}

bool Ambix_meterAudioProcessor::acceptsMidi() const
{
   return false;
}

bool Ambix_meterAudioProcessor::producesMidi() const
{
    return false;
}

bool Ambix_meterAudioProcessor::silenceInProducesSilenceOut() const
{
    return false;
}

double Ambix_meterAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int Ambix_meterAudioProcessor::getNumPrograms()
{
    return 0;
}

int Ambix_meterAudioProcessor::getCurrentProgram()
{
    return 0;
}

void Ambix_meterAudioProcessor::setCurrentProgram (int index)
{
}

const String Ambix_meterAudioProcessor::getProgramName (int index)
{
    return String::empty;
}

void Ambix_meterAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void Ambix_meterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (int i=0; i<NUM_CHANNELS; i++)
    {
        _my_meter_dsp.getUnchecked(i)->setAudioParams((int)sampleRate, samplesPerBlock);
    }
}

void Ambix_meterAudioProcessor::releaseResources()
{
    
}

void Ambix_meterAudioProcessor::setHoldParameter(float hold)
{
    _hold = hold;
    for (int i=0; i<NUM_CHANNELS; i++)
    {
        _my_meter_dsp.getUnchecked(i)->setParams(_hold, _fall);
    }
}

void Ambix_meterAudioProcessor::setFallParameter(float fall)
{
    _fall = fall;
    for (int i=0; i<NUM_CHANNELS; i++)
    {
        _my_meter_dsp.getUnchecked(i)->setParams(_hold, _fall);
    }
}

void Ambix_meterAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    int NumSamples = buffer.getNumSamples();
    
    for (int channel = 0; channel < getNumInputChannels(); channel++)
    {
        _my_meter_dsp.getUnchecked(channel)->calc(buffer.getReadPointer(channel), NumSamples);
    }

}

//==============================================================================
bool Ambix_meterAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* Ambix_meterAudioProcessor::createEditor()
{
    return new Ambix_meterAudioProcessorEditor (this);
}

//==============================================================================
void Ambix_meterAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void Ambix_meterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Ambix_meterAudioProcessor();
}
