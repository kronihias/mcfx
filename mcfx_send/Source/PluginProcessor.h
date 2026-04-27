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
#include <memory>

class McfxSendAudioProcessor : public juce::AudioProcessor,
                               public juce::ChangeBroadcaster
{
public:
    McfxSendAudioProcessor();
    ~McfxSendAudioProcessor() override;

    // Editor-facing: configure the sender's destination. The "Apply" button
    // calls this — it replaces all existing targets with a single one.
    // Empty host clears (Disconnect button uses host="" port=0).
    void setSendTarget (const juce::String& host, int port);
    juce::String getSendHost() const { const juce::ScopedLock sl (paramLock); return sendHost; }
    int          getSendPort() const { const juce::ScopedLock sl (paramLock); return sendPort; }

    // Multi-target additions: clicking a discovered receiver in the editor
    // ADDS that receiver to the targets list (additive), not REPLACES.
    void addTarget    (const juce::String& host, int port) { stream.addTarget (host, port); }
    void removeTarget (const juce::String& host, int port) { stream.removeTarget (host, port); }
    void clearTargets() { stream.clearTargets(); }
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

private:
    void applyTargetIfChanged();

    void refreshSenderAdvertise();

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (McfxSendAudioProcessor)
};
