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

 mcfx_send — multichannel UDP audio sender. Sends the input bus over the
 network to one or more mcfx_receive instances. LAN-first; PCM-only in v1.

 ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "mcfx_buses.h"
#include "mcfx_net_send.h"
#include "mcfx_net_discovery.h"
#include "mcfx_net_reaper.h"
#include "mcfx_net_meter.h"

#include <functional>
#include <map>
#include <memory>
#include <vector>

class McfxSendAudioProcessor : public juce::AudioProcessor,
                               public juce::ChangeBroadcaster,
                               private juce::Timer
{
public:
    McfxSendAudioProcessor();
    ~McfxSendAudioProcessor() override;

    // Editor-facing: configure the sender's destination. The "Apply" button
    // calls this — it replaces all existing targets with a single one.
    // Empty host clears (Disconnect button uses host="" port=0).
    // isUserAction=true (default, from editor) cancels any pending
    // auto-reconnect attempt; setStateInformation passes false.
    void setSendTarget (const juce::String& host, int port, bool isUserAction = true);
    juce::String getSendHost() const { const juce::ScopedLock sl (paramLock); return sendHost; }
    int          getSendPort() const { const juce::ScopedLock sl (paramLock); return sendPort; }

    // Multi-target additions: clicking a discovered receiver in the editor
    // ADDS that receiver to the targets list (additive), not REPLACES.
    // isUserAction=true (default) cancels auto-reconnect; the auto-reconnect
    // timer passes false when re-issuing connects on its own ticks.
    void addTarget    (const juce::String& host, int port, std::uint32_t wireUid = 0,
                       bool isUserAction = true);
    void removeTarget (const juce::String& host, int port, bool isUserAction = true);
    void clearTargets (bool isUserAction = true);
    std::vector<mcfx::net::SendStream::Target> getTargets() const
    {
        return stream.getTargets();
    }
    int getActiveTargetCount() const noexcept { return stream.getActiveTargetCount(); }

    // Password: gates inbound INVITEs. Empty = no auth.
    void setPassword (const juce::String& pw) { stream.setPassword (pw); }
    juce::String getPassword() const { return stream.getPassword(); }

    // For Bonjour: which UDP port we're bound to.
    int getListenPort() const noexcept { return stream.getListenPort(); }

    // Wire format selector. Live — takes effect on the next packet.
    void setWireFormat (mcfx::net::SampleFormat fmt) { stream.setWireFormat (fmt); }
    mcfx::net::SampleFormat getWireFormat() const noexcept { return stream.getWireFormat(); }

    // How many channels to put on the wire. The audio thread caps to this
    // count regardless of how many channels the input bus actually has,
    // and the sender-side packet headers reflect this cap (so the
    // receiver only ever allocates and mixes the channels we actually
    // send). Live — triggers a brief stream rebuild.
    void setSendChannels (int n);
    int  getSendChannels() const;

    // Bonjour-discovered receivers. Editor reads each tick; the callback
    // also fires whenever the list changes (so the editor can repaint
    // out-of-band rather than waiting for its 2 Hz timer).
    std::vector<mcfx::net::Discovery::Service> getDiscoveredReceivers() const
    {
        return discovery ? discovery->getServices() : std::vector<mcfx::net::Discovery::Service>{};
    }
    std::function<void()> onDiscoveredReceiversChanged;

    // Track-name change from the host. Forwarded into the sender's own
    // Bonjour TXT so a receiver browsing the LAN sees a meaningful label.
    void updateTrackProperties (const TrackProperties& props) override
    {
        const juce::ScopedLock sl (paramLock);
        trackName = props.name.value_or (juce::String());
        trackInfoDirty = true;
    }

    // Poll for project-name changes (REAPER's project name can change
    // mid-session via Save As). Editor calls this from its 2 Hz timer.
    void pollHostState()
    {
        const auto proj = reaperIntegration.getProjectName();
        bool needRefresh = false;
        {
            const juce::ScopedLock sl (paramLock);
            if (proj != currentProject || trackInfoDirty)
            {
                currentProject = proj;
                trackInfoDirty = false;
                needRefresh = true;
            }
        }
        if (needRefresh) refreshSenderAdvertise();
    }

    // Telemetry for the editor.
    uint64_t getSentPackets() const noexcept { return stream.getSentPackets(); }
    uint64_t getSentBytes()   const noexcept { return stream.getSentBytes();   }
    uint64_t getOverruns()    const noexcept { return stream.getOverruns();    }
    uint32_t getSenderUid()   const noexcept { return stream.getSenderUid();   }
    bool     hasActiveTarget() const noexcept { return stream.getActiveTargetCount() > 0; }

    // Live audio level meters (input bus, what we're sending). Editor reads.
    mcfx::net::MeterBank& getMeters() noexcept { return meters; }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

   #if MCFX_MULTICHANNEL_BUILD
    MCFX_MULTICHANEL_APPLY_BUS_LAYOUTS_OVERRIDE
   #endif

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    // Tell the editor when the bus layout shifts so it can rebuild meters
    // / channel-count UI without the user reopening the FX chain.
    void numChannelsChanged() override { sendChangeMessage(); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ----- Auto-reconnect on project load ----------------------------------

    // User-visible toggle persisted in plugin state. When off,
    // setStateInformation skips arming the auto-reconnect retry loop.
    void setAutoReconnectEnabled (bool on);
    bool isAutoReconnectEnabled() const noexcept { return autoReconnectEnabled.load (std::memory_order_acquire); }

    // Editor uses these to render a "Reconnecting to N peers… X s" banner.
    int getArmedPeerCount() const;
    int getAutoReconnectSecondsRemaining() const;

private:
    void applyTargetIfChanged();

    void refreshSenderAdvertise();

    // Cancel any in-flight auto-reconnect attempt. Called from any user-
    // driven connect/disconnect to hand control back to the user.
    void cancelAutoReconnect();

    // juce::Timer — drives the 1 Hz auto-reconnect tick on the message
    // thread. Only running while armedPeers is non-empty.
    void timerCallback() override;

    // Look up the current Discovery service matching wireUid. Returns
    // nullopt if no Bonjour entry currently has that UID.
    struct BonjourHint
    {
        juce::String host;     // Bonjour-advertised computer name
        juce::String project;
        juce::String track;
    };
    BonjourHint lookupBonjourHint (std::uint32_t wireUid) const;

public:
    // VST3-only hook: hand REAPER our integration object so we can pull
    // the project name. JUCE calls into this once after construction.
    juce::VST3ClientExtensions* getVST3ClientExtensions() override
    {
        return &reaperIntegration;
    }

private:
    mcfx::net::SendStream stream;
    std::unique_ptr<mcfx::net::Discovery> discovery;          // browse receivers
    std::unique_ptr<mcfx::net::Discovery> senderAdvertiser;   // publish self as sender
    mcfx::net::ReaperVST3Integration reaperIntegration;
    juce::String currentProject;
    juce::String stableUid;
    juce::String trackName;
    bool         trackInfoDirty = true;

    mutable juce::CriticalSection paramLock;
    juce::String sendHost;
    int          sendPort = 0;
    bool         targetDirty = false;
    // 0 = "use full bus channel count". User-set value persists across
    // bus-width changes, so loading a project with a wider bus than the
    // configured cap still respects the cap.
    int          sendChannelsCap = 0;

    int currentBlockSize = 0;
    double currentSampleRate = 48000.0;

    mcfx::net::MeterBank meters;

    // ----- Auto-reconnect state -------------------------------------------

    // One entry per peer the previous session was connected to. Drained as
    // each entry's underlying Target reaches Active, or as the 60 s window
    // expires. All access on the message thread under armedLock.
    struct ArmedPeer
    {
        juce::String  host;
        int           port = 0;
        BonjourHint   hint;            // empty fields if not Bonjour-paired
        juce::Time    armedAt;
        std::uint32_t currentWireUid = 0; // last UID we resolved via Bonjour
    };
    mutable juce::CriticalSection armedLock;
    std::vector<ArmedPeer> armedPeers;

    // Captured at the moment the user clicks Connect on a Bonjour-discovered
    // row, keyed by the resulting target's (host, port). State save reads
    // this so the next session can re-arm by Bonjour identity if the peer's
    // ephemeral IP/port has drifted.
    mutable juce::CriticalSection bonjourHintsLock;
    std::map<std::pair<juce::String, int>, BonjourHint> lastBonjour;

    std::atomic<bool> autoReconnectEnabled { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (McfxSendAudioProcessor)
};
