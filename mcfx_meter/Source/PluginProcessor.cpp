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
_fall(15.0f),
_pk_hold(false),
_offset(0.f)
{
    for (int i=0; i<NUM_CHANNELS; i++)
    {
        _my_meter_dsp.add(new MyMeterDsp());
        _my_meter_dsp.getUnchecked(i)->setAudioParams((int)getSampleRate(), getBlockSize());
        _my_meter_dsp.getUnchecked(i)->setParams(_hold, _fall);
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
    return totalNumParams;
}

float Ambix_meterAudioProcessor::getParameter (int index)
{
    switch (index) {
        case HoldParam:
            return _hold/5.f;
            break;
        
        case FallParam:
            return _fall/99.f;
            break;
        
        case PkHoldParam:
            return (float)_pk_hold;
            break;
            
        case OffsetParam:
            return (_offset+36.f)/54.f;
            break;
            
        default:
            
            break;
    }
    return 0.0f;
}

void Ambix_meterAudioProcessor::setParameter (int index, float newValue)
{
    switch (index) {
        case HoldParam:
            _hold = newValue*5.f;
            for (int i=0; i<NUM_CHANNELS; i++)
            {
                _my_meter_dsp.getUnchecked(i)->setParams(_hold, _fall);
            }
            break;
            
        case FallParam:
            _fall = newValue*99.f;
            for (int i=0; i<NUM_CHANNELS; i++)
            {
                _my_meter_dsp.getUnchecked(i)->setParams(_hold, _fall);
            }
            break;
            
        case PkHoldParam:
            if (newValue < 0.5)
                _pk_hold = false;
            else
                _pk_hold = true;
            break;
            
        case OffsetParam:
            _offset = newValue*54.f-36.f;
            break;
            
        default:
            
            break;
    }
    sendChangeMessage();
}

const String Ambix_meterAudioProcessor::getParameterName (int index)
{
    switch (index) {
        case HoldParam:
            return "Hold [s]";
            break;
            
        case FallParam:
            return "Fall [dB/s]";
            break;
            
        case PkHoldParam:
            return "Show Peak Hold";
            break;
            
        case OffsetParam:
            return "Scale Offset";
            break;
            
        default:
            
            break;
    }
    return String();
}

const String Ambix_meterAudioProcessor::getParameterText (int index)
{
    String text = "";
    
    switch (index) {
        case HoldParam:
            text << String(_hold).substring(0, 3);
            text << " s";
            break;
            
        case FallParam:
            text << String((int)_fall);
            text << " dB/s";
            break;
            
        case PkHoldParam:
            if (_pk_hold)
                text << "On";
            else
                text << "Off";
            break;
            
        case OffsetParam:
            text << String((int)_offset);
            break;
            
        default:
            
            break;
    }
    return text;
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
    return String();
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
        _my_meter_dsp.getUnchecked(i)->setParams(_hold, _fall);
    }
}

void Ambix_meterAudioProcessor::releaseResources()
{
    
}

void Ambix_meterAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    int NumSamples = buffer.getNumSamples();
    
    for (int channel = 0; channel < getTotalNumInputChannels(); channel++)
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
    
    XmlElement xml ("MYPLUGINSETTINGS");
    
    // add some attributes to it..
    for (int i=0; i < getNumParameters(); i++)
    {
        xml.setAttribute (String(i), getParameter(i));
    }
    
    // then use this helper function to stuff it into the binary blob and return it..
    copyXmlToBinary (xml, destData);
}

void Ambix_meterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    
    std::unique_ptr<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    
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
    return new Ambix_meterAudioProcessor();
}
