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

McfxReceiveAudioProcessor::~McfxReceiveAudioProcessor()
{
    stopTimer();
}

bool McfxReceiveAudioProcessor::inviteSender (const juce::String& host, int port,
                                              std::uint32_t wireUid, bool isUserAction)
{
    if (isUserAction)
    {
        cancelAutoReconnect();
        if (wireUid != 0)
        {
            const auto hint = lookupBonjourHint (wireUid);
            const juce::ScopedLock sl (bonjourHintsLock);
            if (! hint.host.isEmpty())
                lastBonjour[std::make_pair (host, port)] = hint;
            else
                lastBonjour.erase (std::make_pair (host, port));
        }
        else
        {
            const juce::ScopedLock sl (bonjourHintsLock);
            lastBonjour.erase (std::make_pair (host, port));
        }
    }
    return stream.inviteSender (host, port, wireUid);
}

void McfxReceiveAudioProcessor::uninviteSender (const juce::String& host, int port,
                                                std::uint32_t wireUid, bool isUserAction)
{
    if (isUserAction)
    {
        cancelAutoReconnect();
        const juce::ScopedLock sl (bonjourHintsLock);
        lastBonjour.erase (std::make_pair (host, port));
    }
    stream.uninviteSender (host, port, wireUid);
}

void McfxReceiveAudioProcessor::uninviteAllSenders (bool isUserAction)
{
    if (isUserAction)
    {
        cancelAutoReconnect();
        const juce::ScopedLock sl (bonjourHintsLock);
        lastBonjour.clear();
    }
    stream.uninviteAllSenders();
}

McfxReceiveAudioProcessor::BonjourHint
McfxReceiveAudioProcessor::lookupBonjourHint (std::uint32_t wireUid) const
{
    BonjourHint h;
    if (wireUid == 0 || ! senderBrowser) return h;
    for (const auto& s : senderBrowser->getServices())
    {
        if (s.wireUid == wireUid)
        {
            h.host    = s.host;
            h.project = s.project;
            h.track   = s.track;
            break;
        }
    }
    return h;
}

void McfxReceiveAudioProcessor::setAutoReconnectEnabled (bool on)
{
    autoReconnectEnabled.store (on, std::memory_order_release);
    if (! on) cancelAutoReconnect();
}

int McfxReceiveAudioProcessor::getArmedPeerCount() const
{
    const juce::ScopedLock sl (armedLock);
    return static_cast<int> (armedPeers.size());
}

int McfxReceiveAudioProcessor::getAutoReconnectSecondsRemaining() const
{
    const juce::ScopedLock sl (armedLock);
    if (armedPeers.empty()) return 0;
    const auto nowT = juce::Time::getCurrentTime();
    int maxRem = 0;
    for (const auto& p : armedPeers)
    {
        const int rem = 60 - static_cast<int> ((nowT - p.armedAt).inSeconds());
        if (rem > maxRem) maxRem = rem;
    }
    return juce::jmax (0, maxRem);
}

void McfxReceiveAudioProcessor::cancelAutoReconnect()
{
    {
        const juce::ScopedLock sl (armedLock);
        armedPeers.clear();
    }
    stopTimer();
}

void McfxReceiveAudioProcessor::timerCallback()
{
    // 1 Hz auto-reconnect tick. For each armed peer:
    //   1. If we're already connected to this peer (by host:port, by UID,
    //      or by Bonjour-hint match against any Active accepted entry),
    //      uninvite any stale (host, port) entry and drop the armed peer.
    //   2. If the 60 s window expired, uninvite the stale entry to clear
    //      the duplicate row from the UI, then drop.
    //   3. Otherwise update (host, port) to the discovered service's
    //      current address (if Bonjour rematch hits) and re-issue the
    //      INVITE.
    if (! autoReconnectEnabled.load (std::memory_order_acquire))
    {
        cancelAutoReconnect();
        return;
    }

    enum class Kind : std::uint8_t { Replace, Invite, Uninvite };
    struct Action
    {
        Kind         kind = Kind::Invite;
        juce::String oldHost;
        int          oldPort = 0;
        juce::String newHost;
        int          newPort = 0;
        std::uint32_t wireUid = 0;
    };
    std::vector<Action> actions;

    const auto nowT     = juce::Time::getCurrentTime();
    const auto accepted = stream.getAcceptedSenders();
    const auto services = senderBrowser ? senderBrowser->getServices()
                                        : std::vector<mcfx::net::Discovery::Service>{};

    {
        const juce::ScopedLock sl (armedLock);

        auto isActiveAt = [&] (const juce::String& h, int p, std::uint32_t uid) {
            for (const auto& a : accepted)
            {
                const bool addrMatch = (a.host == h && a.port == p);
                const bool uidMatch  = (uid != 0 && a.uid == uid);
                if ((addrMatch || uidMatch)
                    && a.state == mcfx::net::RecvStream::SenderState::Active)
                    return true;
            }
            return false;
        };

        // Hint-based match: any Active accepted entry whose UID belongs
        // to a Bonjour service matching the saved identity counts as
        // success — handles "sender re-bound to a different ephemeral
        // port and is already Active under that new address".
        auto activeUidByHint = [&] (const ArmedPeer& ap) -> std::uint32_t {
            if (ap.hint.host.isEmpty()) return 0;
            for (const auto& s : services)
            {
                if (s.host != ap.hint.host) continue;
                if (! ap.hint.project.isEmpty() && s.project != ap.hint.project) continue;
                if (! ap.hint.track  .isEmpty() && s.track   != ap.hint.track)   continue;
                if (s.wireUid == 0) continue;
                for (const auto& a : accepted)
                    if (a.uid == s.wireUid
                        && a.state == mcfx::net::RecvStream::SenderState::Active)
                        return s.wireUid;
            }
            return 0;
        };

        // Loose fallback for the case where the peer's connection came
        // in from the other side (their auto-reconnect's INVITE landed
        // on our listen port and we accepted) and we can't use Bonjour
        // hints to confirm identity. If any Active accepted entry
        // exists at the same IP host as our saved (host, port), call
        // it good — saves the user the 60 s spinner when both sides
        // are auto-reconnecting at once.
        auto activeAtSameHost = [&] (const ArmedPeer& ap) {
            for (const auto& a : accepted)
                if (a.host == ap.host
                    && a.uid != 0
                    && a.state == mcfx::net::RecvStream::SenderState::Active)
                    return true;
            return false;
        };

        for (auto it = armedPeers.begin(); it != armedPeers.end(); )
        {
            if (isActiveAt (it->host, it->port, it->currentWireUid))
            {
                it = armedPeers.erase (it);
                continue;
            }

            if (auto matchedUid = activeUidByHint (*it))
            {
                // Drop the stale (host, port) accepted entry that never
                // ACKed — otherwise the UI shows a "(direct) … Inviting"
                // row alongside the green Bonjour-paired row.
                Action u;
                u.kind = Kind::Uninvite;
                u.oldHost = it->host;
                u.oldPort = it->port;
                actions.push_back (u);
                (void) matchedUid;
                it = armedPeers.erase (it);
                continue;
            }

            // Same-host fallback — connection initiated by the sender,
            // our hint can't confirm identity but the IP matches. Stop.
            if (activeAtSameHost (*it))
            {
                Action u;
                u.kind = Kind::Uninvite;
                u.oldHost = it->host;
                u.oldPort = it->port;
                actions.push_back (u);
                it = armedPeers.erase (it);
                continue;
            }

            if ((nowT - it->armedAt).inSeconds() >= 60)
            {
                Action u;
                u.kind = Kind::Uninvite;
                u.oldHost = it->host;
                u.oldPort = it->port;
                actions.push_back (u);
                it = armedPeers.erase (it);
                continue;
            }

            if (! it->hint.host.isEmpty())
            {
                for (const auto& s : services)
                {
                    if (s.host != it->hint.host) continue;
                    if (! it->hint.project.isEmpty() && s.project != it->hint.project) continue;
                    if (! it->hint.track  .isEmpty() && s.track   != it->hint.track)   continue;

                    const auto newIp = s.ip.toString();
                    if (newIp != it->host || s.port != it->port)
                    {
                        Action a;
                        a.kind    = Kind::Replace;
                        a.oldHost = it->host;
                        a.oldPort = it->port;
                        a.newHost = newIp;
                        a.newPort = s.port;
                        a.wireUid = s.wireUid;
                        actions.push_back (a);
                        it->host = newIp;
                        it->port = s.port;
                        it->currentWireUid = s.wireUid;
                    }
                    else
                    {
                        it->currentWireUid = s.wireUid;
                        Action a;
                        a.kind    = Kind::Invite;
                        a.newHost = it->host;
                        a.newPort = it->port;
                        a.wireUid = it->currentWireUid;
                        actions.push_back (a);
                    }
                    goto next_armed;
                }
            }

            {
                Action a;
                a.kind    = Kind::Invite;
                a.newHost = it->host;
                a.newPort = it->port;
                a.wireUid = it->currentWireUid;
                actions.push_back (a);
            }
        next_armed:
            ++it;
        }
    }

    for (const auto& a : actions)
    {
        switch (a.kind)
        {
            case Kind::Replace:
            {
                BonjourHint hint;
                {
                    const juce::ScopedLock sl (bonjourHintsLock);
                    auto it = lastBonjour.find (std::make_pair (a.oldHost, a.oldPort));
                    if (it != lastBonjour.end())
                    {
                        hint = it->second;
                        lastBonjour.erase (it);
                    }
                }
                uninviteSender (a.oldHost, a.oldPort, /*wireUid=*/0, /*isUserAction=*/false);
                if (! hint.host.isEmpty())
                {
                    const juce::ScopedLock sl (bonjourHintsLock);
                    lastBonjour[std::make_pair (a.newHost, a.newPort)] = hint;
                }
                inviteSender (a.newHost, a.newPort, a.wireUid, /*isUserAction=*/false);
                break;
            }
            case Kind::Invite:
                inviteSender (a.newHost, a.newPort, a.wireUid, /*isUserAction=*/false);
                break;
            case Kind::Uninvite:
                uninviteSender (a.oldHost, a.oldPort, /*wireUid=*/0, /*isUserAction=*/false);
                break;
        }
    }

    {
        const juce::ScopedLock sl (armedLock);
        if (armedPeers.empty())
            stopTimer();
    }
}

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
    // Wire UID — uint32 we write into outbound CommonHeader.sender.
    // Lets the sender's editor pair its targets-list entries with its
    // Bonjour-discovered receivers by UID rather than fragile (host:port).
    info.wireUid  = stream.getReceiverUid();
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
    state.setProperty ("auto_reconnect",
                       isAutoReconnectEnabled() ? 1 : 0, nullptr);

    // Persist every accepted sender (Active or Pending) plus any
    // captured Bonjour identity hints. On reload we re-arm an entry
    // per saved <sender>.
    const auto accepted = stream.getAcceptedSenders();
    for (const auto& a : accepted)
    {
        if (a.state != mcfx::net::RecvStream::SenderState::Active
            && a.state != mcfx::net::RecvStream::SenderState::Pending)
            continue;
        juce::ValueTree node ("sender");
        node.setProperty ("host", a.host, nullptr);
        node.setProperty ("port", a.port, nullptr);

        BonjourHint hint;
        {
            const juce::ScopedLock sl (bonjourHintsLock);
            auto it = lastBonjour.find (std::make_pair (a.host, a.port));
            if (it != lastBonjour.end()) hint = it->second;
        }
        if (! hint.host.isEmpty())    node.setProperty ("bj_host",    hint.host,    nullptr);
        if (! hint.project.isEmpty()) node.setProperty ("bj_project", hint.project, nullptr);
        if (! hint.track.isEmpty())   node.setProperty ("bj_track",   hint.track,   nullptr);
        state.appendChild (node, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void McfxReceiveAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto t = juce::ValueTree::fromXml (*xml);
        if (! (t.isValid() && t.hasType ("mcfx_receive"))) return;

        setListenPort (static_cast<int> (t.getProperty ("port", 0)));
        if (t.hasProperty ("jitter_ms"))
            setJitterFloorMs (static_cast<int> (t.getProperty ("jitter_ms", 25)));
        if (t.hasProperty ("password"))
            setPassword (t.getProperty ("password", "").toString());

        const bool autoOn = t.hasProperty ("auto_reconnect")
                              ? static_cast<int> (t.getProperty ("auto_reconnect", 1)) != 0
                              : true;
        autoReconnectEnabled.store (autoOn, std::memory_order_release);

        struct Saved { juce::String host; int port; BonjourHint hint; };
        std::vector<Saved> saved;

        for (int i = 0; i < t.getNumChildren(); ++i)
        {
            auto c = t.getChild (i);
            if (! c.hasType ("sender")) continue;
            Saved s;
            s.host = c.getProperty ("host", "").toString();
            s.port = static_cast<int> (c.getProperty ("port", 0));
            if (s.host.isEmpty() || s.port <= 0) continue;
            s.hint.host    = c.getProperty ("bj_host",    "").toString();
            s.hint.project = c.getProperty ("bj_project", "").toString();
            s.hint.track   = c.getProperty ("bj_track",   "").toString();
            saved.push_back (std::move (s));
        }

        {
            const juce::ScopedLock sl (bonjourHintsLock);
            lastBonjour.clear();
            for (const auto& s : saved)
                if (! s.hint.host.isEmpty())
                    lastBonjour[std::make_pair (s.host, s.port)] = s.hint;
        }

        // Replace whatever's currently in the accepted-set with the saved
        // set, mirroring the original "setStateInformation = full reset"
        // semantics for hosts that load presets onto an already-running
        // plugin instance.
        uninviteAllSenders (/*isUserAction=*/false);

        for (const auto& s : saved)
            inviteSender (s.host, s.port, /*wireUid=*/0, /*isUserAction=*/false);

        if (autoOn && ! saved.empty())
        {
            const auto nowT = juce::Time::getCurrentTime();
            {
                const juce::ScopedLock sl (armedLock);
                armedPeers.clear();
                for (const auto& s : saved)
                {
                    ArmedPeer ap;
                    ap.host    = s.host;
                    ap.port    = s.port;
                    ap.hint    = s.hint;
                    ap.armedAt = nowT;
                    armedPeers.push_back (std::move (ap));
                }
            }
            startTimer (1000);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new McfxReceiveAudioProcessor();
}
