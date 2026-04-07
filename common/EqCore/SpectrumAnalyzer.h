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
#include <vector>
#include <atomic>

/** Lock-free spectrum analyzer that accumulates audio in a ring buffer,
    computes FFT magnitude on the audio thread, and exposes smoothed
    magnitude data for the GUI to read.

    Usage:
      1. Call prepare() with sample rate and max block size.
      2. Call pushSamples() from processBlock() for input and/or output.
      3. Call getMagnitude(freqHz) from the GUI to read smoothed spectrum.

    Thread safety: pushSamples() runs on the audio thread.
    getMagnitude() can be called from any thread (reads smoothed data). */
class SpectrumAnalyzer
{
public:
    static constexpr int kFFTOrder = 12;                     // 2^12 = 4096
    static constexpr int kFFTSize = 1 << kFFTOrder;          // 4096
    static constexpr int kSpecLen = kFFTSize / 2 + 1;        // 2049

    SpectrumAnalyzer();
    ~SpectrumAnalyzer() = default;

    /** Prepare for a given sample rate and max number of channels. */
    void prepare(double sampleRate, int numChannels);

    /** Push a block of audio from an AudioSampleBuffer (all channels at once).
        Internally accumulates and performs FFT + smoothing when buffer is full. */
    void pushBuffer(const AudioSampleBuffer& buffer, int numSamples);

    /** Get interpolated smoothed magnitude at a given frequency (Hz).
        Returns linear magnitude (not dB). Thread-safe for GUI reads. */
    float getMagnitude(double freqHz) const;

    /** Set which channel to analyze. 0 = average all, 1..N = single channel. */
    void setAnalyzerChannel(int ch) { analyzerChannel_.store(ch, std::memory_order_relaxed); }
    int getAnalyzerChannel() const { return analyzerChannel_.load(std::memory_order_relaxed); }

    /** Reset smoothed magnitudes to zero. */
    void reset();

    double getSampleRate() const { return sampleRate_; }

private:
    void computeFFT();

    juce::dsp::FFT fft_;
    double sampleRate_ = 48000.0;
    int numChannels_ = 0;

    // Blackman-Harris window
    std::vector<float> window_;
    float windowCoherentGain_ = 1.f;

    // Multi-channel ring buffer: [channel][sample]
    AudioSampleBuffer ringBuffer_;
    int writePos_ = 0;

    // FFT working buffers
    std::vector<float> fftData_;      // 2 * kFFTSize (JUCE FFT needs 2x)
    std::vector<float> tmpMag_;       // kSpecLen
    std::vector<float> magnitude_;    // kSpecLen — smoothed output

    float smoothAlpha_ = 0.85f;       // exponential smoothing factor

    std::atomic<int> analyzerChannel_ { 0 }; // 0 = all, 1..N = specific

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};

#endif // SPECTRUMANALYZER_H_INCLUDED
