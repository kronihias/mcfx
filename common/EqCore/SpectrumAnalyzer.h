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

#ifndef SPECTRUMANALYZER_H_INCLUDED
#define SPECTRUMANALYZER_H_INCLUDED

#include "JuceHeader.h"
#include "Dsp/RealFFT.h"
#include <vector>
#include <atomic>

/** Spectrum analyzer with the transform kept off the audio thread.

    Usage:
      1. Call prepare() with sample rate and max block size.
      2. Call pushBuffer() from processBlock() for input and/or output.
      3. Call update() from the GUI timer, then getMagnitude(freqHz).

    Threading follows CQTAnalyzer and mcfx_meter's MultiBandAnalyser: the audio
    thread only ever copies samples into a ring (a try-lock, so a resize in
    prepare() costs a dropped block, never a stall), and the transform runs on
    the GUI thread from update(). It used to run inline in pushBuffer(), which
    concentrated numChannels FFTs of kFFTSize — for BOTH the input and output
    analyzer, whose hops land on the same sample — into whichever block crossed
    the hop boundary.

    update() transforms only when at least half a window of new audio has
    arrived, so the effective rate (and with it the smoothing calibration in
    prepare()) stays what the inline version had, merely quantised to the GUI
    timer. */
class SpectrumAnalyzer
{
public:
    // 8192 rather than 4096: on a log-frequency plot the bottom octaves are what
    // a linear-bin FFT starves, and doubling the length doubles the bins there
    // (20-40 Hz goes from under two bins to over three). The cost is a 171 ms
    // window instead of 85 ms, so the curve settles more slowly — acceptable for
    // a display that is watched for balance, and the spectrogram's constant-Q
    // analysis covers the fast, fine-grained bass detail.
    static constexpr int kFFTOrder = 13;                     // 2^13 = 8192
    static constexpr int kFFTSize = 1 << kFFTOrder;          // 8192
    static constexpr int kSpecLen = kFFTSize / 2 + 1;        // 4097

    SpectrumAnalyzer();
    ~SpectrumAnalyzer() = default;

    /** Prepare for a given sample rate and max number of channels. */
    void prepare(double sampleRate, int numChannels);

    /** Audio thread: append a block. Copy only — no transform here. */
    void pushBuffer(const AudioSampleBuffer& buffer, int numSamples);

    /** GUI thread: transform the newest window, if at least half a window of
        new audio has arrived since the last transform. Cheap no-op otherwise,
        so calling it every paint is fine. */
    void update();

    /** Get interpolated smoothed magnitude at a given frequency (Hz).
        Returns linear magnitude (not dB). Thread-safe for GUI reads. */
    float getMagnitude(double freqHz) const;

    /** Largest magnitude in a frequency range. A pixel high up the log axis spans
        many bins, so sampling one of them lets narrow peaks fall between pixels
        and flicker in and out; taking the peak over the pixel's own span shows
        them. Falls back to interpolation where a pixel is narrower than a bin. */
    float getMaxMagnitude(double freqLoHz, double freqHiHz) const;

    /** Set which channel to analyze. 0 = average all, 1..N = single channel. */
    void setAnalyzerChannel(int ch) { analyzerChannel_.store(ch, std::memory_order_relaxed); }
    int getAnalyzerChannel() const { return analyzerChannel_.load(std::memory_order_relaxed); }

    /** Reset smoothed magnitudes to zero. */
    void reset();

    double getSampleRate() const { return sampleRate_; }

private:
    void computeFFT();

    // Accelerate / FFTW directly — see common/Dsp/RealFFT.h for why this does
    // not go through juce::dsp::FFT.
    mcfx::RealFFT fft_;
    double sampleRate_ = 48000.0;
    int numChannels_ = 0;

    // Blackman-Harris window
    std::vector<float> window_;
    float windowCoherentGain_ = 1.f;

    // Multi-channel ring buffer: [channel][sample]. All channels share one
    // write position so the averaged mode reads every channel from the same
    // instant. The lock covers the buffer against prepare(); the audio thread
    // only ever try-locks it.
    AudioSampleBuffer ringBuffer_;
    std::atomic<int>  writePos_ { 0 };
    std::atomic<int>  samplesSinceFFT_ { 0 };
    juce::SpinLock    ringLock_;

    // FFT working buffers (GUI thread)
    std::vector<float> magSq_;        // kSpecLen bin powers
    std::vector<float> tmpMag_;       // kSpecLen
    std::vector<float> accum_;        // kSpecLen — all-channels average
    std::vector<float> magnitude_;    // kSpecLen — smoothed output

    float smoothAlpha_ = 0.85f;       // exponential smoothing factor

    std::atomic<int> analyzerChannel_ { 0 }; // 0 = all, 1..N = specific

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};

#endif // SPECTRUMANALYZER_H_INCLUDED
