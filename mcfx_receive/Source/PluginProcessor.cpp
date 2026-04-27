/*
 ==============================================================================

 This file is part of the mcfx (Multichannel Effects) plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com

 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)

 Details of these licenses can be found at: www.gnu.org/licenses

 mcfx is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "mcfx_net_wire.h"
#include "mcfx_net_socket.h"

// IReaperHostApplication's IID is declared in mcfx_net_reaper.h via
// DECLARE_CLASS_IID; the VST3 SDK requires a matching DEF_CLASS_IID
// somewhere in a .cpp. This is the one TU that owns it for this binary.
namespace mcfx { namespace net { namespace reaper {
    DEF_CLASS_IID (IReaperHostApplication)
}}}

McfxReceiveAudioProcessor::McfxReceiveAudioProcessor()
    : juce::AudioProcessor (
       #if MCFX_MULTICHANNEL_BUILD
        MCFX_MULTICHANEL_BUSES
       #else
        BusesProperties()
            .withInput  ("Input",  juce::AudioChannelSet::discreteChannels (NUM_CHANNELS), true)
            .withOutput ("Output", juce::AudioChannelSet::discreteChannels (NUM_CHANNELS), true)
       #endif
      )
{
    // Stable per-instance UID — same value for the lifetime of this plugin
    // load. Lets a sender remember "I was pointed at receiver UID=X" and
    // follow it across reconnects / IP changes without manual intervention.
    stableUid = juce::Uuid().toString();

    discovery = std::make_unique<mcfx::net::Discovery> (
        mcfx::net::Discovery::Mode::Publish,
        mcfx::net::Discovery::kReceiverUID);

    senderBrowser = std::make_unique<mcfx::net::Discovery> (
        mcfx::net::Discovery::Mode::Browse,
        mcfx::net::Discovery::kSenderUID);
    senderBrowser->onChange = [this]() {
        if (onDiscoveredSendersChanged) onDiscoveredSendersChanged();
    };
}

McfxReceiveAudioProcessor::~McfxReceiveAudioProcessor() = default;

void McfxReceiveAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    int port;
    {
        const juce::ScopedLock sl (paramLock);
        port = desiredPort;
    }
    stream.bind (port);
    stream.prepareToPlay (sampleRate, samplesPerBlock,
                          getMainBusNumOutputChannels());
    refreshDiscoveryAdvertise();

    meters.setAudioParams (sampleRate, samplesPerBlock);
    meters.resize (getMainBusNumOutputChannels());
}

void McfxReceiveAudioProcessor::refreshDiscoveryAdvertise()
{
    if (! discovery) return;

    mcfx::net::Discovery::PublishInfo info;
    info.uid      = stableUid;
    info.host     = mcfx::net::getFriendlyComputerName();
    info.user     = juce::SystemStats::getLogonName();
    info.channels = getMainBusNumOutputChannels();
    info.format   = "PCM 32-bit float"; // receiver accepts any format the sender ships
    {
        const juce::ScopedLock sl (paramLock);
        info.track = trackName;
        trackInfoDirty = false;
    }
    currentProject = reaperIntegration.getProjectName();
    info.project = currentProject;
    discovery->setPublish (stream.getBoundPort(), info);
}

void McfxReceiveAudioProcessor::releaseResources()
{
    stream.releaseResources();
}

int McfxReceiveAudioProcessor::setListenPort (int port)
{
    {
        const juce::ScopedLock sl (paramLock);
        desiredPort = port;
    }
    const int bound = stream.bind (port);
    // Discovery advertises the actual bound port, not the requested one,
    // so a sender browsing the LAN sees the address that will actually
    // accept its packets.
    refreshDiscoveryAdvertise();
    return bound;
}

int McfxReceiveAudioProcessor::getListenPort() const
{
    const juce::ScopedLock sl (paramLock);
    return desiredPort;
}

bool McfxReceiveAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   #if MCFX_MULTICHANNEL_BUILD
    return mcfx::isMultichannelLayoutSupported (layouts, NUM_CHANNELS, wrapperType);
   #else
    return layouts.getMainInputChannels()  == NUM_CHANNELS
        && layouts.getMainOutputChannels() == NUM_CHANNELS;
   #endif
}

void McfxReceiveAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numOutChans = getMainBusNumOutputChannels();

    // RecvStream::pullAndMix sums into the output. Receiver semantics:
    // replace the bus with the incoming stream(s) — clear first.
    buffer.clear();
    if (numOutChans > 0 && numSamples > 0)
    {
        stream.pullAndMix (buffer.getArrayOfWritePointers(),
                           numOutChans, numSamples);
        meters.measureBlock (buffer);
    }
}

juce::AudioProcessorEditor* McfxReceiveAudioProcessor::createEditor()
{
    return new McfxReceiveAudioProcessorEditor (*this);
}

void McfxReceiveAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("mcfx_receive");
    {
        const juce::ScopedLock sl (paramLock);
        state.setProperty ("port", desiredPort, nullptr);
    }
    state.setProperty ("jitter_ms", getJitterFloorMs(), nullptr);
    state.setProperty ("password", getPassword(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void McfxReceiveAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto t = juce::ValueTree::fromXml (*xml);
        if (t.isValid() && t.hasType ("mcfx_receive"))
        {
            setListenPort (static_cast<int> (t.getProperty ("port", 0)));
            if (t.hasProperty ("jitter_ms"))
                setJitterFloorMs (static_cast<int> (t.getProperty ("jitter_ms", 25)));
            if (t.hasProperty ("password"))
                setPassword (t.getProperty ("password", "").toString());
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new McfxReceiveAudioProcessor();
}
