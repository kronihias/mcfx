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

#include "mcfx_net_recv.h"
#include "mcfx_net_rt.h"

#include <cstring>
#include <cmath>

// dbry/audio-resampler — vendored under common/third_party. Public API is
// pure C; wrap in extern "C" so name-mangling doesn't trip the linker.
// We use the variable-ratio mode: the resampler ratio is updated each
// audio block by the per-peer drift-compensation control loop below, so
// the receiver can stay phase-locked to the sender even when the two
// hardware audio clocks don't agree exactly.
extern "C" {
#include "resampler.h"
}

namespace mcfx { namespace net {

// Same retry / keep windows as the sender side so the UI behaves consistently.
constexpr int kRecvInviteRetryMs    = 1000;
constexpr int kRecvInviteDeadlineMs = 5000;
// Keep Rejected/Timeout entries visible in the Active senders list for
// 30 s after they failed — long enough that the user has time to notice
// "[bad password]" or "[no response]" and react. The accepted-uid filter
// only lets Active entries through, so cosmetic stale entries don't
// admit audio.
constexpr int kRecvRejectKeepMs     = 30000;

namespace {
    void hashPasswordRecv (const juce::String& pw, std::uint8_t out[32]) noexcept
    {
        if (pw.isEmpty())
        {
            std::memset (out, 0, 32);
            return;
        }
        const auto utf8 = pw.toRawUTF8();
        juce::SHA256 sha (juce::MemoryBlock (utf8, std::strlen (utf8)));
        const auto raw = sha.getRawData();
        std::memcpy (out, raw.getData(), 32);
    }
}

// Per-peer state. One per (sender UID) seen on the wire. Constructed on the
// NetThread when the first packet (DESC or DATA) from a new UID arrives.
struct RecvStream::PeerStream
{
    uint32_t uid = 0;

    // Last-seen source endpoint. Updated on every packet — sender may move
    // ports across restarts. Used for the UI snapshot only.
    sockaddr_in lastFrom {};

    int  sampleRate = 48000;
    int  period     = 256;
    int  nchan      = 0;
    SampleFormat fmt = SF_FLOAT32;

    // Ring buffer of interleaved float frames. Sized at peer creation
    // (after we know nchan + period). Capacity = ringFrames * nchan.
    std::vector<float> ring;
    int ringFrames = 0;

    // Frame positions are absolute counters that we modulo into the ring.
    // NetThread writes; audio thread reads. Atomic for cross-thread
    // visibility (SPSC pattern, no contention).
    std::atomic<uint64_t> writeFramePos { 0 };
    std::atomic<uint64_t> readFramePos  { 0 };

    // True once we've buffered at least the prefill target. The audio
    // thread outputs silence until then, so a brand-new stream doesn't
    // start with a stuttery first second.
    std::atomic<bool> primed { false };
    // Adaptive jitter-buffer state. effectiveBufMs = floor + autoExtraMs,
    // clamped, then converted into a frame count for the prefill target.
    // Bumps multiplicatively on loss (gap event) or underrun (audio
    // thread exhausting the ring), and decays linearly during clean
    // periods. Mirrors sonolink's algorithm — fast to react, slow to
    // give back latency.
    std::atomic<int> autoExtraMs { 0 };
    std::atomic<int> effectiveBufMs { 0 };
    std::atomic<int> prefillFrames { 0 };
    std::atomic<double> lastLossTimeSec { -1.0 };
    std::atomic<double> lastDecayTickSec { -1.0 };

    // Sequence tracking for loss / reorder detection. Not authoritative
    // for placement (we use `count` from the data payload for that).
    std::atomic<uint32_t> lastSeq { 0 };
    bool                  haveLastSeq = false;

    // Telemetry counters. Bumped on NetThread, read by message thread.
    std::atomic<uint64_t> recvPackets    { 0 };
    std::atomic<uint64_t> recvBytes      { 0 };
    std::atomic<uint64_t> droppedReorder { 0 };
    std::atomic<uint64_t> gapFrames      { 0 };
    std::atomic<uint64_t> underruns      { 0 };

    int fillFramesNow() const noexcept
    {
        const auto w = writeFramePos.load (std::memory_order_acquire);
        const auto r = readFramePos.load  (std::memory_order_acquire);
        return (w >= r) ? static_cast<int> (w - r) : 0;
    }

    // Stale-eviction state. lastDataTimeSec is updated on every DATA
    // packet from this peer; a peer that has gone silent for the
    // configured timeout gets dropped from the peer list. New peers
    // get the current time as their initial value so they aren't
    // immediately evicted before their first packet arrives.
    std::atomic<double> lastDataTimeSec { 0.0 };

    // Audio thread reads + clears this each block. When non-zero it
    // advances readFramePos by that many frames before mixing — used
    // by the message thread (setJitterFloorMs) to drop excess buffered
    // samples when the user lowers the jitter floor. Without this the
    // audio thread is the sole writer to readFramePos and a direct
    // store from the message thread would race the audio-thread RMW.
    std::atomic<int> pendingReadAdvance { 0 };

    // Recompute effectiveBufMs + prefillFrames from the user-set floor
    // and the per-peer adaptive extra. Called whenever either changes.
    // Caps total at 500 ms to bound worst-case latency.
    void recomputeBuffer (int floorMs) noexcept
    {
        const int extra = autoExtraMs.load (std::memory_order_relaxed);
        const int eff   = juce::jlimit (0, 500, floorMs + extra);
        effectiveBufMs.store (eff, std::memory_order_relaxed);
        const int frames = (eff * sampleRate) / 1000;
        prefillFrames.store (juce::jmax (period, frames),
                             std::memory_order_release);
    }

    // Multiplicative bump on loss / underrun: at least 20 ms, at most
    // double the current extra, capped at 200 ms total of auto headroom.
    // React fast to network glitches, then slowly earn the latency back.
    // Returns the new auto-extra value so callers can recomputeBuffer.
    int bumpAutoExtra (int floorMs, double nowSec) noexcept
    {
        const int cur  = autoExtraMs.load (std::memory_order_relaxed);
        const int bump = juce::jmax (20, cur / 2);
        const int neu  = juce::jmin (200, cur + bump);
        autoExtraMs.store (neu, std::memory_order_relaxed);
        lastLossTimeSec.store (nowSec, std::memory_order_relaxed);
        recomputeBuffer (floorMs);
        return neu;
    }

    // Slow decay — 1 ms per audio block once we've been loss-free for
    // 5 s. That earns ~3 s back per minute of clean network on a 64-
    // sample block at 48k. Called from the audio thread; cheap.
    void maybeDecay (int floorMs, double nowSec) noexcept
    {
        const int extra = autoExtraMs.load (std::memory_order_relaxed);
        if (extra <= 0) return;
        const double lastLoss = lastLossTimeSec.load (std::memory_order_relaxed);
        if (lastLoss > 0.0 && (nowSec - lastLoss) < 5.0) return;
        // Tick at most every ~5 ms so the decay rate doesn't depend on
        // the host's audio block size.
        const double lastTick = lastDecayTickSec.load (std::memory_order_relaxed);
        if (lastTick > 0.0 && (nowSec - lastTick) < 0.005) return;
        autoExtraMs.store (extra - 1, std::memory_order_relaxed);
        lastDecayTickSec.store (nowSec, std::memory_order_relaxed);
        recomputeBuffer (floorMs);
    }

    // ---- Adaptive sample-rate conversion --------------------------------
    //
    // Sender and receiver run on independent audio clocks. Even when the
    // two devices report the same nominal rate (e.g. both 48000 Hz), the
    // physical crystals drift a few ppm — enough that, over minutes, the
    // ring buffer either starves or overflows. To stay phase-locked we
    // run each peer's audio through a variable-ratio resampler, with the
    // ratio nudged each block by a control loop on the ring fill level.
    //
    // The loop is a 3rd-order filter (two cascaded one-pole smoothers
    // plus a leaky integrator) producing rcorr ≈ 1 + small-correction,
    // which multiplies the nominal output/input ratio. Coefficients
    // (w0, w1, w2) are recomputed via setLoopBw() with two bandwidths:
    // a fast bw=0.5 Hz at startup so we lock within a few seconds, then
    // drop to bw=0.05 Hz for smooth long-term tracking.
    //
    // Resampler instance is owned by PeerStream — allocated on the
    // NetThread when the peer is created, freed in the destructor. The
    // audio thread only calls resampleProcessInterleaved + reads/writes
    // the loop state.
    Resample* resampler = nullptr;
    int       resTaps    = 80;     // group delay = taps/2 input frames
    int       resFilters = 32;     // table size; SUBSAMPLE_INTERPOLATE fills the gaps

    int       outputSampleRate = 48000;   // host SR captured at peer create
    int       outputBlockSize  = 0;        // host block, used in loop coeffs

    // Loop state. All audio-thread.
    double rcW0 = 0.0, rcW1 = 0.0, rcW2 = 0.0;
    double rcZ1 = 0.0, rcZ2 = 0.0, rcZ3 = 0.0;
    double rcRcorr = 1.0;
    int    rcBlocksSinceLock     = 0;
    int    rcBlocksToFastPhase   = 0;   // ~4 s of host blocks; computed at create
    bool   rcInited = false;            // false → use first-block jump-correct
    double rcNominalRatio = 1.0;        // outputSR / senderSR

    // Scratch buffers used by the audio thread to stage interleaved
    // input out of the circular ring (resampler can't read across the
    // wrap), and to receive the resampler's output before summing into
    // the host buffer. Sized in PeerStream init.
    std::vector<float> resInScratch;    // capacity: 2*blockSize*nchan, plenty
    std::vector<float> resOutScratch;   // capacity: blockSize*nchan

    // (Re)compute loop coefficients for a given target bandwidth in Hz.
    // Bandwidth is normalised by the host audio block period — so a
    // "0.5 Hz" loop bw means one full settling time per ~2 s regardless
    // of block size.
    void setLoopBw (double bwHz) noexcept
    {
        if (outputBlockSize <= 0 || outputSampleRate <= 0) return;
        const double w = 6.28318530718 * bwHz
                       * (double) outputBlockSize / (double) outputSampleRate;
        rcW0 = 1.0 - std::exp (-20.0 * w);
        rcW1 = w * 2.0 * rcNominalRatio / (double) outputBlockSize;
        rcW2 = w / 2.0;
    }

    void resetRateLoop() noexcept
    {
        rcZ1 = rcZ2 = rcZ3 = 0.0;
        rcRcorr = 1.0;
        rcBlocksSinceLock = 0;
        rcInited = false;
    }

    ~PeerStream()
    {
        if (resampler != nullptr)
            resampleFree (resampler);
        resampler = nullptr;
    }
};

class RecvStream::NetThread : public juce::Thread
{
public:
    explicit NetThread (RecvStream& o) : juce::Thread ("mcfx_recv"), owner (o) {}
    void run() override
    {
        // Period 20ms matches the waitUntilReady timeout. Allow up to 15ms
        // compute per period — a 256-packet drain is the worst case.
        setCurrentThreadRealtime (20.0, 5.0, 15.0);
        while (! threadShouldExit())
            owner.netRecvOnce();
    }
    RecvStream& owner;
};

RecvStream::RecvStream()
{
    peers         = std::make_shared<PeerList>();
    peersSnapshot = peers;
}

RecvStream::~RecvStream()
{
    releaseResources();
}

int RecvStream::bind (int localUdpPort)
{
    requestedPort = localUdpPort;

    // Validate range. UDP ports are 16-bit; 0 is the "any free port"
    // sentinel. Anything outside is silently coerced by JUCE / the kernel
    // and ends up on the wrong port — refuse here so the UI can flag it.
    if (localUdpPort != 0 && (localUdpPort < 1 || localUdpPort > 65535))
    {
        DBG ("mcfx_recv: bind rejected — port " + juce::String (localUdpPort)
             + " out of range 1..65535");
        return boundPort; // leave whatever we currently have
    }

    // If we're already bound to the requested port, no-op.
    if (socketBound && boundPort == localUdpPort)
        return boundPort;
    // If the user asked for ephemeral (0) and we already have *some* port,
    // also no-op — re-binding would needlessly disrupt active streams.
    if (socketBound && localUdpPort == 0)
        return boundPort;

    // Otherwise we have to tear down. The NetThread holds a reference to
    // the socket; pause it across the rebind so a recvfrom doesn't race.
    const bool wasRunning = running.load (std::memory_order_acquire);
    if (wasRunning)
    {
        running.store (false, std::memory_order_release);
        if (netThread)
        {
            netThread->stopThread (400);
            netThread.reset();
        }
    }
    if (socketBound)
    {
        // Drop the socket entirely (NOT shutdown — that bricks the
        // internal handle and any subsequent bindToPort silently fails).
        socket.reset();
        socketBound = false;
        boundPort   = 0;
    }

    socket = std::make_unique<juce::DatagramSocket>();
    if (! socket->bindToPort (localUdpPort))
    {
        DBG ("mcfx_recv: bindToPort(" + juce::String (localUdpPort) + ") failed");
        socket.reset();
        return 0;
    }
    socketBound = true;
    boundPort = socket->getBoundPort();
    DBG ("mcfx_recv: bound to UDP port " + juce::String (boundPort));

    const auto rawHandle = socket->getRawSocketHandle();
    tuneKernelUdpBuffers (rawHandle, "mcfx_recv");
    applyAudioQos (rawHandle, "mcfx_recv");

    if (wasRunning)
    {
        running.store (true, std::memory_order_release);
        netThread = std::make_unique<NetThread> (*this);
        netThread->startThread (juce::Thread::Priority::high);
    }

    return boundPort;
}

void RecvStream::prepareToPlay (double sampleRate, int blockSize, int outputChannels)
{
    outputSampleRate = static_cast<int> (sampleRate);
    outputBlockSize  = blockSize;
    outputChans      = outputChannels;

    if (! socketBound)
        bind (0);

    if (! running.exchange (true, std::memory_order_acq_rel))
    {
        netThread = std::make_unique<NetThread> (*this);
        netThread->startThread (juce::Thread::Priority::high);
    }
}

void RecvStream::releaseResources()
{
    running.store (false, std::memory_order_release);
    if (netThread)
    {
        netThread->stopThread (400);
        netThread.reset();
    }
    if (socketBound)
    {
        socket.reset();
        socketBound = false;
    }
    {
        const juce::ScopedLock sl (peersLock);
        peers = std::make_shared<PeerList>();
    }
    peersSnapshot = peers;
}

int RecvStream::getNumPeers() const noexcept
{
    const juce::ScopedLock sl (peersLock);
    return peers ? static_cast<int> (peers->size()) : 0;
}

void RecvStream::setJitterFloorMs (int ms) noexcept
{
    const int clamped  = juce::jlimit (0, 500, ms);
    jitterFloorMs.store (clamped, std::memory_order_release);

    std::shared_ptr<PeerList> snap;
    {
        const juce::ScopedLock sl (peersLock);
        snap = peers;
    }
    if (! snap) return;

    for (auto& p : *snap)
    {
        if (! p) continue;
        p->recomputeBuffer (clamped);

        // Two cases — audible behaviour differs intentionally:
        //   * RAISING the floor: drop primed so the audio thread waits
        //     for the deeper target to fill before resuming. Brief
        //     silence, then audio resumes with extra latency.
        //   * LOWERING the floor: the existing buffer is too deep for
        //     the new target. Compute how many frames to skip and queue
        //     it via pendingReadAdvance — the audio thread applies it
        //     atomically at the start of its next pull. Audible: a
        //     single skip/click as the read pointer jumps; afterwards
        //     latency drops to the new floor.
        const int curFill   = p->fillFramesNow();
        const int targetFill = p->prefillFrames.load (std::memory_order_acquire);
        if (curFill > targetFill + (p->period / 2))
        {
            const int toSkip = curFill - targetFill;
            p->pendingReadAdvance.fetch_add (toSkip, std::memory_order_release);
        }
        else if (curFill < targetFill)
        {
            p->primed.store (false, std::memory_order_release);
        }
    }
}

std::vector<RecvStream::PeerSnapshot> RecvStream::snapshotPeers() const
{
    std::shared_ptr<PeerList> snap;
    {
        const juce::ScopedLock sl (peersLock);
        snap = peers;
    }
    std::vector<PeerSnapshot> out;
    if (! snap) return out;
    out.reserve (snap->size());
    for (const auto& p : *snap)
    {
        if (! p) continue;
        PeerSnapshot s;
        s.uid          = p->uid;
        s.port         = ntohs (p->lastFrom.sin_port);
        char buf[INET_ADDRSTRLEN] {};
        ::inet_ntop (AF_INET, &p->lastFrom.sin_addr, buf, sizeof (buf));
        s.host         = juce::String (buf);
        s.nchan        = p->nchan;
        s.sampleRate   = p->sampleRate;
        s.fmt          = p->fmt;
        s.recvPackets    = p->recvPackets.load    (std::memory_order_relaxed);
        s.recvBytes      = p->recvBytes.load      (std::memory_order_relaxed);
        s.droppedReorder = p->droppedReorder.load (std::memory_order_relaxed);
        s.gapFrames      = p->gapFrames.load      (std::memory_order_relaxed);
        s.underruns      = p->underruns.load      (std::memory_order_relaxed);
        s.primed         = p->primed.load         (std::memory_order_relaxed);
        s.fillFrames     = p->fillFramesNow();
        s.effectiveBufMs = p->effectiveBufMs.load (std::memory_order_relaxed);
        s.autoExtraMs    = p->autoExtraMs.load    (std::memory_order_relaxed);
        s.period         = p->period;
        // Estimated one-way latency. sender block + effective jitter +
        // receiver block + adaptive resampler group delay. Network
        // transit is unknown without synced clocks; on a LAN it's
        // negligible vs. these.
        const int senderBlockMs = (p->sampleRate > 0)
                                    ? (p->period * 1000) / p->sampleRate : 0;
        const int recvBlockMs   = (outputSampleRate > 0)
                                    ? (outputBlockSize * 1000) / outputSampleRate : 0;
        // Resampler group delay = numTaps/2 input frames at the sender's
        // rate. With taps=80 at 48 kHz that's ~0.83 ms — small but it
        // sums into the total budget.
        const int resampMs = (p->sampleRate > 0)
                                    ? ((p->resTaps / 2) * 1000) / p->sampleRate : 0;
        s.totalLatencyMs = senderBlockMs + s.effectiveBufMs + recvBlockMs + resampMs;
        out.push_back (std::move (s));
    }
    return out;
}

// ---------------------------------------------------------------------------
// Bidirectional connect: invite-out + accepted-senders set
// ---------------------------------------------------------------------------

void RecvStream::setPassword (const juce::String& pw)
{
    const juce::ScopedLock sl (passwordLock);
    password = pw;
}

juce::String RecvStream::getPassword() const
{
    const juce::ScopedLock sl (passwordLock);
    return password;
}

bool RecvStream::passwordMatches (const std::uint8_t inviterHash[32]) const noexcept
{
    juce::String pw;
    {
        const juce::ScopedLock sl (passwordLock);
        pw = password;
    }
    std::uint8_t mine[32];
    hashPasswordRecv (pw, mine);
    return std::memcmp (mine, inviterHash, 32) == 0;
}

bool RecvStream::isAcceptedUid (uint32_t uid) const noexcept
{
    if (uid == 0) return false;
    const juce::ScopedLock sl (acceptedLock);
    for (auto& s : accepted)
        if (s.state == SenderState::Active && s.uid == uid)
            return true;
    return false;
}

bool RecvStream::inviteSender (const juce::String& host, int port)
{
    if (host.isEmpty() || port < 1 || port > 65535) return false;
    sockaddr_in addr {};
    if (! resolveIPv4 (host, port, addr)) return false;
    if (! socketBound || ! socket) return false;

    // Record the attempt up front so the editor's banner shows
    // "Inviting…" immediately, even before any ACK comes back. The
    // ACK handler will overwrite this with Accepted / Rejected.
    {
        const juce::ScopedLock al (lastAttemptLock);
        lastAttempt.valid  = true;
        lastAttempt.host   = host;
        lastAttempt.port   = port;
        lastAttempt.state  = AttemptState::Inviting;
        lastAttempt.reason = IAR_OK;
        lastAttempt.when   = juce::Time::getCurrentTime();
    }
    std::cerr << "mcfx_recv: inviteSender(" << host.toRawUTF8()
              << ":" << port << ")" << std::endl;

    {
        const juce::ScopedLock sl (acceptedLock);
        // Re-arm an existing entry on duplicate invite — useful for
        // nudging a sender that didn't ACK the first round, or for
        // reconnecting after a disconnect cycle. CRITICAL: reset
        // addedAt to NOW. If we preserved the old addedAt (which we
        // used to do "if it was zero"), tickInviteRetries would see
        // sinceAdded > 5 s on the first sweep and instantly mark the
        // entry Timeout before any ACK could arrive — making
        // reconnects after the first session appear to hang in
        // "Inviting..." until the 5 s deadline.
        const auto nowT = juce::Time::getCurrentTime();
        for (auto& s : accepted)
        {
            if (s.host == host && s.port == port)
            {
                s.state         = SenderState::Pending;
                s.rejectReason  = IAR_OK;
                s.addedAt       = nowT;
                s.lastInviteAt  = nowT;
                sendOneInvite (s);
                return true;
            }
        }

        AcceptedSender s;
        s.addr        = addr;
        s.host        = host;
        s.port        = port;
        s.state       = SenderState::Pending;
        s.addedAt     = juce::Time::getCurrentTime();
        s.lastInviteAt = s.addedAt;
        accepted.push_back (s);
        sendOneInvite (accepted.back());
    }
    return true;
}

void RecvStream::uninviteSender (const juce::String& host, int port)
{
    // Two-pass eviction. First, find ANY entry matching host:port and
    // capture its UID. Then erase EVERY entry matching either that
    // host:port OR that UID — this catches duplicate / stale entries
    // that may have accumulated if the sender re-bound to a new
    // ephemeral port mid-session (we'd then have one Pending entry at
    // the old port and one Active entry at the new one). Without this,
    // the user clicks "Disconnect" on the active row, only the active
    // entry goes away, and the stale Pending sits there forever.
    AcceptedSender evicted;
    uint32_t evictedUid = 0;
    bool found = false;
    {
        const juce::ScopedLock sl (acceptedLock);
        for (auto& s : accepted)
        {
            if (s.host == host && s.port == port)
            {
                evicted = s;
                evictedUid = s.uid;
                found = true;
                break;
            }
        }
        if (found)
        {
            for (auto it = accepted.begin(); it != accepted.end(); )
            {
                const bool hostPortMatch = (it->host == host && it->port == port);
                const bool uidMatch      = (evictedUid != 0 && it->uid == evictedUid);
                if (hostPortMatch || uidMatch) it = accepted.erase (it);
                else                           ++it;
            }
        }
    }
    if (found)
    {
        sendOneUninvite (evicted);
        // Drop the PeerStream right away. Without this the audio thread
        // would keep reading from the dead peer's ring buffer (~25 ms of
        // valid audio, then random ring contents — "garbage tail").
        if (evictedUid != 0) dropPeerByUid (evictedUid);
    }
}

void RecvStream::uninviteAllSenders()
{
    std::vector<AcceptedSender> evicted;
    {
        const juce::ScopedLock sl (acceptedLock);
        evicted.swap (accepted);
    }
    for (auto& s : evicted)
    {
        sendOneUninvite (s);
        if (s.uid != 0) dropPeerByUid (s.uid);
    }
}

void RecvStream::dropPeerByUid (uint32_t uid) noexcept
{
    if (uid == 0) return;
    const juce::ScopedLock sl (peersLock);
    if (! peers) return;
    auto newList = std::make_shared<PeerList>();
    newList->reserve (peers->size());
    bool changed = false;
    for (auto& p : *peers)
    {
        if (p && p->uid == uid) { changed = true; continue; }
        newList->push_back (p);
    }
    if (changed)
    {
        peers = newList;
        DBG ("mcfx_recv: dropped peer uid="
             << juce::String::toHexString ((juce::int64) uid));
    }
}

std::vector<RecvStream::AcceptedSender> RecvStream::getAcceptedSenders() const
{
    const juce::ScopedLock sl (acceptedLock);
    return accepted;
}

void RecvStream::sendOneInvite (AcceptedSender& s) noexcept
{
    if (! socket || ! socketBound) return;

    uint8_t buf[kInviteTotalBytes] {};
    auto* hdr  = reinterpret_cast<CommonHeader*> (buf);
    auto* body = reinterpret_cast<InvitePayload*> (buf + sizeof (CommonHeader));

    std::memcpy (hdr->magic, kMagic, sizeof (kMagic));
    hdr->ptype  = PT_INVITE;
    hdr->flags  = 0;
    hdr->sfmt   = 0;
    hdr->nchan  = 0;
    hdr->sender = 0; // receiver doesn't have a fixed-stream UID; sender ignores this field on INVITE
    hdr->seq    = 0;

    body->listen_port = static_cast<uint32_t> (boundPort);
    {
        juce::String pw;
        {
            const juce::ScopedLock sl (passwordLock);
            pw = password;
        }
        hashPasswordRecv (pw, body->pw_hash);
    }

    const auto rawHandle = socket->getRawSocketHandle();
    ::sendto (rawHandle, reinterpret_cast<const char*> (buf),
              sizeof (buf), 0,
              reinterpret_cast<const sockaddr*> (&s.addr),
              sizeof (s.addr));
    s.lastInviteAt = juce::Time::getCurrentTime();
}

void RecvStream::sendOneUninvite (const AcceptedSender& s) noexcept
{
    if (! socket || ! socketBound) return;

    uint8_t buf[kUninviteTotalBytes] {};
    auto* hdr  = reinterpret_cast<CommonHeader*> (buf);
    auto* body = reinterpret_cast<UninvitePayload*> (buf + sizeof (CommonHeader));

    std::memcpy (hdr->magic, kMagic, sizeof (kMagic));
    hdr->ptype = PT_UNINVITE;
    body->listen_port = static_cast<uint32_t> (boundPort);

    const auto rawHandle = socket->getRawSocketHandle();
    ::sendto (rawHandle, reinterpret_cast<const char*> (buf),
              sizeof (buf), 0,
              reinterpret_cast<const sockaddr*> (&s.addr),
              sizeof (s.addr));
}

void RecvStream::sendInviteAck (const sockaddr_in& to,
                                const InviteAckPayload& body) noexcept
{
    if (! socket || ! socketBound) return;
    uint8_t buf[kInviteAckTotalBytes] {};
    auto* hdr  = reinterpret_cast<CommonHeader*> (buf);
    auto* dst  = reinterpret_cast<InviteAckPayload*> (buf + sizeof (CommonHeader));

    std::memcpy (hdr->magic, kMagic, sizeof (kMagic));
    hdr->ptype = PT_INVITE_ACK;
    *dst = body;

    const auto rawHandle = socket->getRawSocketHandle();
    ::sendto (rawHandle, reinterpret_cast<const char*> (buf),
              sizeof (buf), 0,
              reinterpret_cast<const sockaddr*> (&to),
              sizeof (to));
}

void RecvStream::tickInviteRetries() noexcept
{
    const auto nowT = juce::Time::getCurrentTime();
    const juce::ScopedLock sl (acceptedLock);
    std::vector<size_t> toErase;
    for (size_t i = 0; i < accepted.size(); ++i)
    {
        auto& s = accepted[i];
        if (s.state == SenderState::Pending)
        {
            const auto sinceAdded = (nowT - s.addedAt).inMilliseconds();
            const auto sinceLast  = (nowT - s.lastInviteAt).inMilliseconds();
            if (sinceAdded > kRecvInviteDeadlineMs)
            {
                s.state = SenderState::Timeout;
                DBG ("mcfx_recv: INVITE to " << s.host << ":" << s.port << " timed out");
                // Surface timeout in the editor banner. Without this
                // update the banner would stay on "Inviting…" forever
                // even after we've given up retrying.
                const juce::ScopedLock al (lastAttemptLock);
                lastAttempt.valid  = true;
                lastAttempt.host   = s.host;
                lastAttempt.port   = s.port;
                lastAttempt.state  = AttemptState::Timeout;
                lastAttempt.reason = IAR_OK;
                lastAttempt.when   = nowT;
            }
            else if (sinceLast > kRecvInviteRetryMs)
            {
                sendOneInvite (s);
            }
        }
        else if (s.state == SenderState::Rejected || s.state == SenderState::Timeout)
        {
            const auto sinceAdded = (nowT - s.addedAt).inMilliseconds();
            if (sinceAdded > (kRecvInviteDeadlineMs + kRecvRejectKeepMs))
                toErase.push_back (i);
        }
    }
    for (auto it = toErase.rbegin(); it != toErase.rend(); ++it)
        accepted.erase (accepted.begin() + *it);
}

void RecvStream::handleInvitePacket (const sockaddr_in& from,
                                     const CommonHeader& hdr,
                                     const InvitePayload& body) noexcept
{
    const bool ok = passwordMatches (body.pw_hash);

    InviteAckPayload ack {};
    ack.accepted = ok ? 1 : 0;
    ack.reason   = ok ? IAR_OK : IAR_BAD_PASSWORD;
    ack.receiver_jitter_ms = static_cast<uint32_t> (jitterFloorMs.load (std::memory_order_relaxed));
    ack.receiver_period    = static_cast<uint32_t> (outputBlockSize);

    if (! ok)
    {
        DBG ("mcfx_recv: INVITE rejected (bad password) from uid="
             << juce::String::toHexString ((juce::int64) hdr.sender));
        sendInviteAck (from, ack);
        return;
    }

    // Sender wants to push to us. Add them to accepted set in Active state
    // (they're invite-INITIATING us; no need to wait for our own INVITE
    // round-trip). UID is hdr.sender. The audio addr we'll see DATA from
    // is the source of this INVITE (sender's listening port == listen_port).
    sockaddr_in senderAddr = from;
    senderAddr.sin_port = htons (static_cast<uint16_t> (body.listen_port));

    char ipbuf[INET_ADDRSTRLEN] {};
    ::inet_ntop (AF_INET, &senderAddr.sin_addr, ipbuf, sizeof (ipbuf));
    const juce::String hostStr (ipbuf);
    const int portInt = static_cast<int> (body.listen_port);

    {
        const juce::ScopedLock sl (acceptedLock);
        bool found = false;
        for (auto& s : accepted)
        {
            if (s.host == hostStr && s.port == portInt)
            {
                s.uid   = hdr.sender;
                s.state = SenderState::Active;
                s.addr  = senderAddr;
                found = true;
                break;
            }
        }
        if (! found)
        {
            AcceptedSender s;
            s.uid       = hdr.sender;
            s.addr      = senderAddr;
            s.host      = hostStr;
            s.port      = portInt;
            s.state     = SenderState::Active;
            s.addedAt   = juce::Time::getCurrentTime();
            s.lastInviteAt = s.addedAt;
            accepted.push_back (s);
        }
    }
    DBG ("mcfx_recv: INVITE accepted from " << hostStr << ":" << portInt);
    sendInviteAck (from, ack);
}

void RecvStream::handleUninvitePacket (const sockaddr_in& from,
                                       const CommonHeader& hdr,
                                       const UninvitePayload& body) noexcept
{
    char ipbuf[INET_ADDRSTRLEN] {};
    ::inet_ntop (AF_INET, &from.sin_addr, ipbuf, sizeof (ipbuf));
    const juce::String hostStr (ipbuf);
    const int portInt = static_cast<int> (body.listen_port);

    std::cerr << "mcfx_recv: UNINVITE from " << hostStr.toRawUTF8()
              << ":" << portInt
              << " (sender uid=" << std::hex << hdr.sender << std::dec
              << ")" << std::endl;

    uint32_t uidToDrop = 0;
    {
        const juce::ScopedLock sl (acceptedLock);
        // Two-pass match. host:port is the natural primary key, but it
        // CAN drift: when the sender's prepareToPlay re-runs (transport
        // restart, sample-rate change), the SendStream re-binds and gets
        // a new ephemeral port. The accepted entry still holds the old
        // port, so a strict host:port match misses. Fall back to the UID
        // — it's stable for the lifetime of the sender plug-in load.
        for (auto it = accepted.begin(); it != accepted.end(); )
        {
            const bool hostPortMatch = (it->host == hostStr && it->port == portInt);
            const bool uidMatch      = (hdr.sender != 0 && it->uid == hdr.sender);
            if (hostPortMatch || uidMatch)
            {
                DBG ("mcfx_recv: UNINVITE matched accepted entry "
                     << it->host << ":" << it->port
                     << " (uid=" << juce::String::toHexString ((juce::int64) it->uid) << ")");
                if (uidToDrop == 0) uidToDrop = it->uid;
                if (uidToDrop == 0) uidToDrop = hdr.sender;
                it = accepted.erase (it);
            }
            else
            {
                ++it;
            }
        }
        if (uidToDrop == 0 && hdr.sender != 0) uidToDrop = hdr.sender;
    }

    if (uidToDrop != 0) dropPeerByUid (uidToDrop);
}

void RecvStream::handleInviteAckPacket (const sockaddr_in& from,
                                        const CommonHeader& hdr,
                                        const InviteAckPayload& body) noexcept
{
    char ipbuf[INET_ADDRSTRLEN] {};
    ::inet_ntop (AF_INET, &from.sin_addr, ipbuf, sizeof (ipbuf));
    const juce::String hostStr (ipbuf);
    const int portInt = static_cast<int> (ntohs (from.sin_port));

    // Record the outcome unconditionally, BEFORE attempting to match an
    // accepted-set entry. This way the editor banner can show
    // "[bad password]" the instant an INVITE_ACK arrives, even if the
    // entry-matching cascade below fails to find a corresponding entry
    // (which the user reported on first-attempt rejections).
    DBG ("mcfx_recv: INVITE_ACK from " << hostStr << ":" << portInt
         << " accepted=" << (int) body.accepted
         << " reason=" << (int) body.reason);
    {
        const juce::ScopedLock al (lastAttemptLock);
        lastAttempt.valid    = true;
        lastAttempt.host     = hostStr;
        lastAttempt.port     = portInt;
        lastAttempt.state    = (body.accepted == 1) ? AttemptState::Accepted
                                                    : AttemptState::Rejected;
        lastAttempt.reason   = static_cast<InviteAckReason> (body.reason);
        lastAttempt.when     = juce::Time::getCurrentTime();
    }
    // Also write to stderr — DBG is silenced in Release builds, and the
    // user reported they see no debug output. cerr is unconditional and
    // shows up when Reaper is launched from a terminal.
    std::cerr << "mcfx_recv: INVITE_ACK from " << hostStr.toRawUTF8()
              << ":" << portInt
              << " accepted=" << (int) body.accepted
              << " reason=" << (int) body.reason << std::endl;

    const juce::ScopedLock sl (acceptedLock);

    auto applyToEntry = [&] (AcceptedSender& s)
    {
        s.uid = hdr.sender;
        if (body.accepted == 1)
        {
            s.state = SenderState::Active;
            DBG ("mcfx_recv: ACK accepted by " << s.host << ":" << s.port);
        }
        else
        {
            s.state = SenderState::Rejected;
            s.rejectReason = static_cast<InviteAckReason> (body.reason);
            DBG ("mcfx_recv: ACK rejected by " << s.host << ":" << s.port
                 << " reason=" << (int) body.reason);
        }
    };

    // Primary: exact host:port match. This is the normal case and what
    // the receiver-initiated INVITE established.
    for (auto& s : accepted)
        if (s.host == hostStr && s.port == portInt) { applyToEntry (s); return; }

    // Fallback A: same host, any Pending entry. Catches the case where
    // the sender's bound port differs from what we INVITED — it'll have
    // re-bound, e.g. on a transport restart. Without this fallback the
    // ACK gets silently dropped, the entry stays Pending until the 5 s
    // retry deadline, and the user sees "[no response]" instead of a
    // useful "[bad password]" / "[connected]" outcome.
    for (auto& s : accepted)
        if (s.host == hostStr && s.state == SenderState::Pending)
        {
            DBG ("mcfx_recv: ACK from " << hostStr << ":" << portInt
                 << " matched Pending entry at " << s.host << ":" << s.port
                 << " (sender re-bound?)");
            applyToEntry (s);
            return;
        }

    // Fallback B: same UID. Last resort — survives both port and IP
    // changes (e.g. interface flip). Requires us to already have a UID
    // recorded for some Pending entry, which only happens if a prior
    // packet carried it; on the first INVITE/ACK round-trip this won't
    // help, but it's cheap and harmless.
    if (hdr.sender != 0)
    {
        for (auto& s : accepted)
            if (s.uid == hdr.sender) { applyToEntry (s); return; }
    }

    DBG ("mcfx_recv: INVITE_ACK from " << hostStr << ":" << portInt
         << " did not match any accepted entry — dropped");
}

// ---------------------------------------------------------------------------
// NetThread / packet path
// ---------------------------------------------------------------------------

void RecvStream::netRecvOnce()
{
    if (! socketBound || ! socket) { juce::Thread::sleep (5); return; }
    if (! socket->waitUntilReady (true, 20)) return;

    uint8_t buf[2048];
    juce::String senderHost;
    int senderPort = 0;

    constexpr int kMaxBurst = 256;
    for (int i = 0; i < kMaxBurst; ++i)
    {
        const auto nbytes = socket->read (buf, sizeof (buf), false,
                                          senderHost, senderPort);
        if (nbytes <= 0) break;

        if (! looksLikeMcfxPacket (buf, (int) nbytes)) continue;

        sockaddr_in from {};
        from.sin_family = AF_INET;
        from.sin_port   = htons (static_cast<uint16_t> (senderPort));
        ::inet_pton (AF_INET, senderHost.toRawUTF8(), &from.sin_addr);

        // Dispatch by ptype: control packets go to dedicated handlers,
        // DESC/DATA fall through to the audio path (parsePacket gates by
        // accepted-senders set).
        CommonHeader hdr {};
        std::memcpy (&hdr, buf, sizeof (hdr));
        if (hdr.ptype == PT_INVITE && (int) nbytes >= kInviteTotalBytes)
        {
            InvitePayload body {};
            std::memcpy (&body, buf + sizeof (CommonHeader), sizeof (body));
            handleInvitePacket (from, hdr, body);
            continue;
        }
        if (hdr.ptype == PT_UNINVITE && (int) nbytes >= kUninviteTotalBytes)
        {
            UninvitePayload body {};
            std::memcpy (&body, buf + sizeof (CommonHeader), sizeof (body));
            handleUninvitePacket (from, hdr, body);
            continue;
        }
        if (hdr.ptype == PT_INVITE_ACK && (int) nbytes >= kInviteAckTotalBytes)
        {
            InviteAckPayload body {};
            std::memcpy (&body, buf + sizeof (CommonHeader), sizeof (body));
            handleInviteAckPacket (from, hdr, body);
            continue;
        }

        parsePacket (buf, (int) nbytes, from);
    }

    // Cheap retry tick — runs every ~20ms (the recv loop's natural cadence)
    // since we got past the waitUntilReady. tickInviteRetries early-exits
    // if there's nothing pending so the no-op cost is a single locked walk
    // of `accepted`.
    tickInviteRetries();
}

RecvStream::PeerStream* RecvStream::findOrCreatePeer (uint32_t uid,
                                                     const sockaddr_in& from,
                                                     const CommonHeader& hdr)
{
    bool needsRebuild = false;
    {
        const juce::ScopedLock sl (peersLock);
        for (auto& p : *peers)
            if (p && p->uid == uid)
            {
                p->lastFrom = from;
                // If the sender's channel count changed (e.g. user
                // dialled "Channels to send" up or down), the old
                // PeerStream's ring is sized wrong and the audio thread
                // is reading p->nchan-shaped frames from it. We can't
                // resize the ring in place — the audio thread would
                // race with us. Drop the peer here and let the rebuild
                // path below allocate a fresh one.
                if (hdr.nchan != p->nchan)
                {
                    DBG ("mcfx_recv: peer uid="
                         + juce::String::toHexString ((juce::int64) uid)
                         + " channel count changed " + juce::String (p->nchan)
                         + " -> " + juce::String ((int) hdr.nchan)
                         + " — rebuilding peer");
                    needsRebuild = true;
                    break;
                }
                return p.get();
            }
    }

    if (needsRebuild)
    {
        // Drop the old PeerStream from the list. The shared_ptr in the
        // current audio-thread snapshot keeps the old object alive for
        // one more block; new packets land in the freshly allocated
        // entry below.
        const juce::ScopedLock sl (peersLock);
        auto newList = std::make_shared<PeerList>();
        newList->reserve (peers->size());
        for (auto& p : *peers)
            if (p && p->uid != uid) newList->push_back (p);
        peers = newList;
    }

    if (uid == 0) return nullptr;          // 0 is the sentinel "no UID"
    if (hdr.nchan == 0) return nullptr;    // need a channel count to size the ring

    auto entry = std::make_shared<PeerStream>();
    entry->uid       = uid;
    entry->lastFrom  = from;
    entry->nchan     = hdr.nchan;
    entry->fmt       = static_cast<SampleFormat> (hdr.sfmt);

    // Initial sizing — refined when a DESC arrives. Use the receiver's
    // own period as the prefill yardstick if we don't know the sender's
    // yet (the audio thread is already running on outputBlockSize).
    entry->sampleRate    = outputSampleRate;
    entry->period        = juce::jmax (64, outputBlockSize);
    entry->recomputeBuffer (jitterFloorMs.load (std::memory_order_relaxed));

    // Ring sized at 8 sender periods of headroom — enough to handle a
    // burst of late packets without overruning, while bounding worst-case
    // latency. Sized in float regardless of wire format (we always store
    // float frames in the ring so the audio thread reads cheaply).
    // Ring must be big enough to hold the user's max jitter floor PLUS
    // some headroom for auto-extra growth. Without this the user-set
    // jitter slider was effectively no-op past ~40 ms because the ring
    // physically couldn't hold more buffered audio.
    constexpr int kMaxJitterFloorMs = 500;   // matches RecvStream::setJitterFloorMs clamp
    constexpr int kRingHeadroomMs   = 100;
    const int ringMs = kMaxJitterFloorMs + kRingHeadroomMs;
    const int ringFramesByMs = (ringMs * entry->sampleRate) / 1000;
    entry->ringFrames = juce::jmax (8 * entry->period, ringFramesByMs);
    entry->ring.assign (static_cast<size_t> (entry->ringFrames * entry->nchan),
                        0.0f);
    entry->lastDataTimeSec.store (juce::Time::getMillisecondCounterHiRes() / 1000.0,
                                  std::memory_order_relaxed);

    // Adaptive sample-rate-conversion setup. Allocate the resampler here
    // (NetThread, no audio thread) so the audio thread only ever sees a
    // ready-to-call instance. Use SUBSAMPLE_INTERPOLATE for variable-
    // ratio operation; numTaps controls quality (≈ taps/2 group delay)
    // and numFilters is the precomputed sub-filter table density.
    entry->outputSampleRate = outputSampleRate;
    entry->outputBlockSize  = outputBlockSize;
    entry->rcNominalRatio   = (entry->sampleRate > 0)
                                ? ((double) outputSampleRate / (double) entry->sampleRate)
                                : 1.0;
    if (entry->resampler != nullptr)
        resampleFree (entry->resampler);
    entry->resampler = resampleInit (entry->nchan,
                                     entry->resTaps,
                                     entry->resFilters,
                                     0.95,                        // lowpassRatio
                                     SUBSAMPLE_INTERPOLATE);      // variable-ratio mode
    entry->resetRateLoop();
    entry->setLoopBw (0.5);                                       // fast lock
    if (outputSampleRate > 0)
        entry->rcBlocksToFastPhase = juce::jmax (1, (4 * outputSampleRate) / juce::jmax (1, outputBlockSize));
    // Stage / output scratch — sized for the worst case (sender slower
    // than us, so we may need ~blockSize / 0.95 input frames per call).
    const int maxInPerBlock  = (outputBlockSize > 0)
                                 ? (int) std::ceil ((double) outputBlockSize / 0.90)
                                 : 0;
    entry->resInScratch.assign  ((size_t) (maxInPerBlock + entry->resTaps) * entry->nchan, 0.0f);
    entry->resOutScratch.assign ((size_t) outputBlockSize * entry->nchan, 0.0f);

    // Publish to the peers list under the lock. The audio thread reads
    // peersSnapshot lock-free and refreshes opportunistically.
    {
        const juce::ScopedLock sl (peersLock);
        auto newList = std::make_shared<PeerList> (*peers);
        newList->push_back (entry);
        peers = newList;
    }
    DBG ("mcfx_recv: new peer uid=" + juce::String::toHexString ((juce::int64) uid)
         + " ch=" + juce::String (entry->nchan));
    return entry.get();
}

void RecvStream::parsePacket (const uint8_t* buf, int nbytes,
                              const sockaddr_in& from)
{
    if (nbytes < kHeaderBytes) return;
    CommonHeader hdr {};
    std::memcpy (&hdr, buf, sizeof (hdr));

    // Accepted-senders filter. Drop DESC/DATA from anyone we haven't
    // accepted via INVITE handshake. This is what makes the password
    // gate effective — without it, a unicast push from any LAN device
    // would just create a peer entry and play.
    if (! isAcceptedUid (hdr.sender)) return;

    auto* peer = findOrCreatePeer (hdr.sender, from, hdr);
    if (peer == nullptr) return;

    peer->recvPackets.fetch_add (1, std::memory_order_relaxed);
    peer->recvBytes.fetch_add (static_cast<uint64_t> (nbytes), std::memory_order_relaxed);

    if (hdr.ptype == PT_DESC)
    {
        if (nbytes < kDescTotalBytes) return;
        DescPayload d {};
        std::memcpy (&d, buf + sizeof (CommonHeader), sizeof (d));
        // Apply sender's reported period to refine the prefill target if
        // the ring was sized to the receiver's blockSize as a placeholder.
        if (d.period > 0 && d.period != static_cast<uint32_t> (peer->period))
        {
            peer->period = static_cast<int> (d.period);
            // Don't resize the ring mid-flight — it'd race the audio
            // reader. The 8x outputBlockSize sizing accommodates most
            // sender periods; for unusually large sender periods we'd
            // need a re-allocation pass, deferred.
        }
        if (d.fsamp > 0)
            peer->sampleRate = static_cast<int> (d.fsamp);
        peer->recomputeBuffer (jitterFloorMs.load (std::memory_order_relaxed));
        return;
    }

    if (hdr.ptype != PT_DATA) return;
    if (nbytes < kDataHeaderBytes) return;

    // Refresh the liveness timestamp. The audio thread checks this and
    // evicts peers that have been silent for >15 s (sender process died,
    // unplugged, etc.) — without this, dead peers would sit in the list
    // forever showing "(priming)" with no traffic.
    peer->lastDataTimeSec.store (juce::Time::getMillisecondCounterHiRes() / 1000.0,
                                 std::memory_order_relaxed);

    DataPayload data {};
    std::memcpy (&data, buf + sizeof (CommonHeader), sizeof (data));

    const SampleFormat fmt = static_cast<SampleFormat> (hdr.sfmt);
    if (fmt != SF_FLOAT32 && fmt != SF_INT16 && fmt != SF_INT24)
        return;

    const int frames = data.nfram;
    const int chans  = hdr.nchan;
    if (frames <= 0 || chans <= 0 || chans != peer->nchan) return;

    const int sampSize = sampleSizeBytes (fmt);
    const int payloadBytes = frames * chans * sampSize;
    if (nbytes < kDataHeaderBytes + payloadBytes) return;
    // Track the most recent format we saw — useful for the UI snapshot.
    peer->fmt = fmt;

    // Reorder check — ignore packets whose data belongs to a position
    // that's already in the past. We accept "future" packets and let the
    // ring write them at their absolute offset; gaps before that future
    // position get zero-filled.
    const uint64_t startFrame = data.count;
    const uint64_t endFrame   = startFrame + static_cast<uint64_t> (frames);

    auto curWrite = peer->writeFramePos.load (std::memory_order_acquire);
    if (endFrame <= curWrite)
    {
        peer->droppedReorder.fetch_add (1, std::memory_order_relaxed);
        return;
    }

    // Gap fill: if startFrame > curWrite, the ring positions in
    // [curWrite, startFrame) are zero-filled (already zero from
    // construction; we just have to advance writeFramePos through them).
    if (startFrame > curWrite)
    {
        const uint64_t gap = startFrame - curWrite;
        peer->gapFrames.fetch_add (gap, std::memory_order_relaxed);

        // Adaptive jitter: a gap means packets were lost in transit.
        // Grow the per-peer buffer so the next loss is more likely to
        // be absorbed by buffered audio rather than reaching the audio
        // thread as an underrun.
        const double nowSec = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        peer->bumpAutoExtra (jitterFloorMs.load (std::memory_order_relaxed), nowSec);

        // Zero out the ring slots we're about to skip — otherwise stale
        // audio from a wrap-around earlier could leak through.
        const uint64_t ringMod = static_cast<uint64_t> (peer->ringFrames);
        if (gap >= ringMod)
        {
            std::fill (peer->ring.begin(), peer->ring.end(), 0.0f);
        }
        else
        {
            const int fromIdx = static_cast<int> (curWrite % ringMod);
            const int toIdx   = static_cast<int> (startFrame % ringMod);
            if (toIdx > fromIdx)
            {
                std::fill (peer->ring.begin() + fromIdx * chans,
                           peer->ring.begin() + toIdx   * chans,
                           0.0f);
            }
            else
            {
                std::fill (peer->ring.begin() + fromIdx * chans,
                           peer->ring.end(), 0.0f);
                std::fill (peer->ring.begin(),
                           peer->ring.begin() + toIdx * chans,
                           0.0f);
            }
        }
        curWrite = startFrame;
    }

    // Decode the wire payload into the ring (which always stores float
    // frames so the audio thread reads cheaply). For SF_FLOAT32 this is
    // a memcpy; for int16/int24 we expand on the fly. Bandwidth savings
    // happen on the wire; CPU cost stays modest because nbytes is small.
    const uint8_t* payload = buf + kDataHeaderBytes;
    const uint64_t ringMod = static_cast<uint64_t> (peer->ringFrames);
    const int dstFrameBase = static_cast<int> (startFrame % ringMod);
    const int totalSamples = frames * chans;

    auto decodeInto = [fmt, payload, chans] (float* dst, int firstSample, int nSamples)
    {
        if (fmt == SF_FLOAT32)
        {
            std::memcpy (dst,
                         reinterpret_cast<const float*> (payload) + firstSample,
                         static_cast<size_t> (nSamples) * sizeof (float));
        }
        else if (fmt == SF_INT16)
        {
            const auto* src = reinterpret_cast<const int16_t*> (payload) + firstSample;
            constexpr float scale = 1.0f / 32767.0f;
            for (int i = 0; i < nSamples; ++i)
                dst[i] = static_cast<float> (src[i]) * scale;
        }
        else // SF_INT24
        {
            const uint8_t* src = payload + firstSample * 3;
            constexpr float scale = 1.0f / 8388607.0f;
            for (int i = 0; i < nSamples; ++i)
            {
                // Sign-extend a 24-bit little-endian value to 32-bit
                // signed by shifting it into the high bits and back.
                int32_t s = (static_cast<int32_t> (src[0])      )
                          | (static_cast<int32_t> (src[1]) <<  8)
                          | (static_cast<int32_t> (src[2]) << 16);
                if (s & 0x800000) s |= 0xff000000;
                dst[i] = static_cast<float> (s) * scale;
                src += 3;
            }
            juce::ignoreUnused (chans);
        }
    };

    if (dstFrameBase + frames <= peer->ringFrames)
    {
        decodeInto (peer->ring.data() + dstFrameBase * chans, 0, totalSamples);
    }
    else
    {
        const int firstFrames  = peer->ringFrames - dstFrameBase;
        const int firstSamples = firstFrames * chans;
        decodeInto (peer->ring.data() + dstFrameBase * chans, 0, firstSamples);
        decodeInto (peer->ring.data(), firstSamples, totalSamples - firstSamples);
    }

    peer->writeFramePos.store (endFrame, std::memory_order_release);

    // Become primed once the ring fill crosses the prefill threshold.
    if (! peer->primed.load (std::memory_order_relaxed))
    {
        const auto fill = peer->fillFramesNow();
        if (fill >= peer->prefillFrames.load (std::memory_order_acquire))
            peer->primed.store (true, std::memory_order_release);
    }
}

// ---------------------------------------------------------------------------
// Audio thread
// ---------------------------------------------------------------------------

void RecvStream::pullAndMix (float* const* out,
                             int outputChannels, int blockSize) noexcept
{
    // Refresh the audio-thread snapshot opportunistically. tryEnter avoids
    // ever blocking the audio thread; if NetThread is mid-publish we
    // reuse the previous snapshot for one tick.
    if (peersLock.tryEnter())
    {
        peersSnapshot = peers;
        peersLock.exit();
    }
    auto snap = peersSnapshot;
    if (! snap) return;

    const int floorMs = jitterFloorMs.load (std::memory_order_relaxed);
    const double nowSec = juce::Time::getMillisecondCounterHiRes() / 1000.0;

    // Stale-peer sweep. A peer that hasn't sent a DATA packet in
    // kStaleTimeoutSec is dropped from the list — sender process died,
    // network unplugged, unrelated machine that wandered off the LAN.
    // Done on the audio thread because it already holds peersSnapshot;
    // the actual removal happens under peersLock with a tryEnter so we
    // never block. If we can't grab the lock this tick, we'll try next
    // block — eviction is a slow path that doesn't need to be precise.
    constexpr double kStaleTimeoutSec = 15.0;
    bool needsCompact = false;
    for (const auto& p : *snap)
    {
        if (! p) continue;
        const double last = p->lastDataTimeSec.load (std::memory_order_relaxed);
        if (last > 0.0 && (nowSec - last) > kStaleTimeoutSec)
        {
            needsCompact = true;
            break;
        }
    }
    if (needsCompact && peersLock.tryEnter())
    {
        std::vector<uint32_t> evictedUids;
        if (peers)
        {
            auto newList = std::make_shared<PeerList>();
            newList->reserve (peers->size());
            for (auto& p : *peers)
            {
                if (! p) continue;
                const double last = p->lastDataTimeSec.load (std::memory_order_relaxed);
                if (last > 0.0 && (nowSec - last) > kStaleTimeoutSec)
                {
                    DBG ("mcfx_recv: evicting stale peer uid="
                         + juce::String::toHexString ((juce::int64) p->uid)
                         + " (silent " + juce::String (nowSec - last, 1) + " s)");
                    if (p->uid != 0) evictedUids.push_back (p->uid);
                    continue;
                }
                newList->push_back (p);
            }
            peers = newList;
        }
        peersLock.exit();
        // Refresh the audio-thread snapshot immediately so we don't keep
        // mixing from the just-evicted peer for one more block.
        peersSnapshot = peers;
        snap = peersSnapshot;

        // Defense-in-depth: also drop the matching accepted-senders
        // entry. If the peer's audio path went silent, the user-visible
        // "Active senders" entry should also disappear, regardless of
        // whether an UNINVITE actually reached us. This keeps the two
        // lists in sync even when the wire control path mis-fires.
        if (! evictedUids.empty())
        {
            const juce::ScopedLock asl (acceptedLock);
            for (auto uid : evictedUids)
            {
                for (auto it = accepted.begin(); it != accepted.end(); )
                {
                    if (it->uid == uid && it->state == SenderState::Active)
                    {
                        DBG ("mcfx_recv: silent-evicting accepted entry "
                             << it->host << ":" << it->port
                             << " (peer went silent)");
                        it = accepted.erase (it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
        }

        if (! snap) return;
    }

    for (const auto& p : *snap)
    {
        if (! p) continue;

        // Adaptive decay runs every block regardless of primed state, so
        // a peer that's still priming after a network glitch eventually
        // earns its latency back.
        p->maybeDecay (floorMs, nowSec);

        if (! p->primed.load (std::memory_order_acquire)) continue;

        const int chans  = p->nchan;
        const int mixCh  = juce::jmin (chans, outputChannels);
        if (chans <= 0 || mixCh <= 0) continue;

        // Apply any pending read-pointer advance queued by setJitterFloorMs
        // when the user lowered the floor. We're the sole writer to
        // readFramePos so the read-modify-write here is race-free.
        const int skip = p->pendingReadAdvance.exchange (0, std::memory_order_acq_rel);
        if (skip > 0)
            p->readFramePos.fetch_add (static_cast<uint64_t> (skip),
                                       std::memory_order_release);

        // ---------------------------------------------------------------
        // Adaptive sample-rate conversion driven by ring-fill control loop.
        //
        // For each block we:
        //   1. Measure fill error (current fill minus target fill).
        //   2. Step the 3rd-order PI loop:
        //          z1 += w0 * (w1*err - z1);
        //          z2 += w0 * (z1 - z2);
        //          z3 += w2 * z2;
        //          rcorr = 1 - (z2 + z3);
        //   3. Compute the per-block resampler ratio = nominal * rcorr,
        //      clamped to [0.95, 1.05] (== ±5%).
        //   4. Ask the resampler how many input frames it needs for our
        //      blockSize output frames at that ratio.
        //   5. Stage that many frames out of the ring (handling the
        //      circular wrap), call resampleProcessInterleaved, sum the
        //      output into the host buffer.
        //   6. Advance the ring read pointer by result.input_used (the
        //      resampler may consume slightly fewer than we offered).
        // ---------------------------------------------------------------

        if (p->resampler == nullptr)
            continue;   // peer not yet ready (shouldn't normally happen)

        // Compute ring fill in *peer-rate* frames. Our control target is
        // the expected MEAN steady-state fill, not the prime threshold.
        // The sender pumps `period` frames in at a time; between pumps
        // the receiver drains in blockSize chunks. So in steady state the
        // ring oscillates between (prefill threshold) and (prefill +
        // sender_period). Its MEAN is therefore prefill + period/2.
        //
        // If we used the bare prefill threshold as the loop target, err
        // would have a persistent +period/2 bias, driving the integrator
        // (z3 = ∫z2) into the anti-windup ceiling within seconds, which
        // would cyclically trip the loop reset → brief silence → re-prime
        // → loop converges again → repeat. Audible as periodic glitches
        // every ~few seconds.
        const uint64_t curRead0  = p->readFramePos.load  (std::memory_order_acquire);
        const uint64_t curWrite0 = p->writeFramePos.load (std::memory_order_acquire);
        const int      fillNow   = (curWrite0 > curRead0)
                                     ? (int) (curWrite0 - curRead0) : 0;
        const int      prefill   = p->prefillFrames.load (std::memory_order_acquire);
        const int      loopTarget = prefill + (p->period / 2);
        const double   err       = (double) (fillNow - loopTarget);

        // First block after (re)priming: jump-correct the read pointer
        // straight to the target fill, then engage the loop. Avoids a
        // slow ramp-in.
        if (! p->rcInited)
        {
            // We're already at "fill = target" by definition of priming
            // (primed flag was set the moment fill crossed prefillFrames).
            // Nothing to snap, just zero the integrator and start.
            p->resetRateLoop();
            p->setLoopBw (0.5);
            p->rcInited = true;
            p->rcBlocksSinceLock = 0;
        }

        // Loop step (only when primed; primed gate sits above this block).
        p->rcZ1 += p->rcW0 * (p->rcW1 * err - p->rcZ1);
        p->rcZ2 += p->rcW0 * (p->rcZ1 - p->rcZ2);
        p->rcZ3 += p->rcW2 * p->rcZ2;

        // Anti-windup: integrator runaway means the source rate is way
        // outside our ±5% range, or the loop is unstable. Reset and let
        // the prefill mechanism re-prime.
        if (std::abs (p->rcZ3) > 0.05)
        {
            p->resetRateLoop();
            p->primed.store (false, std::memory_order_release);
            // Bump auto-headroom — next prime sits deeper, giving the
            // loop more slack the next time.
            p->bumpAutoExtra (floorMs, nowSec);
            continue;
        }

        p->rcRcorr = juce::jlimit (0.95, 1.05, 1.0 - (p->rcZ2 + p->rcZ3));

        // After ~4 s of fast-bw operation, switch to slow tracking. The
        // user-perceptible behaviour: first few seconds the pitch may
        // wobble a few cents while we lock; then it settles.
        if (++p->rcBlocksSinceLock == p->rcBlocksToFastPhase)
            p->setLoopBw (0.05);

        const double ratio = p->rcNominalRatio * p->rcRcorr;

        // Ask the resampler how much input it needs for our output
        // block. This reads internal state, so it must be called on the
        // same thread that calls resampleProcess (== this audio thread).
        const unsigned int wantIn =
            resampleGetRequiredSamples (p->resampler, blockSize, ratio);

        // Underrun gate: if the ring doesn't hold enough source frames
        // for this resampler call, treat as underrun. Drop priming so
        // we re-buffer; bump auto-headroom so the next prime is deeper.
        const auto curRead  = p->readFramePos.load  (std::memory_order_acquire);
        const auto curWrite = p->writeFramePos.load (std::memory_order_acquire);
        const auto avail    = (curWrite > curRead) ? (curWrite - curRead) : 0;
        if (avail < (uint64_t) wantIn)
        {
            p->primed.store (false, std::memory_order_release);
            p->underruns.fetch_add (1, std::memory_order_relaxed);
            p->bumpAutoExtra (floorMs, nowSec);
            continue;
        }

        // Stage `wantIn` frames out of the ring into the contiguous
        // input scratch. The ring is circular so we may need two memcpys.
        // Resampler scratch was sized for the worst case at peer create.
        const size_t scratchCap = p->resInScratch.size() / (size_t) chans;
        if ((size_t) wantIn > scratchCap)
            continue;   // shouldn't happen — sizing is generous

        const uint64_t ringMod  = (uint64_t) p->ringFrames;
        const int srcBase   = (int) (curRead % ringMod);
        const int firstChunk = juce::jmin ((int) wantIn,
                                           p->ringFrames - srcBase);
        const int secondChunk = (int) wantIn - firstChunk;

        std::memcpy (p->resInScratch.data(),
                     p->ring.data() + (size_t) srcBase * chans,
                     (size_t) firstChunk * chans * sizeof (float));
        if (secondChunk > 0)
            std::memcpy (p->resInScratch.data() + (size_t) firstChunk * chans,
                         p->ring.data(),
                         (size_t) secondChunk * chans * sizeof (float));

        // Resample.
        const ResampleResult rr = resampleProcessInterleaved (
            p->resampler,
            p->resInScratch.data(), (int) wantIn,
            p->resOutScratch.data(), blockSize,
            ratio);

        // Sum resampler output into host buffer (same accumulate-mix
        // semantics as the previous direct path).
        const float* src = p->resOutScratch.data();
        const int nFramesOut = (int) rr.output_generated;
        for (int f = 0; f < nFramesOut; ++f)
            for (int c = 0; c < mixCh; ++c)
                out[c][f] += src[f * chans + c];

        // Advance the read pointer by what the resampler actually used,
        // not what we offered. Keeps the ring fill arithmetic exact.
        p->readFramePos.store (curRead + (uint64_t) rr.input_used,
                               std::memory_order_release);
    }
}

}} // namespace mcfx::net
