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
Mcfx_delayAudioProcessor::Mcfx_delayAudioProcessor() :
    AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::discreteChannels(NUM_CHANNELS), true)
        .withOutput ("Output", juce::AudioChannelSet::discreteChannels(NUM_CHANNELS), true)
    ),
    _delay_ms(0.f),
    _delay_smpls(0),
    _delay_buffer(2,256),
    _buf_write_pos(0),
    _buf_read_pos(0),
    _buf_size(256)
{
    _samplerate = getSampleRate();

    if (_samplerate == 0.f) {
        _samplerate = 44100.f;
    }

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
    _delay_smpls = (int)floor(newValue*MAX_DELAYTIME_S*_samplerate+0.5f);

    _delay_ms = _delay_smpls/_samplerate/MAX_DELAYTIME_S;

    sendChangeMessage();
}

const String Mcfx_delayAudioProcessor::getParameterName (int index)
{
    return "Delay in ms";
}

const String Mcfx_delayAudioProcessor::getParameterText (int index)
{
    String text;
    // round toward 0.01 ms
    text = String(floor(_delay_ms*MAX_DELAYTIME_S*100000.f+0.5f)/100.f);
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
    return String();
}

void Mcfx_delayAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void Mcfx_delayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    _samplerate = sampleRate;

    _buf_size = (int)(MAX_DELAYTIME_S * sampleRate + samplesPerBlock + 1); // MAX_DELAYTIME_S maximum
    _delay_buffer.clear();
    _delay_smpls = (int)(_delay_ms*MAX_DELAYTIME_S*sampleRate);
}

int Mcfx_delayAudioProcessor::getDelayInSmpls()
{
    return _delay_smpls;
}

float Mcfx_delayAudioProcessor::getDelayInMs()
{
    return _delay_ms*MAX_DELAYTIME_S*1000;
}

void Mcfx_delayAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool Mcfx_delayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return ((layouts.getMainOutputChannelSet().size() == NUM_CHANNELS) &&
            (layouts.getMainInputChannelSet().size() == NUM_CHANNELS));
}

void Mcfx_delayAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{

    // compute read position
    _buf_read_pos = _buf_write_pos - _delay_smpls;
    if (_buf_read_pos < 0)
        _buf_read_pos = _buf_size + _buf_read_pos;

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
    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
    {
        buffer.clear (i, 0, buffer.getNumSamples());
    }
}

//==============================================================================
bool Mcfx_delayAudioProcessor::hasEditor() const
{
    return true;
}

AudioProcessorEditor* Mcfx_delayAudioProcessor::createEditor()
{
    return new Mcfx_delayAudioProcessorEditor (this);
    //return nullptr;
}

//==============================================================================
void Mcfx_delayAudioProcessor::getStateInformation (MemoryBlock& destData)
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

void Mcfx_delayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new Mcfx_delayAudioProcessor();
}
