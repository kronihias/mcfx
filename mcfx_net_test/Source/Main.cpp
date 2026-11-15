/*
  ==============================================================================

  mcfx_net_loopback_test — single-process integration test for SendStream
  + RecvStream over a localhost UDP loopback.

  This is the heart of the network-audio test suite. Both stream classes
  are instantiated directly (no DAW, no audio device, no Bonjour) and
  driven from paced helper threads, so we can simulate clock drift,
  pre-roll bursts, and long-running stability without any host-side
  variation.

  CLI:
    mcfx_net_loopback_test --scenario <name> [options...]
    mcfx_net_loopback_test --rtwake-test     (RtWake unit check, exit 0 = pass)

  Scenarios are documented in the kScenarios table below. Most common:
    smoke           5 s    basic connect + audio
    passthrough    10 s    sine-wave round-trip fidelity
    drift_match    30 s    matched SR, no-underrun verification
    drift_small    60 s    ±200 ppm SR mismatch, convergence
    drift_large    60 s    ±2000 ppm
    drift_extreme  30 s    ±50000 ppm, must degrade gracefully
    burst          60 s    2× sender push for 5 s, recovery
    channel_change 30 s    mid-stream channel-count change
    reconnect      60 s    10× connect/disconnect cycles
    reprepare      18 s    sender prepareToPlay re-run mid-stream
    stall          18 s    sender stops pushing for 50 ms mid-stream
    split_fast_mode 12 s   pre-render burst, then periods split into sub-calls
    bunch_fast_mode 12 s   pre-render burst, then full blocks bunched per device period
    counter_rewind 18 s    sender frame counter jumps back to 0 mid-stream
    counter_wrap   18 s    sender frame counter wraps 2^32 mid-stream
    soak          14400 s  hours-long matched-rate run

  Pacing: every audio block is scheduled by `std::chrono::steady_clock`
  + `sleep_until`. The "sender clock" thread mirrors the sender plugin's
  audio thread (calls SendStream::pushAudioBlock); the "receiver clock"
  thread mirrors the receiver plugin's audio thread (calls
  RecvStream::pullAndMix into a capture buffer). Crucially, pacing is
  realtime — the SendThread inside SendStream uses the wall clock for
  outbound rate limiting, so synthetic time would defeat the purpose.

  Metrics output (--metrics-out path):
    JSON with summary fields the pytest harness asserts on. Sampled
    once per second during the run, plus a final summary block.

  Exit code:
    0 — scenario ran to completion. (Pass/fail interpretation lives
        in the pytest wrapper; the binary just reports.)
    1 — internal error (couldn't bind, couldn't connect, etc.).

  ==============================================================================
*/

#include <JuceHeader.h>

#include "mcfx_net_send.h"
#include "mcfx_net_recv.h"
#include "mcfx_net_rt.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace mcfx::net;

// ---------------------------------------------------------------------------
// CLI args
// ---------------------------------------------------------------------------
namespace {
struct Options
{
    juce::String scenario      = "smoke";
    double       durationSec   = 5.0;
    double       senderSampleRate = 48000.0;   // reported via prepareToPlay + DESC
    double       recvSampleRate   = 48000.0;   // reported via prepareToPlay
    // Wallclock pacing for the test driver's helper threads. Defaults to
    // the corresponding SR, which simulates "two devices that report the
    // truth". Setting them differently from the reported SR simulates
    // unreported clock drift (the ONLY thing the rate-control loop can
    // actually compensate for — reported SR mismatch is absorbed by the
    // resampler's nominal output/input ratio with no loop work needed).
    // 0 means "use the reported SR" (the default behaviour).
    double       senderPaceSampleRate = 0.0;
    double       recvPaceSampleRate   = 0.0;
    int          senderPeriod  = 64;
    int          recvPeriod    = 64;
    int          channels      = 4;
    double       burstMultiplier = 1.0;   // sender push rate as multiple of realtime, scenario-specific
    double       burstDurationSec = 0.0;  // how long the burst lasts (then back to 1×)
    // Capture buffer length, in seconds. Default keeps a short ring for
    // the smoke / passthrough fidelity tests; the pitch-stability test
    // bumps it up to cover the full duration. Capped per-scenario to
    // avoid 11 GB allocations on the 4-hour soak run.
    double       captureSeconds = 10.0;
    // Host-behaviour events (scenario-specific, off by default).
    int          splitCalls     = 1;      // push each sender period as this many back-to-back calls
    int          bunchCalls     = 1;      // push this many whole periods back-to-back, then wait for all of them
    double       reprepareAtSec = -1.0;   // re-run SendStream::prepareToPlay at this time
    double       stallAtSec     = -1.0;   // stop pushing for stallMs at this time
    double       stallMs        = 50.0;   // length of the --stall-at / --reprepare-at gap
    double       rewindAtSec    = -1.0;   // set the sender frame counter to 0 at this time
    double       counterStart   = -1.0;   // seed the sender frame counter before the run
    // Report tail_* metrics: is the end of the run still carrying audio?
    // Needs captureSeconds >= durationSec.
    bool         tailCheck      = false;
    juce::String metricsOut    = {};
    bool         verbose       = false;

    double effectiveSenderPace() const
    {
        return senderPaceSampleRate > 0.0 ? senderPaceSampleRate : senderSampleRate;
    }
    double effectiveRecvPace() const
    {
        return recvPaceSampleRate > 0.0 ? recvPaceSampleRate : recvSampleRate;
    }
};

bool parseArgs (int argc, char** argv, Options& o)
{
    auto need = [&] (int i) {
        if (i >= argc) { std::cerr << "missing arg after " << argv[i - 1] << "\n"; return false; }
        return true;
    };
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if      (a == "--scenario")        { if (! need (++i)) return false; o.scenario = argv[i]; }
        else if (a == "--duration-sec")    { if (! need (++i)) return false; o.durationSec = std::atof (argv[i]); }
        else if (a == "--sender-sr")       { if (! need (++i)) return false; o.senderSampleRate = std::atof (argv[i]); }
        else if (a == "--recv-sr")         { if (! need (++i)) return false; o.recvSampleRate = std::atof (argv[i]); }
        else if (a == "--sender-pace-sr")  { if (! need (++i)) return false; o.senderPaceSampleRate = std::atof (argv[i]); }
        else if (a == "--recv-pace-sr")    { if (! need (++i)) return false; o.recvPaceSampleRate = std::atof (argv[i]); }
        else if (a == "--sender-period")   { if (! need (++i)) return false; o.senderPeriod = std::atoi (argv[i]); }
        else if (a == "--recv-period")     { if (! need (++i)) return false; o.recvPeriod = std::atoi (argv[i]); }
        else if (a == "--channels")        { if (! need (++i)) return false; o.channels = std::atoi (argv[i]); }
        else if (a == "--burst-multiplier"){ if (! need (++i)) return false; o.burstMultiplier = std::atof (argv[i]); }
        else if (a == "--burst-duration")  { if (! need (++i)) return false; o.burstDurationSec = std::atof (argv[i]); }
        else if (a == "--split-calls")     { if (! need (++i)) return false; o.splitCalls = std::atoi (argv[i]); }
        else if (a == "--bunch-calls")     { if (! need (++i)) return false; o.bunchCalls = std::atoi (argv[i]); }
        else if (a == "--reprepare-at")    { if (! need (++i)) return false; o.reprepareAtSec = std::atof (argv[i]); }
        else if (a == "--stall-at")        { if (! need (++i)) return false; o.stallAtSec = std::atof (argv[i]); }
        else if (a == "--stall-ms")        { if (! need (++i)) return false; o.stallMs = std::atof (argv[i]); }
        else if (a == "--rewind-at")       { if (! need (++i)) return false; o.rewindAtSec = std::atof (argv[i]); }
        else if (a == "--counter-start")   { if (! need (++i)) return false; o.counterStart = std::atof (argv[i]); }
        else if (a == "--metrics-out")     { if (! need (++i)) return false; o.metricsOut = argv[i]; }
        else if (a == "-v" || a == "--verbose") { o.verbose = true; }
        else if (a == "-h" || a == "--help")
        {
            std::cout << "mcfx_net_loopback_test — see Main.cpp header for scenarios\n";
            return false;
        }
        else { std::cerr << "unknown arg: " << a << "\n"; return false; }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test signal generator. Single-frequency sine, channel-multiplexed by
// adding a small frequency offset per channel so we can verify channel
// integrity (no cross-channel mixing).
// ---------------------------------------------------------------------------
class SineGen
{
public:
    SineGen (double sampleRate, int numChannels)
        : sr (sampleRate), nch (numChannels)
    {
        phase.assign ((size_t) nch, 0.0);
    }

    void fill (float* const* chans, int numFrames)
    {
        // Channel k → 1000 + 100*k Hz.
        for (int c = 0; c < nch; ++c)
        {
            const double freq  = 1000.0 + 100.0 * c;
            const double dphi  = 2.0 * M_PI * freq / sr;
            float* dst = chans[c];
            double ph = phase[(size_t) c];
            for (int f = 0; f < numFrames; ++f)
            {
                dst[f] = static_cast<float> (0.5 * std::sin (ph));
                ph += dphi;
                if (ph > 2.0 * M_PI) ph -= 2.0 * M_PI;
            }
            phase[(size_t) c] = ph;
        }
    }

private:
    double sr;
    int    nch;
    std::vector<double> phase;
};

// ---------------------------------------------------------------------------
// Scope for the test driver. Owns both streams + paced threads + capture.
// ---------------------------------------------------------------------------
struct Driver
{
    Options opts;

    SendStream send;
    RecvStream recv;

    // Capture buffer: one row per receiver channel, raw concatenated.
    // Used by passthrough fidelity tests; we don't need to buffer the
    // entire long-running scenarios so it's pre-sized small and we drop
    // overflow.
    std::mutex             captureMutex;
    std::vector<std::vector<float>> capture;
    int                    captureMaxFrames = 0;
    int                    captureFramesNow = 0;

    // Sampled per-second metrics for trend analysis.
    struct PerSecond {
        double t;
        int    fillFrames;
        double rcorr;
        double rcZ3;
        uint64_t underruns;
        uint64_t recvPackets;
        uint64_t sentPackets;
    };
    std::vector<PerSecond> trace;

    // Counters at scenario end.
    uint64_t finalUnderruns       = 0;
    uint64_t finalRecvPackets     = 0;
    uint64_t finalSentPackets     = 0;
    uint64_t finalAudioOverruns   = 0;   // sender-side push failures
    uint64_t finalGapFrames       = 0;
    uint64_t finalDroppedReorder  = 0;
    uint64_t finalStreamRestarts  = 0;
    uint32_t finalFastModeEntries = 0;
    bool     finalFastModeActive  = false;
    int      finalFillFrames      = 0;
    double   finalRcorr           = 1.0;
    double   finalRcZ3            = 0.0;
    double   finalRcNominal       = 1.0;
    bool     fillStable           = true;  // fill stayed within ±3*period of target after settle
    int      peakFillFrames       = 0;
    bool     peerEverPrimed       = false;
    bool     internalError        = false;
    juce::String errorMsg;

    std::atomic<bool> stopFlag { false };

    Driver (const Options& o) : opts (o) {}

    bool setup()
    {
        const int recvPort = recv.bind (0);   // ephemeral
        if (recvPort <= 0) { errorMsg = "RecvStream bind failed"; return false; }
        recv.prepareToPlay (opts.recvSampleRate, opts.recvPeriod, opts.channels);

        send.prepareToPlay (opts.senderSampleRate, opts.senderPeriod,
                            opts.channels, SF_FLOAT32);

        // Direct-IP: tell the sender to push to the receiver's bound port.
        // Pass the receiver's wireUid up front so the rate-loop has a
        // chance to pair correctly from the start.
        send.addTarget ("127.0.0.1", recvPort, recv.getReceiverUid());

        if (opts.counterStart >= 0.0)
            send.setFrameCounterForTesting ((uint32_t) opts.counterStart);

        // Pre-size the capture buffer for the passthrough fidelity test
        // (10 s max). Larger scenarios will just stop appending past
        // captureMaxFrames; we only need the first few seconds for FFTs.
        captureMaxFrames = (int) (opts.recvSampleRate
                                  * juce::jmin (opts.durationSec, opts.captureSeconds));
        capture.assign ((size_t) opts.channels, std::vector<float> ((size_t) captureMaxFrames, 0.0f));
        return true;
    }

    void teardown()
    {
        send.releaseResources();
        recv.releaseResources();
    }

    // Sender clock thread: paces SendStream::pushAudioBlock at
    // (senderPeriod / senderSampleRate) seconds per block, optionally
    // multiplied by burstMultiplier for the first burstDurationSec
    // seconds of the run. After that, returns to realtime.
    void senderClock()
    {
        SineGen gen (opts.senderSampleRate, opts.channels);
        std::vector<std::vector<float>> chanBuf ((size_t) opts.channels,
            std::vector<float> ((size_t) opts.senderPeriod, 0.0f));
        std::vector<float*> chanPtrs ((size_t) opts.channels);
        for (int c = 0; c < opts.channels; ++c)
            chanPtrs[(size_t) c] = chanBuf[(size_t) c].data();

        const auto start = std::chrono::steady_clock::now();
        auto next = start;
        // Pace at the *actual* sender rate, not the *reported* one.
        // Defaults to reported (no drift); set --sender-pace-sr to
        // simulate a sender clock that drifts relative to what its
        // DESC packet advertises.
        const double basePeriodSec = (double) opts.senderPeriod / opts.effectiveSenderPace();

        // Sub-call layout for --split-calls: the period is pushed as
        // `splitCalls` back-to-back calls, the way a host that splits its
        // device period (automation breakpoints, sub-block scheduling)
        // calls processBlock.
        const int splits = juce::jlimit (1, opts.senderPeriod, opts.splitCalls);
        // --bunch-calls: a device period of several full host blocks, all
        // processed back-to-back when the device asks (WASAPI shared mode
        // with small blocks, aggregate / Bluetooth devices).
        const int bunch = juce::jmax (1, opts.bunchCalls);
        std::vector<float*> subPtrs ((size_t) opts.channels);

        bool reprepared = false;
        bool stalled    = false;
        bool rewound    = false;

        while (! stopFlag.load (std::memory_order_acquire))
        {
            std::this_thread::sleep_until (next);

            const double elapsedSec = std::chrono::duration<double>
                (std::chrono::steady_clock::now() - start).count();

            // Host re-activates the plug-in: audio stops briefly, then
            // prepareToPlay runs again with the same settings.
            if (! reprepared && opts.reprepareAtSec >= 0.0 && elapsedSec >= opts.reprepareAtSec)
            {
                reprepared = true;
                std::this_thread::sleep_for (std::chrono::duration<double, std::milli> (opts.stallMs));
                send.prepareToPlay (opts.senderSampleRate, opts.senderPeriod,
                                    opts.channels, SF_FLOAT32);
                next = std::chrono::steady_clock::now();   // no catch-up burst
            }
            // Host stops calling processBlock for a moment (CPU spike,
            // device hiccup) without re-preparing.
            if (! stalled && opts.stallAtSec >= 0.0 && elapsedSec >= opts.stallAtSec)
            {
                stalled = true;
                std::this_thread::sleep_for (std::chrono::duration<double, std::milli> (opts.stallMs));
                next = std::chrono::steady_clock::now();
            }
            if (! rewound && opts.rewindAtSec >= 0.0 && elapsedSec >= opts.rewindAtSec)
            {
                rewound = true;
                send.setFrameCounterForTesting (0);
            }

            const double mult = (opts.burstDurationSec > 0.0
                                 && elapsedSec < opts.burstDurationSec)
                                  ? opts.burstMultiplier : 1.0;
            const double thisPeriodSec = basePeriodSec / mult;

            for (int b = 0; b < bunch; ++b)
            {
                gen.fill (chanPtrs.data(), opts.senderPeriod);
                int offset = 0;
                for (int k = 0; k < splits; ++k)
                {
                    const int n = (k == splits - 1) ? opts.senderPeriod - offset
                                                    : opts.senderPeriod / splits;
                    for (int c = 0; c < opts.channels; ++c)
                        subPtrs[(size_t) c] = chanPtrs[(size_t) c] + offset;
                    send.pushAudioBlock (subPtrs.data(), opts.channels, n);
                    offset += n;
                }
            }

            next += std::chrono::duration_cast<std::chrono::steady_clock::duration> (
                std::chrono::duration<double> (thisPeriodSec * bunch));
        }
    }

    // Receiver clock thread: paces RecvStream::pullAndMix at
    // (recvPeriod / recvSampleRate) seconds per block. Output samples
    // are appended to the capture buffer up to captureMaxFrames.
    void receiverClock()
    {
        std::vector<std::vector<float>> chanBuf ((size_t) opts.channels,
            std::vector<float> ((size_t) opts.recvPeriod, 0.0f));
        std::vector<float*> chanPtrs ((size_t) opts.channels);
        for (int c = 0; c < opts.channels; ++c)
            chanPtrs[(size_t) c] = chanBuf[(size_t) c].data();

        const auto start = std::chrono::steady_clock::now();
        auto next = start;
        // Pace at the *actual* receiver rate. With --recv-pace-sr ==
        // recv-sr (the default), the loop has no work to do (rcorr ≈ 1).
        // Setting them differently simulates an unreported clock drift
        // — exactly the regime the rate-control loop is built to fix.
        const double periodSec = (double) opts.recvPeriod / opts.effectiveRecvPace();

        while (! stopFlag.load (std::memory_order_acquire))
        {
            std::this_thread::sleep_until (next);

            // Zero the per-block output (pullAndMix accumulates).
            for (int c = 0; c < opts.channels; ++c)
                std::memset (chanPtrs[(size_t) c], 0,
                             (size_t) opts.recvPeriod * sizeof (float));

            recv.pullAndMix (chanPtrs.data(), opts.channels, opts.recvPeriod);

            // Append to capture (up to captureMaxFrames).
            {
                std::lock_guard<std::mutex> lk (captureMutex);
                if (captureFramesNow < captureMaxFrames)
                {
                    const int room  = captureMaxFrames - captureFramesNow;
                    const int copyN = juce::jmin (opts.recvPeriod, room);
                    for (int c = 0; c < opts.channels; ++c)
                        std::memcpy (capture[(size_t) c].data() + captureFramesNow,
                                     chanPtrs[(size_t) c],
                                     (size_t) copyN * sizeof (float));
                    captureFramesNow += copyN;
                }
            }

            next += std::chrono::duration_cast<std::chrono::steady_clock::duration> (
                std::chrono::duration<double> (periodSec));
        }
    }

    // Main loop: spawn clocks, sample metrics every second, stop after
    // duration, snapshot final state.
    void run()
    {
        const auto start = std::chrono::steady_clock::now();

        std::thread senderT (&Driver::senderClock,   this);
        std::thread recvT   (&Driver::receiverClock, this);

        // Ramp evaluation kicks in after the first 5 s — let priming
        // and convergence settle before declaring fill-stability.
        constexpr double kSettleSec = 5.0;
        const int    settlePeriods = (int) (opts.recvSampleRate * kSettleSec
                                            / juce::jmax (1, opts.recvPeriod));

        int blocksSinceStart = 0;
        double nextSampleAt = 0.0;
        while (! stopFlag.load (std::memory_order_acquire))
        {
            const double t = std::chrono::duration<double>
                (std::chrono::steady_clock::now() - start).count();
            if (t >= opts.durationSec)
                break;

            if (t >= nextSampleAt)
            {
                const auto peers = recv.snapshotPeers();
                if (! peers.empty())
                {
                    const auto& p = peers.front();
                    PerSecond rec;
                    rec.t           = t;
                    rec.fillFrames  = p.fillFrames;
                    rec.rcorr       = p.rcorr;
                    rec.rcZ3        = p.rcZ3;
                    rec.underruns   = p.underruns;
                    rec.recvPackets = p.recvPackets;
                    rec.sentPackets = send.getSentPackets();
                    trace.push_back (rec);

                    if (p.primed) peerEverPrimed = true;
                    if (p.fillFrames > peakFillFrames)
                        peakFillFrames = p.fillFrames;

                    // Fill-stability check (after settle): fill should
                    // stay within ~3× the sender period of the target.
                    if (blocksSinceStart > settlePeriods && p.primed)
                    {
                        const int target = p.effectiveBufMs * (int) opts.senderSampleRate / 1000
                                         + p.period / 2;
                        const int slack  = 3 * juce::jmax (1, p.period);
                        if (std::abs (p.fillFrames - target) > slack)
                            fillStable = false;
                    }
                }
                if (opts.verbose && ! peers.empty())
                {
                    const auto& p = peers.front();
                    std::cerr << "t=" << t
                              << " fill=" << p.fillFrames
                              << " rcorr=" << p.rcorr
                              << " z3=" << p.rcZ3
                              << " under=" << p.underruns
                              << " primed=" << p.primed
                              << "\n";
                }
                nextSampleAt = t + 1.0;
            }
            ++blocksSinceStart;
            std::this_thread::sleep_for (std::chrono::milliseconds (50));
        }

        stopFlag.store (true, std::memory_order_release);
        senderT.join();
        recvT.join();

        // Final snapshot.
        const auto peers = recv.snapshotPeers();
        if (! peers.empty())
        {
            const auto& p = peers.front();
            finalUnderruns   = p.underruns;
            finalRecvPackets = p.recvPackets;
            finalFillFrames  = p.fillFrames;
            finalRcorr       = p.rcorr;
            finalRcZ3        = p.rcZ3;
            finalRcNominal   = p.rcNominalRatio;
            finalGapFrames   = p.gapFrames;
            finalDroppedReorder = p.droppedReorder;
            finalStreamRestarts = p.streamRestarts;
        }
        finalSentPackets   = send.getSentPackets();
        finalAudioOverruns = send.getOverruns();
        finalFastModeEntries = send.getFastModeEntries();
        finalFastModeActive  = send.isInFastMode();
    }

    // Capture analysis: fundamental-frequency check on channel 0. Used
    // by the `passthrough` scenario.
    double measureFundamentalHz (int channel) const
    {
        if (channel < 0 || channel >= (int) capture.size()) return -1.0;
        if (captureFramesNow < 4096) return -1.0;
        // Skip the first 5 s — covers priming + the rate loop's fast-bw
        // lock-in window during which the receiver's output is muted to
        // hide convergence transients.
        const int skip = (int) (opts.recvSampleRate * 5.0);
        if (skip + 4096 > captureFramesNow) return -1.0;
        // Goertzel-bin scan around expected (1 kHz on ch0, 1 kHz + 100·c
        // on chN). Find the bin with max magnitude in [500, 5000] Hz.
        const int N = 4096;
        const float* x = capture[(size_t) channel].data() + skip;
        double bestMag = -1.0;
        double bestHz  = -1.0;
        // 1-Hz bin resolution in the search range.
        for (double f = 500.0; f <= 5000.0; f += 1.0)
        {
            const double w = 2.0 * M_PI * f / opts.recvSampleRate;
            const double cw = std::cos (w);
            double s0 = 0.0, s1 = 0.0, s2 = 0.0;
            for (int n = 0; n < N; ++n)
            {
                s0 = x[n] + 2.0 * cw * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            const double mag = s1 * s1 + s2 * s2 - 2.0 * cw * s1 * s2;
            if (mag > bestMag) { bestMag = mag; bestHz = f; }
        }
        return bestHz;
    }

    // High-precision fundamental frequency measurement on one window.
    // Uses an FFT-magnitude spectrum and parabolic interpolation around
    // the peak bin to recover sub-bin precision. With N = 131072 at
    // 48 kHz, raw bin width = 0.366 Hz; parabolic fit on a clean sine
    // gives ≈ 1/100 bin precision = 4 mHz = 4 ppm at 1 kHz.
    double measurePreciseHzAt (int channel, int offsetSamples, int N) const
    {
        if (channel < 0 || channel >= (int) capture.size()) return -1.0;
        if (offsetSamples < 0 || offsetSamples + N > captureFramesNow) return -1.0;
        // N must be a power of two for juce::dsp::FFT.
        const int log2N = (int) std::round (std::log2 ((double) N));
        if ((1 << log2N) != N) return -1.0;

        std::vector<float> buf ((size_t) (2 * N), 0.0f);   // real-imag interleaved scratch
        const float* src = capture[(size_t) channel].data() + offsetSamples;
        for (int n = 0; n < N; ++n)
            buf[(size_t) n] = src[n];

        juce::dsp::FFT fft (log2N);
        // Frequency-only forward transform: writes magnitudes into buf[0..N/2-1].
        fft.performFrequencyOnlyForwardTransform (buf.data());

        const double sr = opts.recvSampleRate;
        const int    loBin = juce::jmax (1, (int) std::floor (500.0  * N / sr));
        const int    hiBin = juce::jmin (N / 2 - 2,
                                         (int) std::ceil  (5000.0 * N / sr));
        int    peakBin = loBin;
        float  peakMag = 0.0f;
        for (int b = loBin; b <= hiBin; ++b)
        {
            const float m = buf[(size_t) b];
            if (m > peakMag) { peakMag = m; peakBin = b; }
        }
        if (peakBin <= 0 || peakBin >= N / 2 - 1)
            return (double) peakBin * sr / (double) N;

        // Parabolic interpolation on the log magnitudes (more accurate
        // than linear-magnitude fit for sinusoidal peaks under a flat-top
        // window's mainlobe). For a clean sine in a rectangular window,
        // linear-magnitude works fine too; we use linear here for
        // simplicity and add a tiny epsilon to avoid div-by-zero.
        const double y0 = buf[(size_t) (peakBin - 1)];
        const double y1 = buf[(size_t) peakBin];
        const double y2 = buf[(size_t) (peakBin + 1)];
        const double denom = y0 - 2.0 * y1 + y2;
        const double delta = (std::abs (denom) > 1e-30)
                                ? 0.5 * (y0 - y2) / denom : 0.0;
        return ((double) peakBin + delta) * sr / (double) N;
    }

    // RMS of the last `seconds` of captured output on one channel. The
    // host-event scenarios use it to tell "audio came back" from "silent
    // after the event". A 0.5-amplitude sine reads ~0.354.
    double tailRms (int channel, double seconds) const
    {
        if (channel < 0 || channel >= (int) capture.size()) return -1.0;
        const int n = juce::jmin (captureFramesNow, (int) (opts.recvSampleRate * seconds));
        if (n <= 0) return -1.0;
        const float* x = capture[(size_t) channel].data() + (captureFramesNow - n);
        double acc = 0.0;
        for (int i = 0; i < n; ++i) acc += (double) x[i] * (double) x[i];
        return std::sqrt (acc / (double) n);
    }

    // Measure fundamental frequency in `numWindows` evenly-spaced
    // non-overlapping windows of size N starting after `skipSec` of
    // priming. Returns the array of measured Hz; empty on insufficient
    // capture.
    std::vector<double> measurePreciseHzWindowed (int channel, int N,
                                                  int numWindows,
                                                  double skipSec) const
    {
        std::vector<double> out;
        if (channel < 0 || channel >= (int) capture.size()) return out;
        const int skip = (int) (opts.recvSampleRate * skipSec);
        if (skip + N > captureFramesNow) return out;

        const int usableStart = skip;
        const int usableEnd   = captureFramesNow - N;
        if (usableEnd <= usableStart) return out;

        // Distribute numWindows starts evenly across the usable range,
        // ensuring non-overlap (start_i+1 - start_i >= N).
        const int span     = usableEnd - usableStart;
        const int strideMin = N;
        if (numWindows < 1) numWindows = 1;
        if (numWindows > 1)
        {
            const int maxNumWindows = juce::jmax (1, span / strideMin + 1);
            if (numWindows > maxNumWindows) numWindows = maxNumWindows;
        }

        out.reserve ((size_t) numWindows);
        for (int w = 0; w < numWindows; ++w)
        {
            const int offset = usableStart
                             + (numWindows == 1 ? span / 2
                                                : (int) ((double) w
                                                         * (span - strideMin)
                                                         / (double) (numWindows - 1)));
            const double f = measurePreciseHzAt (channel, offset, N);
            if (f > 0.0) out.push_back (f);
        }
        return out;
    }
};

// ---------------------------------------------------------------------------
// Scenario configurations. Each scenario tweaks Options.
// ---------------------------------------------------------------------------
struct Scenario
{
    const char* name;
    void (*config) (Options&);
    const char* docstring;
};

void scn_smoke (Options& o)
{
    if (o.durationSec <= 0.0) o.durationSec = 5.0;
}
void scn_passthrough (Options& o)
{
    if (o.durationSec <= 0.0) o.durationSec = 10.0;
}
void scn_pitch_stability (Options& o)
{
    // Long-window pitch stability check. Run at matched rate for 60 s
    // and capture the entire receiver output so the harness can run
    // multiple FFTs across the duration and verify the fundamental
    // doesn't drift or wobble. Capture buffer is sized to the full
    // duration (60 s × 4 ch × float = 46 MB at 48 k — fine).
    if (o.durationSec   <= 0.0) o.durationSec   = 60.0;
    if (o.captureSeconds < o.durationSec) o.captureSeconds = o.durationSec;
}
void scn_drift_match (Options& o)
{
    if (o.durationSec <= 0.0) o.durationSec = 30.0;
}
void scn_drift_small (Options& o)
{
    // Both sides REPORT 48 kHz. Receiver's wallclock paces 200 ppm fast.
    // From the loop's POV: we consume input slightly faster than the
    // sender provides. Loop must drive rcorr DOWN to consume less per
    // output. Expected steady-state rcorr = sender/recv = 1/1.0002.
    if (o.durationSec <= 0.0) o.durationSec = 60.0;
    if (o.recvPaceSampleRate <= 0.0)
        o.recvPaceSampleRate = 48000.0 * (1.0 + 200.0e-6);
}
void scn_drift_large (Options& o)
{
    // ±2000 ppm — well within the ±5% clamp, must converge.
    if (o.durationSec <= 0.0) o.durationSec = 60.0;
    if (o.recvPaceSampleRate <= 0.0)
        o.recvPaceSampleRate = 48000.0 * (1.0 + 2000.0e-6);
}
void scn_drift_extreme (Options& o)
{
    // ±60000 ppm — past the 50000 ppm clamp. Loop pins at the extreme;
    // verify it doesn't crash, leak, or loop infinitely on anti-windup.
    if (o.durationSec <= 0.0) o.durationSec = 30.0;
    if (o.recvPaceSampleRate <= 0.0)
        o.recvPaceSampleRate = 48000.0 * (1.0 + 60000.0e-6);
}
void scn_burst (Options& o)
{
    if (o.durationSec <= 0.0)      o.durationSec = 60.0;
    if (o.burstMultiplier == 1.0)  o.burstMultiplier = 2.0;
    if (o.burstDurationSec <= 0.0) o.burstDurationSec = 5.0;
}
void scn_channel_change (Options& o)
{
    // Channel-change is implemented in the run() override, not via Options
    // alone. Default duration kept short.
    if (o.durationSec <= 0.0) o.durationSec = 30.0;
}
void scn_reconnect (Options& o)
{
    if (o.durationSec <= 0.0) o.durationSec = 60.0;
}
void scn_tail (Options& o, double defaultDuration)
{
    if (o.durationSec <= 0.0) o.durationSec = defaultDuration;
    if (o.captureSeconds < o.durationSec) o.captureSeconds = o.durationSec;
    o.tailCheck = true;
}
void scn_reprepare (Options& o)
{
    // Host re-activates the sender (setActive off/on) with unchanged
    // settings. The stream must carry on: the frame count may not rewind
    // under the same sender UID.
    scn_tail (o, 18.0);
    if (o.reprepareAtSec < 0.0) o.reprepareAtSec = 6.0;
}
void scn_stall (Options& o)
{
    // Host stops calling processBlock for 50 ms (overloaded machine), then
    // carries on. Same gap as `reprepare` without the re-prepare.
    scn_tail (o, 18.0);
    if (o.stallAtSec < 0.0) o.stallAtSec = 6.0;
}
void scn_split_fast_mode (Options& o)
{
    // 0.5 s of 8x pre-render trips the sender's anticipative-render
    // detector; afterwards the host runs at realtime but pushes every
    // period as 4 back-to-back calls. The detector has to notice the
    // realtime pacing and resume sending.
    scn_tail (o, 12.0);
    if (o.senderPeriod == 64)     o.senderPeriod = 256;
    if (o.splitCalls <= 1)        o.splitCalls = 4;
    if (o.burstMultiplier == 1.0) o.burstMultiplier = 8.0;
    if (o.burstDurationSec <= 0.0) o.burstDurationSec = 0.5;
}
void scn_bunch_fast_mode (Options& o)
{
    // Same burst, then a device period of 4 full-size blocks processed
    // back-to-back: fast, fast, fast, slow. Judged per block, the fast ones
    // reset the exit count every period and the sender never resumed.
    scn_tail (o, 12.0);
    if (o.senderPeriod == 64)      o.senderPeriod = 128;
    if (o.bunchCalls <= 1)         o.bunchCalls = 4;
    if (o.burstMultiplier == 1.0)  o.burstMultiplier = 16.0;
    if (o.burstDurationSec <= 0.0) o.burstDurationSec = 0.5;
}
void scn_counter_rewind (Options& o)
{
    // Sender frame count jumps back to 0 under the same UID (what older
    // senders did on every prepareToPlay). Receiver must re-align.
    scn_tail (o, 18.0);
    if (o.rewindAtSec < 0.0) o.rewindAtSec = 6.0;
}
void scn_counter_wrap (Options& o)
{
    // 32-bit wire frame count wraps ~6 s into the run.
    scn_tail (o, 18.0);
    if (o.counterStart < 0.0)
        o.counterStart = 4294967296.0 - 6.0 * o.senderSampleRate;
}
void scn_soak (Options& o)
{
    if (o.durationSec <= 0.0) o.durationSec = 14400.0;  // 4 hours
}

const Scenario kScenarios[] = {
    { "smoke",          scn_smoke,          "5 s minimal connect + audio" },
    { "passthrough",    scn_passthrough,    "10 s sine fidelity check" },
    { "pitch_stability",scn_pitch_stability,"60 s long-term pitch-stability FFT" },
    { "drift_match",    scn_drift_match,    "30 s matched SR" },
    { "drift_small",    scn_drift_small,    "60 s ~200 ppm mismatch" },
    { "drift_large",    scn_drift_large,    "60 s ~2000 ppm mismatch" },
    { "drift_extreme",  scn_drift_extreme,  "30 s ~50000 ppm (past clamp)" },
    { "burst",          scn_burst,          "60 s with 5 s @ 2× burst" },
    { "channel_change", scn_channel_change, "30 s with mid-stream chan change" },
    { "reconnect",      scn_reconnect,      "60 s of connect/disconnect cycles" },
    { "reprepare",      scn_reprepare,      "18 s with sender re-prepare at 6 s" },
    { "stall",          scn_stall,          "18 s with a 50 ms sender stall at 6 s" },
    { "split_fast_mode",scn_split_fast_mode,"12 s pre-render burst then split periods" },
    { "bunch_fast_mode",scn_bunch_fast_mode,"12 s pre-render burst then bunched blocks" },
    { "counter_rewind", scn_counter_rewind, "18 s with frame counter reset at 6 s" },
    { "counter_wrap",   scn_counter_wrap,   "18 s with 32-bit frame counter wrap at ~6 s" },
    { "soak",           scn_soak,           "4 h matched-rate soak" },
};

const Scenario* findScenario (const juce::String& name)
{
    for (const auto& s : kScenarios)
        if (name == s.name) return &s;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Metrics JSON output. Keep the shape stable so the pytest wrapper can
// rely on it across versions.
// ---------------------------------------------------------------------------
void writeMetricsJson (const Driver& d, double measuredFreqCh0Hz,
                       const std::vector<double>& fundHzWindows,
                       double tailRmsCh0 = -1.0, double tailHzCh0 = -1.0)
{
    if (d.opts.metricsOut.isEmpty()) return;

    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("scenario",          d.opts.scenario);
    root->setProperty ("duration_sec",      d.opts.durationSec);
    root->setProperty ("sender_sample_rate", d.opts.senderSampleRate);
    root->setProperty ("recv_sample_rate",   d.opts.recvSampleRate);
    root->setProperty ("sender_pace_sr",     d.opts.effectiveSenderPace());
    root->setProperty ("recv_pace_sr",       d.opts.effectiveRecvPace());
    root->setProperty ("sender_period",      d.opts.senderPeriod);
    root->setProperty ("recv_period",        d.opts.recvPeriod);
    root->setProperty ("channels",           d.opts.channels);

    root->setProperty ("internal_error",     d.internalError);
    root->setProperty ("error_message",      d.errorMsg);
    root->setProperty ("peer_ever_primed",   d.peerEverPrimed);

    root->setProperty ("underruns",          (juce::int64) d.finalUnderruns);
    root->setProperty ("audio_overruns",     (juce::int64) d.finalAudioOverruns);
    root->setProperty ("gap_frames",         (juce::int64) d.finalGapFrames);
    root->setProperty ("recv_packets",       (juce::int64) d.finalRecvPackets);
    root->setProperty ("sent_packets",       (juce::int64) d.finalSentPackets);

    root->setProperty ("final_fill_frames",  d.finalFillFrames);
    root->setProperty ("peak_fill_frames",   d.peakFillFrames);
    root->setProperty ("fill_stable",        d.fillStable);

    root->setProperty ("final_rcorr",        d.finalRcorr);
    root->setProperty ("final_rcZ3",         d.finalRcZ3);
    root->setProperty ("final_rcNominalRatio", d.finalRcNominal);

    root->setProperty ("measured_fundamental_hz", measuredFreqCh0Hz);

    root->setProperty ("dropped_reorder",      (juce::int64) d.finalDroppedReorder);
    root->setProperty ("stream_restarts",      (juce::int64) d.finalStreamRestarts);
    root->setProperty ("fast_mode_entries",    (juce::int64) d.finalFastModeEntries);
    root->setProperty ("fast_mode_active_end", d.finalFastModeActive);
    // -1 when the scenario doesn't request a tail check.
    root->setProperty ("tail_rms_ch0",         tailRmsCh0);
    root->setProperty ("tail_fundamental_hz",  tailHzCh0);

    // Per-window fundamental frequency (channel 0) for the long-term
    // pitch-stability test. Empty for scenarios that don't request it.
    juce::Array<juce::var> fundArr;
    for (double f : fundHzWindows) fundArr.add (juce::var (f));
    root->setProperty ("fundamental_hz_windows", fundArr);

    // Trace as an array of objects. Pytest can use these for ramp/converge
    // analysis.
    juce::Array<juce::var> trace;
    for (const auto& t : d.trace)
    {
        juce::DynamicObject::Ptr e = new juce::DynamicObject();
        e->setProperty ("t",            t.t);
        e->setProperty ("fillFrames",   t.fillFrames);
        e->setProperty ("rcorr",        t.rcorr);
        e->setProperty ("rcZ3",         t.rcZ3);
        e->setProperty ("underruns",    (juce::int64) t.underruns);
        e->setProperty ("recvPackets",  (juce::int64) t.recvPackets);
        e->setProperty ("sentPackets",  (juce::int64) t.sentPackets);
        trace.add (juce::var (e.get()));
    }
    root->setProperty ("trace", trace);

    juce::var v (root.get());
    juce::File out (d.opts.metricsOut);
    out.replaceWithText (juce::JSON::toString (v, true));
}

}  // namespace

// ---------------------------------------------------------------------------
// RtWake self-test (--rtwake-test): signals while the waiter is awake must
// not leave posts behind, or later waits stop sleeping. Exit 0 = pass.
// ---------------------------------------------------------------------------
int runRtWakeTest()
{
    mcfx::net::RtWake wake;

    // Signal while the waiter is awake: each wait() finds the flag set.
    for (int i = 0; i < 100; ++i)
    {
        wake.signal();
        wake.wait (50);
    }

    // Nothing signalled now, so each wait must sleep for its timeout.
    constexpr int kTimeoutMs = 20;
    for (int i = 0; i < 3; ++i)
    {
        const auto t0 = std::chrono::steady_clock::now();
        wake.wait (kTimeoutMs);
        const auto ms = std::chrono::duration<double, std::milli> (
                            std::chrono::steady_clock::now() - t0).count();
        if (ms < kTimeoutMs / 2)
        {
            std::cerr << "rtwake: wait " << i << " returned after " << ms
                      << " ms with nothing signalled (timeout " << kTimeoutMs << " ms)\n";
            return 1;
        }
    }

    // And a signal still wakes a sleeping waiter early.
    std::thread signaller ([&wake] {
        std::this_thread::sleep_for (std::chrono::milliseconds (10));
        wake.signal();
    });
    const auto t0 = std::chrono::steady_clock::now();
    wake.wait (2000);
    const auto ms = std::chrono::duration<double, std::milli> (
                        std::chrono::steady_clock::now() - t0).count();
    signaller.join();
    if (ms > 1000)
    {
        std::cerr << "rtwake: signal did not wake the waiter (" << ms << " ms)\n";
        return 1;
    }

    std::cerr << "rtwake: ok\n";
    return 0;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main (int argc, char** argv)
{
    if (argc == 2 && std::strcmp (argv[1], "--rtwake-test") == 0)
        return runRtWakeTest();

    Options opts;
    if (! parseArgs (argc, argv, opts)) return 2;

    const auto* scenario = findScenario (opts.scenario);
    if (scenario == nullptr)
    {
        std::cerr << "unknown scenario: " << opts.scenario.toStdString() << "\n";
        std::cerr << "available scenarios:\n";
        for (const auto& s : kScenarios)
            std::cerr << "  " << s.name << " — " << s.docstring << "\n";
        return 2;
    }
    scenario->config (opts);

    std::cerr << "mcfx_net_loopback_test scenario=" << opts.scenario.toStdString()
              << " duration=" << opts.durationSec << "s"
              << " senderSR=" << opts.senderSampleRate
              << " recvSR=" << opts.recvSampleRate
              << " senderPeriod=" << opts.senderPeriod
              << " recvPeriod=" << opts.recvPeriod
              << " channels=" << opts.channels
              << " burstMultiplier=" << opts.burstMultiplier
              << " burstDurationSec=" << opts.burstDurationSec
              << "\n";

    Driver d (opts);
    if (! d.setup())
    {
        d.internalError = true;
        std::cerr << "setup failed: " << d.errorMsg.toStdString() << "\n";
        writeMetricsJson (d, -1.0, {});
        return 1;
    }

    d.run();

    const double measuredHz = (opts.scenario == "passthrough"
                               || opts.scenario == "pitch_stability")
                                ? d.measureFundamentalHz (0) : -1.0;

    // Long-term pitch stability — only for the pitch_stability scenario.
    // 131072 = 2^17 samples (~2.7 s at 48 kHz). 8 non-overlapping windows
    // span ~22 s of audio; we skip the first 5 s to give the rate loop
    // time to settle past its fast-bw lock-in phase.
    std::vector<double> fundHzWindows;
    if (opts.scenario == "pitch_stability")
        fundHzWindows = d.measurePreciseHzWindowed (0, 131072, 8, 5.0);

    // Tail check: last 3 s RMS and a ~2.7 s FFT ending at the last frame.
    double tailRmsCh0 = -1.0, tailHzCh0 = -1.0;
    if (opts.tailCheck)
    {
        tailRmsCh0 = d.tailRms (0, 3.0);
        constexpr int kTailN = 131072;
        if (d.captureFramesNow >= kTailN)
            tailHzCh0 = d.measurePreciseHzAt (0, d.captureFramesNow - kTailN, kTailN);
    }

    d.teardown();

    std::cerr << "DONE underruns=" << d.finalUnderruns
              << " peakFill=" << d.peakFillFrames
              << " finalFill=" << d.finalFillFrames
              << " finalRcorr=" << d.finalRcorr
              << " finalRcZ3=" << d.finalRcZ3
              << " sentPkts=" << d.finalSentPackets
              << " recvPkts=" << d.finalRecvPackets
              << " reorderDrop=" << d.finalDroppedReorder
              << " restarts=" << d.finalStreamRestarts
              << " fastModeEntries=" << d.finalFastModeEntries
              << " tailRms=" << tailRmsCh0
              << " tailHz=" << tailHzCh0
              << "\n";

    writeMetricsJson (d, measuredHz, fundHzWindows, tailRmsCh0, tailHzCh0);
    return 0;
}
