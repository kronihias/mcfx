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

 mcfx_receive — multichannel UDP audio receiver. Mixes one or more
 mcfx_send streams into the output bus. LAN-first; PCM-only in v1.

 ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "mcfx_buses.h"
#include "mcfx_net_recv.h"
#include "mcfx_net_discovery.h"
#include "mcfx_net_reaper.h"
#include "mcfx_net_meter.h"

#include <memory>
#include <vector>

class McfxReceiveAudioProcessor : public juce::AudioProcessor,
                                  public juce::ChangeBroadcaster
{
public:
    McfxReceiveAudioProcessor();
    ~McfxReceiveAudioProcessor() override;

    // Editor-facing: change the listening UDP port. 0 = ephemeral. Returns
    // the actually-bound port. Re-bind only happens if the port differs.
    int  setListenPort (int port);
    int  getListenPort() const;
    int  getBoundPort()  const noexcept { return stream.getBoundPort(); }

    // Jitter buffer floor in ms. Adaptive auto-extra adds on top per-peer.
    void setJitterFloorMs (int ms) { stream.setJitterFloorMs (ms); }
    int  getJitterFloorMs() const noexcept { return stream.getJitterFloorMs(); }

    // ----- Bidirectional connect -------------------------------------------

    bool inviteSender    (const juce::String& host, int port) { return stream.inviteSender (host, port); }
    void uninviteSender  (const juce::String& host, int port) { stream.uninviteSender (host, port); }
    void uninviteAllSenders() { stream.uninviteAllSenders(); }
    std::vector<mcfx::net::RecvStream::AcceptedSender> getAcceptedSenders() const
    {
        return stream.getAcceptedSenders();
    }

    void setPassword (const juce::String& pw) { stream.setPassword (pw); }
    juce::String getPassword() const { return stream.getPassword(); }

    mcfx::net::RecvStream::LastAttempt getLastAttempt() const { return stream.getLastAttempt(); }
    void clearLastAttempt() { stream.clearLastAttempt(); }

    // Live audio level meter on the mixed-output bus (what the user hears
    // after summing every primed peer).
    mcfx::net::MeterBank& getMeters() noexcept { return meters; }

    // Bonjour-discovered senders. Editor reads each tick.
    std::vector<mcfx::net::Discovery::Service> getDiscoveredSenders() const
    {
        return senderBrowser ? senderBrowser->getServices()
                              : std::vector<mcfx::net::Discovery::Service>{};
    }
    std::function<void()> onDiscoveredSendersChanged;

    // Track-name change from the host. Forwarded into the Bonjour TXT
    // so a sender's UI can label this receiver "Main bus" instead of a
    // bare host:port.
    void updateTrackProperties (const TrackProperties& props) override
    {
        const juce::ScopedLock sl (paramLock);
        trackName = props.name.value_or (juce::String());
        trackInfoDirty = true;
    }

    // Poll REAPER for project-name changes; called from the editor timer.
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
        if (needRefresh) refreshDiscoveryAdvertise();
    }

    // VST3-only hook so REAPER hands us its host application interface.
    juce::VST3ClientExtensions* getVST3ClientExtensions() override
    {
        return &reaperIntegration;
    }

    // Telemetry — message thread only.
    std::vector<mcfx::net::RecvStream::PeerSnapshot> snapshotPeers() const
    {
        return stream.snapshotPeers();
    }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

   #if MCFX_MULTICHANNEL_BUILD
    MCFX_MULTICHANEL_APPLY_BUS_LAYOUTS_OVERRIDE
   #endif

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

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
    void refreshDiscoveryAdvertise();

    mcfx::net::RecvStream stream;
    mutable juce::CriticalSection paramLock;
    int desiredPort = 0;
    juce::String trackName;
    juce::String currentProject;
    bool         trackInfoDirty = true;
    juce::String stableUid;
    mcfx::net::ReaperVST3Integration reaperIntegration;
    mcfx::net::MeterBank meters;

    int currentBlockSize  = 0;
    double currentSampleRate = 48000.0;

    // Lifetime: spans plugin load. Construction kicks off the timer so
    // the advertiser comes up shortly after first prepareToPlay.
    std::unique_ptr<mcfx::net::Discovery> discovery;        // publish self as receiver
    std::unique_ptr<mcfx::net::Discovery> senderBrowser;    // browse senders

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (McfxReceiveAudioProcessor)
};
