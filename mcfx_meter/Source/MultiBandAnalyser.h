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

#ifndef MULTIBANDANALYSER_H_INCLUDED
#define MULTIBANDANALYSER_H_INCLUDED

#include "JuceHeader.h"
#include "Dsp/RealFFT.h"
#include <atomic>
#include <vector>

/** Fractional-octave band levels for every channel at once.

    The suite's existing analysers cannot do this: SpectrumAnalyzer keeps one
    output spectrum however many channels it is fed, and CQTAnalyzer has a mono
    ring. The waterfall view needs N independent spectra simultaneously, so this
    keeps a per-channel ring and a per-channel band array.

    Threading follows CQTAnalyzer: the audio thread only ever copies samples,
    and the transform runs on the GUI thread. Putting 128 FFTs in processBlock
    is not an option.

    Every channel is transformed on every call, from a single latched read
    position, because the view's whole purpose is comparing channels against
    each other and that only means anything if they describe the same instant.
    An earlier version cycled through a slice of the channels per call to bound
    the cost; identical audio on 64 channels then read 3.4 dB apart across a
    level change, because the slices were transformed up to 160 ms apart.

    Cost is kept down by the two things that do not compromise that:
      - it does nothing at all unless setActive(true) — the waterfall being on
        screen is the only reason to pay for this;
      - buffers are allocated on first activation, so a session that never opens
        the waterfall never pays the memory either.
    The transform goes straight to the platform's vector library: at 128
    channels and 48 kHz, every frame that arrives (about two per 40 ms tick)
    costs 1.6 ms a tick on an M-series Mac, 4% of a core.

    Band levels are linear magnitudes calibrated so a full-scale sine reads 1.0
    in the band containing it. */
class MultiBandAnalyser
{
public:
    /** Bands per octave. 6 gives 60 bands over the audio range — fine enough to
        show a resonance, coarse enough to stay legible stacked 128 deep. */
    static constexpr int   kBandsPerOctave = 6;
    static constexpr float kBandRefHz      = 1000.f;   // bands are anchored here
    static constexpr int   kLowestBand     = -34;      // ~19.7 Hz
    static constexpr int   kHighestBand    =  26;      // ~20.1 kHz
    static constexpr int   kNumBands       = kHighestBand - kLowestBand + 1;   // 61

    MultiBandAnalyser() = default;

    /** Build for a sample rate and channel count. Cheap no-op when neither
        changed. Allocates, so call it from the GUI thread, not processBlock. */
    void prepare (double sampleRate, int numChannels);

    /** Gate. While inactive, push() returns after one relaxed atomic load. */
    void setActive (bool shouldBeActive);
    bool isActive() const { return active_.load (std::memory_order_relaxed); }

    /** Audio thread: append a block. Copy only — no transform here. */
    void push (const AudioSampleBuffer& buffer, int numSamples);

    /** GUI thread: analyse every frame that has arrived since the last call,
        every channel from the same frames.

        Frames are 4096 points on a fixed grid of absolute sample positions, a
        quarter of a frame apart, with a Hann window: at that overlap its
        squared windows sum to a constant, so a click or a note counts the
        same wherever it falls, and however the timer driving this is spaced.
        The earlier version took one frame of the newest audio per call; at
        a 40 ms timer that is 53% overlap and 8.5 dB of ripple, 25 dB when a
        call came 20 ms late. Hann rather than the spectrum's Blackman-Harris
        because it needs half the frames for a flat sum (N/4 against N/8),
        and band sums do not need Blackman-Harris's far sidelobes: Hann's are
        75 dB down a band away from 1 kHz, and its narrower main lobe keeps
        the low bands, only a bin or two wide, apart better.

        At most kMaxFramesPerCall are analysed in one call; behind by more
        (a stall), it skips ahead to the newest frames rather than freeze the
        GUI catching up. */
    void compute();

    /** Time constant of each band's exponential average, in seconds, 0 = no
        smoothing. Applied per frame, so it means the same whatever calls
        compute(). One 85 ms frame of anything noise-like is a chi-squared
        estimate with several dB of spread, so without this the display
        shimmers even on a steady signal. */
    void setSmoothingTime (float seconds);

    /** Linear band magnitudes for a channel — kNumBands entries, or nullptr. */
    const float* getChannelBands (int channel) const;

    static float getBandCentreHz (int band);
    static float getBandLowHz (int band);
    static float getBandHighHz (int band);

    /** Below this the bands are narrower than an FFT bin and adjacent bands
        share bins, so the low end is resolution-limited rather than wrong.
        Worth showing rather than hiding. */
    float getResolutionFloorHz() const;

    int getNumChannels() const { return numChannels_; }
    bool isReady() const { return ready_; }

    void reset();

private:
    void buildBandTable();

    double sampleRate_  = 0.0;
    int    numChannels_ = 0;
    static constexpr int kMaxFramesPerCall = 8;

    int    fftOrder_    = 12;                 // 4096: 85 ms window, ~101 Hz floor
    int    fftSize_     = 1 << 12;
    int    hop_         = 1 << 10;            // fftSize_ / 4
    bool   ready_       = false;

    std::atomic<bool> active_ { false };

    // Accelerate on Apple, FFTW elsewhere — see RealFFT for why this does not
    // go through juce::dsp::FFT.
    mcfx::RealFFT fft_;
    std::vector<float> window_;
    float bandNorm_ = 1.f;            // sqrt (band power) -> full-scale sine is 1.0

    // Per-band FFT bin range. The first and last bin are weighted by how much
    // of them the band actually covers — taking whole bins would over-count the
    // narrow low bands and tilt broadband content by a few tenths of a dB per
    // octave, which is visible across the width of the plot.
    struct BandBins { int lo = 0, hi = 0; float wLo = 1.f, wHi = 1.f; bool resolutionLimited = false; };
    std::vector<BandBins> bands_;

    // All channels share one write position so they stay time-aligned — the
    // whole point of the view is comparing them against each other. The ring
    // is a power of two and sample i (counted since prepare) sits at
    // i & (size - 1), so frames are addressed by absolute position. It holds
    // a frame plus the catch-up allowance plus headroom for blocks written
    // while a pass is reading.
    AudioSampleBuffer  ring_;
    std::atomic<int64> written_ { 0 };
    juce::SpinLock     ringLock_;
    int64 nextFrame_ = -1;            // grid index of the next frame, -1 = from now

    std::vector<float> magSq_;        // fftSize_/2 + 1 bin powers
    std::vector<float> levels_;       // numChannels_ * kNumBands — smoothed
    float smoothTau_ = 0.f;
    bool  primed_ = false;            // first pass loads rather than averages

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiBandAnalyser)
};

#endif // MULTIBANDANALYSER_H_INCLUDED
