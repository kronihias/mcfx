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

// Before SpectrumAnalyzer.h: JuceHeader's `using namespace juce` makes
// Carbon's Point ambiguous if Accelerate is parsed after it.
#if defined(__APPLE__)
 #include <Accelerate/Accelerate.h>
#endif

#include "SpectrumAnalyzer.h"

SpectrumAnalyzer::SpectrumAnalyzer()

{
    // Blackman-Harris window
    window_.resize(kFFTSize);
    double windowSum = 0.0;
    for (int i = 0; i < kFFTSize; ++i)
    {
        double x = (double)i / (double)(kFFTSize - 1);
        window_[i] = (float)(0.35875 - 0.48829 * std::cos(2.0 * MathConstants<double>::pi * x)
                                     + 0.14128 * std::cos(4.0 * MathConstants<double>::pi * x)
                                     - 0.01168 * std::cos(6.0 * MathConstants<double>::pi * x));
        windowSum += window_[i];
    }
    windowCoherentGain_ = (float)(windowSum / (double)kFFTSize);

    fft_.prepare(kFFTOrder);
    magSq_.resize(kSpecLen, 0.f);
    tmpMag_.resize(kSpecLen, 0.f);
    accum_.resize(kSpecLen, 0.f);
    magnitude_.resize(kSpecLen, 0.f);
}

void SpectrumAnalyzer::prepare(double sampleRate, int numChannels)
{
    sampleRate_ = sampleRate;
    numChannels_ = numChannels;
    {
        const juce::SpinLock::ScopedLockType sl(ringLock_);
        ringBuffer_.setSize(numChannels, kFFTSize);
        ringBuffer_.clear();
        writePos_.store(0, std::memory_order_relaxed);
    }
    samplesSinceFFT_.store(0, std::memory_order_relaxed);
    std::fill(magnitude_.begin(), magnitude_.end(), 0.f);

    // Adjust smoothing based on sample rate (similar to mcfx_filter)
    // At 48kHz with hop=2048, we get ~23 FFTs/sec. Alpha=0.85 gives ~6 updates to settle.
    // Scale for other rates.
    double updatesPerSec = sampleRate / (double)(kFFTSize / 2);
    double targetDecayTime = 0.15; // seconds to decay to ~5%
    double targetAlpha = 1.0 - 3.0 / (updatesPerSec * targetDecayTime);
    smoothAlpha_ = (float)jlimit(0.5, 0.95, targetAlpha);
}

void SpectrumAnalyzer::reset()
{
    {
        const juce::SpinLock::ScopedLockType sl(ringLock_);
        ringBuffer_.clear();
        writePos_.store(0, std::memory_order_relaxed);
    }
    samplesSinceFFT_.store(0, std::memory_order_relaxed);
    std::fill(magnitude_.begin(), magnitude_.end(), 0.f);
}

void SpectrumAnalyzer::pushBuffer(const AudioSampleBuffer& buffer, int numSamples)
{
    // Skip the block rather than write into a ring prepare() may be replacing.
    const juce::SpinLock::ScopedTryLockType sl(ringLock_);
    if (!sl.isLocked() || ringBuffer_.getNumSamples() < kFFTSize)
        return;

    const int chans = jmin(buffer.getNumChannels(), numChannels_);
    const int n     = jmin(numSamples, kFFTSize);
    const int skip  = numSamples - n;         // oversized block: keep the newest
    const int wp    = writePos_.load(std::memory_order_relaxed);

    // Two contiguous segments per channel, never a per-sample wrap.
    const int first = jmin(n, kFFTSize - wp);
    const int rest  = n - first;

    for (int ch = 0; ch < chans; ++ch)
    {
        const float* src = buffer.getReadPointer(ch) + skip;
        ringBuffer_.copyFrom(ch, wp, src, first);
        if (rest > 0)
            ringBuffer_.copyFrom(ch, 0, src + first, rest);
    }

    writePos_.store((wp + n) % kFFTSize, std::memory_order_release);
    samplesSinceFFT_.fetch_add(numSamples, std::memory_order_relaxed);
}

void SpectrumAnalyzer::update()
{
    if (numChannels_ <= 0 || !fft_.isReady())
        return;

    // Half a window is the hop the inline version had, so the effective FFT
    // rate — and the smoothing calibrated to it in prepare() — is unchanged.
    if (samplesSinceFFT_.load(std::memory_order_relaxed) < kFFTSize / 2)
        return;
    samplesSinceFFT_.store(0, std::memory_order_relaxed);

    computeFFT();
}

void SpectrumAnalyzer::computeFFT()
{
    const float alpha = smoothAlpha_;
    const float oneMinusAlpha = 1.f - alpha;
    const float fftScale = 1.f / (windowCoherentGain_ * (float)kFFTSize);
    const int analyzerCh = analyzerChannel_.load(std::memory_order_relaxed);

    // Latched once so the averaged mode reads every channel from the same
    // instant. Audio pushed while the pass runs can overwrite at most a
    // block's worth of the window's oldest samples — invisible under the
    // window's own taper.
    const int wp = writePos_.load(std::memory_order_acquire);

    // Lambda: compute FFT magnitude for one channel into tmpMag_
    auto computeChannelMag = [&](int ch)
    {
        // Ring to time buffer in two segments, oldest to newest, so the frame
        // ends at the write head. Hold the lock only for the copy — holding it
        // across the transform would make the audio thread drop blocks.
        {
            const juce::SpinLock::ScopedLockType sl(ringLock_);
            if (ch >= ringBuffer_.getNumChannels())
                return;
            const float* src = ringBuffer_.getReadPointer(ch);
            float* t = fft_.getTimeBuffer();
            FloatVectorOperations::copy(t, src + wp, kFFTSize - wp);
            if (wp > 0)
                FloatVectorOperations::copy(t + (kFFTSize - wp), src, wp);
        }

        FloatVectorOperations::multiply(fft_.getTimeBuffer(), window_.data(), kFFTSize);

        fft_.magnitudesSquared(magSq_.data());

#if defined(__APPLE__)
        const int n = kSpecLen;
        vvsqrtf(tmpMag_.data(), magSq_.data(), &n);
        FloatVectorOperations::multiply(tmpMag_.data(), fftScale, kSpecLen);
#else
        for (int i = 0; i < kSpecLen; ++i)
            tmpMag_[i] = std::sqrt(magSq_[i]) * fftScale;
#endif
    };

    if (analyzerCh > 0 && analyzerCh <= numChannels_)
    {
        // Single channel analysis
        computeChannelMag(analyzerCh - 1);
        for (int i = 0; i < kSpecLen; ++i)
            magnitude_[i] = alpha * magnitude_[i] + oneMinusAlpha * tmpMag_[i];
    }
    else
    {
        // Average across all channels
        FloatVectorOperations::clear(accum_.data(), kSpecLen);
        for (int ch = 0; ch < numChannels_; ++ch)
        {
            computeChannelMag(ch);
            FloatVectorOperations::add(accum_.data(), tmpMag_.data(), kSpecLen);
        }
        float scale = 1.f / (float)jmax(1, numChannels_);
        FloatVectorOperations::multiply(accum_.data(), scale, kSpecLen);

        for (int i = 0; i < kSpecLen; ++i)
            magnitude_[i] = alpha * magnitude_[i] + oneMinusAlpha * accum_[i];
    }
}

float SpectrumAnalyzer::getMaxMagnitude(double freqLoHz, double freqHiHz) const
{
    const double binScale = (double) kFFTSize / sampleRate_;
    const double lo = freqLoHz * binScale;
    const double hi = freqHiHz * binScale;

    // Narrower than a bin (the low end of the plot): interpolate as before, so the
    // curve stays smooth instead of turning into a staircase.
    if (! (hi - lo >= 1.0))
        return getMagnitude(0.5 * (freqLoHz + freqHiHz));

    const int b0 = jlimit(0, kSpecLen - 1, (int) std::floor(lo));
    const int b1 = jlimit(0, kSpecLen - 1, (int) std::ceil (hi));
    float m = 0.f;
    for (int b = b0; b <= b1; ++b)
        m = jmax(m, magnitude_[(size_t) b]);
    return m;
}

float SpectrumAnalyzer::getMagnitude(double freqHz) const
{
    float bin = (float)(freqHz / sampleRate_ * (double)kFFTSize);
    int b0 = jlimit(0, kSpecLen - 2, (int)bin);
    int b1 = b0 + 1;
    float frac = bin - (float)b0;
    return magnitude_[b0] + frac * (magnitude_[b1] - magnitude_[b0]);
}
