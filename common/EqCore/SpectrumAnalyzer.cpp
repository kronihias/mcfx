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

#include "SpectrumAnalyzer.h"

SpectrumAnalyzer::SpectrumAnalyzer()
    : fft_(kFFTOrder)
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

    fftData_.resize(kFFTSize * 2, 0.f);
    tmpMag_.resize(kSpecLen, 0.f);
    magnitude_.resize(kSpecLen, 0.f);
}

void SpectrumAnalyzer::prepare(double sampleRate, int numChannels)
{
    sampleRate_ = sampleRate;
    numChannels_ = numChannels;
    ringBuffer_.setSize(numChannels, kFFTSize);
    ringBuffer_.clear();
    writePos_ = 0;
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
    std::fill(magnitude_.begin(), magnitude_.end(), 0.f);
    writePos_ = 0;
    ringBuffer_.clear();
}

void SpectrumAnalyzer::pushBuffer(const AudioSampleBuffer& buffer, int numSamples)
{
    int chans = jmin(buffer.getNumChannels(), numChannels_);
    int srcPos = 0;
    int remaining = numSamples;

    while (remaining > 0)
    {
        int space = kFFTSize - writePos_;
        int toCopy = jmin(remaining, space);

        for (int ch = 0; ch < chans; ++ch)
            ringBuffer_.copyFrom(ch, writePos_, buffer.getReadPointer(ch) + srcPos, toCopy);

        writePos_ += toCopy;
        srcPos += toCopy;
        remaining -= toCopy;

        if (writePos_ >= kFFTSize)
        {
            computeFFT();

            // 50% overlap: shift second half to first half
            int half = kFFTSize / 2;
            for (int ch = 0; ch < chans; ++ch)
            {
                FloatVectorOperations::copy(
                    ringBuffer_.getWritePointer(ch),
                    ringBuffer_.getReadPointer(ch, half),
                    half);
            }
            writePos_ = half;
        }
    }
}

void SpectrumAnalyzer::computeFFT()
{
    const float alpha = smoothAlpha_;
    const float oneMinusAlpha = 1.f - alpha;
    const float fftScale = 1.f / (windowCoherentGain_ * (float)kFFTSize);
    const int analyzerCh = analyzerChannel_.load(std::memory_order_relaxed);

    // Lambda: compute FFT magnitude for one channel into tmpMag_
    auto computeChannelMag = [&](int ch)
    {
        // Copy windowed signal into FFT buffer
        FloatVectorOperations::copy(fftData_.data(), ringBuffer_.getReadPointer(ch), kFFTSize);
        FloatVectorOperations::multiply(fftData_.data(), window_.data(), kFFTSize);

        // Zero the second half (JUCE FFT expects 2*N buffer)
        FloatVectorOperations::clear(fftData_.data() + kFFTSize, kFFTSize);

        // Forward FFT
        fft_.performRealOnlyForwardTransform(fftData_.data());

        // Compute magnitude from interleaved real/imag pairs
        for (int i = 0; i < kSpecLen; ++i)
        {
            float re = fftData_[i * 2];
            float im = fftData_[i * 2 + 1];
            tmpMag_[i] = std::sqrt(re * re + im * im) * fftScale;
        }
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
        std::vector<float> accum(kSpecLen, 0.f);
        for (int ch = 0; ch < numChannels_; ++ch)
        {
            computeChannelMag(ch);
            FloatVectorOperations::add(accum.data(), tmpMag_.data(), kSpecLen);
        }
        float scale = 1.f / (float)jmax(1, numChannels_);
        FloatVectorOperations::multiply(accum.data(), scale, kSpecLen);

        for (int i = 0; i < kSpecLen; ++i)
            magnitude_[i] = alpha * magnitude_[i] + oneMinusAlpha * accum[i];
    }
}

float SpectrumAnalyzer::getMagnitude(double freqHz) const
{
    float bin = (float)(freqHz / sampleRate_ * (double)kFFTSize);
    int b0 = jlimit(0, kSpecLen - 2, (int)bin);
    int b1 = b0 + 1;
    float frac = bin - (float)b0;
    return magnitude_[b0] + frac * (magnitude_[b1] - magnitude_[b0]);
}
