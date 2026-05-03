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

#include "PluginEditor.h"

#define MCFX_NET_Q(x)     #x
#define MCFX_NET_QUOTE(x) MCFX_NET_Q(x)

// ---------------------------------------------------------------------------
// Tab content panels
// ---------------------------------------------------------------------------

void McfxSendAudioProcessorEditor::LocalNetworkTab::resized()
{
    auto a = getLocalBounds().reduced (6);
    headerLabel.setBounds (a.removeFromTop (20));
    a.removeFromTop (4);
    filterEditor.setBounds (a.removeFromTop (24));
    a.removeFromTop (4);
    list.setBounds (a);
}

void McfxSendAudioProcessorEditor::DirectIpTab::resized()
{
    auto a = getLocalBounds().reduced (6);
    headerLabel.setBounds (a.removeFromTop (20));
    a.removeFromTop (8);
    auto row = a.removeFromTop (28);
    hostLabel.setBounds (row.removeFromLeft (50));
    hostEditor.setBounds (row.removeFromLeft (juce::jmax (180, row.getWidth() - 200)));
    row.removeFromLeft (8);
    portLabel.setBounds (row.removeFromLeft (40));
    portEditor.setBounds (row.removeFromLeft (70));
    row.removeFromLeft (8);
    connectButton.setBounds (row.removeFromLeft (90));
}

// ---------------------------------------------------------------------------
// Unified targets list
// ---------------------------------------------------------------------------

void McfxSendAudioProcessorEditor::UnifiedTargetsList::paintListBoxItem (
    int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) rows.size()) return;
    const auto& r = rows[(size_t) row];

    if (selected) g.fillAll (juce::Colour (0xff2c4a78));

    juce::Colour dot;
    switch (r.state)
    {
        case RowState::Discovered: dot = juce::Colour (0xff707070); break;
        case RowState::Inviting:   dot = juce::Colour (0xffd9b438); break;
        case RowState::Active:     dot = juce::Colour (0xff4ec96b); break;
        case RowState::Rejected:   dot = juce::Colour (0xffe04040); break;
        case RowState::Timeout:    dot = juce::Colour (0xffd97a38); break;
    }
    const float dotR = 6.0f;
    g.setColour (dot);
    g.fillEllipse (8.0f, h * 0.5f - dotR, dotR * 2.0f, dotR * 2.0f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));

    juce::String text = r.host;
    if (r.project.isNotEmpty()) text += " [" + r.project + "]";
    if (r.track.isNotEmpty())   text += "  /  " + r.track;
    text += "    " + r.ip + ":" + juce::String (r.port);
    if (r.channels > 0) text += "    " + juce::String (r.channels) + " ch";
    if (r.state == RowState::Rejected
        && r.rejectReason == mcfx::net::IAR_BAD_PASSWORD)
        text += "    [bad password]";

    g.drawText (text, 28, 0, w - 36, h, juce::Justification::centredLeft, true);
}

void McfxSendAudioProcessorEditor::UnifiedTargetsList::listBoxItemDoubleClicked (
    int row, const juce::MouseEvent&)
{
    if (! editor) return;
    if (row < 0 || row >= (int) rows.size()) return;
    selectRow (row);
    const auto& r = rows[(size_t) row];
    if (r.state == RowState::Active || r.state == RowState::Inviting)
        editor->disconnectSelected();
    else
        editor->connectSelected();
}

// ---------------------------------------------------------------------------
// Editor lifecycle
// ---------------------------------------------------------------------------

McfxSendAudioProcessorEditor::McfxSendAudioProcessorEditor (McfxSendAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&getLookAndFeel());

    addAndMakeVisible (titleLabel);
    titleLabel.setText ("mcfx_send", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (18.0f).withStyle ("Bold")));
    titleLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (chansLabel);
    addAndMakeVisible (chansCombo);
    chansCombo.setTooltip ("Cap the number of channels actually sent on the wire. "
                            "'All bus channels' sends whatever the bus exposes.");
    rebuildChansCombo (processor.getMainBusNumInputChannels());
    chansCombo.onChange = [this]() {
        const int id = chansCombo.getSelectedId();
        processor.setSendChannels (id <= 1 ? 0 : (id - 1));
    };

    addAndMakeVisible (formatLabel);
    addAndMakeVisible (formatCombo);
    formatCombo.addItem ("PCM 16-bit",       static_cast<int> (mcfx::net::SF_INT16));
    formatCombo.addItem ("PCM 24-bit",       static_cast<int> (mcfx::net::SF_INT24));
    formatCombo.addItem ("PCM 32-bit float", static_cast<int> (mcfx::net::SF_FLOAT32));
    formatCombo.setSelectedId (static_cast<int> (processor.getWireFormat()),
                                juce::dontSendNotification);
    formatCombo.onChange = [this]() {
        const int id = formatCombo.getSelectedId();
        if (id >= 1 && id <= 3)
            processor.setWireFormat (static_cast<mcfx::net::SampleFormat> (id));
    };

    addAndMakeVisible (pwLabel);
    addAndMakeVisible (pwEditor);
    pwEditor.setPasswordCharacter ((juce_wchar) 0x2022);
    pwEditor.setText (processor.getPassword(), juce::dontSendNotification);
    pwEditor.setTextToShowWhenEmpty ("none", juce::Colours::grey);
    pwEditor.onFocusLost = [this]() { processor.setPassword (pwEditor.getText()); };
    pwEditor.onReturnKey = [this]() { processor.setPassword (pwEditor.getText()); };

    addAndMakeVisible (boundLabel);
    boundLabel.setJustificationType (juce::Justification::centredLeft);
    boundLabel.setTooltip (
        "Local UDP port this sender is bound to. Hand this address to\n"
        "the receiving peer if it can't see this sender via Bonjour\n"
        "(different subnet, mDNS blocked, etc.) — they paste it into\n"
        "their Direct IP tab and click Connect.");

    addAndMakeVisible (statusLabel);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setTooltip (
        "Active targets summary. Each accepted receiver in the targets\n"
        "list gets one packet per audio period; bandwidth scales\n"
        "linearly with the target count. uid=... is this sender's\n"
        "stable Bonjour identifier, useful when looking at packet\n"
        "captures or matching log lines across both sides.");

    addAndMakeVisible (networkLabel);
    networkLabel.setJustificationType (juce::Justification::centredLeft);
    networkLabel.setTooltip (
        "Aggregate sender stats (smoothed over ~0.5 s).\n\n"
        "MB/s, kB/s, B/s\n"
        "    Outbound throughput across all targets combined.\n\n"
        "<N> pkt/s\n"
        "    Packet send rate. Pack as many frames per packet as fit\n"
        "    in a 1400-byte UDP datagram, send N packets per audio\n"
        "    period. Multiply by the number of active targets for\n"
        "    the per-target rate.\n\n"
        "overruns <N>\n"
        "    Times the audio thread couldn't push into the SPSC ring\n"
        "    because the SendThread hadn't drained fast enough. A\n"
        "    non-zero count under sustained load means the network\n"
        "    can't keep up — try a lower channel count or 16-bit PCM.");
    networkLabel.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                                          12.0f, juce::Font::plain)));
    networkLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc8c8c8));

    // Tabs.
    localTab.list.setEditor (this);
    localTab.list.setRowHeight (22);
    localTab.list.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff181818));
    localTab.list.setMultipleSelectionEnabled (false);
    localTab.filterEditor.setTextToShowWhenEmpty ("filter (host / project / track / IP)",
                                                   juce::Colours::grey);
    localTab.filterEditor.onTextChange = [this]() { rebuildUnifiedRows(); };
    localTab.addAndMakeVisible (localTab.headerLabel);
    localTab.addAndMakeVisible (localTab.filterEditor);
    localTab.addAndMakeVisible (localTab.list);

    directTab.setEditor (this);
    directTab.addAndMakeVisible (directTab.headerLabel);
    directTab.addAndMakeVisible (directTab.hostLabel);
    directTab.addAndMakeVisible (directTab.hostEditor);
    directTab.hostEditor.setTextToShowWhenEmpty ("e.g. 192.168.1.42", juce::Colours::grey);
    directTab.addAndMakeVisible (directTab.portLabel);
    directTab.addAndMakeVisible (directTab.portEditor);
    directTab.portEditor.setTextToShowWhenEmpty ("port", juce::Colours::grey);
    directTab.portEditor.setInputRestrictions (5, "0123456789");
    directTab.addAndMakeVisible (directTab.connectButton);
    directTab.connectButton.onClick = [this]() { applyDirectIp(); };
    directTab.hostEditor.onReturnKey = [this]() { applyDirectIp(); };
    directTab.portEditor.onReturnKey = [this]() { applyDirectIp(); };

    addAndMakeVisible (tabs);
    tabs.setTabBarDepth (28);
    tabs.addTab ("Local network", juce::Colour (0xff242429), &localTab,  false);
    tabs.addTab ("Direct IP",     juce::Colour (0xff242429), &directTab, false);

    addAndMakeVisible (connectButton);
    connectButton.onClick = [this]() { connectSelected(); };
    addAndMakeVisible (disconnectButton);
    disconnectButton.onClick = [this]() { disconnectSelected(); };
    addAndMakeVisible (refreshButton);
    refreshButton.onClick = [this]() { rebuildUnifiedRows(); };

    processor.onDiscoveredReceiversChanged = [this]() { rebuildUnifiedRows(); };

    addAndMakeVisible (meter);
    meter.setMeterBank (&processor.getMeters());

    constrainer.setMinimumSize (560, 720);
    constrainer.setMaximumSize (1600, 1400);
    setResizable (true, true);
    setConstrainer (&constrainer);
    resizer = std::make_unique<juce::ResizableCornerComponent> (this, &constrainer);
    addAndMakeVisible (*resizer);

    setSize (660, 800);
    rebuildUnifiedRows();
    timerCallback();
    startTimerHz (2);
}

McfxSendAudioProcessorEditor::~McfxSendAudioProcessorEditor()
{
    stopTimer();
    processor.onDiscoveredReceiversChanged = nullptr;
}

// ---------------------------------------------------------------------------
// Channels-to-send combo
// ---------------------------------------------------------------------------

void McfxSendAudioProcessorEditor::rebuildChansCombo (int busWidth)
{
    if (busWidth == lastBusWidth) return;
    lastBusWidth = busWidth;

    // Preserve the currently-selected cap. setSendChannels caps to bus
    // width inside the processor, but we want the UI to reflect the
    // cap-after-clamp so the user sees a consistent selected item even
    // if they previously asked for more than the bus now provides.
    const int existingCap = juce::jlimit (0, 64, processor.getSendChannels());

    chansCombo.clear (juce::dontSendNotification);
    chansCombo.addItem ("All bus channels", 1);
    const int maxN = juce::jlimit (0, 64, busWidth);
    for (int n = 1; n <= maxN; ++n)
        chansCombo.addItem (juce::String (n), n + 1);

    // Snap the selection to whatever the new max permits.
    int desired = existingCap;
    if (desired > maxN) desired = maxN;
    chansCombo.setSelectedId (desired == 0 ? 1 : (desired + 1),
                              juce::dontSendNotification);
    if (desired != existingCap)
        processor.setSendChannels (desired);
}

// ---------------------------------------------------------------------------
// User actions
// ---------------------------------------------------------------------------

void McfxSendAudioProcessorEditor::applyDirectIp()
{
    const auto host = directTab.hostEditor.getText().trim();
    const int port = directTab.portEditor.getText().trim().getIntValue();
    if (host.isEmpty() || port < 1 || port > 65535)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
            "Invalid host or port",
            "Enter a valid IPv4 host and a UDP port between 1 and 65535.",
            "OK");
        return;
    }
    processor.addTarget (host, port);
}

void McfxSendAudioProcessorEditor::connectSelected()
{
    const int row = localTab.list.getSelectedRow();
    if (row < 0 || row >= (int) localTab.list.rows.size()) return;
    const auto& r = localTab.list.rows[(size_t) row];
    if (r.ip.isEmpty() || r.port <= 0) return;
    // Pass wireUid for Bonjour-discovered rows (=0 for Direct-IP) so
    // the target entry pairs against the discovered row from the very
    // first refresh, no transient "(direct)" while the ACK is in flight.
    processor.addTarget (r.ip, r.port, r.wireUid);
}

void McfxSendAudioProcessorEditor::disconnectSelected()
{
    const int row = localTab.list.getSelectedRow();
    if (row < 0 || row >= (int) localTab.list.rows.size()) return;
    const auto& r = localTab.list.rows[(size_t) row];
    if (r.ip.isEmpty() || r.port <= 0) return;
    processor.removeTarget (r.ip, r.port);
}

bool McfxSendAudioProcessorEditor::currentSelectionIsConnected() const
{
    const int row = localTab.list.getSelectedRow();
    if (row < 0 || row >= (int) localTab.list.rows.size()) return false;
    const auto& r = localTab.list.rows[(size_t) row];
    return r.state == RowState::Active || r.state == RowState::Inviting;
}

void McfxSendAudioProcessorEditor::rebuildUnifiedRows()
{
    const auto discovered = processor.getDiscoveredReceivers();
    const auto targets    = processor.getTargets();
    const auto filter     = localTab.filterEditor.getText().trim().toLowerCase();

    auto filterPasses = [&filter] (const UnifiedRow& r) {
        if (filter.isEmpty()) return true;
        return r.host   .toLowerCase().contains (filter)
            || r.project.toLowerCase().contains (filter)
            || r.track  .toLowerCase().contains (filter)
            || r.ip                  .contains (filter);
    };

    auto stateFromTarget = [] (mcfx::net::SendStream::TargetState s) {
        switch (s)
        {
            case mcfx::net::SendStream::TargetState::Pending:  return RowState::Inviting;
            case mcfx::net::SendStream::TargetState::Active:   return RowState::Active;
            case mcfx::net::SendStream::TargetState::Rejected: return RowState::Rejected;
            case mcfx::net::SendStream::TargetState::Timeout:  return RowState::Timeout;
        }
        return RowState::Discovered;
    };

    std::vector<UnifiedRow> rows;
    rows.reserve (discovered.size() + targets.size());

    // Pair discovered Bonjour entries with our targets list by wire UID
    // (Bonjour TXT "wuid" == receiver's stream UID, same value the
    // receiver writes into CommonHeader.sender on its outbound INVITEs).
    // UID is the only identity — host:port can drift across re-binds
    // and multi-homed source-IP differences.
    //
    // Direct-IP target entries (created via the user typing a host in
    // the Direct IP tab) have receiverUid=0 until the first INVITE_ACK
    // arrives; they correctly DON'T pair against any discovered row
    // and fall through to the "(direct)" rendering below.
    std::vector<bool> targetsConsumed (targets.size(), false);
    for (const auto& d : discovered)
    {
        UnifiedRow r;
        r.host       = d.host;
        r.project    = d.project;
        r.track      = d.track;
        r.ip         = d.ip.toString();
        r.port       = d.port;
        r.channels   = d.channels;
        r.wireUid    = d.wireUid;
        r.viaBonjour = true;
        r.state      = RowState::Discovered;

        for (size_t i = 0; i < targets.size(); ++i)
        {
            const auto& t = targets[i];
            if (t.receiverUid != 0 && t.receiverUid == d.wireUid)
            {
                r.state         = stateFromTarget (t.state);
                r.rejectReason  = t.rejectReason;
                targetsConsumed[i] = true;
                break;
            }
        }
        if (filterPasses (r)) rows.push_back (std::move (r));
    }
    for (size_t i = 0; i < targets.size(); ++i)
    {
        if (targetsConsumed[i]) continue;
        const auto& t = targets[i];
        UnifiedRow r;
        r.host       = "(direct)";
        r.ip         = t.host;
        r.port       = t.port;
        r.viaBonjour = false;
        r.state      = stateFromTarget (t.state);
        r.rejectReason = t.rejectReason;
        if (filterPasses (r)) rows.push_back (std::move (r));
    }

    localTab.list.rows = std::move (rows);
    localTab.list.updateContent();
    localTab.list.repaint();

    const bool hasSelection = localTab.list.getSelectedRow() >= 0;
    const bool connected    = currentSelectionIsConnected();
    connectButton.setEnabled    (hasSelection && ! connected);
    disconnectButton.setEnabled (hasSelection && connected);
}

// ---------------------------------------------------------------------------
// Painting + layout
// ---------------------------------------------------------------------------

void McfxSendAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    g.setColour (juce::Colours::grey);
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::plain)));
    g.drawText ("mcfx_send v" MCFX_NET_QUOTE (VERSION) "  built " __DATE__ " " __TIME__,
                getWidth() - 360, getHeight() - 14, 340, 12,
                juce::Justification::bottomRight, true);
}

void McfxSendAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    titleLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (6);

    // Channels-to-send and Format share one row to save vertical space —
    // the chans combo only ever holds small numbers so 80 px is plenty;
    // the format combo needs room for "PCM 32-bit float".
    auto rowChansFmt = area.removeFromTop (28);
    chansLabel.setBounds  (rowChansFmt.removeFromLeft (130));
    chansCombo.setBounds  (rowChansFmt.removeFromLeft (80));
    rowChansFmt.removeFromLeft (16);
    formatLabel.setBounds (rowChansFmt.removeFromLeft (70));
    formatCombo.setBounds (rowChansFmt.removeFromLeft (160));
    area.removeFromTop (6);

    auto rowPw = area.removeFromTop (28);
    pwLabel.setBounds (rowPw.removeFromLeft (140));
    pwEditor.setBounds (rowPw.removeFromLeft (200));
    area.removeFromTop (6);

    // Listening-port label first (mirrors the receiver's boundLabel
    // position above its connectionStatusLabel) so the user can grab
    // the sender's UDP port for manual / Direct-IP peering.
    boundLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (2);
    // Status banner sits just above the tabs (mirrors the receiver's
    // connectionStatusLabel position). Aggregate-stats networkLabel
    // moves to the bottom strip, between the buttons and the meter,
    // so the tab list is the visually dominant element on both sides.
    statusLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);

    // Bottom strip: meter pinned at the bottom, then per-stream stats,
    // then the Connect / Disconnect / Refresh button row. Identical
    // ordering to the receiver editor.
    constexpr int kMeterHeight = 140;
    auto meterRow = area.removeFromBottom (kMeterHeight);
    meter.setBounds (meterRow);
    area.removeFromBottom (4);

    networkLabel.setBounds (area.removeFromBottom (20));
    area.removeFromBottom (4);

    auto buttonRow = area.removeFromBottom (32);
    area.removeFromBottom (4);
    connectButton.setBounds    (buttonRow.removeFromLeft (110));
    buttonRow.removeFromLeft (6);
    disconnectButton.setBounds (buttonRow.removeFromLeft (110));
    buttonRow.removeFromLeft (6);
    refreshButton.setBounds    (buttonRow.removeFromLeft (90));

    // Whatever's left in the middle is the tabbed connect panel; the
    // inner ListBox auto-scrolls when the row count exceeds the height.
    tabs.setBounds (area);

    if (resizer)
        resizer->setBounds (getWidth() - 16, getHeight() - 16, 16, 16);
}

// ---------------------------------------------------------------------------
// Timer
// ---------------------------------------------------------------------------

void McfxSendAudioProcessorEditor::timerCallback()
{
    processor.pollHostState();

    // Track the host's bus width so the "Channels to send" combo can't
    // offer values that exceed what the input bus actually provides.
    rebuildChansCombo (processor.getMainBusNumInputChannels());

    // Local listen-port label. Updated every tick because the port can
    // change across releaseResources / prepareToPlay cycles (the host
    // tearing down and re-arming the plugin), and we want the displayed
    // value to follow the current binding.
    {
        const int bound = processor.getListenPort();
        boundLabel.setText ((bound > 0)
            ? "Listening on UDP port " + juce::String (bound)
            : juce::String ("Not bound"),
            juce::dontSendNotification);
        boundLabel.setColour (juce::Label::textColourId,
            (bound > 0) ? juce::Colours::white : juce::Colours::orange);
    }

    const int active = processor.getActiveTargetCount();
    statusLabel.setText (active > 0
        ? juce::String (active) + " active target"
            + (active == 1 ? juce::String() : juce::String ("s"))
            + "  uid=" + juce::String::toHexString ((juce::int64) processor.getSenderUid())
        : juce::String ("No active targets  uid=")
            + juce::String::toHexString ((juce::int64) processor.getSenderUid()),
        juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId,
                           active > 0 ? juce::Colours::white : juce::Colours::orange);

    const auto pkts  = processor.getSentPackets();
    const auto bytes = processor.getSentBytes();
    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    juce::String net;
    if (lastSampleTimeSec > 0.0)
    {
        const double dt = now - lastSampleTimeSec;
        if (dt > 0.05)
        {
            const double pps = (pkts >= lastPackets) ? (pkts - lastPackets) / dt : 0.0;
            const double bps = (bytes >= lastBytes)  ? (bytes - lastBytes) / dt  : 0.0;
            auto fmtRate = [] (double v) {
                if (v >= 1.0e6) return juce::String (v / 1.0e6, 2) + " MB/s";
                if (v >= 1.0e3) return juce::String (v / 1.0e3, 1) + " kB/s";
                return juce::String ((int) v) + " B/s";
            };
            net = fmtRate (bps) + "  " + juce::String ((int) pps) + " pkt/s"
                + "  overruns " + juce::String (processor.getOverruns());
        }
    }
    if (net.isEmpty()) net = "(idle)";
    networkLabel.setText (net, juce::dontSendNotification);

    lastPackets = pkts;
    lastBytes   = bytes;
    lastSampleTimeSec = now;

    rebuildUnifiedRows();
}
