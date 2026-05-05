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
#include "mcfx_net_socket.h"  // mcfx::net::getLocalIPv4Addresses

#define MCFX_NET_Q(x)     #x
#define MCFX_NET_QUOTE(x) MCFX_NET_Q(x)

// ---------------------------------------------------------------------------
// Tab content panels
// ---------------------------------------------------------------------------

void McfxReceiveAudioProcessorEditor::LocalNetworkTab::resized()
{
    auto a = getLocalBounds().reduced (6);
    headerLabel.setBounds (a.removeFromTop (20));
    a.removeFromTop (4);
    filterEditor.setBounds (a.removeFromTop (24));
    a.removeFromTop (4);
    list.setBounds (a);
}

void McfxReceiveAudioProcessorEditor::DirectIpTab::resized()
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
// Unified senders list (rendered in the Local network tab)
// ---------------------------------------------------------------------------

void McfxReceiveAudioProcessorEditor::UnifiedSendersList::paintListBoxItem (
    int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) rows.size()) return;
    const auto& r = rows[(size_t) row];

    if (selected) g.fillAll (juce::Colour (0xff2c4a78));

    // Status dot — encodes RowState. Drawn left-aligned, vertically
    // centred. Same colour vocabulary as the Active senders list had.
    juce::Colour dot;
    juce::String tooltip;
    switch (r.state)
    {
        case RowState::Discovered: dot = juce::Colour (0xff707070); tooltip = "discovered"; break;
        case RowState::Inviting:   dot = juce::Colour (0xffd9b438); tooltip = "inviting...";  break;
        case RowState::Active:     dot = juce::Colour (0xff4ec96b); tooltip = "connected";    break;
        case RowState::Rejected:   dot = juce::Colour (0xffe04040); tooltip = "rejected";     break;
        case RowState::Timeout:    dot = juce::Colour (0xffd97a38); tooltip = "no response";  break;
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
    if (! r.format.isEmpty()) text += "  " + r.format;
    if (r.state == RowState::Rejected
        && r.rejectReason == mcfx::net::IAR_BAD_PASSWORD)
        text += "    [bad password]";

    g.drawText (text, 28, 0, w - 36, h, juce::Justification::centredLeft, true);
}

void McfxReceiveAudioProcessorEditor::UnifiedSendersList::listBoxItemDoubleClicked (
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

McfxReceiveAudioProcessorEditor::McfxReceiveAudioProcessorEditor (McfxReceiveAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&getLookAndFeel());

    addAndMakeVisible (titleLabel);
    titleLabel.setText ("mcfx_receive", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (18.0f).withStyle ("Bold")));
    titleLabel.setJustificationType (juce::Justification::centredLeft);

    // --- Settings rows ---
    addAndMakeVisible (jitterLabel);
    addAndMakeVisible (jitterSlider);
    jitterSlider.setRange (0.0, 500.0, 1.0);
    jitterSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    jitterSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 22);
    jitterSlider.setTextValueSuffix (" ms");
    jitterSlider.setValue ((double) processor.getJitterFloorMs(), juce::dontSendNotification);
    jitterSlider.onValueChange = [this]() {
        processor.setJitterFloorMs ((int) jitterSlider.getValue());
    };

    addAndMakeVisible (pwLabel);
    addAndMakeVisible (pwEditor);
    pwEditor.setPasswordCharacter ((juce_wchar) 0x2022);
    pwEditor.setText (processor.getPassword(), juce::dontSendNotification);
    pwEditor.setTextToShowWhenEmpty ("none", juce::Colours::grey);
    pwEditor.onFocusLost = [this]() { processor.setPassword (pwEditor.getText()); };
    pwEditor.onReturnKey = [this]() { processor.setPassword (pwEditor.getText()); };

    addAndMakeVisible (autoReconnectToggle);
    autoReconnectToggle.setTooltip (
        "When a project is reopened, automatically retry the last\n"
        "connected sender(s) for ~60 s, so the audio link comes\n"
        "back as soon as the remote side is up. The retry stops the\n"
        "moment you click Connect or Disconnect on any peer.");
    autoReconnectToggle.setToggleState (processor.isAutoReconnectEnabled(),
                                        juce::dontSendNotification);
    autoReconnectToggle.onClick = [this]() {
        processor.setAutoReconnectEnabled (autoReconnectToggle.getToggleState());
    };

    addAndMakeVisible (boundLabel);
    boundLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (connectionStatusLabel);
    connectionStatusLabel.setJustificationType (juce::Justification::centredLeft);

    // --- Tabs ---
    localTab.setEditor (this);
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

    processor.onDiscoveredSendersChanged = [this]() { rebuildUnifiedRows(); };

    addAndMakeVisible (peersLabel);
    peersLabel.setJustificationType (juce::Justification::centredLeft);
    peersLabel.setTooltip (
        "Connected peers (one entry per accepted sender). Format:\n"
        "    <channels> ch  (<sender IP>:<sender source port>)\n"
        "Multiple peers are pipe-separated. \"(priming)\" means the\n"
        "ring buffer is still filling to the prefill threshold and no\n"
        "audio is being mixed yet.");

    addAndMakeVisible (networkLabel);
    networkLabel.setJustificationType (juce::Justification::centredLeft);
    networkLabel.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                                          12.0f, juce::Font::plain)));
    networkLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc8c8c8));
    networkLabel.setTooltip (
        "Per-peer network stats (smoothed over ~0.5 s).\n\n"
        "MB/s, kB/s, B/s\n"
        "    Inbound throughput from this sender.\n\n"
        "<N> pkt/s\n"
        "    Packet arrival rate. Senders pack as many frames per\n"
        "    packet as fit in a 1400-byte UDP datagram, so pkt/s\n"
        "    scales with channel count and sample format.\n\n"
        "buf <X>ms (+<Y>ms)\n"
        "    Effective jitter buffer depth. <X> is the user-set\n"
        "    floor; the optional (+<Y>ms) is per-peer auto-headroom\n"
        "    that grows on packet loss / underrun and decays during\n"
        "    clean periods, capped at 200 ms.\n\n"
        "lat ~<X>ms\n"
        "    Estimated total one-way latency: sender block + jitter\n"
        "    buffer + receiver block + resampler group delay. Does\n"
        "    not include actual network transit (would need synced\n"
        "    clocks); on a LAN that's negligible vs. the buffer.\n\n"
        "fill <N> fr\n"
        "    Current ring fill in source frames. Targets prefill +\n"
        "    period/2 in steady state; the adaptive resampler nudges\n"
        "    its ratio to keep this stable as the two clocks drift.\n\n"
        "gap <N>      frames zero-filled to cover packet loss.\n"
        "underrun <N> times the audio thread found the ring empty.\n"
        "reorderDrop <N> packets dropped because they arrived after\n"
        "             a later one had already been written.");

    // --- Audio meter at the bottom ---
    addAndMakeVisible (meter);
    meter.setMeterBank (&processor.getMeters());

    constrainer.setMinimumSize (560, 720);
    constrainer.setMaximumSize (1600, 1400);
    setResizable (true, true);
    setConstrainer (&constrainer);
    resizer = std::make_unique<juce::ResizableCornerComponent> (this, &constrainer);
    addAndMakeVisible (*resizer);

    setSize (660, 820);
    rebuildUnifiedRows();
    timerCallback();
    startTimerHz (2);
}

McfxReceiveAudioProcessorEditor::~McfxReceiveAudioProcessorEditor()
{
    stopTimer();
    processor.onDiscoveredSendersChanged = nullptr;
}

// ---------------------------------------------------------------------------
// User actions
// ---------------------------------------------------------------------------

void McfxReceiveAudioProcessorEditor::applyDirectIp()
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
    processor.clearLastAttempt();
    processor.inviteSender (host, port);
}

void McfxReceiveAudioProcessorEditor::connectSelected()
{
    const int row = localTab.list.getSelectedRow();
    if (row < 0 || row >= (int) localTab.list.rows.size()) return;
    const auto& r = localTab.list.rows[(size_t) row];
    if (r.ip.isEmpty() || r.port <= 0) return;
    processor.clearLastAttempt();
    // Pass wireUid for Bonjour-discovered rows (=0 for Direct-IP) so the
    // accepted entry pairs against the discovered row from the very
    // first refresh — no flicker through "(direct)" while we wait for
    // the INVITE_ACK to populate the UID.
    processor.inviteSender (r.ip, r.port, r.wireUid);
}

void McfxReceiveAudioProcessorEditor::disconnectSelected()
{
    const int row = localTab.list.getSelectedRow();
    if (row < 0 || row >= (int) localTab.list.rows.size()) return;
    const auto& r = localTab.list.rows[(size_t) row];
    if (r.ip.isEmpty() || r.port <= 0) return;
    // Pass wireUid for Bonjour-paired rows. Without this, after the
    // sender re-binds to a different ephemeral source port (e.g. on
    // its own prepareToPlay) the accepted entry's host:port no longer
    // matches the discovered row, and a host:port-only uninvite would
    // silently fail to find anything to evict.
    processor.uninviteSender (r.ip, r.port, r.wireUid);
}

bool McfxReceiveAudioProcessorEditor::currentSelectionIsConnected() const
{
    const int row = localTab.list.getSelectedRow();
    if (row < 0 || row >= (int) localTab.list.rows.size()) return false;
    const auto& r = localTab.list.rows[(size_t) row];
    return r.state == RowState::Active || r.state == RowState::Inviting;
}

// ---------------------------------------------------------------------------
// Build the unified sender row list (discovered + accepted, deduped by UID
// then by host:port).
// ---------------------------------------------------------------------------

void McfxReceiveAudioProcessorEditor::rebuildUnifiedRows()
{
    const auto discovered = processor.getDiscoveredSenders();
    const auto accepted   = processor.getAcceptedSenders();
    const auto filter     = localTab.filterEditor.getText().trim().toLowerCase();

    auto filterPasses = [&filter] (const UnifiedRow& r) {
        if (filter.isEmpty()) return true;
        return r.host   .toLowerCase().contains (filter)
            || r.project.toLowerCase().contains (filter)
            || r.track  .toLowerCase().contains (filter)
            || r.ip                  .contains (filter);
    };

    auto stateFromAccepted = [] (mcfx::net::RecvStream::SenderState s) {
        switch (s)
        {
            case mcfx::net::RecvStream::SenderState::Pending:  return RowState::Inviting;
            case mcfx::net::RecvStream::SenderState::Active:   return RowState::Active;
            case mcfx::net::RecvStream::SenderState::Rejected: return RowState::Rejected;
            case mcfx::net::RecvStream::SenderState::Timeout:  return RowState::Timeout;
        }
        return RowState::Discovered;
    };

    std::vector<UnifiedRow> rows;
    rows.reserve (discovered.size() + accepted.size());

    // 1. Walk discovered senders. For each, look up the matching accepted
    //    entry by wire UID (Bonjour TXT "wuid" == sender's stream UID,
    //    same value the sender writes into CommonHeader.sender). UID is
    //    the only identity — host:port can drift across re-binds and
    //    multi-homed source-IP differences.
    //
    //    Direct-IP accepted entries (created via the user typing a host
    //    in the Direct IP tab) have uid=0 until the first INVITE_ACK
    //    arrives; they correctly DON'T pair against any discovered row
    //    and fall through to the "(direct)" rendering below.
    std::vector<bool> acceptedConsumed (accepted.size(), false);
    for (const auto& d : discovered)
    {
        UnifiedRow r;
        r.host       = d.host;
        r.project    = d.project;
        r.track      = d.track;
        r.ip         = d.ip.toString();
        r.port       = d.port;
        r.channels   = d.channels;
        r.format     = d.format;
        r.wireUid    = d.wireUid;
        r.viaBonjour = true;
        r.state      = RowState::Discovered;

        for (size_t i = 0; i < accepted.size(); ++i)
        {
            const auto& a = accepted[i];
            if (a.uid != 0 && a.uid == d.wireUid)
            {
                r.state         = stateFromAccepted (a.state);
                r.rejectReason  = a.rejectReason;
                acceptedConsumed[i] = true;
                break;
            }
        }
        if (filterPasses (r))
            rows.push_back (std::move (r));
    }

    // 2. Append any accepted entries we didn't pair with a discovered
    //    one (Direct-IP path, sender that vanished from Bonjour, etc.).
    for (size_t i = 0; i < accepted.size(); ++i)
    {
        if (acceptedConsumed[i]) continue;
        const auto& a = accepted[i];
        UnifiedRow r;
        r.host       = "(direct)";
        r.ip         = a.host;
        r.port       = a.port;
        r.viaBonjour = false;
        r.state      = stateFromAccepted (a.state);
        r.rejectReason = a.rejectReason;
        if (filterPasses (r))
            rows.push_back (std::move (r));
    }

    localTab.list.rows = std::move (rows);
    localTab.list.updateContent();
    localTab.list.repaint();

    // Keep the connect/disconnect buttons reflecting the selection.
    const bool hasSelection = localTab.list.getSelectedRow() >= 0;
    const bool connected    = currentSelectionIsConnected();
    connectButton.setEnabled    (hasSelection && ! connected);
    disconnectButton.setEnabled (hasSelection && connected);
}

// ---------------------------------------------------------------------------
// Painting + layout
// ---------------------------------------------------------------------------

void McfxReceiveAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::grey);
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::plain)));
    g.drawText ("mcfx_receive v" MCFX_NET_QUOTE (VERSION) "  built " __DATE__ " " __TIME__,
                getWidth() - 360, getHeight() - 14, 340, 12,
                juce::Justification::bottomRight, true);
}

void McfxReceiveAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    titleLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (6);

    auto row2 = area.removeFromTop (28);
    jitterLabel.setBounds (row2.removeFromLeft (130));
    jitterSlider.setBounds (row2);
    area.removeFromTop (6);

    auto rowPw = area.removeFromTop (28);
    pwLabel.setBounds (rowPw.removeFromLeft (140));
    pwEditor.setBounds (rowPw.removeFromLeft (200));
    rowPw.removeFromLeft (16);
    autoReconnectToggle.setBounds (rowPw.removeFromLeft (juce::jmax (200, rowPw.getWidth())));
    area.removeFromTop (6);

    boundLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (2);
    connectionStatusLabel.setBounds (area.removeFromTop (22));
    area.removeFromTop (8);

    // Bottom strip: meter (~140 px), per-peer telemetry above it.
    constexpr int kMeterHeight = 140;
    auto meterRow = area.removeFromBottom (kMeterHeight);
    meter.setBounds (meterRow);
    area.removeFromBottom (4);

    networkLabel.setBounds (area.removeFromBottom (20));
    peersLabel.setBounds (area.removeFromBottom (24));
    area.removeFromBottom (4);

    // Connect / Disconnect / Refresh row (just above the telemetry).
    auto buttonRow = area.removeFromBottom (32);
    area.removeFromBottom (4);
    connectButton.setBounds    (buttonRow.removeFromLeft (110));
    buttonRow.removeFromLeft (6);
    disconnectButton.setBounds (buttonRow.removeFromLeft (110));
    buttonRow.removeFromLeft (6);
    refreshButton.setBounds    (buttonRow.removeFromLeft (90));

    // Whatever's left is the tabbed connect panel. The inner ListBox
    // surfaces a vertical scrollbar automatically once the row count
    // exceeds the visible area, so a long discovered list still works
    // even when the window is small.
    tabs.setBounds (area);

    if (resizer)
        resizer->setBounds (getWidth() - 16, getHeight() - 16, 16, 16);
}

// ---------------------------------------------------------------------------
// Timer (2 Hz)
// ---------------------------------------------------------------------------

void McfxReceiveAudioProcessorEditor::timerCallback()
{
    processor.pollHostState();

    // Local listen-port + this machine's IPv4 addresses. The user can
    // read the IP off the screen and give it to a sender peer who has
    // to type it into Direct IP (no Bonjour across subnets etc.).
    {
        const int bound = processor.getBoundPort();
        const auto ips  = mcfx::net::getLocalIPv4Addresses();
        juce::String txt = "Listening on UDP port " + juce::String (bound);
        if (! ips.isEmpty())
            txt += juce::String::fromUTF8 ("  \xE2\x80\x94  this machine: ")
                 + ips.joinIntoString (", ");
        boundLabel.setText (txt, juce::dontSendNotification);
        boundLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    }

    // Refresh the unified row list each tick — this picks up state
    // transitions (Active → Rejected etc.) without waiting for the
    // Bonjour-change callback.
    rebuildUnifiedRows();

    // Per-peer telemetry. Always run — used to live behind a peers.empty()
    // early-return that suppressed banner updates on bad-password rejects.
    const auto peers = processor.snapshotPeers();
    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;

    if (peers.empty())
    {
        peersLabel.setText ("Peers: (none)", juce::dontSendNotification);
        networkLabel.setText ("(idle)",     juce::dontSendNotification);
        rateHistory.clear();
        lastSampleTimeSec = now;
    }
    else
    {
        juce::String peersText = "Peers: ";
        juce::String netText;

        juce::StringArray live;
        for (auto& p : peers) live.add (p.host + ":" + juce::String (p.port));

        for (size_t i = 0; i < peers.size(); ++i)
        {
            const auto& p = peers[i];
            if (i > 0) { peersText += " | "; netText += " | "; }
            peersText += juce::String (p.nchan) + " ch  ("
                       + p.host + ":" + juce::String (p.port) + ")"
                       + (p.primed ? "" : " (priming)");

            auto& h = rateHistory[p.uid];
            if (lastSampleTimeSec > 0.0)
            {
                const double dt = now - lastSampleTimeSec;
                if (dt > 0.05)
                {
                    if (p.recvBytes >= h.lastBytes)
                        h.ratePerSec = 0.5 * h.ratePerSec
                                     + 0.5 * (double) (p.recvBytes - h.lastBytes) / dt;
                    if (p.recvPackets >= h.lastPackets)
                        h.ppsRate = 0.5 * h.ppsRate
                                  + 0.5 * (double) (p.recvPackets - h.lastPackets) / dt;
                }
            }
            h.lastBytes   = p.recvBytes;
            h.lastPackets = p.recvPackets;

            auto fmtRate = [] (double v) {
                if (v >= 1.0e6) return juce::String (v / 1.0e6, 2) + " MB/s";
                if (v >= 1.0e3) return juce::String (v / 1.0e3, 1) + " kB/s";
                return juce::String ((int) v) + " B/s";
            };

            netText += fmtRate (h.ratePerSec)
                     + "  " + juce::String ((int) h.ppsRate) + " pkt/s"
                     + "  buf " + juce::String (p.effectiveBufMs) + "ms"
                     + (p.autoExtraMs > 0
                           ? " (+" + juce::String (p.autoExtraMs) + "ms)"
                           : juce::String())
                     + "  lat ~" + juce::String (p.totalLatencyMs) + "ms"
                     + "  fill " + juce::String (p.fillFrames) + " fr";
            if (p.gapFrames)      netText += "  gap "       + juce::String (p.gapFrames);
            if (p.underruns)      netText += "  underrun "  + juce::String (p.underruns);
            if (p.droppedReorder) netText += "  reorderDrop " + juce::String (p.droppedReorder);
        }
        peersLabel.setText (peersText, juce::dontSendNotification);
        networkLabel.setText (netText,  juce::dontSendNotification);
        lastSampleTimeSec = now;
    }

    // Connection-status banner. Sourced from the dedicated lastAttempt
    // field so it shows the most recent INVITE_ACK outcome regardless
    // of whether the corresponding accepted-set entry survived to
    // render. Falls back to scanning the unified rows for an Active
    // connection.
    juce::String banner;
    juce::Colour bannerColour = juce::Colours::white;

    const auto la = processor.getLastAttempt();
    const double nowSec = juce::Time::getCurrentTime().toMilliseconds() / 1000.0;
    const double laAgeSec = la.valid
        ? (nowSec - la.when.toMilliseconds() / 1000.0) : 1.0e9;

    bool sawActive = false;
    for (const auto& r : localTab.list.rows)
        if (r.state == RowState::Active) sawActive = true;

    using AS = mcfx::net::RecvStream::AttemptState;
    if (la.valid && laAgeSec < 30.0)
    {
        const juce::String hostStr = la.host + ":" + juce::String (la.port);
        if (la.state == AS::Rejected)
        {
            bannerColour = juce::Colours::orange;
            banner = "Last attempt to " + hostStr + ": ";
            banner += (la.reason == mcfx::net::IAR_BAD_PASSWORD)
                        ? "REJECTED - passwords don't match"
                        : (la.reason == mcfx::net::IAR_MAX_TARGETS)
                            ? "REJECTED - sender's target list is full"
                            : "REJECTED";
        }
        else if (la.state == AS::Inviting)
        {
            bannerColour = juce::Colours::lightgoldenrodyellow;
            banner = "Inviting " + hostStr + " ...";
        }
        else if (la.state == AS::Accepted)
        {
            bannerColour = juce::Colour (0xff80c080);
            banner = "Connected to " + hostStr;
        }
        else if (la.state == AS::Timeout)
        {
            bannerColour = juce::Colours::orange;
            banner = "No response from " + hostStr
                   + " - check the sender is running and reachable";
        }
    }
    else if (sawActive)
    {
        bannerColour = juce::Colour (0xff80c080);
        banner = "Connection healthy";
    }

    // Auto-reconnect status takes precedence — it's the action currently
    // happening, and the lastAttempt banner would otherwise be stale
    // info from before the project closed.
    const int armed = processor.getArmedPeerCount();
    if (armed > 0)
    {
        const int rem = processor.getAutoReconnectSecondsRemaining();
        bannerColour = juce::Colour (0xffd9b438); // amber, same as Inviting dot
        banner = "Reconnecting to " + juce::String (armed)
               + (armed == 1 ? juce::String (" peer") : juce::String (" peers"))
               + juce::String::fromUTF8 ("\xE2\x80\xA6 ")
               + juce::String (rem) + " s remaining";
    }

    connectionStatusLabel.setText (banner, juce::dontSendNotification);
    connectionStatusLabel.setColour (juce::Label::textColourId, bannerColour);
}
