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

 mcfx_net RecvStream — owns the listening UDP socket and one PeerStream per
 sender UID seen on the wire. Pulls audio for the audio thread to mix.

 Threading:
   * Message thread — bind(), prepareToPlay(), releaseResources(),
                       snapshotPeers() for the UI.
   * NetThread (RT) — select() then drain-burst recvfrom(); per packet,
                       parses the wire header, looks up / creates a
                       PeerStream by sender UID, writes the audio frames
                       into that peer's ring buffer.
   * Audio thread   — pulls blockSize frames from each peer's ring buffer
                       into a caller-provided sum buffer.

 v1 scope: PCM float32 + int16/int24 (Phase 5 follow-up). Adaptive jitter
 buffer is Phase 5 — v1 is fixed prefill at 4*period frames.

 ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "mcfx_net_wire.h"
#include "mcfx_net_socket.h"

#include <atomic>
#include <memory>
#include <vector>

namespace mcfx { namespace net {

class RecvStream
{
public:
    RecvStream();
    ~RecvStream();

    // Bind to localUdpPort (0 = ephemeral, or 1..65535). Idempotent if the
    // requested port is already bound. Otherwise tears down the existing
    // socket and re-binds; the NetThread is recycled if it was running.
    // Returns the actually-bound port, or 0 on failure.
    int bind (int localUdpPort);

    // Audio config for the consumer (the receive plugin's bus). The ring
    // buffers are still per-stream (sized from the sender's descriptor),
    // but `blockSize` is what the audio thread will pull each call.
    void prepareToPlay (double sampleRate, int blockSize, int outputChannels);

    void releaseResources();

    // Audio thread: pull blockSize frames from each peer, sum into out.
    // out is a non-interleaved channel pointer array, channels x blockSize.
    // The caller is responsible for clearing out first.
    void pullAndMix (float* const* out, int outputChannels, int blockSize) noexcept;

    // User-set jitter buffer floor, in milliseconds. The effective buffer
    // size for each peer is `floor + autoExtra`, where autoExtra is per-
    // peer state that grows when packets get lost / the audio thread
    // underruns, and decays slowly during clean periods. Setting a floor
    // is the user's "I trust this network this much" knob; the adaptive
    // path handles network glitches above the floor.
    void setJitterFloorMs (int ms) noexcept;
    int  getJitterFloorMs() const noexcept { return jitterFloorMs.load (std::memory_order_relaxed); }

    // ----- Bidirectional connect -------------------------------------------

    // Send an INVITE to the given sender host:port. The sender will
    // password-check and reply with an INVITE_ACK. On accepted ACK, the
    // sender is added to the local "accepted" set and inbound DATA from
    // its UID will pass the filter; on rejected ACK, no audio flows.
    // Idempotent: re-inviting an existing sender refreshes the entry.
    // wireUid is the sender's stream UID — pass 0 if unknown (Direct IP
    // path); pass the value from the Bonjour-discovered entry's wireUid
    // when the user clicks Connect on a discovered row. Populating it
    // up front lets the editor's row-pairing UID-match this entry from
    // the very first refresh, instead of waiting for the INVITE_ACK.
    bool inviteSender (const juce::String& host, int port, std::uint32_t wireUid = 0);

    // UNINVITE + remove from the accepted set.
    // Like inviteSender, takes an optional wireUid. Pass it from the
    // discovered row's wireUid when disconnecting a Bonjour-paired
    // peer — the accepted entry's host:port may have drifted (sender
    // re-bound to a different ephemeral source port), so matching by
    // host:port alone would silently fail to find the entry. UID
    // matching survives that.
    void uninviteSender (const juce::String& host, int port, std::uint32_t wireUid = 0);
    void uninviteAllSenders();

    // Password gate. Empty = no auth (matches all-zeros hash).
    void setPassword (const juce::String& pw);
    juce::String getPassword() const;

    enum class SenderState : std::uint8_t
    {
        Pending, Active, Rejected, Timeout,
    };

    struct AcceptedSender
    {
        uint32_t      uid = 0;        // 0 until ACK arrives (we know remote UID then)
        sockaddr_in   addr {};        // sender's listening addr (for sendto)
        juce::String  host;
        int           port = 0;
        SenderState   state = SenderState::Pending;
        InviteAckReason rejectReason = IAR_OK;
        juce::Time    addedAt;
        juce::Time    lastInviteAt;
    };

    std::vector<AcceptedSender> getAcceptedSenders() const;

    // Most recent INVITE_ACK received. Separate from the accepted list
    // because the user reported scenarios where the accepted entry's
    // state didn't update visibly — recording the outcome here as soon
    // as the ACK arrives lets the editor banner show "[bad password]"
    // unconditionally, regardless of any entry-lifecycle bugs.
    enum class AttemptState : std::uint8_t
    {
        None = 0,
        Inviting,    // click registered, INVITE sent, ACK not yet
        Accepted,    // ACK arrived with accepted=1
        Rejected,    // ACK arrived with accepted=0 (use `reason` for why)
        Timeout,     // gave up retrying after kRecvInviteDeadlineMs
    };

    struct LastAttempt
    {
        bool          valid = false;
        juce::String  host;
        int           port = 0;
        AttemptState  state = AttemptState::None;
        InviteAckReason reason = IAR_OK;
        juce::Time    when;
    };
    LastAttempt getLastAttempt() const
    {
        const juce::ScopedLock sl (lastAttemptLock);
        return lastAttempt;
    }
    // Clear after the user acknowledges. Editor calls this on a fresh
    // invite click so a stale "[bad password]" banner doesn't linger
    // when the user has obviously moved on.
    void clearLastAttempt()
    {
        const juce::ScopedLock sl (lastAttemptLock);
        lastAttempt = {};
    }

    int  getBoundPort()        const noexcept { return boundPort; }
    int  getNumPeers()         const noexcept;

    // Stable per-process wire UID. Embedded in outbound INVITE / UNINVITE /
    // INVITE_ACK headers and republished in Bonjour TXT, so the sender's
    // editor can pair its accepted-targets entries with Bonjour-discovered
    // receivers by UID — robust against multi-homed source-IP differences.
    uint32_t getReceiverUid()  const noexcept { return receiverUid; }

    // What was the most recently *requested* port (raw user input).
    // Lets the UI tell the user "you typed an invalid port; we're on the
    // ephemeral fallback".
    int  getRequestedPort()    const noexcept { return requestedPort; }

    struct PeerSnapshot
    {
        uint32_t uid;
        juce::String host;       // dotted-quad (peer's source address)
        int          port;
        int          nchan;
        int          sampleRate;
        int          period;          // sender's reported audio block in frames
        SampleFormat fmt;
        uint64_t     recvPackets;
        uint64_t     recvBytes;
        uint64_t     droppedReorder;
        uint64_t     gapFrames;       // frames zero-filled across gaps
        uint64_t     underruns;       // audio-thread reads with empty buffer
        bool         primed;
        int          fillFrames;      // current read-ahead in the ring
        int          effectiveBufMs;  // floor + auto extra applied right now
        int          autoExtraMs;     // per-peer adaptive headroom on top of the floor
        // Estimated total one-way latency: sender block + jitter buffer +
        // receiver block. Doesn't include actual network transit (which
        // would need synchronised clocks); on a LAN this is the dominant
        // component and is a good "how delayed is this audio" indicator.
        int          totalLatencyMs;
        // Rate-control loop state — exposed so the loopback test driver
        // can verify that clock-drift compensation actually converges to
        // the right ratio. Also useful as a future editor debug overlay.
        // - `rcorr`: current per-block multiplier on the resampler ratio.
        //   Steady state with matched clocks ≈ 1.0; with the sender X
        //   ppm faster than the receiver, settles at outputSR / senderSR.
        //   Clamped to [0.95, 1.05].
        // - `rcZ3`: integrator state. |z3| > 0.05 trips anti-windup and
        //   resets the loop; sustained near-threshold values mean the
        //   loop is fighting against more drift than it can compensate.
        // - `rcNominalRatio`: outputSR / senderSR captured at peer
        //   create. The actual ratio applied each block is rcNominalRatio
        //   * rcorr.
        double       rcorr;
        double       rcZ3;
        double       rcNominalRatio;
        // EMA of (arrival_rx - sender_tx) per packet. Sender and receiver
        // steady_clock origins differ per machine, so the absolute number
        // is meaningless across hosts — but its stability over time is a
        // useful proxy for network jitter health, and changes signal
        // path-quality changes. Reported in milliseconds.
        double       smoothedNetSpreadMs;
    };
    // For UI use only — message thread, not audio.
    std::vector<PeerSnapshot> snapshotPeers() const;

private:
    class NetThread;
    struct PeerStream;

    void netRecvOnce();
    void parsePacket (const uint8_t* buf, int nbytes,
                      const sockaddr_in& from);

    // Control-packet handlers. Run on the NetThread.
    void handleInvitePacket    (const sockaddr_in& from, const CommonHeader& hdr,
                                const InvitePayload& body) noexcept;
    void handleUninvitePacket  (const sockaddr_in& from, const CommonHeader& hdr,
                                const UninvitePayload& body) noexcept;
    void handleInviteAckPacket (const sockaddr_in& from, const CommonHeader& hdr,
                                const InviteAckPayload& body) noexcept;

    void sendOneInvite   (AcceptedSender& s) noexcept;
    void sendOneUninvite (const AcceptedSender& s) noexcept;
    void sendInviteAck   (const sockaddr_in& to, const InviteAckPayload& body) noexcept;
    void tickInviteRetries() noexcept;
    bool passwordMatches (const std::uint8_t inviterHash[32]) const noexcept;
    bool isAcceptedUid   (uint32_t uid) const noexcept;

    // Drop the PeerStream matching the given UID from the peers list.
    // Called whenever a sender goes away (sender's UNINVITE arrives, or
    // we initiate uninviteSender). Without this, the audio thread keeps
    // mixing from the dead peer's ring buffer until it drains and the
    // 15 s stale-bytes sweep evicts it — manifesting as "garbage tail
    // for a few seconds after Disconnect".
    void dropPeerByUid (uint32_t uid) noexcept;

    PeerStream* findOrCreatePeer (uint32_t uid, const sockaddr_in& from,
                                  const CommonHeader& hdr);

    // unique_ptr so we can REPLACE the socket on re-bind. JUCE's
    // DatagramSocket::shutdown() invalidates the internal handle (sets it
    // to -1), and bindToPort then refuses any further bind. The only way
    // to re-bind to a different port is to construct a new socket.
    std::unique_ptr<juce::DatagramSocket> socket;
    bool                 socketBound = false;
    int                  boundPort   = 0;
    int                  requestedPort = 0; // last bind(N) argument

    // Per-peer table. Lookup on the NetThread via linear scan (we expect
    // O(1..few) peers per receiver). The audio thread iterates the same
    // list to pull and mix; mutation happens only on NetThread under
    // peersLock, and the audio thread takes a brief shared_ptr snapshot
    // (no locking on the read path — reads `peersSnapshot`).
    mutable juce::CriticalSection peersLock;
    using PeerList = std::vector<std::shared_ptr<PeerStream>>;
    std::shared_ptr<PeerList> peers;
    std::shared_ptr<PeerList> peersSnapshot; // audio-thread cache

    int outputBlockSize = 0;
    int outputChans     = 0;
    int outputSampleRate = 48000;

    uint32_t receiverUid = 0;

    // Accepted-senders set. Inbound DATA from a UID not in this set is
    // dropped (with a once-per-source DBG log). Entries are added via
    // inviteSender (we initiate) or via inbound INVITE (sender wants to
    // push to us — we password-check and add). Mutated under acceptedLock.
    mutable juce::CriticalSection acceptedLock;
    std::vector<AcceptedSender>   accepted;
    // Lock-free copy for the NetThread's per-packet check. Refreshed
    // opportunistically — a brief stale read just delays accept by one
    // packet which is harmless.
    mutable juce::CriticalSection passwordLock;
    juce::String                  password;

    mutable juce::CriticalSection lastAttemptLock;
    LastAttempt                   lastAttempt;

    // User-set jitter buffer floor in milliseconds. Default 25 ms — same
    // default as sonolink, comfortable on a clean LAN. Atomic for
    // lock-free read on the audio thread.
    std::atomic<int> jitterFloorMs { 25 };

    // Anticipative-render detection (audio thread only — single producer/
    // consumer of these). Some hosts (REAPER's anticipative-FX setting)
    // call processBlock at multiples of realtime to pre-render plugin
    // output ahead of playback. For a network-streaming receiver this is
    // hopeless — the future audio doesn't exist yet (the sender is
    // producing it in real time). Detection: compare the wallclock
    // interval between consecutive pullAndMix calls against the expected
    // blockSize/sampleRate. With hysteresis, mark the plugin as "fast
    // mode" and bail out cleanly: output silence, leave the rate loop
    // and DLL state untouched. When intervals normalise we resume.
    double pullLastSec       = 0.0;
    int    pullFastRunCount  = 0;     // consecutive fast-interval blocks observed
    int    pullSlowRunCount  = 0;     // consecutive realtime-interval blocks
    bool   pullInFastMode    = false;
    int    pullFastSilenceLogged = 0; // counts log spam — only print once per entry

    std::atomic<bool>   running { false };
    std::unique_ptr<NetThread> netThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecvStream)
};

}} // namespace mcfx::net
