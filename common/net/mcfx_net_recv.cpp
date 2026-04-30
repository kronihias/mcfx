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

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <random>

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

    // True once we've processed the first DATA packet for this peer.
    // Used to suppress the bogus "gap" event on the very first packet
    // (writeFramePos starts at 0, but sender's `count` field is wherever
    // its absoluteFrame counter happened to be — typically large if the
    // sender has been running before the receiver connected). Without
    // this, the initial connect spuriously bumps autoExtra → prefill
    // grows → 5 s later the autoExtra decay drops prefill back, the
    // rate loop sees a huge target jump, slams rcorr to its lower
    // clamp, underruns, re-bumps autoExtra, repeats. NetThread-only.
    bool haveSeenData = false;

    // Smoothed prefill in frames — used by the audio loop's err target
    // so step changes in prefillFrames (e.g. an autoExtra bump or its
    // decay) don't appear to the rate loop as instantaneous target
    // jumps. Without smoothing, a 20 ms autoExtra decay drops
    // prefillFrames by ~960 samples in 100 ms, and the rate loop slams
    // rcorr to its ±5 % clamp trying to chase the moving target,
    // underrunning the ring on the way down. Audio-thread-only state.
    double prefillSmoothFrames = 0.0;
    bool   prefillSmoothInited = false;

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

    // Multiplicative bump on packet-loss gap events: at least 20 ms, at
    // most double the current extra, capped at 200 ms total. Used for
    // network-loss events where a graduated response is appropriate (a
    // one-off lost packet shouldn't lock the buffer to maximum).
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

    // Aggressive bump used for audio-thread underrun events. Underruns
    // mean the ring went below the resampler's input demand — either
    // the sender's audio thread is stalling (e.g. Windows scheduling
    // jitter) or the network is bunching packets. Either way the
    // buffer was clearly too small. Geometric growth from 20 ms takes
    // 6 underrun events (= 6 audible glitches) to reach the 200 ms
    // cap; jumping to the cap on the first underrun makes recovery
    // single-event. Latency cost is real (extra 200 ms of buffering)
    // but that's strictly better than a continuous glitch cascade.
    void bumpAutoExtraToCap (int floorMs, double nowSec) noexcept
    {
        autoExtraMs.store (200, std::memory_order_relaxed);
        lastLossTimeSec.store (nowSec, std::memory_order_relaxed);
        recomputeBuffer (floorMs);
    }

    // Slow decay — 1 ms per second once we've been clean for 60 s.
    // That earns ~60 ms back per minute of pristine network. The slow
    // schedule deliberately favours stability over latency-recovery:
    // a single bump event makes the buffer LARGER for hours, on the
    // assumption that whatever caused the bump is likely to recur. If
    // the network is genuinely clean for many minutes, autoExtra will
    // eventually drift back down. Called from the audio thread; cheap.
    void maybeDecay (int floorMs, double nowSec) noexcept
    {
        const int extra = autoExtraMs.load (std::memory_order_relaxed);
        if (extra <= 0) return;
        const double lastLoss = lastLossTimeSec.load (std::memory_order_relaxed);
        if (lastLoss > 0.0 && (nowSec - lastLoss) < 60.0) return;
        const double lastTick = lastDecayTickSec.load (std::memory_order_relaxed);
        if (lastTick > 0.0 && (nowSec - lastTick) < 1.0) return;
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
    // Output-mute state — silences this peer's output until the rate
    // loop has stabilised. The lock-in transient (loop running at fast
    // bw chasing a possibly-large initial err) can pin rcorr to its
    // ±5 % clamp for a second or two, which sounds like the audio is
    // sliding sharply out of tune before settling. Muting hides this.
    //
    // Adaptive unmute logic:
    //   1. Wait at least `rcMuteMinBlocks` (≈ 0.5 s) after (re)prime
    //      regardless of loop state — the rate-loop fast-bw response
    //      is itself ~0.3 s, so we shouldn't even check stability
    //      before then.
    //   2. After the min-time, check that |z2| has been below a tight
    //      threshold for `rcMuteStableNeed` consecutive blocks. z2
    //      reflects the proportional path output of the loop; it stays
    //      tiny in steady state regardless of what DC offset the
    //      integrator (z3) eventually carries. So this criterion works
    //      across both matched-rate and genuine-drift cases.
    //   3. Safety cap: force unmute after a generous timeout
    //      (~10 s) even if z2 never settles to threshold.
    bool   rcMuteActive          = false;
    int    rcMuteStableConsec    = 0;     // consecutive stable blocks
    int    rcMuteMinBlocksLeft   = 0;     // count down min-mute time first
    int    rcMuteBlocksRemaining = 0;     // safety-cap countdown

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

    // ---- Arrival-time DLL ------------------------------------------------
    //
    // The 3rd-order rate loop above can't separate slow clock drift (signal)
    // from fast network jitter (noise) when both arrive on the same scalar
    // (`fillNow`). On a same-machine loopback there is no jitter so the
    // loop locks easily; on a real cross-machine LAN, packet jitter (~ms)
    // dominates the err input to the loop and produces audible pitch
    // wobble at the 0.5 Hz fast-startup bandwidth, plus occasional anti-
    // windup-triggered glitches once the integrator drifts.
    //
    // To fix that we run a 2nd-order PLL/DLL on packet arrival times — one
    // update per sender period (HF_LAST_OF_PERIOD packets only). Its output
    // (filtered arrival time `dllT0`, filtered period duration `dllDt`) is
    // published via a single-producer/single-consumer seqlock-style pair
    // that the audio thread snapshots once per block. The audio thread then
    // computes the rate-loop's `err` from the DLL's predicted-write-pos
    // (interpolated between the two most recent anchors) instead of the
    // raw, jitter-modulated `fillNow`.
    //
    // Per-packet `tx_us` from the wire is folded into the
    // `smoothedNetSpread` telemetry only — the DLL math itself uses
    // arrival times (sender and receiver steady_clock origins differ; only
    // arrival deltas are clock-domain-consistent).
    double   dllT0          = 0.0;   // predicted arrival time of next sender period (rx-clock secs)
    double   dllDt          = 0.0;   // sender period duration in rx-clock secs (drift-tracking)
    double   dllW1          = 0.0;   // PI proportional gain
    double   dllW2          = 0.0;   // PI integral gain
    bool     dllInited      = false; // first-period bootstrap done

    // SPSC seqlock pair. NetThread writes the inactive slot and bumps
    // `dllSeq` by 2 with release ordering; audio thread acquire-loads,
    // reads the indexed slot, re-loads to verify stability.
    struct DllSnap { uint64_t k = 0; double t = 0.0; double dt = 0.0; };
    std::atomic<uint32_t> dllSeq { 0 };
    DllSnap               dllSlot[2];

    // Audio-side anchor pair (last two distinct snapshots) — interpolated
    // between to estimate the predicted write-pos at the current rx-clock
    // instant. Only the audio thread writes these.
    uint64_t dllAK0 = 0, dllAK1 = 0;
    double   dllAT0 = 0.0, dllAT1 = 0.0;
    double   dllADt = 0.0;
    bool     dllAudioPrimed   = false;
    int      dllAudioSnapsSeen = 0;

    // Telemetry: EMA of (arrival_rx - tx_us). Sender/receiver clock
    // origins differ across machines so the absolute value is unitless,
    // but stability over time is a useful network-health proxy.
    double   smoothedNetSpreadSec = 0.0;

    // Recompute DLL PI coefficients for a target bandwidth in Hz. Caller
    // must set `dllDt` first.
    void setDllBw (double bwHz) noexcept
    {
        const double w = 6.28318530718 * bwHz * dllDt;
        dllW1 = 3.0 * w;
        dllW2 = w * w;
    }

    void resetDll() noexcept
    {
        dllInited            = false;
        dllAudioPrimed       = false;
        dllAudioSnapsSeen    = 0;
        smoothedNetSpreadSec = 0.0;
    }

    // NetThread: publish a fresh anchor for the audio thread to consume.
    void publishDllSnap (uint64_t k, double t, double dt) noexcept
    {
        const uint32_t seq  = dllSeq.load (std::memory_order_relaxed);
        const int      slot = static_cast<int> ((seq >> 1) & 1u) ^ 1;
        dllSlot[slot].k  = k;
        dllSlot[slot].t  = t;
        dllSlot[slot].dt = dt;
        dllSeq.store (seq + 2, std::memory_order_release);
    }

    // Audio thread: snapshot the most recently published anchor.
    DllSnap readDllSnap() const noexcept
    {
        DllSnap snap;
        for (;;)
        {
            const uint32_t s1 = dllSeq.load (std::memory_order_acquire);
            const int      slot = static_cast<int> ((s1 >> 1) & 1u);
            snap.k  = dllSlot[slot].k;
            snap.t  = dllSlot[slot].t;
            snap.dt = dllSlot[slot].dt;
            const uint32_t s2 = dllSeq.load (std::memory_order_acquire);
            if (s1 == s2) return snap;
            // NetThread published mid-read; retry. Bounded — NetThread
            // only publishes at sender-period rate (every few ms).
        }
    }

    // NetThread: PI step + publish, called once per sender period when
    // an HF_LAST_OF_PERIOD packet arrives. arrivalSec is in the receiver's
    // steady_clock domain; endFrame is the absolute sender-frame index of
    // the start of the *next* period (i.e. data.count + nfram of the
    // packet that just arrived). txUs is the sender-side timestamp from
    // the packet header (used for telemetry only).
    void updateDll (uint64_t endFrame, double arrivalSec, uint64_t txUs) noexcept
    {
        if (sampleRate <= 0 || period <= 0) return;

        if (! dllInited)
        {
            // Bootstrap: seed period from sender's nominal SR and predict
            // the next period boundary one period from now in rx-clock.
            dllDt = (double) period / (double) sampleRate;
            setDllBw (0.05);
            // Anchor pair: pin (k = endFrame, t = arrivalSec). Predict the
            // next packet to arrive at arrivalSec + dllDt.
            publishDllSnap (endFrame, arrivalSec, dllDt);
            dllT0     = arrivalSec + dllDt;
            dllInited = true;
            // Telemetry seed.
            smoothedNetSpreadSec = arrivalSec - (double) txUs * 1e-6;
            return;
        }

        // PI step. Anti-jump clamp to ±dllDt absorbs single-period misses
        // (e.g. a lost HF_LAST_OF_PERIOD packet) without a wild jump.
        double err = arrivalSec - dllT0;
        if (err >  dllDt) err =  dllDt;
        if (err < -dllDt) err = -dllDt;
        dllT0 += dllW1 * err;
        dllDt += dllW2 * err;

        // Refresh PI coefficients in case dllDt drifted significantly
        // (clock skew slowly changes the operating point). Cheap.
        setDllBw (0.05);

        // Publish the just-filtered anchor — paired with the sender frame
        // index of *this* period boundary (endFrame). The audio thread
        // interpolates between this and the previously published anchor.
        publishDllSnap (endFrame, dllT0, dllDt);

        // Advance prediction by one period for the next packet.
        dllT0 += dllDt;

        // Telemetry EMA.
        constexpr double telBlend = 0.05;
        const double inst = arrivalSec - (double) txUs * 1e-6;
        smoothedNetSpreadSec = (1.0 - telBlend) * smoothedNetSpreadSec
                             + telBlend * inst;
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

    // Random per-process wire UID (mirrors SendStream::SendStream). Used in
    // outbound INVITE / UNINVITE / INVITE_ACK headers so the sender can
    // identify which receiver a given control packet came from. Also
    // republished in Bonjour TXT (field "wuid") so the sender's editor
    // can pair Bonjour-discovered receivers with its accepted-targets
    // entries by UID rather than fragile (host:port) match — the source
    // IP of an inbound INVITE doesn't always equal the Bonjour-published
    // IP on multi-homed hosts.
    std::random_device rd;
    std::mt19937 gen (rd());
    std::uniform_int_distribution<uint32_t> dist;
    receiverUid = dist (gen);
    if (receiverUid == 0) receiverUid = 1;
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

    // Reset anticipative-render detection so a fresh prepareToPlay starts
    // from a clean slate. (Reaper re-issues prepareToPlay when the host
    // sample rate, block size, or channel count changes.)
    pullLastSec       = 0.0;
    pullFastRunCount  = 0;
    pullSlowRunCount  = 0;
    pullInFastMode    = false;
    pullFastSilenceLogged = 0;

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
        // Rate-loop snapshot. The doubles are written only by the audio
        // thread; the message-thread read here is a benign torn-read
        // (each is a single 8-byte aligned store on macOS x86_64 / ARM)
        // and we don't need exact coherence — these are diagnostics, not
        // anything the snapshot consumer drives behaviour from.
        s.rcorr           = p->rcRcorr;
        s.rcZ3            = p->rcZ3;
        s.rcNominalRatio  = p->rcNominalRatio;
        s.smoothedNetSpreadMs = p->smoothedNetSpreadSec * 1000.0;
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

bool RecvStream::inviteSender (const juce::String& host, int port, std::uint32_t wireUid)
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
        // Match an existing entry first by UID (when the caller knows it
        // up front from the discovered row), then by host:port.
        AcceptedSender* match = nullptr;
        if (wireUid != 0)
            for (auto& s : accepted)
                if (s.uid == wireUid) { match = &s; break; }
        if (! match)
            for (auto& s : accepted)
                if (s.host == host && s.port == port) { match = &s; break; }

        if (match != nullptr)
        {
            match->state         = SenderState::Pending;
            match->rejectReason  = IAR_OK;
            match->addedAt       = nowT;
            match->lastInviteAt  = nowT;
            if (wireUid != 0) match->uid = wireUid;
            sendOneInvite (*match);
            return true;
        }

        AcceptedSender s;
        s.uid         = wireUid;
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

void RecvStream::uninviteSender (const juce::String& host, int port, std::uint32_t wireUid)
{
    // Two-pass eviction. First, find a matching entry — UID first when
    // the caller passed it (e.g. the user clicked Disconnect on a
    // Bonjour-paired row whose accepted entry's host:port may not match
    // the discovered IP because the sender's outbound interface differs
    // from its Bonjour-published IP), then host:port. Second, erase
    // EVERY entry matching either the captured host:port OR the
    // captured UID — catches duplicate / stale entries.
    AcceptedSender evicted;
    uint32_t evictedUid = 0;
    juce::String evictedHost;
    int evictedPort = 0;
    bool found = false;
    {
        const juce::ScopedLock sl (acceptedLock);
        if (wireUid != 0)
        {
            for (auto& s : accepted)
            {
                if (s.uid == wireUid)
                {
                    evicted     = s;
                    evictedUid  = s.uid;
                    evictedHost = s.host;
                    evictedPort = s.port;
                    found = true;
                    break;
                }
            }
        }
        if (! found)
        {
            for (auto& s : accepted)
            {
                if (s.host == host && s.port == port)
                {
                    evicted     = s;
                    evictedUid  = s.uid;
                    evictedHost = s.host;
                    evictedPort = s.port;
                    found = true;
                    break;
                }
            }
        }
        if (found)
        {
            for (auto it = accepted.begin(); it != accepted.end(); )
            {
                const bool hostPortMatch = (it->host == evictedHost && it->port == evictedPort);
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
    hdr->sender = receiverUid;
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
    hdr->ptype  = PT_UNINVITE;
    hdr->sender = receiverUid;
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
    hdr->ptype  = PT_INVITE_ACK;
    hdr->sender = receiverUid;
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

        // Match by wire UID — the only identity. Refresh the sockaddr we
        // use for outbound replies but leave host/port alone so they
        // stay the canonical identity used by the editor's row pairing.
        // Multi-homed sender source-IP differences and source-port
        // shifts across re-binds all fold into "same UID, refresh addr".
        if (hdr.sender != 0)
        {
            for (auto& s : accepted)
            {
                if (s.uid == hdr.sender)
                {
                    s.state = SenderState::Active;
                    s.addr  = senderAddr;
                    found = true;
                    break;
                }
            }
        }
        // Direct-IP race: we initiated via inviteSender(host, port) and
        // the peer's INVITE arrived before our INVITE_ACK could land.
        // Our entry still has uid=0; populate it from this INVITE rather
        // than spawning a duplicate. Bonjour-discovered entries carry
        // their wireUid up front (set by inviteSender from the editor),
        // so they don't take this branch.
        if (! found)
        {
            for (auto& s : accepted)
            {
                if (s.uid == 0 && s.host == hostStr && s.port == portInt)
                {
                    s.uid   = hdr.sender;
                    s.state = SenderState::Active;
                    s.addr  = senderAddr;
                    found = true;
                    break;
                }
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
    entry->resetDll();                                            // arrival-time DLL — bootstraps on first HF_LAST_OF_PERIOD packet
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
    std::cerr << "mcfx_recv: new peer uid=" << std::hex << uid << std::dec
              << " ch=" << entry->nchan
              << " placeholder period=" << entry->period
              << " placeholder sampleRate=" << entry->sampleRate
              << " outputBlockSize=" << outputBlockSize
              << " outputSampleRate=" << outputSampleRate
              << " ringFrames=" << entry->ringFrames
              << " prefillFrames=" << entry->prefillFrames.load()
              << " trigger=" << (hdr.ptype == PT_DATA ? "DATA"
                                : hdr.ptype == PT_DESC ? "DESC" : "other")
              << std::endl;
    return entry.get();
}

void RecvStream::parsePacket (const uint8_t* buf, int nbytes,
                              const sockaddr_in& from)
{
    if (nbytes < kHeaderBytes) return;

    // Capture arrival time as early as possible — feeds the per-peer
    // arrival-time DLL on HF_LAST_OF_PERIOD packets. Receiver-side
    // steady_clock is monotonic across macOS / Linux / Windows.
    const double arrivalSec = std::chrono::duration<double> (
        std::chrono::steady_clock::now().time_since_epoch()).count();

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
        const bool periodChanged = (d.period > 0
                                    && d.period != static_cast<uint32_t> (peer->period));
        if (periodChanged)
        {
            peer->period = static_cast<int> (d.period);
            // Don't resize the ring mid-flight — it'd race the audio
            // reader. The 8x outputBlockSize sizing accommodates most
            // sender periods; for unusually large sender periods we'd
            // need a re-allocation pass, deferred.
        }
        const bool rateChanged = (d.fsamp > 0
                                  && static_cast<int> (d.fsamp) != peer->sampleRate);
        if (d.fsamp > 0)
            peer->sampleRate = static_cast<int> (d.fsamp);
        peer->recomputeBuffer (jitterFloorMs.load (std::memory_order_relaxed));

        // Reset the rate-control loop when EITHER the sender's reported
        // sample rate or its period changes — both feed into the loop's
        // operating point. Sample-rate change shifts the resampler's
        // nominal output/input ratio (otherwise audio plays detuned).
        // Period change shifts the loop target (`prefill + period/2` is
        // the natural mean fill given the sender's burst size); without
        // a reset the already-accumulated z's wind up the integrator
        // against the new target and pin rcorr to its ±5% clamp,
        // producing detuning + glitching until the user rebuilds the
        // peer (e.g. by changing channel count on the sender).
        if (rateChanged || periodChanged)
        {
            std::cerr << "mcfx_recv: DESC reset uid="
                      << std::hex << hdr.sender << std::dec
                      << " period " << peer->period
                      << " (was " << (periodChanged ? "different" : "same") << ")"
                      << " sampleRate " << peer->sampleRate
                      << " (was " << (rateChanged ? "different" : "same") << ")"
                      << " outputSR=" << peer->outputSampleRate
                      << " outputBlock=" << peer->outputBlockSize
                      << std::endl;
            if (peer->outputSampleRate > 0 && peer->sampleRate > 0)
            {
                peer->rcNominalRatio = (double) peer->outputSampleRate
                                     / (double) peer->sampleRate;
                peer->resetRateLoop();
                peer->setLoopBw (0.5);
            }
            // Sender SR or period change invalidates dllDt (which is in
            // rx-clock secs per sender-period, scaled by sender's SR and
            // period). Re-bootstrap on the next HF_LAST_OF_PERIOD packet.
            peer->resetDll();
            peer->primed.store (false, std::memory_order_release);
        }
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

    // First DATA packet for this peer: align positions to the sender's
    // current count instead of leaving them at the default 0. Without
    // this we'd treat the sender's accumulated frame count as a "gap"
    // (bumping autoExtra) and trigger the ring overrun guard (because
    // fill = startFrame >> ringFrames). The spurious autoExtra bump in
    // particular kicks off a 5 s cycle: prefill grows on the bump,
    // collapses on decay, the rate loop overshoots, the ring drains
    // into underrun, underrun re-bumps autoExtra, repeat.
    if (! peer->haveSeenData)
    {
        peer->haveSeenData = true;
        peer->readFramePos.store  (startFrame, std::memory_order_release);
        peer->writeFramePos.store (startFrame, std::memory_order_release);
    }

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

    // Drive the per-peer arrival-time DLL once per sender period, on the
    // packet bearing HF_LAST_OF_PERIOD. Within-period packets are
    // intentionally ignored — their inter-packet spacing carries
    // sendto-burst noise rather than clock-drift information. The DLL's
    // filtered output is what the audio thread reads to compute the
    // rate-loop's err each block.
    if ((hdr.flags & HF_LAST_OF_PERIOD) != 0)
        peer->updateDll (endFrame, arrivalSec, data.tx_us);

    // Overrun guard. The receiver's adaptive resampler can only drain at
    // most ~5% faster than the sender writes — that's enough for clock-
    // drift compensation but not for a sender that's pumping multiples
    // of realtime (e.g. REAPER's anticipative-FX pre-roll calls
    // processBlock several times per audio cycle to fill plugin
    // lookahead buffers; the sender obediently ships every sample).
    //
    // Without this guard, the writer just laps the unread reader: the
    // ring physically only holds `ringFrames` worth of audio, but the
    // 64-bit write/read counters keep growing, so fillFramesNow() can
    // exceed ringFrames by huge amounts while the actual buffer
    // contents are silently overwritten — gibberish out, glitches
    // everywhere, anti-windup loop spinning forever.
    //
    // Detect when the new write would cross the reader (fill is about
    // to exceed ringFrames) and jump the read pointer forward to
    // `endFrame - prefillFrames` — drop the backlog, keep just enough
    // audio to satisfy a fresh prime. Drop primed so the audio thread
    // re-engages with a clean ring + zeroed loop state.
    {
        const auto curRead = peer->readFramePos.load (std::memory_order_acquire);
        const auto fill    = (endFrame > curRead) ? (endFrame - curRead) : 0;
        const auto cap     = (uint64_t) peer->ringFrames;
        if (fill > cap)
        {
            const int  keep    = juce::jmin ((int) cap / 2,
                                             peer->prefillFrames.load (std::memory_order_acquire));
            const auto newRead = endFrame - (uint64_t) keep;
            peer->readFramePos.store (newRead, std::memory_order_release);
            peer->primed.store      (false,   std::memory_order_release);
            peer->resetRateLoop();
            peer->underruns.fetch_add (1, std::memory_order_relaxed);
            // One-line stderr so this is visible in normal operation —
            // a sustained overrun stream means the sender is producing
            // faster than realtime (anticipative FX, offline render,
            // etc.) and the receiver can't stay locked. Throttle to
            // every 50th to avoid spamming the terminal.
            const auto cnt = peer->underruns.load (std::memory_order_relaxed);
            if (cnt < 5 || cnt % 50 == 0)
                std::cerr << "mcfx_recv: ring overrun #" << cnt
                          << " uid=" << std::hex << peer->uid << std::dec
                          << " fill=" << fill << " > ringFrames=" << cap
                          << " (sender is producing > realtime; dropping "
                          << (fill - keep) << " frames)" << std::endl;
        }
    }

    // Become primed once the ring fill crosses the rate-loop's TARGET
    // fill — not just the prefill threshold. The loop targets
    // `prefillFrames + period/2` (the natural mean fill given the
    // sender's burst size); priming exactly at prefillFrames means the
    // loop sees err = -period/2 sustained on the first audio block,
    // pinning rcorr to its +5% clamp and spinning the integrator into
    // anti-windup → reset → re-prime → repeat. Waiting for the target
    // means the loop starts on its operating point with err ≈ 0.
    if (! peer->primed.load (std::memory_order_relaxed))
    {
        const int prefill = peer->prefillFrames.load (std::memory_order_acquire);
        const int target  = prefill + (peer->period / 2);
        const auto fill   = peer->fillFramesNow();
        if (fill >= target)
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

    // ---------------------------------------------------------------------
    // Anticipative-render detection. REAPER (and a few other hosts) can
    // call processBlock at multiples of realtime to pre-render plugin
    // output for the buffered playback queue. For a network-streaming
    // receiver this is fundamentally impossible — the future audio
    // doesn't exist yet (the sender is generating it in real time).
    // Letting the rate loop fight that mode produces the canonical
    // pitch-up + glitch cycle (rcorr pins at +5% clamp, anti-windup
    // trips, re-prime, repeat).
    //
    // Detection: compare the wallclock interval between consecutive
    // pullAndMix calls against the expected blockSize / sampleRate.
    // Hysteresis is set high (~64 sustained fast blocks ≈ 80 ms at
    // 48k/64) to ignore transient OS-scheduling catch-up bursts. In
    // fast mode we output silence and bypass the rate loop — letting
    // the user notice anticipative-FX is on rather than producing
    // glitchy audio.
    if (outputSampleRate > 0 && blockSize > 0)
    {
        const double expectedSec  = static_cast<double> (blockSize)
                                  / static_cast<double> (outputSampleRate);
        if (pullLastSec > 0.0)
        {
            const double interval = nowSec - pullLastSec;
            if (interval < expectedSec * 0.5)
            {
                pullFastRunCount++;
                pullSlowRunCount = 0;
                if (pullFastRunCount >= 64 && ! pullInFastMode)
                {
                    pullInFastMode = true;
                    pullFastSilenceLogged++;
                    std::cerr << "mcfx_recv: host calling pullAndMix at "
                              << static_cast<int> (expectedSec * 1000.0
                                                   / juce::jmax (1e-9, interval))
                              << "x realtime — likely anticipative-FX "
                                 "pre-render. Future network audio "
                                 "doesn't exist; outputting silence. "
                                 "Disable anticipative FX in the host. "
                                 "(occurrence #"
                              << pullFastSilenceLogged << ")"
                              << std::endl;
                }
            }
            else if (interval > expectedSec * 0.8)
            {
                pullSlowRunCount++;
                pullFastRunCount = 0;
                if (pullSlowRunCount >= 16 && pullInFastMode)
                {
                    pullInFastMode = false;
                    pullFastSilenceLogged = 0;
                    std::cerr << "mcfx_recv: host returned to realtime "
                                 "pacing — resuming network audio."
                              << std::endl;
                    // Force a clean re-prime — DLL anchor pair and ring
                    // fill have drifted relative to the audio thread's
                    // tNow during the fast burst, so the rate loop's
                    // err signal would explode if we just continued.
                    for (const auto& p : *snap)
                    {
                        if (! p) continue;
                        p->primed.store (false, std::memory_order_release);
                        p->resetRateLoop();
                    }
                }
            }
            // Else: interval is in the "uncertain" middle band; don't
            // change run counts. Avoids flapping on a single weird block.
        }
        pullLastSec = nowSec;
    }
    if (pullInFastMode)
    {
        // Output silence (caller already cleared the buffer per pullAndMix
        // contract). Leave rate loop / DLL state untouched so we resume
        // cleanly when fast mode ends.
        return;
    }

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

        // Smoothed prefill — exponentially low-passes the discrete
        // prefillFrames so autoExtra bumps/decays don't jolt the rate
        // loop's target. Time constant must be SLOWER than the loop's
        // slow-bw settling time (~3.2 s at 0.05 Hz) — otherwise the
        // loop chases the moving target and wobbles. We use 30 s, which
        // is a full order of magnitude slower than the loop and absorbs
        // both the autoExtra bump (~20 ms appears, peer must ramp up
        // to it gracefully) and the eventual decay (~200 ms drop, loop
        // sees a barely-moving target). User-initiated floor changes
        // also propagate at 30 s — acceptable since those are rare and
        // the priming threshold (which controls actual buffered audio
        // depth) responds immediately. On (re)prime we snap to the
        // current prefill to avoid a ramp-in transient.
        if (! p->prefillSmoothInited)
        {
            p->prefillSmoothFrames = (double) prefill;
            p->prefillSmoothInited = true;
        }
        else
        {
            // alpha = 1 - exp(-blockTime / tau) for tau = 30 s.
            // For blockTime ≈ 1.33 ms (64 frames @ 48 kHz), alpha ≈ 4.4e-5.
            const double alpha = (outputBlockSize > 0 && outputSampleRate > 0)
                ? (1.0 - std::exp (-(double) outputBlockSize
                                    / ((double) outputSampleRate * 30.0)))
                : 4.4e-5;
            p->prefillSmoothFrames += alpha * ((double) prefill - p->prefillSmoothFrames);
        }
        const int loopTarget = (int) std::lround (p->prefillSmoothFrames + (double) p->period / 2.0);

        // First block after (re)priming: zero the rate loop, drop any
        // stale DLL anchor pair, and engage at fast bandwidth. Must run
        // BEFORE the err-shape selection below — otherwise a stale anchor
        // pair from a prior epoch would feed an extrapolation made at a
        // wildly different rx-clock instant into the new loop.
        if (! p->rcInited)
        {
            p->resetRateLoop();
            p->setLoopBw (0.5);
            p->rcInited = true;
            p->rcBlocksSinceLock = 0;
            p->dllAudioPrimed   = false;
            p->dllAudioSnapsSeen = 0;
            // Snap smoothed prefill to current prefill so re-prime
            // doesn't introduce its own tracking transient.
            p->prefillSmoothInited = false;
            // Mute the output until the rate loop stabilises. Min-mute
            // covers the fast-bw response time (~0.5 s); after that we
            // check |z2| < 5e-5 sustained as the unmute trigger. A
            // generous safety cap (≈ 10 s) force-unmutes if the loop
            // never quite settles by criterion.
            p->rcMuteActive = true;
            p->rcMuteStableConsec = 0;
            if (outputSampleRate > 0 && outputBlockSize > 0)
            {
                const int blocksPerSec = outputSampleRate / outputBlockSize;
                p->rcMuteMinBlocksLeft   = blocksPerSec / 2;     // 0.5 s
                p->rcMuteBlocksRemaining = blocksPerSec * 10;    // 10 s safety
            }
            else
            {
                p->rcMuteMinBlocksLeft   = 375;
                p->rcMuteBlocksRemaining = 7500;
            }
        }

        // Snapshot the latest DLL anchor and shift our two-anchor pair
        // when a new one is observed. Both flags are written only by this
        // (audio) thread, so no atomic.
        const auto newest = p->readDllSnap();
        if (! p->dllAudioPrimed)
        {
            if (newest.dt > 0.0)
            {
                if (p->dllAudioSnapsSeen == 0)
                {
                    // First snapshot seeds both anchors identically. Cold
                    // path remains active until a second distinct anchor.
                    p->dllAK0 = p->dllAK1 = newest.k;
                    p->dllAT0 = p->dllAT1 = newest.t;
                    p->dllADt = newest.dt;
                    ++p->dllAudioSnapsSeen;
                }
                else if (newest.k != p->dllAK1)
                {
                    p->dllAK0 = p->dllAK1;
                    p->dllAT0 = p->dllAT1;
                    p->dllAK1 = newest.k;
                    p->dllAT1 = newest.t;
                    p->dllADt = newest.dt;
                    p->dllAudioPrimed = true;
                    ++p->dllAudioSnapsSeen;
                }
            }
        }
        else if (newest.k != p->dllAK1)
        {
            p->dllAK0 = p->dllAK1;
            p->dllAT0 = p->dllAT1;
            p->dllAK1 = newest.k;
            p->dllAT1 = newest.t;
            p->dllADt = newest.dt;
        }

        // err signal feeding the 3rd-order rate loop. With the DLL primed
        // (≥ 2 distinct anchors observed), we use a linearly-interpolated
        // predicted-write-pos that filters network jitter out of the err
        // — see arrival-time DLL block in PeerStream above. Until then,
        // fall back to the raw fill so behaviour matches pre-DLL on the
        // very first block(s) after a re-prime. The 3rd-order loop math
        // (z1/z2/z3, anti-windup, ratio clamp, bw schedule) is unchanged.
        // Loop target adjusted for the err signal we use. The cold path
        // err = fillNow - target needs a +period/2 offset because fillNow
        // is BURSTY (oscillates between prefill and prefill+period at
        // sender's packet cadence; mean is prefill+period/2). The DLL
        // path err = predictedWriteNow - readPos - target uses a CONTINUOUS
        // predictedWriteNow that already sits +period/2 above the bursty
        // writeFramePos on average — so the right target there is
        // prefill+period (not prefill+period/2). Without this distinction
        // the DLL path drives mean(fillNow) down to prefill (= the priming
        // threshold), risking underrun on every minor jitter.
        double err;
        if (p->dllAudioPrimed && p->dllAT1 != p->dllAT0)
        {
            const double tNow = std::chrono::duration<double> (
                std::chrono::steady_clock::now().time_since_epoch()).count();
            const double d1   = tNow      - p->dllAT0;
            const double d2   = p->dllAT1 - p->dllAT0;
            const double kSpan = (double) (int64_t) (p->dllAK1 - p->dllAK0);
            const double predictedWriteNow =
                (double) p->dllAK0 + kSpan * (d1 / d2);

            // The DLL's predictedWriteNow grows smoothly between snap
            // updates; the actual writeFramePos is stair-step, jumping
            // by (period / N_packets_per_period) at each packet arrival.
            // Mean(predictedWriteNow − writeFramePos) = period_per_packet
            // / 2. For 1 packet-per-period (low channel count) that's
            // period/2; for many packets-per-period (high channel count
            // with MTU-fragmented audio) it's small. We need to add this
            // offset to dllTarget so the loop drives mean(fillNow) to
            // the same operating point regardless of packet count.
            const int nPackets   = packetsPerPeriod (p->period, p->nchan, p->fmt);
            const double offset  = (nPackets > 0)
                                    ? ((double) p->period / (2.0 * (double) nPackets))
                                    : 0.0;
            const double dllTarget = p->prefillSmoothFrames
                                   + (double) p->period / 2.0
                                   + offset;
            err = (predictedWriteNow - (double) curRead0) - dllTarget;
        }
        else
        {
            // Cold path — no two-anchor pair yet (new peer / re-prime).
            err = (double) (fillNow - loopTarget);
        }

        // Periodic state log — only enabled if MCFX_NET_DEBUG=1 in the
        // environment. cerr writes from the audio thread are too slow to
        // run on every block; even once per second of unconditional
        // logging perturbs realtime scheduling enough to push the test
        // suite over its audio-overrun budget. Gating by env var keeps
        // the logging available for in-DAW diagnosis ("export
        // MCFX_NET_DEBUG=1 ; open -a REAPER") without perturbing
        // production runs.
        static const bool sLogEnabled = std::getenv ("MCFX_NET_DEBUG") != nullptr;
        if (sLogEnabled && outputBlockSize > 0 && outputSampleRate > 0)
        {
            const int logEvery = juce::jmax (1, outputSampleRate / outputBlockSize);
            static thread_local int logCounter = 0;
            if ((++logCounter % logEvery) == 1)
            {
                std::cerr << "mcfx_recv: rcorr=" << p->rcRcorr
                          << " z3=" << p->rcZ3
                          << " z2=" << p->rcZ2
                          << " err=" << err
                          << " fill=" << fillNow
                          << " prefill=" << prefill
                          << " autoExtra=" << p->autoExtraMs.load()
                          << " period=" << p->period
                          << " dllPrimed=" << (p->dllAudioPrimed ? "y" : "n")
                          << std::endl;
            }
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
            std::cerr << "mcfx_recv: anti-windup reset uid="
                      << std::hex << p->uid << std::dec
                      << " z3=" << p->rcZ3
                      << " rcorr=" << p->rcRcorr
                      << " fill=" << fillNow << " target=" << loopTarget
                      << " period=" << p->period
                      << std::endl;
            p->resetRateLoop();
            p->primed.store (false, std::memory_order_release);
            // Geometric autoExtra bump (20 ms minimum). The output mute
            // mechanism hides convergence transients audibly, so we
            // don't need to jump to the cap on the first event — gradual
            // growth keeps steady-state latency low on clean networks
            // where jitter is small (e.g. wired LAN).
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
            const auto un = p->underruns.fetch_add (1, std::memory_order_relaxed) + 1;
            if (un < 5 || un % 50 == 0)
                std::cerr << "mcfx_recv: underrun #" << un
                          << " uid=" << std::hex << p->uid << std::dec
                          << " avail=" << avail << " wantIn=" << wantIn
                          << " ratio=" << ratio
                          << " period=" << p->period
                          << " prefill=" << prefill
                          << " target=" << loopTarget
                          << std::endl;
            p->primed.store (false, std::memory_order_release);
            // Geometric autoExtra bump. Mute mechanism hides the brief
            // convergence transient after re-prime, so we don't need
            // to jump to cap; gradual growth keeps wired-LAN latency
            // low while still climbing fast enough to handle bursty
            // senders within a few events.
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

        // Adaptive un-mute. We sum the resampler output into the host
        // buffer UNLESS the peer is still muted from a recent (re)prime.
        // While muted, we count consecutive blocks where the rate loop
        // appears stable (rcorr within ±1 % of unity); after 50
        // consecutive stable blocks (~67 ms @ 48 k/64) we unmute. A
        // safety countdown also force-unmutes after the fast-bw window
        // elapses regardless, so a never-stabilising stream eventually
        // emits audio.
        if (p->rcMuteActive)
        {
            // First serve the min-mute window (loop hasn't had time to
            // do anything meaningful yet). After that, unmute as soon
            // as rcorr is close to unity (within 1000 ppm) for 100
            // consecutive blocks (~133 ms @ 48 k/64). This covers the
            // matched-rate case (rcorr ≈ 1.0 ± a few ppm of test-
            // driver thread bias) and reasonable real-drift cases
            // (most clock mismatches are well under 1000 ppm). For
            // extreme drift the safety cap eventually fires.
            if (p->rcMuteMinBlocksLeft > 0)
            {
                p->rcMuteMinBlocksLeft--;
            }
            else if (std::abs (p->rcRcorr - 1.0) < 1e-3)
            {
                if (++p->rcMuteStableConsec >= 100)
                    p->rcMuteActive = false;
            }
            else
            {
                p->rcMuteStableConsec = 0;
            }
            if (--p->rcMuteBlocksRemaining <= 0)
                p->rcMuteActive = false;
        }

        if (! p->rcMuteActive)
        {
            const float* src = p->resOutScratch.data();
            const int nFramesOut = (int) rr.output_generated;
            for (int f = 0; f < nFramesOut; ++f)
                for (int c = 0; c < mixCh; ++c)
                    out[c][f] += src[f * chans + c];
        }

        // Advance the read pointer by what the resampler actually used,
        // not what we offered. Keeps the ring fill arithmetic exact.
        p->readFramePos.store (curRead + (uint64_t) rr.input_used,
                               std::memory_order_release);
    }
}

}} // namespace mcfx::net
