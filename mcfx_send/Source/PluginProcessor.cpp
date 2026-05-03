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
#include "mcfx_net_socket.h"  // mcfx::net::getFriendlyComputerName

// IReaperHostApplication's IID is declared in mcfx_net_reaper.h via
// DECLARE_CLASS_IID; the VST3 SDK requires a matching DEF_CLASS_IID
// somewhere in a .cpp for linkage. This is the one TU that owns it.
namespace mcfx { namespace net { namespace reaper {
    DEF_CLASS_IID (IReaperHostApplication)
}}}

McfxSendAudioProcessor::McfxSendAudioProcessor()
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
    discovery = std::make_unique<mcfx::net::Discovery> (
        mcfx::net::Discovery::Mode::Browse,
        mcfx::net::Discovery::kReceiverUID);
    // Re-fire on every list change so the editor doesn't have to poll.
    discovery->onChange = [this]() {
        if (onDiscoveredReceiversChanged) onDiscoveredReceiversChanged();
    };

    // Stable per-load UID so a receiver browsing the LAN can recognise
    // a sender that re-bound its ephemeral port.
    stableUid = juce::Uuid().toString();

    senderAdvertiser = std::make_unique<mcfx::net::Discovery> (
        mcfx::net::Discovery::Mode::Publish,
        mcfx::net::Discovery::kSenderUID);
}

void McfxSendAudioProcessor::refreshSenderAdvertise()
{
    if (! senderAdvertiser) return;
    mcfx::net::Discovery::PublishInfo info;
    info.uid      = stableUid;
    // Wire UID = the uint32 written into outbound CommonHeader.sender.
    // Republishing it in Bonjour TXT lets the receiver's editor pair
    // discovered-sender entries with its accepted-set entries by UID,
    // surviving multi-homed source-IP differences and source-port shifts
    // across re-binds.
    info.wireUid  = stream.getSenderUid();
    // Use the friendly Sharing-pref-pane name on macOS (e.g. "Matthias's
    // MacBook Pro") rather than the unix hostname (e.g. "Mac" or
    // "Matthiass-MacBook-Pro-M2-2"). That's what the user recognises.
    info.host     = mcfx::net::getFriendlyComputerName();
    info.user     = juce::SystemStats::getLogonName();
    // Always advertise the configured wire channel count, even before any
    // target is connected. Otherwise a fresh sender shows up in receivers'
    // browse lists with "0 ch" until somebody connects, which is exactly
    // the wrong UX — the receiver wants to see "this sender will give me
    // N channels" before deciding to connect.
    {
        const int configured = getSendChannels();
        const int busWidth   = getMainBusNumInputChannels();
        info.channels = (configured > 0)
                          ? juce::jmin (configured, busWidth)
                          : busWidth;
    }
    {
        const juce::ScopedLock sl (paramLock);
        info.track = trackName;
        trackInfoDirty = false;
    }
    // Project name comes from REAPER (other DAWs return empty). Cache it
    // so refreshAdvertiseIfNeeded (called from the editor's 2 Hz timer)
    // can detect changes after a project save-as without spamming the
    // Bonjour publish path.
    currentProject = reaperIntegration.getProjectName();
    info.project = currentProject;
    senderAdvertiser->setPublish (stream.getListenPort(), info);
}

McfxSendAudioProcessor::~McfxSendAudioProcessor() = default;

void McfxSendAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    const int busChans = getMainBusNumInputChannels();
    if (busChans <= 0 || samplesPerBlock <= 0) return;

    // Effective wire channel count = min(user cap, bus). Cap=0 means "no
    // cap, send the whole bus".
    int cap;
    {
        const juce::ScopedLock sl (paramLock);
        cap = sendChannelsCap;
    }
    const int wireChans = (cap > 0) ? juce::jmin (cap, busChans) : busChans;

    stream.prepareToPlay (sampleRate, samplesPerBlock, wireChans,
                          stream.getWireFormat());
    applyTargetIfChanged();
    refreshSenderAdvertise();

    meters.setAudioParams (sampleRate, samplesPerBlock);
    meters.resize (busChans);
}

void McfxSendAudioProcessor::setSendChannels (int n)
{
    {
        const juce::ScopedLock sl (paramLock);
        sendChannelsCap = juce::jmax (0, n);
    }
    // Apply live. setRingChannels takes the SendStream's reconfig lock
    // internally; the audio thread tryEnter's that same lock and just
    // skips one block in the unlikely race window.
    const int busChans = getMainBusNumInputChannels();
    if (busChans <= 0) return;
    const int wireChans = (n > 0) ? juce::jmin (n, busChans) : busChans;
    stream.setRingChannels (wireChans);
    // Re-publish so receivers see the new channel count in their browse
    // list. Cheap — same TXT-only update path the host name / project
    // name changes also use.
    refreshSenderAdvertise();
}

int McfxSendAudioProcessor::getSendChannels() const
{
    const juce::ScopedLock sl (paramLock);
    return sendChannelsCap;
}

void McfxSendAudioProcessor::releaseResources()
{
    stream.releaseResources();
}

void McfxSendAudioProcessor::setSendTarget (const juce::String& host, int port)
{
    {
        const juce::ScopedLock sl (paramLock);
        if (host == sendHost && port == sendPort) return;
        sendHost    = host;
        sendPort    = port;
        targetDirty = true;
    }
    applyTargetIfChanged();
}

void McfxSendAudioProcessor::applyTargetIfChanged()
{
    juce::String h;
    int p = 0;
    bool dirty = false;
    {
        const juce::ScopedLock sl (paramLock);
        if (! targetDirty) return;
        h = sendHost;
        p = sendPort;
        targetDirty = false;
        dirty = true;
    }
    if (dirty)
        stream.setSingleTarget (h, p);
}

bool McfxSendAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   #if MCFX_MULTICHANNEL_BUILD
    return mcfx::isMultichannelLayoutSupported (layouts, NUM_CHANNELS, wrapperType);
   #else
    // Per-channel VST2 build is fixed-width.
    return layouts.getMainInputChannels()  == NUM_CHANNELS
        && layouts.getMainOutputChannels() == NUM_CHANNELS;
   #endif
}

void McfxSendAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn     = getMainBusNumInputChannels();
    const int numOut    = getMainBusNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Hand the input bus to the SendStream (RT-safe — interleaves into a
    // lock-free FIFO and signals SendThread).
    if (numIn > 0)
    {
        stream.pushAudioBlock (buffer.getArrayOfReadPointers(),
                               numIn, numSamples);

        // Measure ALL input channels for the local meter — even those
        // beyond the configured wire-channel cap, so the user can see
        // what they're cutting off when they dial "Channels to send"
        // below the bus width.
        meters.measureBlock (buffer);
    }

    // Passthrough — leave input samples in the output channels (buffer is
    // shared between in/out for symmetric layouts), clear any extras.
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);
}

juce::AudioProcessorEditor* McfxSendAudioProcessor::createEditor()
{
    return new McfxSendAudioProcessorEditor (*this);
}

void McfxSendAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("mcfx_send");
    {
        const juce::ScopedLock sl (paramLock);
        state.setProperty ("host", sendHost, nullptr);
        state.setProperty ("port", sendPort, nullptr);
        state.setProperty ("ch_cap", sendChannelsCap, nullptr);
    }
    state.setProperty ("fmt", static_cast<int> (getWireFormat()), nullptr);
    state.setProperty ("password", getPassword(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void McfxSendAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto t = juce::ValueTree::fromXml (*xml);
        if (t.isValid() && t.hasType ("mcfx_send"))
        {
            setSendTarget (t.getProperty ("host", "").toString(),
                           static_cast<int> (t.getProperty ("port", 0)));
            if (t.hasProperty ("fmt"))
            {
                const int f = juce::jlimit (1, 3, static_cast<int> (t.getProperty ("fmt", 3)));
                setWireFormat (static_cast<mcfx::net::SampleFormat> (f));
            }
            if (t.hasProperty ("ch_cap"))
                setSendChannels (static_cast<int> (t.getProperty ("ch_cap", 0)));
            if (t.hasProperty ("password"))
                setPassword (t.getProperty ("password", "").toString());
        }
    }
}

// Plugin factory — JUCE calls this once per plugin host load.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new McfxSendAudioProcessor();
}
