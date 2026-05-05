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

McfxSendAudioProcessor::~McfxSendAudioProcessor()
{
    stopTimer();
}

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

void McfxSendAudioProcessor::setSendTarget (const juce::String& host, int port, bool isUserAction)
{
    if (isUserAction) cancelAutoReconnect();
    {
        const juce::ScopedLock sl (paramLock);
        if (host == sendHost && port == sendPort) return;
        sendHost    = host;
        sendPort    = port;
        targetDirty = true;
    }
    applyTargetIfChanged();
}

void McfxSendAudioProcessor::addTarget (const juce::String& host, int port,
                                        std::uint32_t wireUid, bool isUserAction)
{
    if (isUserAction)
    {
        cancelAutoReconnect();
        // Capture Bonjour identity at connect time so the next project
        // load can rematch by computer-name if the peer's ip/port drifts.
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
    stream.addTarget (host, port, wireUid);
}

void McfxSendAudioProcessor::removeTarget (const juce::String& host, int port, bool isUserAction)
{
    if (isUserAction)
    {
        cancelAutoReconnect();
        const juce::ScopedLock sl (bonjourHintsLock);
        lastBonjour.erase (std::make_pair (host, port));
    }
    // Auto-reconnect rematch path leaves lastBonjour intact — the timer
    // moves the hint to the new (host, port) key explicitly.
    stream.removeTarget (host, port);
}

void McfxSendAudioProcessor::clearTargets (bool isUserAction)
{
    if (isUserAction)
    {
        cancelAutoReconnect();
        const juce::ScopedLock sl (bonjourHintsLock);
        lastBonjour.clear();
    }
    stream.clearTargets();
}

McfxSendAudioProcessor::BonjourHint
McfxSendAudioProcessor::lookupBonjourHint (std::uint32_t wireUid) const
{
    BonjourHint h;
    if (wireUid == 0 || ! discovery) return h;
    for (const auto& s : discovery->getServices())
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

void McfxSendAudioProcessor::setAutoReconnectEnabled (bool on)
{
    autoReconnectEnabled.store (on, std::memory_order_release);
    if (! on) cancelAutoReconnect();
}

int McfxSendAudioProcessor::getArmedPeerCount() const
{
    const juce::ScopedLock sl (armedLock);
    return static_cast<int> (armedPeers.size());
}

int McfxSendAudioProcessor::getAutoReconnectSecondsRemaining() const
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

void McfxSendAudioProcessor::cancelAutoReconnect()
{
    {
        const juce::ScopedLock sl (armedLock);
        armedPeers.clear();
    }
    stopTimer();
}

void McfxSendAudioProcessor::timerCallback()
{
    // 1 Hz auto-reconnect tick. For each armed peer:
    //   1. If we're already connected to this peer (by host:port, by UID,
    //      or by Bonjour-hint match against any Active target), uninvite
    //      any stale (host, port) entry and drop the armed peer.
    //   2. If the 60 s window expired, uninvite the stale (host, port)
    //      entry to clear the duplicate from the UI, then drop.
    //   3. Otherwise update (host, port) to the discovered service's
    //      current address (if Bonjour rematch hits) and re-issue the
    //      INVITE — refreshes the SendStream's per-target retry.
    //
    // When armedPeers empties, the timer stops.
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
    const auto targets  = stream.getTargets();
    const auto services = discovery ? discovery->getServices()
                                    : std::vector<mcfx::net::Discovery::Service>{};

    {
        const juce::ScopedLock sl (armedLock);

        auto isActiveAt = [&] (const juce::String& h, int p, std::uint32_t uid) {
            for (const auto& t : targets)
            {
                const bool addrMatch = (t.host == h && t.port == p);
                const bool uidMatch  = (uid != 0 && t.receiverUid == uid);
                if ((addrMatch || uidMatch)
                    && t.state == mcfx::net::SendStream::TargetState::Active)
                    return true;
            }
            return false;
        };

        // Hint-based match: if any Bonjour service matches our saved
        // identity, then any Active target with that service's wireUid
        // counts as "we're connected to the right peer" — even if the
        // target's host:port doesn't match our saved (host, port).
        // Returns the wireUid we matched on, or 0.
        auto activeUidByHint = [&] (const ArmedPeer& ap) -> std::uint32_t {
            if (ap.hint.host.isEmpty()) return 0;
            for (const auto& s : services)
            {
                if (s.host != ap.hint.host) continue;
                if (! ap.hint.project.isEmpty() && s.project != ap.hint.project) continue;
                if (! ap.hint.track  .isEmpty() && s.track   != ap.hint.track)   continue;
                if (s.wireUid == 0) continue;
                for (const auto& t : targets)
                    if (t.receiverUid == s.wireUid
                        && t.state == mcfx::net::SendStream::TargetState::Active)
                        return s.wireUid;
            }
            return 0;
        };

        // Loose fallback for the case where the peer's connection was
        // initiated by the other side (e.g. their auto-reconnect's
        // INVITE landed on our listen port and we accepted) and we
        // can't use Bonjour hints to confirm identity (no hint stored,
        // or hint can't be paired against a current service).
        // If any Active target exists at the same IP host as our
        // saved (host, port), call it good — saves the user the
        // 60 s spinner when both sides are auto-reconnecting at once.
        auto activeAtSameHost = [&] (const ArmedPeer& ap) {
            for (const auto& t : targets)
                if (t.host == ap.host
                    && t.receiverUid != 0
                    && t.state == mcfx::net::SendStream::TargetState::Active)
                    return true;
            return false;
        };

        for (auto it = armedPeers.begin(); it != armedPeers.end(); )
        {
            // Direct match by (host, port) or currentWireUid.
            if (isActiveAt (it->host, it->port, it->currentWireUid))
            {
                it = armedPeers.erase (it);
                continue;
            }

            // Hint-based match — sender re-bound to a different ephemeral
            // port and is Active under that new address.
            if (auto matchedUid = activeUidByHint (*it))
            {
                // Cleanup: drop the stale (host, port) target entry that
                // never ACKed, otherwise the UI would carry a "(direct)
                // … Inviting" row in parallel with the green Bonjour row.
                Action u;
                u.kind = Kind::Uninvite;
                u.oldHost = it->host;
                u.oldPort = it->port;
                actions.push_back (u);
                (void) matchedUid;
                it = armedPeers.erase (it);
                continue;
            }

            // Same-host fallback — connection initiated by the other
            // side, our hint can't confirm identity but the IP matches.
            // Stop trying.
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

            // 60 s window expired — give up. Clean up the stale entry so
            // the editor list isn't littered with a dead Pending row for
            // another 30 s of normal Timeout-display.
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

            // Bonjour rematch — if a service matching our saved identity
            // is up at a different (ip, port) than what we have armed,
            // swap to the discovered address.
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
                        // Same address — capture the UID so the editor's
                        // row-pairing finds the entry on the very next
                        // refresh, and so isActiveAt by uid hits.
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

            // No rematch — just re-issue the saved (host, port).
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
                // Carry the Bonjour hint from old (host, port) → new key
                // so a subsequent project save still records the peer's
                // identity.
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
                removeTarget (a.oldHost, a.oldPort, /*isUserAction=*/false);
                if (! hint.host.isEmpty())
                {
                    const juce::ScopedLock sl (bonjourHintsLock);
                    lastBonjour[std::make_pair (a.newHost, a.newPort)] = hint;
                }
                addTarget (a.newHost, a.newPort, a.wireUid, /*isUserAction=*/false);
                break;
            }
            case Kind::Invite:
                addTarget (a.newHost, a.newPort, a.wireUid, /*isUserAction=*/false);
                break;
            case Kind::Uninvite:
                removeTarget (a.oldHost, a.oldPort, /*isUserAction=*/false);
                break;
        }
    }

    {
        const juce::ScopedLock sl (armedLock);
        if (armedPeers.empty())
            stopTimer();
    }
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
        // Legacy single-target attrs retained for older versions reading
        // back a state saved by this build. We mirror the first target.
        state.setProperty ("host", sendHost, nullptr);
        state.setProperty ("port", sendPort, nullptr);
        state.setProperty ("ch_cap", sendChannelsCap, nullptr);
    }
    state.setProperty ("fmt", static_cast<int> (getWireFormat()), nullptr);
    state.setProperty ("password", getPassword(), nullptr);
    state.setProperty ("auto_reconnect",
                       isAutoReconnectEnabled() ? 1 : 0, nullptr);

    // Persist every currently-tracked target (Active or Pending) plus
    // any captured Bonjour identity hints. On reload we re-arm one entry
    // per saved <target>.
    const auto targets = stream.getTargets();
    for (const auto& t : targets)
    {
        if (t.state != mcfx::net::SendStream::TargetState::Active
            && t.state != mcfx::net::SendStream::TargetState::Pending)
            continue;
        juce::ValueTree node ("target");
        node.setProperty ("host", t.host, nullptr);
        node.setProperty ("port", t.port, nullptr);

        BonjourHint hint;
        {
            const juce::ScopedLock sl (bonjourHintsLock);
            auto it = lastBonjour.find (std::make_pair (t.host, t.port));
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

void McfxSendAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto t = juce::ValueTree::fromXml (*xml);
        if (! (t.isValid() && t.hasType ("mcfx_send"))) return;

        if (t.hasProperty ("fmt"))
        {
            const int f = juce::jlimit (1, 3, static_cast<int> (t.getProperty ("fmt", 3)));
            setWireFormat (static_cast<mcfx::net::SampleFormat> (f));
        }
        if (t.hasProperty ("ch_cap"))
            setSendChannels (static_cast<int> (t.getProperty ("ch_cap", 0)));
        if (t.hasProperty ("password"))
            setPassword (t.getProperty ("password", "").toString());

        const bool autoOn = t.hasProperty ("auto_reconnect")
                              ? static_cast<int> (t.getProperty ("auto_reconnect", 1)) != 0
                              : true;
        autoReconnectEnabled.store (autoOn, std::memory_order_release);

        // Build the list of peers to (re-)connect. New format: <target>
        // children. Legacy: single host/port attrs. Either way we feed
        // the result through addTarget(... isUserAction=false) so the
        // auto-reconnect arm doesn't immediately cancel itself.
        struct Saved { juce::String host; int port; BonjourHint hint; };
        std::vector<Saved> saved;

        for (int i = 0; i < t.getNumChildren(); ++i)
        {
            auto c = t.getChild (i);
            if (! c.hasType ("target")) continue;
            Saved s;
            s.host = c.getProperty ("host", "").toString();
            s.port = static_cast<int> (c.getProperty ("port", 0));
            if (s.host.isEmpty() || s.port <= 0) continue;
            s.hint.host    = c.getProperty ("bj_host",    "").toString();
            s.hint.project = c.getProperty ("bj_project", "").toString();
            s.hint.track   = c.getProperty ("bj_track",   "").toString();
            saved.push_back (std::move (s));
        }

        // Back-compat: no <target> children but legacy host/port attrs.
        if (saved.empty() && t.hasProperty ("host"))
        {
            const auto h = t.getProperty ("host", "").toString();
            const int  p = static_cast<int> (t.getProperty ("port", 0));
            if (h.isNotEmpty() && p > 0)
                saved.push_back ({ h, p, {} });
        }

        // Mirror the first saved target into the legacy single-target
        // sendHost/sendPort fields for getSendHost/getSendPort callers.
        if (! saved.empty())
        {
            const juce::ScopedLock sl (paramLock);
            sendHost = saved.front().host;
            sendPort = saved.front().port;
        }

        // Stash the captured Bonjour hints for the targets we're about
        // to (re)create — getStateInformation needs them for next save.
        {
            const juce::ScopedLock sl (bonjourHintsLock);
            lastBonjour.clear();
            for (const auto& s : saved)
                if (! s.hint.host.isEmpty())
                    lastBonjour[std::make_pair (s.host, s.port)] = s.hint;
        }

        // Replace whatever's currently in the targets list with the saved
        // set — matches the original setSendTarget→setSingleTarget
        // semantics for hosts that re-call setStateInformation on an
        // already-running plugin (preset switch).
        clearTargets (/*isUserAction=*/false);

        // (Re)issue the connects. isUserAction=false so we don't cancel
        // ourselves before we even start the timer.
        for (const auto& s : saved)
            addTarget (s.host, s.port, /*wireUid=*/0, /*isUserAction=*/false);

        // Arm the auto-reconnect retry loop.
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

// Plugin factory — JUCE calls this once per plugin host load.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new McfxSendAudioProcessor();
}
