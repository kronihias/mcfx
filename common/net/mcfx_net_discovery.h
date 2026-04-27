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

 Local-network peer discovery for mcfx_send / mcfx_receive. Wraps
 juce::NetworkServiceDiscovery with TXT-style descriptions.

 Role inversion vs. sonolink: in mcfx, **receivers** publish (so senders
 can find which UDP port to send to), and **senders** browse. The data
 flow is sender → receiver, so the sender is the side that needs the
 target address.

 Both sides publish a quiet heartbeat under their respective UID — this
 isn't surfaced anywhere in the UI; it exists so macOS recognises each
 process as a local-network participant and the inbound UDP traffic is
 actually delivered (Local Network privacy gate). Without it a fresh
 install can't see broadcasts at all.

 Uses a per-process DiscoveryHub so multiple plugin instances inside a
 single DAW share one underlying NSD listener — necessary because macOS
 only delivers each broadcast packet to *one* of the listeners bound to a
 given port, even with SO_REUSEADDR.

 Lifted nearly verbatim from sonolink/Source/Discovery.h, simplified to
 use a callback instead of a ChangeBroadcaster and renamed from sonolink
 → mcfx::net.

 ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "mcfx_net_discovery_hub.h"

#include <functional>
#include <vector>

namespace mcfx { namespace net {

class Discovery : private juce::Timer
{
public:
    // Bonjour-ish broadcast ports — one per UID. Originally both sides
    // shared a single port, but JUCE's AvailableServiceList binds the
    // broadcast port for its socket and on macOS the kernel only
    // delivers each incoming broadcast to ONE of the listeners on a
    // given port (even with SO_REUSEADDR). With sender + receiver in
    // the same Reaper process that meant only one of the two browsers
    // ever saw anything. Splitting the ports gives each browser its own
    // socket and the conflict goes away.
    static constexpr int kBroadcastPortSenderUID   = 35517;
    static constexpr int kBroadcastPortReceiverUID = 35518;

    static constexpr const char* kReceiverUID = "mcfx_receive.v1";
    static constexpr const char* kSenderUID   = "mcfx_send.v1";

    static int broadcastPortFor (const juce::String& uid) noexcept
    {
        return (uid == kSenderUID) ? kBroadcastPortSenderUID
                                    : kBroadcastPortReceiverUID;
    }

    enum class Mode
    {
        Publish, // call setPublish() to advertise
        Browse,  // call onChange + getServices() to read
    };

    struct Service
    {
        juce::String   uid;          // Bonjour-stable id (TXT field)
        juce::IPAddress ip;
        int            port = 0;     // remote's UDP audio port
        juce::String   track;
        juce::String   host;         // computer name
        juce::String   user;
        juce::String   project;      // REAPER project name (empty in other DAWs)
        int            channels = 0;
        juce::String   format;       // "PCM 32-bit float" etc.
        juce::Time     lastSeen;
    };

    struct PublishInfo
    {
        juce::String uid;
        juce::String track;
        juce::String host;
        juce::String user;
        juce::String project;
        int          channels = 0;
        juce::String format;
    };

    explicit Discovery (Mode m, const juce::String& whichUid)
        : mode (m), serviceUid (whichUid)
    {
        // Plugin scan/instantiate races leave us 1–2 s before the
        // process is actually settled. Defer the first broadcast so
        // dead-on-arrival instances don't litter the network.
        firstBroadcastAt = juce::Time::getCurrentTime() + juce::RelativeTime::seconds (2.0);

        if (mode == Mode::Browse)
        {
            // Heartbeat publish under the OTHER side's UID so macOS opens
            // the local-network gate for inbound broadcasts. Empty TXT,
            // connectionPort=0 — this is just a presence ping.
            const juce::String heartbeatUid =
                (serviceUid == kReceiverUID) ? kSenderUID : kReceiverUID;
            juce::StringPairArray fields;
            fields.set ("host", juce::SystemStats::getComputerName());
            // Heartbeat advertises on the OPPOSITE-UID's broadcast port —
            // that's where it would be heard. (No one listens for it on
            // our own browse port.)
            heartbeatAdvertiser =
                std::make_unique<juce::NetworkServiceDiscovery::Advertiser> (
                    heartbeatUid, buildDescription (fields),
                    broadcastPortFor (heartbeatUid), 0);

            hubSubscription = DiscoveryHub::instance().subscribe (
                serviceUid, broadcastPortFor (serviceUid),
                [this]() { rebuildList(); if (onChange) onChange(); });
        }

        startTimerHz (2);
    }

    ~Discovery() override
    {
        stopTimer();
        advertiser.reset();
        heartbeatAdvertiser.reset();
        hubSubscription.reset();
    }

    // Publisher side — call after the audio UDP socket is bound. Pass
    // port=0 to stop advertising. The TXT record is rebuilt on every call
    // so live changes (track rename, channel-count change, format change)
    // propagate to browsers.
    void setPublish (int audioPort, const PublishInfo& info)
    {
        if (mode != Mode::Publish) return;

        const bool changed = (audioPort != currentPort)
                          || (info.uid      != lastInfo.uid)
                          || (info.track    != lastInfo.track)
                          || (info.host     != lastInfo.host)
                          || (info.user     != lastInfo.user)
                          || (info.project  != lastInfo.project)
                          || (info.channels != lastInfo.channels)
                          || (info.format   != lastInfo.format);
        if (! changed) return;

        currentPort = audioPort;
        lastInfo = info;
        rebuildAdvertiser();
    }

    // Browser side — UI reads the cached list each tick.
    std::vector<Service> getServices() const
    {
        const juce::ScopedLock sl (cacheLock);
        return cache;
    }

    // Browser side — invalidate + re-bind the hub's listener. Useful
    // after sleep/wake or wifi reassociation.
    void refresh()
    {
        if (mode != Mode::Browse) return;
        {
            const juce::ScopedLock sl (cacheLock);
            cache.clear();
        }
        if (onChange) onChange();
        DiscoveryHub::instance().refresh();
    }

    // Fired (on the message thread) whenever the browsed list changes.
    std::function<void()> onChange;

    // ASCII-safe TXT serialisation. Pipe-separated key=value pairs.
    // Embedded '|' and '=' get replaced with private-use codepoints so
    // round-trip parsing never breaks.
    static juce::String buildDescription (const juce::StringPairArray& fields)
    {
        juce::String s;
        const auto& keys = fields.getAllKeys();
        for (int i = 0; i < keys.size(); ++i)
        {
            const auto& k = keys[i];
            juce::String v = fields[k];
            if (v.length() > 120) v = v.substring (0, 120);
            v = v.replaceCharacter ('|', (juce::juce_wchar) 0xE001);
            v = v.replaceCharacter ('=', (juce::juce_wchar) 0xE002);
            if (i > 0) s << '|';
            s << k << '=' << v;
        }
        return s;
    }

    static juce::StringPairArray parseDescription (const juce::String& desc)
    {
        juce::StringPairArray result;
        juce::StringArray parts;
        parts.addTokens (desc, "|", {});
        for (const auto& part : parts)
        {
            const int eq = part.indexOfChar ('=');
            if (eq <= 0) continue;
            const auto key = part.substring (0, eq).trim();
            auto val       = part.substring (eq + 1);
            val = val.replaceCharacter ((juce::juce_wchar) 0xE001, '|');
            val = val.replaceCharacter ((juce::juce_wchar) 0xE002, '=');
            result.set (key, val);
        }
        return result;
    }

private:
    void timerCallback() override
    {
        if (mode == Mode::Publish && advertiser == nullptr
            && currentPort > 0
            && juce::Time::getCurrentTime() >= firstBroadcastAt)
        {
            rebuildAdvertiser();
        }
        if (mode == Mode::Browse)
            rebuildList();
    }

    void rebuildAdvertiser()
    {
        advertiser.reset();
        if (currentPort <= 0) return;
        if (juce::Time::getCurrentTime() < firstBroadcastAt) return;

        juce::StringPairArray fields;
        fields.set ("uid",      lastInfo.uid);
        if (lastInfo.track .isNotEmpty()) fields.set ("track", lastInfo.track);
        if (lastInfo.host  .isNotEmpty()) fields.set ("host",  lastInfo.host);
        if (lastInfo.user  .isNotEmpty()) fields.set ("user",  lastInfo.user);
        if (lastInfo.project.isNotEmpty()) fields.set ("proj", lastInfo.project);
        if (lastInfo.channels > 0)        fields.set ("ch",    juce::String (lastInfo.channels));
        if (lastInfo.format.isNotEmpty()) fields.set ("fmt",   lastInfo.format);

        advertiser = std::make_unique<juce::NetworkServiceDiscovery::Advertiser> (
            serviceUid, buildDescription (fields),
            broadcastPortFor (serviceUid), currentPort);
    }

    void rebuildList()
    {
        if (mode != Mode::Browse) return;
        const auto live = DiscoveryHub::instance().getServices (
            serviceUid, broadcastPortFor (serviceUid));

        std::vector<Service> out;
        out.reserve (live.size());
        for (const auto& svc : live)
        {
            // Heartbeat entries from the OTHER side advertise port=0
            // under our own UID just to open the macOS local-network
            // gate. Skip them — they're not real services.
            if (svc.port == 0) continue;
            const auto fields = parseDescription (svc.description);
            Service d;
            d.uid      = fields.getValue ("uid",   {});
            d.ip       = svc.address;
            d.port     = svc.port;
            d.lastSeen = svc.lastSeen;
            d.track    = fields.getValue ("track", {});
            d.host     = fields.getValue ("host",  {});
            d.user     = fields.getValue ("user",  {});
            d.project  = fields.getValue ("proj",  {});
            d.channels = fields.getValue ("ch",    "0").getIntValue();
            d.format   = fields.getValue ("fmt",   {});
            out.push_back (std::move (d));
        }

        bool changed = false;
        {
            const juce::ScopedLock sl (cacheLock);
            if (out.size() != cache.size())
            {
                changed = true;
            }
            else
            {
                for (size_t i = 0; i < out.size(); ++i)
                {
                    const auto& a = out[i];
                    const auto& b = cache[i];
                    if (a.uid != b.uid || a.ip.toString() != b.ip.toString()
                        || a.port != b.port || a.track != b.track || a.host != b.host
                        || a.user != b.user || a.project != b.project
                        || a.channels != b.channels || a.format != b.format)
                    { changed = true; break; }
                }
            }
            cache = std::move (out);
        }
        if (changed && onChange) onChange();
    }

    Mode mode;
    juce::String serviceUid;

    std::unique_ptr<juce::NetworkServiceDiscovery::Advertiser> advertiser;
    std::unique_ptr<juce::NetworkServiceDiscovery::Advertiser> heartbeatAdvertiser;
    DiscoveryHub::Subscription                                 hubSubscription;

    int           currentPort = 0;
    PublishInfo   lastInfo;

    juce::Time firstBroadcastAt;

    mutable juce::CriticalSection cacheLock;
    std::vector<Service>          cache;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Discovery)
};

}} // namespace mcfx::net
