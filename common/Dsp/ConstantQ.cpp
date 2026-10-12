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

#include "ConstantQ.h"
#include "JuceHeader.h"
#include <cmath>

namespace mcfx
{

void ConstantQ::prepare (const Config& config)
{
    if (prepared_ && config == config_)
        return;

    config_   = config;
    prepared_ = true;
    numBins_  = 0;

    if (config_.sampleRate <= 0.0)
        return;

    fft_.prepare (config_.fftOrder);
    buildKernels();
}

double ConstantQ::getBinFrequency (int k) const
{
    return config_.fMin * std::pow (2.0, (double) k / (double) config_.binsPerOctave);
}

double ConstantQ::q() const
{
    const int res = config_.resolutionBinsPerOctave > 0 ? config_.resolutionBinsPerOctave
                                                        : config_.binsPerOctave;
    return 1.0 / (std::pow (2.0, 1.0 / res) - 1.0);
}

double ConstantQ::getConstantQFloorHz() const
{
    const int N = fft_.getSize();
    if (N <= 0)
        return config_.fMin;
    return q() * config_.sampleRate / (double) N;
}

int ConstantQ::getCentreFromEnd() const
{
    return config_.alignment == Alignment::Centred ? fft_.getSize() / 2 : 0;
}

void ConstantQ::buildKernels()
{
    kernelIndex_.clear();
    kernelValue_.clear();
    kernelOffset_.clear();

    const int N = fft_.getSize();
    fMax_ = juce::jmin (config_.fMax, 0.45 * config_.sampleRate);
    if (N <= 0 || fMax_ <= config_.fMin)
        return;

    const double fs = config_.sampleRate;
    const double Q  = q();
    const int bins  = (int) std::ceil (config_.binsPerOctave * std::log2 (fMax_ / config_.fMin));
    kernelOffset_.reserve ((size_t) bins + 1);
    kernelOffset_.push_back (0);

    std::vector<float> scratch ((size_t) (2 * N), 0.f);
    juce::dsp::FFT kernelFFT (config_.fftOrder);
    const double twoPi = juce::MathConstants<double>::twoPi;

    for (int k = 0; k < bins; ++k)
    {
        const double fk = getBinFrequency (k);
        // Ideal window is Q periods long; cap it at the frame and let Q fall.
        const int Nk = (int) juce::jmin ((double) N, std::ceil (Q * fs / fk));

        // End: the window stops at "now". Centred: every window shares the
        // frame's centre, so all bins describe the same instant.
        const int off = config_.alignment == Alignment::Centred ? N / 2 - Nk / 2 : N - Nk;

        std::fill (scratch.begin(), scratch.end(), 0.f);

        // Hamming, normalised to unit sum so the transform reads level directly.
        double wSum = 0.0;
        for (int n = 0; n < Nk; ++n)
            wSum += 0.54 - 0.46 * std::cos (twoPi * n / juce::jmax (1, Nk - 1));
        const double wNorm = wSum > 0.0 ? 1.0 / wSum : 1.0;

        for (int n = 0; n < Nk; ++n)
        {
            const double w = (0.54 - 0.46 * std::cos (twoPi * n / juce::jmax (1, Nk - 1))) * wNorm;
            // Absolute sample index in the frame keeps the centre frequency
            // right wherever the window sits and however it was capped.
            const double ph = twoPi * fk * (double) (off + n) / fs;
            scratch[(size_t) (2 * (off + n))]     = (float) (w * std::cos (ph));
            scratch[(size_t) (2 * (off + n) + 1)] = (float) (w * std::sin (ph));
        }

        kernelFFT.perform (reinterpret_cast<const juce::dsp::Complex<float>*> (scratch.data()),
                           reinterpret_cast<juce::dsp::Complex<float>*> (scratch.data()),
                           false);

        float peak = 0.f;
        for (int j = 0; j < N; ++j)
            peak = juce::jmax (peak, std::abs (std::complex<float> (scratch[(size_t) (2 * j)],
                                                                    scratch[(size_t) (2 * j + 1)])));

        // Every kernel is concentrated around its own (positive) centre
        // frequency, so nothing past the Nyquist bin survives the cut — which
        // is what lets transform() use a real-input FFT.
        const float cut = peak * config_.sparsity;
        for (int j = 0; j <= N / 2; ++j)
        {
            const std::complex<float> v (scratch[(size_t) (2 * j)], scratch[(size_t) (2 * j + 1)]);
            if (std::abs (v) >= cut)
            {
                kernelIndex_.push_back (j);
                kernelValue_.push_back (std::conj (v));
            }
        }
        kernelOffset_.push_back ((int) kernelIndex_.size());
    }

    numBins_ = bins;
}

void ConstantQ::transform (float* magnitudesOut)
{
    if (! isReady())
        return;

    fft_.forward();

    // Kernels carry a unit-sum window, so the dot product comes out scaled by
    // the frame length and by the half-amplitude of a real sine's analytic part.
    const float cal = 2.f / (float) fft_.getSize();

    // Backend-agnostic bin access (see RealFFT::forward()).
    const float* rp = fft_.realData();
    const float* ip = fft_.imagData();
    const int    st = fft_.binStride();
    const int    nb = fft_.getNumComplexBins();

    for (int k = 0; k < numBins_; ++k)
    {
        std::complex<float> acc (0.f, 0.f);
        for (int j = kernelOffset_[(size_t) k]; j < kernelOffset_[(size_t) k + 1]; ++j)
        {
            const int b = kernelIndex_[(size_t) j];
            if (b >= nb) continue;            // Nyquist: never inside a kernel in practice
            acc += std::complex<float> (rp[(size_t) (b * st)], ip[(size_t) (b * st)])
                       * kernelValue_[(size_t) j];
        }
        magnitudesOut[k] = std::abs (acc) * cal;
    }
}

float ConstantQ::magnitudeAt (const float* m, double freqHz) const
{
    if (numBins_ <= 0)
        return 0.f;

    const double pos = (double) config_.binsPerOctave * std::log2 (juce::jmax (1.0e-6, freqHz) / config_.fMin);
    if (pos <= 0.0)
        return m[0];
    if (pos >= (double) (numBins_ - 1))
        return m[numBins_ - 1];

    const int   k = (int) pos;
    const float f = (float) (pos - k);
    return m[k] * (1.f - f) + m[k + 1] * f;
}

} // namespace mcfx
