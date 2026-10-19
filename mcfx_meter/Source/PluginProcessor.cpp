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
#include "mcfx_buses.h"


//==============================================================================
Ambix_meterAudioProcessor::Ambix_meterAudioProcessor() :
    AudioProcessor (
#if MCFX_MULTICHANNEL_BUILD
        MCFX_MULTICHANEL_BUSES
#else
        BusesProperties()
            .withInput  ("Input",  juce::AudioChannelSet::discreteChannels(NUM_CHANNELS), true)
            .withOutput ("Output", juce::AudioChannelSet::discreteChannels(NUM_CHANNELS), true)
#endif
    ),
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

bool Ambix_meterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if MCFX_MULTICHANNEL_BUILD
    return mcfx::isMultichannelLayoutSupported (layouts, NUM_CHANNELS, wrapperType);
#else
    return layouts.getMainInputChannelSet().size()  == NUM_CHANNELS
        && layouts.getMainOutputChannelSet().size() == NUM_CHANNELS;
#endif
}

void Ambix_meterAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    int NumSamples = buffer.getNumSamples();

    for (int channel = 0; channel < getTotalNumInputChannels(); channel++)
    {
        _my_meter_dsp.getUnchecked(channel)->calc(buffer.getReadPointer(channel), NumSamples);
    }

    // Copy only, and only while the waterfall is on screen — the transform runs
    // on the GUI thread. This is the first audio-thread copying this plug-in has
    // ever done, so it stays a memcpy behind a gate.
    _band_analyser.push (buffer, NumSamples);
    _scope_capture.push (buffer, NumSamples);
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

    // Named, not indexed: the loop above owns the numeric keys "0".."N", so a
    // name cannot collide with them now or when a real parameter is appended.
    xml.setAttribute ("view_mode", (int) _view_mode);
    xml.setAttribute ("circle_w", _size_circle_w);
    xml.setAttribute ("circle_h", _size_circle_h);
    xml.setAttribute ("wf_w",     _size_wf_w);
    xml.setAttribute ("wf_h",     _size_wf_h);
    xml.setAttribute ("wf_sel",   _selected_ch);
    xml.setAttribute ("dots_w",   _size_dots_w);
    xml.setAttribute ("dots_h",   _size_dots_h);
    xml.setAttribute ("scope_w",  _size_scope_w);
    xml.setAttribute ("scope_h",  _size_scope_h);
    xml.setAttribute ("scope_first",   _scope_settings.firstCh);
    xml.setAttribute ("scope_last",    _scope_settings.lastCh);
    xml.setAttribute ("scope_stacked", _scope_settings.stacked);
    xml.setAttribute ("scope_time",    _scope_settings.timeIdx);
    xml.setAttribute ("scope_amp",     _scope_settings.ampIdx);
    xml.setAttribute ("scope_trig",    _scope_settings.trigger);
    xml.setAttribute ("spec_w",        _size_spec_w);
    xml.setAttribute ("spec_h",        _size_spec_h);
    xml.setAttribute ("spec_first",    _spectro_settings.firstCh);
    xml.setAttribute ("spec_last",     _spectro_settings.lastCh);
    xml.setAttribute ("spec_stacked",  _spectro_settings.stacked);
    xml.setAttribute ("spec_span",     _spectro_settings.spanIdx);
    xml.setAttribute ("spec_top",      _spectro_settings.topIdx);
    xml.setAttribute ("spec_range",    _spectro_settings.rangeIdx);
    xml.setAttribute ("spec_zlo",      _spectro_settings.zoomLo);
    xml.setAttribute ("spec_zhi",      _spectro_settings.zoomHi);
    xml.setAttribute ("spec_fine",     _spectro_settings.fineBass);
    xml.setAttribute ("spec_cmap",     _spectro_settings.colourMap);
    xml.setAttribute ("spec_cscale",   _spectro_settings.colourScale);
    xml.setAttribute ("spec_norm",     _spectro_settings.normalise);
    xml.setAttribute ("spec_fscale",   _spectro_settings.freqScale);
    xml.setAttribute ("spec_notes",    _spectro_settings.noteLabels);
    xml.setAttribute ("spec_window",   _spectro_settings.windowIdx);
    xml.setAttribute ("spec_smooth",   _spectro_settings.smooth);
    xml.setAttribute ("spec_strip",    _spectro_settings.showStrip);
    xml.setAttribute ("spec_harm",     _spectro_settings.harmonics);

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

            // Absent in sessions saved before these existed, which is exactly
            // the default: the classic bar view at its usual size.
            _view_mode = (ViewMode) jlimit (0, 5, xmlState->getIntAttribute ("view_mode", 0));
            _size_circle_w = xmlState->getIntAttribute ("circle_w", _size_circle_w);
            _size_circle_h = xmlState->getIntAttribute ("circle_h", _size_circle_h);
            _size_wf_w     = xmlState->getIntAttribute ("wf_w",     _size_wf_w);
            _size_wf_h     = xmlState->getIntAttribute ("wf_h",     _size_wf_h);
            _selected_ch = xmlState->getIntAttribute ("wf_sel", -1);
            _size_dots_w   = xmlState->getIntAttribute ("dots_w",   _size_dots_w);
            _size_dots_h   = xmlState->getIntAttribute ("dots_h",   _size_dots_h);
            _size_scope_w  = xmlState->getIntAttribute ("scope_w",  _size_scope_w);
            _size_scope_h  = xmlState->getIntAttribute ("scope_h",  _size_scope_h);

            // Range-checked by the view against the live channel count.
            _scope_settings.firstCh = xmlState->getIntAttribute ("scope_first", _scope_settings.firstCh);
            _scope_settings.lastCh  = xmlState->getIntAttribute ("scope_last",  _scope_settings.lastCh);
            _scope_settings.stacked = xmlState->getBoolAttribute ("scope_stacked", _scope_settings.stacked);
            _scope_settings.timeIdx = xmlState->getIntAttribute ("scope_time",  _scope_settings.timeIdx);
            _scope_settings.ampIdx  = xmlState->getIntAttribute ("scope_amp",   _scope_settings.ampIdx);
            _scope_settings.trigger = xmlState->getBoolAttribute ("scope_trig", _scope_settings.trigger);

            _size_spec_w = xmlState->getIntAttribute ("spec_w", _size_spec_w);
            _size_spec_h = xmlState->getIntAttribute ("spec_h", _size_spec_h);
            _spectro_settings.firstCh  = xmlState->getIntAttribute ("spec_first", _spectro_settings.firstCh);
            _spectro_settings.lastCh   = xmlState->getIntAttribute ("spec_last",  _spectro_settings.lastCh);
            _spectro_settings.stacked  = xmlState->getBoolAttribute ("spec_stacked", _spectro_settings.stacked);
            _spectro_settings.spanIdx  = xmlState->getIntAttribute ("spec_span",  _spectro_settings.spanIdx);
            _spectro_settings.topIdx   = xmlState->getIntAttribute ("spec_top",   _spectro_settings.topIdx);
            _spectro_settings.rangeIdx = xmlState->getIntAttribute ("spec_range", _spectro_settings.rangeIdx);
            _spectro_settings.zoomLo   = (float) xmlState->getDoubleAttribute ("spec_zlo", _spectro_settings.zoomLo);
            _spectro_settings.zoomHi   = (float) xmlState->getDoubleAttribute ("spec_zhi", _spectro_settings.zoomHi);
            _spectro_settings.fineBass = xmlState->getBoolAttribute ("spec_fine", _spectro_settings.fineBass);
            {
                auto& s = _spectro_settings;     // range-checked by the view
                s.colourMap   = xmlState->getIntAttribute  ("spec_cmap",   s.colourMap);
                s.colourScale = xmlState->getIntAttribute  ("spec_cscale", s.colourScale);
                s.normalise   = xmlState->getIntAttribute  ("spec_norm",   s.normalise);
                s.freqScale   = xmlState->getIntAttribute  ("spec_fscale", s.freqScale);
                s.noteLabels  = xmlState->getBoolAttribute ("spec_notes",  s.noteLabels);
                s.windowIdx   = xmlState->getIntAttribute  ("spec_window", s.windowIdx);
                s.smooth      = xmlState->getBoolAttribute ("spec_smooth", s.smooth);
                s.showStrip   = xmlState->getBoolAttribute ("spec_strip",  s.showStrip);
                s.harmonics   = xmlState->getBoolAttribute ("spec_harm",   s.harmonics);
            }

            _state_generation.fetch_add (1, std::memory_order_release);
            sendChangeMessage();
        }

    }
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Ambix_meterAudioProcessor();
}
