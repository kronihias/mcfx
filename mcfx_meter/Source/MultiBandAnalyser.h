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
    A full 128-channel pass costs well under a millisecond now that the
    transform goes straight to the platform's vector library.

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

    /** GUI thread: transform every channel from one latched read position.

        The window is 85 ms and always ends at "now", so successive calls
        overlap rather than tile: a late call means more overlap, never a gap.
        Only an interval longer than the window itself could miss audio, which
        is why this is safe to drive from a GUI timer at 40 ms — there is better
        than 2:1 headroom — even though the timer's spacing is not exact. */
    void compute();

    /** Per-call pole for the exponential average of each band, 0 = no
        smoothing. One 85 ms frame of anything noise-like is a chi-squared
        estimate with several dB of spread, so without this the display shimmers
        even on a steady signal. */
    void setSmoothing (float alpha);

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
    int    fftOrder_    = 12;                 // 4096: 85 ms window, ~101 Hz floor
    int    fftSize_     = 1 << 12;
    bool   ready_       = false;

    std::atomic<bool> active_ { false };

    // Accelerate on Apple, FFTW elsewhere — see RealFFT for why this does not
    // go through juce::dsp::FFT.
    mcfx::RealFFT fft_;
    std::vector<float> window_;
    float windowGain_ = 1.f;

    // Per-band FFT bin range. The first and last bin are weighted by how much
    // of them the band actually covers — taking whole bins would over-count the
    // narrow low bands and tilt broadband content by a few tenths of a dB per
    // octave, which is visible across the width of the plot.
    struct BandBins { int lo = 0, hi = 0; float wLo = 1.f, wHi = 1.f; bool resolutionLimited = false; };
    std::vector<BandBins> bands_;

    // All channels share one write position so they stay time-aligned — the
    // whole point of the view is comparing them against each other.
    AudioSampleBuffer ring_;
    std::atomic<int>  writePos_ { 0 };
    juce::SpinLock    ringLock_;

    std::vector<float> magSq_;        // fftSize_/2 + 1 bin powers
    std::vector<float> levels_;       // numChannels_ * kNumBands — smoothed
    float smoothAlpha_ = 0.f;
    bool  primed_ = false;            // first pass loads rather than averages

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiBandAnalyser)
};

#endif // MULTIBANDANALYSER_H_INCLUDED
