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

#pragma once

#include "JuceHeader.h"
#include "PluginProcessor.h"
#include "mcfx_net_meter.h"

#include <unordered_map>

class McfxReceiveAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit McfxReceiveAudioProcessorEditor (McfxReceiveAudioProcessor& p);
    ~McfxReceiveAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    enum class RowState : std::uint8_t
    {
        Discovered, // not connected (gray dot)
        Inviting,   // invite in flight  (yellow dot)
        Active,     // connected, audio flowing (green dot)
        Rejected,   // ack came back rejected (red dot)
        Timeout,    // no ack within deadline (orange dot)
    };

    // Unified row used by the Local-network sender list. Each row is
    // either a Bonjour-discovered sender (with possibly an accepted-set
    // entry attached), or an accepted entry without a Bonjour match
    // (manual Direct-IP path, or sender that's gone from the network
    // but we still hold the connection).
    struct UnifiedRow
    {
        juce::String host;          // computer name (Bonjour) or "(direct)"
        juce::String project;
        juce::String track;
        juce::String ip;            // dotted-quad
        int          port = 0;
        int          channels = 0;
        juce::String format;
        RowState     state = RowState::Discovered;
        mcfx::net::InviteAckReason rejectReason = mcfx::net::IAR_OK;
        bool         viaBonjour = true;
    };

private:
    void timerCallback() override;
    void applyDirectIp();
    void rebuildUnifiedRows();
    void connectSelected();
    void disconnectSelected();
    bool currentSelectionIsConnected() const;

    McfxReceiveAudioProcessor& processor;

    juce::Label titleLabel;

    // --- Settings rows --------------------------------------------------
    juce::Label  jitterLabel { {}, "Jitter floor (ms):" };
    juce::Slider jitterSlider;

    juce::Label    pwLabel { {}, "Password (optional):" };
    juce::TextEditor pwEditor;

    juce::Label boundLabel;
    juce::Label connectionStatusLabel;

    // --- Tabbed connect panel ------------------------------------------

    // Single unified list combining discovered + accepted senders. Status
    // dot encodes RowState. Double-click toggles connect/disconnect.
    class UnifiedSendersList : public juce::ListBox, public juce::ListBoxModel
    {
    public:
        UnifiedSendersList() = default;
        void setEditor (McfxReceiveAudioProcessorEditor* e) { editor = e; setModel (this); }
        std::vector<UnifiedRow> rows;

        int getNumRows() override { return (int) rows.size(); }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    private:
        McfxReceiveAudioProcessorEditor* editor = nullptr;
    };

    // "Local network" tab content.
    class LocalNetworkTab : public juce::Component
    {
    public:
        LocalNetworkTab() = default;
        void setEditor (McfxReceiveAudioProcessorEditor* e) { editor = e; }
        void resized() override;

        juce::Label    headerLabel { {}, "Senders found on this network:" };
        juce::TextEditor filterEditor;
        UnifiedSendersList list;
    private:
        McfxReceiveAudioProcessorEditor* editor = nullptr;
    };

    // "Direct IP" tab content — manual host:port entry.
    class DirectIpTab : public juce::Component
    {
    public:
        DirectIpTab() = default;
        void setEditor (McfxReceiveAudioProcessorEditor* e) { editor = e; }
        void resized() override;

        juce::Label    headerLabel { {}, "Type a sender's IP and listening UDP port:" };
        juce::Label    hostLabel { {}, "Host:" };
        juce::TextEditor hostEditor;
        juce::Label    portLabel { {}, "Port:" };
        juce::TextEditor portEditor;
        juce::TextButton connectButton { "Connect" };
    private:
        McfxReceiveAudioProcessorEditor* editor = nullptr;
    };

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    LocalNetworkTab localTab;
    DirectIpTab     directTab;

    juce::TextButton connectButton    { "Connect" };
    juce::TextButton disconnectButton { "Disconnect" };
    juce::TextButton refreshButton    { "Refresh" };

    // --- Per-peer telemetry --------------------------------------------
    juce::Label peersLabel;
    juce::Label networkLabel;

    struct History
    {
        uint64_t lastBytes   = 0;
        uint64_t lastPackets = 0;
        double   ratePerSec  = 0.0;
        double   ppsRate     = 0.0;
    };
    std::unordered_map<uint32_t, History> rateHistory;
    double lastSampleTimeSec = 0.0;

    // --- Bottom audio meter --------------------------------------------
    mcfx::net::MultiChannelMeter meter;

    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<juce::ResizableCornerComponent> resizer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (McfxReceiveAudioProcessorEditor)
};
