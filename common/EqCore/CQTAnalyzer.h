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
#include "Dsp/ConstantQ.h"
#include <atomic>
#include <vector>

/** Constant-Q analyzer for the spectrogram display.

    A plain FFT has linearly spaced bins, which is the wrong shape for a
    log-frequency plot: at 48 kHz a 4096-point FFT puts under two bins in the
    20-40 Hz octave while spending 850 on 10-20 kHz. The constant-Q transform
    instead places a fixed number of bins per octave, so the analysis window
    lengthens towards the bass and shortens towards the treble — matching how
    the result is drawn. The transform itself is mcfx::ConstantQ.

    Every bin's window is centred on the same instant, so a click is drawn as
    a vertical line instead of bending later into the bass (see
    ConstantQ::Alignment). The cost is that the spectrogram shows the instant
    getLatencySeconds() ago — 0.34 s at 48 kHz.

    Threading: push() runs on the audio thread and only copies samples.
    compute() does the transform and is meant for the GUI thread. */
class CQTAnalyzer
{
public:
    static constexpr int   kResolutionPerOctave = 24;   // quarter-tone sharp...
    static constexpr int   kBinsPerOctave       = 48;   // ...placed at eighth tones, so a tone
                                                        // between bins does not read ~1.8 dB low
    static constexpr float kFMin          = 20.f;
    static constexpr float kFMax          = 20000.f;

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
    int    getNumBins() const { return cq_.getNumBins(); }
    double getBinFrequency(int k) const { return cq_.getBinFrequency(k); }
    /** Frequency below which the window is capped and Q starts to fall. */
    double getConstantQFloorHz() const { return cq_.getConstantQFloorHz(); }
    /** How long ago the instant a compute() describes was. */
    double getLatencySeconds() const;

    void reset();

private:
    double sampleRate_ = 0.0;
    std::atomic<bool> ready_ { false };   // GUI writes, audio thread reads

    mcfx::ConstantQ cq_;

    // Mono ring of the analysed channel. prepare() reallocates it from the GUI
    // thread, so the audio thread takes a try-lock and simply skips a block
    // rather than writing into a buffer that is being replaced.
    std::vector<float> ring_;
    std::atomic<int>   writePos_ { 0 };
    juce::SpinLock     ringLock_;

    std::vector<float> magnitude_;    // getNumBins()

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CQTAnalyzer)
};

#endif // CQTANALYZER_H_INCLUDED
