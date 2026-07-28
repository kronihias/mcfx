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

#include "MultiBandAnalyser.h"
#include "Fftw/FftwPlanner.h"

float MultiBandAnalyser::getBandCentreHz (int band)
{
    return kBandRefHz * std::pow (2.f, (float) band / (float) kBandsPerOctave);
}

float MultiBandAnalyser::getBandLowHz (int band)
{
    return getBandCentreHz (band) * std::pow (2.f, -0.5f / (float) kBandsPerOctave);
}

float MultiBandAnalyser::getBandHighHz (int band)
{
    return getBandCentreHz (band) * std::pow (2.f, 0.5f / (float) kBandsPerOctave);
}

float MultiBandAnalyser::getResolutionFloorHz() const
{
    if (fftSize_ <= 0 || sampleRate_ <= 0.0)
        return 0.f;

    // A band is one bin wide when fc * (2^(1/2B) - 2^(-1/2B)) == binWidth.
    const float binHz = (float) (sampleRate_ / (double) fftSize_);
    const float rel = std::pow (2.f, 0.5f / (float) kBandsPerOctave)
                    - std::pow (2.f, -0.5f / (float) kBandsPerOctave);
    return binHz / rel;
}

void MultiBandAnalyser::prepare (double sampleRate, int numChannels)
{
    const int nCh = jmax (1, numChannels);
    if (sampleRate <= 0.0 || (sampleRate == sampleRate_ && nCh == numChannels_ && ready_))
        return;

    ready_ = false;                 // stop the audio thread touching the old ring

    sampleRate_  = sampleRate;
    numChannels_ = nCh;
    fftSize_     = 1 << fftOrder_;
    fft_.prepare (fftOrder_);

    // Blackman-Harris, matching SpectrumAnalyzer, with the same coherent-gain
    // normalisation so a full-scale sine reads 1.0 in its band.
    window_.resize ((size_t) fftSize_);
    double sum = 0.0;
    for (int i = 0; i < fftSize_; ++i)
    {
        const double t = MathConstants<double>::twoPi * i / (double) (fftSize_ - 1);
        const double w = 0.35875 - 0.48829 * std::cos (t)
                       + 0.14128 * std::cos (2.0 * t) - 0.01168 * std::cos (3.0 * t);
        window_[(size_t) i] = (float) w;
        sum += w;
    }
    windowGain_ = (float) (sum / (double) fftSize_);

    buildBandTable();

    magSq_.assign ((size_t) (fftSize_ / 2 + 1), 0.f);
    levels_.assign ((size_t) numChannels_ * kNumBands, 0.f);
    primed_ = false;

    {
        const juce::SpinLock::ScopedLockType sl (ringLock_);
        ring_.setSize (numChannels_, fftSize_, false, true, false);
        ring_.clear();
        writePos_.store (0, std::memory_order_relaxed);
    }

    ready_ = true;
}

void MultiBandAnalyser::buildBandTable()
{
    bands_.assign ((size_t) kNumBands, BandBins{});

    const double binHz = sampleRate_ / (double) fftSize_;
    const int maxBin = fftSize_ / 2;

    for (int b = 0; b < kNumBands; ++b)
    {
        const int band = kLowestBand + b;
        const double lo = getBandLowHz (band) / binHz;
        const double hi = getBandHighHz (band) / binHz;

        const int b0 = jlimit (0, maxBin, (int) std::floor (lo));
        const int b1 = jlimit (0, maxBin, (int) std::floor (hi));

        auto& e = bands_[(size_t) b];
        e.lo = b0;
        e.hi = b1;
        e.resolutionLimited = (hi - lo) < 1.0;

        if (b1 == b0)
        {
            // Band sits inside a single bin: it only gets the fraction it covers.
            e.wLo = e.wHi = (float) jlimit (0.0, 1.0, hi - lo);
        }
        else
        {
            e.wLo = (float) jlimit (0.0, 1.0, (double) (b0 + 1) - lo);
            e.wHi = (float) jlimit (0.0, 1.0, hi - (double) b1);
        }
    }
}

void MultiBandAnalyser::setActive (bool shouldBeActive)
{
    active_.store (shouldBeActive, std::memory_order_relaxed);
}

void MultiBandAnalyser::push (const AudioSampleBuffer& buffer, int numSamples)
{
    if (! ready_ || ! active_.load (std::memory_order_relaxed))
        return;

    // Skip the block rather than write into a ring prepare() may be replacing.
    const juce::SpinLock::ScopedTryLockType sl (ringLock_);
    if (! sl.isLocked() || ring_.getNumSamples() <= 0)
        return;

    const int size = ring_.getNumSamples();
    const int nCh  = jmin (buffer.getNumChannels(), ring_.getNumChannels());
    const int n    = jmin (numSamples, size);
    const int wp   = writePos_.load (std::memory_order_relaxed);

    // Two segments per channel rather than a per-sample modulo: at 128 channels
    // the difference is a memcpy against a quarter of a million branches.
    const int first = jmin (n, size - wp);
    const int rest  = n - first;

    for (int ch = 0; ch < nCh; ++ch)
    {
        const float* src = buffer.getReadPointer (ch);
        ring_.copyFrom (ch, wp, src, first);
        if (rest > 0)
            ring_.copyFrom (ch, 0, src + first, rest);
    }

    writePos_.store ((wp + n) % size, std::memory_order_release);
}

void MultiBandAnalyser::setSmoothing (float alpha)
{
    smoothAlpha_ = jlimit (0.f, 0.99f, alpha);
}

void MultiBandAnalyser::compute()
{
    if (! ready_ || ! fft_.isReady() || numChannels_ <= 0)
        return;

    // Latched once for the whole pass. Every channel is then read from the same
    // instant, which is the property the view depends on — and it is free,
    // because a pass takes under a millisecond while the ring holds 85 ms, so
    // nothing under this position can be overwritten before the pass ends.
    const int wp = writePos_.load (std::memory_order_acquire);

    for (int ch = 0; ch < numChannels_; ++ch)
    {
        // Hold the lock only for the copy — holding it across the transform
        // would make the audio thread's try-lock fail and drop blocks.
        {
            const juce::SpinLock::ScopedLockType sl (ringLock_);
            if (ch >= ring_.getNumChannels())
                continue;

            const int size = ring_.getNumSamples();
            const float* src = ring_.getReadPointer (ch);
            const int first = size - wp;
            // Oldest-to-newest, so the frame ends at the write head. Copied
            // straight into the transform's own buffer — no staging array.
            float* t = fft_.getTimeBuffer();
            FloatVectorOperations::copy (t, src + wp, first);
            if (wp > 0)
                FloatVectorOperations::copy (t + first, src, wp);
        }

        FloatVectorOperations::multiply (fft_.getTimeBuffer(), window_.data(), fftSize_);
        fft_.magnitudesSquared (magSq_.data());

        // Sum power across each band, then take the root: that makes a band's
        // reading independent of how many bins happen to fall inside it, which
        // is what lets broadband content read at the right level.
        const float norm = 1.f / (windowGain_ * (float) fftSize_);
        float* out = levels_.data() + (size_t) ch * kNumBands;

        const float* mag = magSq_.data();

        for (int b = 0; b < kNumBands; ++b)
        {
            const auto& e = bands_[(size_t) b];
            float power;

            if (e.hi == e.lo)
            {
                // Band inside a single bin. The weight applies once — the old
                // form multiplied the low and high weights together and so
                // squared a fraction, under-reading every sub-bin-wide band.
                power = e.wLo * mag[e.lo];
            }
            else
            {
                power = e.wLo * mag[e.lo] + e.wHi * mag[e.hi];
                for (int bin = e.lo + 1; bin < e.hi; ++bin)
                    power += mag[bin];
            }

            // 2x for the negative-frequency half a real transform folds away.
            const float mag = std::sqrt (2.f * power) * norm;

            // Averaged in power, not amplitude: it is power that is additive for
            // uncorrelated content, so this converges on the right level for
            // noise instead of biasing low.
            out[b] = (primed_ && smoothAlpha_ > 0.f)
                         ? std::sqrt (smoothAlpha_ * out[b] * out[b]
                                      + (1.f - smoothAlpha_) * mag * mag)
                         : mag;
        }
    }

    primed_ = true;
}

const float* MultiBandAnalyser::getChannelBands (int channel) const
{
    if (! ready_ || ! isPositiveAndBelow (channel, numChannels_))
        return nullptr;
    return levels_.data() + (size_t) channel * kNumBands;
}

void MultiBandAnalyser::reset()
{
    {
        const juce::SpinLock::ScopedLockType sl (ringLock_);
        ring_.clear();
        writePos_.store (0, std::memory_order_relaxed);
    }
    std::fill (levels_.begin(), levels_.end(), 0.f);
    primed_ = false;
}
