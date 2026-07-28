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

#include "CQTAnalyzer.h"
#include "Fftw/FftwPlanner.h"

namespace
{
    /** Frame length, fixed at 32768. It sets both the cost of a frame and how far
        down constant-Q holds before the window caps and Q starts to fall — at
        48 kHz, 0.68 s and a floor of 50 Hz.

        Doubling it would carry constant-Q down to 25 Hz, but at double the cost:
        the transform grows with the frame and so does the number of kernel
        coefficients, which together are the whole per-frame budget. 50 Hz is a
        good trade — below it Q tapers off gradually rather than falling off a
        cliff, and it is still far finer than the linear-bin analyzer this
        replaced, which was down to Q of about 1.7 at 20 Hz.

        Held constant across sample rates so the cost is too; the floor rises in
        proportion (100 Hz at 96 kHz, 200 Hz at 192 kHz). */
    int chooseFFTOrder (double)
    {
        return 15;                                   // 32768
    }
}

void CQTAnalyzer::prepare (double sampleRate)
{
    if (sampleRate <= 0.0 || sampleRate == sampleRate_)
        return;

    ready_ = false;                    // stop the audio thread using the old ring

    sampleRate_ = sampleRate;
    fftOrder_   = chooseFFTOrder (sampleRate);
    fftSize_    = 1 << fftOrder_;
    // FFTW's planner is process-global and not thread-safe; juce::dsp::FFT
    // plans here when built against it.
    mcfx::ensureFftwPlannerThreadSafe();
    fft_        = std::make_unique<juce::dsp::FFT> (fftOrder_);
    frame_.assign ((size_t) (2 * fftSize_), 0.f);

    buildKernels();                    // the expensive part, no lock held
    magnitude_.assign ((size_t) jmax (1, numBins_), 0.f);

    {
        const juce::SpinLock::ScopedLockType sl (ringLock_);
        ring_.assign ((size_t) fftSize_, 0.f);
        writePos_.store (0, std::memory_order_relaxed);
    }

    ready_ = numBins_ > 0;
}

void CQTAnalyzer::buildKernels()
{
    kernelIndex_.clear();
    kernelValue_.clear();
    kernelOffset_.clear();

    const double fMax = jmin ((double) kFMax, 0.45 * sampleRate_);
    if (fMax <= (double) kFMin)
    {
        numBins_ = 0;
        return;
    }

    const double Q = 1.0 / (std::pow (2.0, 1.0 / kBinsPerOctave) - 1.0);
    numBins_ = (int) std::ceil (kBinsPerOctave * std::log2 (fMax / (double) kFMin));
    kernelOffset_.reserve ((size_t) numBins_ + 1);
    kernelOffset_.push_back (0);

    std::vector<float> scratch ((size_t) (2 * fftSize_), 0.f);
    juce::dsp::FFT kernelFFT (fftOrder_);

    for (int k = 0; k < numBins_; ++k)
    {
        const double fk = getBinFrequency (k);
        // Ideal window is Q periods long; cap it at the frame and let Q fall.
        const int Nk = (int) jmin ((double) fftSize_, std::ceil (Q * sampleRate_ / fk));
        const int off = fftSize_ - Nk;               // window ends at "now"

        std::fill (scratch.begin(), scratch.end(), 0.f);

        // Hamming, normalised to unit sum so the transform reads level directly.
        double wSum = 0.0;
        for (int n = 0; n < Nk; ++n)
            wSum += 0.54 - 0.46 * std::cos (2.0 * MathConstants<double>::pi * n / jmax (1, Nk - 1));
        const double wNorm = wSum > 0.0 ? 1.0 / wSum : 1.0;

        for (int n = 0; n < Nk; ++n)
        {
            const double w = (0.54 - 0.46 * std::cos (2.0 * MathConstants<double>::pi * n / jmax (1, Nk - 1)))
                                 * wNorm;
            // Absolute sample index in the frame keeps the centre frequency
            // correct even where Nk has been capped.
            const double ph = 2.0 * MathConstants<double>::pi * fk * (double) (off + n) / sampleRate_;
            scratch[(size_t) (2 * (off + n))]     = (float) (w * std::cos (ph));
            scratch[(size_t) (2 * (off + n) + 1)] = (float) (w * std::sin (ph));
        }

        kernelFFT.perform (reinterpret_cast<const juce::dsp::Complex<float>*> (scratch.data()),
                           reinterpret_cast<juce::dsp::Complex<float>*> (scratch.data()),
                           false);

        float peak = 0.f;
        for (int j = 0; j < fftSize_; ++j)
            peak = jmax (peak, std::abs (std::complex<float> (scratch[(size_t) (2 * j)],
                                                              scratch[(size_t) (2 * j + 1)])));

        const float cut = peak * kKernelSparsity;
        for (int j = 0; j < fftSize_; ++j)
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
}

double CQTAnalyzer::getBinFrequency (int k) const
{
    return (double) kFMin * std::pow (2.0, (double) k / (double) kBinsPerOctave);
}

double CQTAnalyzer::getConstantQFloorHz() const
{
    if (fftSize_ <= 0)
        return (double) kFMin;
    const double Q = 1.0 / (std::pow (2.0, 1.0 / kBinsPerOctave) - 1.0);
    return Q * sampleRate_ / (double) fftSize_;
}

void CQTAnalyzer::push (const AudioSampleBuffer& buffer, int numSamples, int channel)
{
    if (! ready_ || ring_.empty())
        return;

    const int numCh = buffer.getNumChannels();
    if (numCh <= 0)
        return;

    const juce::SpinLock::ScopedTryLockType sl (ringLock_);
    if (! sl.isLocked() || ring_.empty())
        return;

    const int size = (int) ring_.size();
    int wp = writePos_.load (std::memory_order_relaxed);

    if (channel >= 1 && channel <= numCh)
    {
        const float* src = buffer.getReadPointer (channel - 1);
        for (int i = 0; i < numSamples; ++i)
        {
            ring_[(size_t) wp] = src[i];
            wp = (wp + 1) % size;
        }
    }
    else
    {
        const float scale = 1.f / (float) numCh;
        for (int i = 0; i < numSamples; ++i)
        {
            float sum = 0.f;
            for (int ch = 0; ch < numCh; ++ch)
                sum += buffer.getReadPointer (ch)[i];
            ring_[(size_t) wp] = sum * scale;
            wp = (wp + 1) % size;
        }
    }

    writePos_.store (wp, std::memory_order_release);
}

void CQTAnalyzer::compute()
{
    if (! ready_ || fft_ == nullptr)
        return;

    // Copy the ring oldest-to-newest so the frame ends at the write head. The
    // audio thread may advance mid-copy; for a display that is harmless.
    // The input is real, so it goes in unpacked (not interleaved) — the real-only
    // transform is ~1.8x cheaper than treating it as complex with a zero imaginary
    // part, and costs nothing here: every kernel is concentrated around its own
    // centre frequency, so no kernel index reaches past the Nyquist bin that the
    // real transform stops at.
    std::fill (frame_.begin(), frame_.end(), 0.f);
    {
        const juce::SpinLock::ScopedLockType sl (ringLock_);
        const int size = (int) ring_.size();
        const int wp = writePos_.load (std::memory_order_acquire);
        for (int i = 0; i < size; ++i)
            frame_[(size_t) i] = ring_[(size_t) ((wp + i) % size)];
    }

    fft_->performRealOnlyForwardTransform (frame_.data(), true);

    // Kernels carry a unit-sum window, so the dot product comes out scaled by
    // the frame length and by the half-amplitude of a real sine's analytic part.
    // No smoothing here: the spectrogram already smooths per display column, and
    // compounding the two would blur away the fast treble response the short
    // constant-Q windows exist to provide.
    const float cal = 2.f / (float) fftSize_;

    for (int k = 0; k < numBins_; ++k)
    {
        std::complex<float> acc (0.f, 0.f);
        for (int j = kernelOffset_[(size_t) k]; j < kernelOffset_[(size_t) k + 1]; ++j)
        {
            const int b = kernelIndex_[(size_t) j];
            acc += std::complex<float> (frame_[(size_t) (2 * b)], frame_[(size_t) (2 * b + 1)])
                       * kernelValue_[(size_t) j];
        }
        magnitude_[(size_t) k] = std::abs (acc) * cal;
    }
}

float CQTAnalyzer::getMagnitude (double freqHz) const
{
    if (! ready_ || numBins_ <= 0)
        return 0.f;

    const double pos = (double) kBinsPerOctave * std::log2 (jmax (1.0e-6, freqHz) / (double) kFMin);
    if (pos <= 0.0)
        return magnitude_[0];
    if (pos >= (double) (numBins_ - 1))
        return magnitude_[(size_t) (numBins_ - 1)];

    const int   k = (int) pos;
    const float f = (float) (pos - k);
    return magnitude_[(size_t) k] * (1.f - f) + magnitude_[(size_t) (k + 1)] * f;
}

void CQTAnalyzer::reset()
{
    {
        const juce::SpinLock::ScopedLockType sl (ringLock_);
        std::fill (ring_.begin(), ring_.end(), 0.f);
        writePos_.store (0, std::memory_order_relaxed);
    }
    std::fill (magnitude_.begin(), magnitude_.end(), 0.f);
}
