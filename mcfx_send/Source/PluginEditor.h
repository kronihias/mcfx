/*
 ==============================================================================

 This file is part of the mcfx (Multichannel Effects) plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com

 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)

 Details of these licenses can be found at: www.gnu.org/licenses

 ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "PluginProcessor.h"
#include "mcfx_net_meter.h"

class McfxSendAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit McfxSendAudioProcessorEditor (McfxSendAudioProcessor& p);
    ~McfxSendAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    enum class RowState : std::uint8_t
    {
        Discovered, Inviting, Active, Rejected, Timeout,
    };

    // Unified row mirroring the receiver's design: discovered receivers
    // joined with the sender's own targets list (deduped by host:port).
    struct UnifiedRow
    {
        juce::String host;          // computer name or "(direct)"
        juce::String project;
        juce::String track;
        juce::String ip;
        int          port = 0;
        int          channels = 0;
        RowState     state = RowState::Discovered;
        mcfx::net::InviteAckReason rejectReason = mcfx::net::IAR_OK;
        bool         viaBonjour = true;
    };

private:
    void timerCallback() override;
    void rebuildUnifiedRows();
    void connectSelected();
    void disconnectSelected();
    void applyDirectIp();
    bool currentSelectionIsConnected() const;
    void rebuildChansCombo (int busWidth);

    McfxSendAudioProcessor& processor;

    juce::Label titleLabel;

    juce::Label    chansLabel { {}, "Channels to send:" };
    juce::ComboBox chansCombo;
    int            lastBusWidth = -1;   // for detecting bus changes in timer

    juce::Label    formatLabel { {}, "Format:" };
    juce::ComboBox formatCombo;

    juce::Label    pwLabel { {}, "Password (optional):" };
    juce::TextEditor pwEditor;

    juce::Label statusLabel;
    juce::Label networkLabel;

    // Single unified targets list. Status dot encodes RowState; double-
    // click to toggle connect/disconnect.
    class UnifiedTargetsList : public juce::ListBox, public juce::ListBoxModel
    {
    public:
        UnifiedTargetsList() = default;
        void setEditor (McfxSendAudioProcessorEditor* e) { editor = e; setModel (this); }
        std::vector<UnifiedRow> rows;

        int getNumRows() override { return (int) rows.size(); }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    private:
        McfxSendAudioProcessorEditor* editor = nullptr;
    };

    class LocalNetworkTab : public juce::Component
    {
    public:
        void resized() override;
        juce::Label    headerLabel { {}, "Receivers found on this network:" };
        juce::TextEditor filterEditor;
        UnifiedTargetsList list;
    };

    class DirectIpTab : public juce::Component
    {
    public:
        void setEditor (McfxSendAudioProcessorEditor* e) { editor = e; }
        void resized() override;

        juce::Label    headerLabel { {}, "Type a receiver's IP and listening UDP port:" };
        juce::Label    hostLabel { {}, "Host:" };
        juce::TextEditor hostEditor;
        juce::Label    portLabel { {}, "Port:" };
        juce::TextEditor portEditor;
        juce::TextButton connectButton { "Connect" };
    private:
        McfxSendAudioProcessorEditor* editor = nullptr;
    };

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    LocalNetworkTab localTab;
    DirectIpTab     directTab;

    juce::TextButton connectButton    { "Connect" };
    juce::TextButton disconnectButton { "Disconnect" };
    juce::TextButton refreshButton    { "Refresh" };

    mcfx::net::MultiChannelMeter meter;

    uint64_t lastPackets = 0;
    uint64_t lastBytes   = 0;
    double   lastSampleTimeSec = 0.0;

    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<juce::ResizableCornerComponent> resizer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (McfxSendAudioProcessorEditor)
};
