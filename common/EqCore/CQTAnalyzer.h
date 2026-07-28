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

#ifndef CQTANALYZER_H_INCLUDED
#define CQTANALYZER_H_INCLUDED

#include "JuceHeader.h"
#include "Dsp/RealFFT.h"
#include <atomic>
#include <complex>
#include <vector>

/** Constant-Q analyzer for the spectrogram display.

    A plain FFT has linearly spaced bins, which is the wrong shape for a
    log-frequency plot: at 48 kHz a 4096-point FFT puts under two bins in the
    20-40 Hz octave while spending 850 on 10-20 kHz. The constant-Q transform
    instead places a fixed number of bins per octave, so the analysis window
    lengthens towards the bass (fine pitch resolution where the ear needs it)
    and shortens towards the treble (fast transient response) — matching how the
    result is drawn.

    Implemented with the Brown-Puckette spectral-kernel method: each bin's
    windowed complex exponential is transformed once at prepare() time and kept
    as a sparse spectrum, so a frame costs one FFT plus a short dot product per
    bin instead of a separate transform per bin.

    Two details this implementation depends on:

    - Each kernel sits at the *end* of the frame, so every bin's window stops at
      "now". Placed at the start (as the original paper has it) the short
      treble kernels would report audio from a whole frame ago, lagging the
      display badly.
    - Below kMinConstantQHz the ideal window would outrun the frame, so it is
      capped and Q falls off gracefully. The kernel keeps its true centre
      frequency there — deriving it from the window length instead (the usual
      shortcut, valid only while the window is uncapped) would collapse every
      capped bin onto the same frequency.

    Threading: push() runs on the audio thread and only copies samples.
    compute() does the transform and is meant for the GUI thread. */
class CQTAnalyzer
{
public:
    static constexpr int   kBinsPerOctave = 24;      // quarter-tone resolution
    static constexpr float kFMin          = 20.f;
    static constexpr float kFMax          = 20000.f;
    /** Kernel spectrum entries below this fraction of the peak are dropped.
        2e-2 keeps ~215 of 32768 bins per kernel for ~0.02 dB of error. */
    static constexpr float kKernelSparsity = 2.0e-2f;

    CQTAnalyzer() = default;

    /** Build the kernels for a sample rate. Expensive (one FFT per bin) — call
        off the audio thread. Safe to call repeatedly; rebuilds only on change. */
    void prepare(double sampleRate);

    /** Audio thread: append one channel of a block to the ring buffer.
        channel is 1-based; 0 averages all channels. */
    void push(const AudioSampleBuffer& buffer, int numSamples, int channel);

    /** GUI thread: transform the most recent frame into bin magnitudes. */
    void compute();

    /** Linear magnitude at a frequency, interpolated between bins in log
        frequency. Calibrated so a full-scale sine reads 1.0. */
    float getMagnitude(double freqHz) const;

    bool   isReady() const { return ready_; }
    int    getNumBins() const { return numBins_; }
    double getBinFrequency(int k) const;
    /** Frequency below which the window is capped and Q starts to fall. */
    double getConstantQFloorHz() const;

    void reset();

private:
    void buildKernels();

    double sampleRate_ = 0.0;
    int    fftOrder_   = 0;
    int    fftSize_    = 0;
    int    numBins_    = 0;
    bool   ready_      = false;

    // Accelerate / FFTW directly — see common/Dsp/RealFFT.h. The kernel
    // construction below still uses juce::dsp::FFT: that one is a genuine
    // complex transform, and it runs once per prepare().
    mcfx::RealFFT fft_;

    // Sparse spectral kernels, flattened: bin k spans [offset_[k], offset_[k+1]).
    std::vector<int>                  kernelIndex_;
    std::vector<std::complex<float>>  kernelValue_;
    std::vector<int>                  kernelOffset_;

    // Mono ring of the analysed channel. prepare() reallocates it from the GUI
    // thread, so the audio thread takes a try-lock and simply skips a block
    // rather than writing into a buffer that is being replaced.
    std::vector<float> ring_;
    std::atomic<int>   writePos_ { 0 };
    juce::SpinLock     ringLock_;

    // compute() scratch + results.
    // No frame buffer: the ring is copied straight into the transform's own
    // input buffer.
    std::vector<float> magnitude_;    // numBins_, smoothed

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CQTAnalyzer)
};

#endif // CQTANALYZER_H_INCLUDED
