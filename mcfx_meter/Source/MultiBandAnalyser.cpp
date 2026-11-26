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

    hop_ = fftSize_ / 4;

    // Periodic Hann: at a quarter-frame hop its squares sum exactly to a
    // constant (see compute()).
    window_.resize ((size_t) fftSize_);
    double sumSq = 0.0;
    for (int i = 0; i < fftSize_; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (MathConstants<double>::twoPi * i / (double) fftSize_);
        window_[(size_t) i] = (float) w;
        sumSq += w * w;
    }
    // A sine of amplitude A puts (A/2)^2 * N * sum(w^2) into the positive-
    // frequency bins around it, whatever the window, so this reads A as 1.0
    // for a full-scale sine and band power for noise.
    bandNorm_ = (float) std::sqrt (4.0 / ((double) fftSize_ * sumSq));

    buildBandTable();

    magSq_.assign ((size_t) (fftSize_ / 2 + 1), 0.f);
    levels_.assign ((size_t) numChannels_ * kNumBands, 0.f);
    primed_ = false;

    {
        const juce::SpinLock::ScopedLockType sl (ringLock_);
        const int headroom = (int) std::ceil (0.1 * sampleRate_);
        ring_.setSize (numChannels_, nextPowerOfTwo (fftSize_ + kMaxFramesPerCall * hop_ + headroom),
                       false, true, false);
        ring_.clear();
        written_.store (0, std::memory_order_relaxed);
    }
    nextFrame_ = -1;

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
    const int skip = numSamples - n;          // oversized block: keep the newest
    const int wp   = (int) (written_.load (std::memory_order_relaxed) & (int64) (size - 1));

    // Two segments per channel rather than a per-sample modulo: at 128 channels
    // the difference is a memcpy against a quarter of a million branches.
    const int first = jmin (n, size - wp);
    const int rest  = n - first;

    for (int ch = 0; ch < nCh; ++ch)
    {
        const float* src = buffer.getReadPointer (ch) + skip;
        ring_.copyFrom (ch, wp, src, first);
        if (rest > 0)
            ring_.copyFrom (ch, 0, src + first, rest);
    }

    written_.fetch_add (n, std::memory_order_release);
}

void MultiBandAnalyser::setSmoothingTime (float seconds)
{
    smoothTau_ = jmax (0.f, seconds);
}

void MultiBandAnalyser::compute()
{
    if (! ready_ || ! fft_.isReady() || numChannels_ <= 0)
        return;

    const int64 written = written_.load (std::memory_order_acquire);
    if (written < fftSize_)
        return;

    // Frame k covers [k * hop, k * hop + N). Start at the newest complete
    // one, and start over there after falling too far behind.
    const int64 newest = (written - fftSize_) / hop_;
    if (nextFrame_ < 0 || newest - nextFrame_ + 1 > kMaxFramesPerCall)
        nextFrame_ = jmax ((int64) 0, newest - (nextFrame_ < 0 ? 0 : kMaxFramesPerCall - 1));

    const float alpha = smoothTau_ > 0.f
                          ? (float) std::exp (-(double) hop_ / (smoothTau_ * sampleRate_)) : 0.f;

    // Every channel of a frame is read from the same absolute position, which
    // is what the view depends on. Oldest frame first; the ring's headroom
    // covers whatever the audio thread writes while the pass runs.
    for (; nextFrame_ <= newest; ++nextFrame_)
    {
        const int64 start = nextFrame_ * hop_;

        for (int ch = 0; ch < numChannels_; ++ch)
        {
            // Hold the lock only for the copy — holding it across the
            // transform would make the audio thread's try-lock fail and drop
            // blocks.
            {
                const juce::SpinLock::ScopedLockType sl (ringLock_);
                if (ch >= ring_.getNumChannels())
                    continue;

                const int size = ring_.getNumSamples();
                const int pos  = (int) (start & (int64) (size - 1));
                const float* src = ring_.getReadPointer (ch);
                const int first = jmin (fftSize_, size - pos);
                // Copied straight into the transform's own buffer.
                float* t = fft_.getTimeBuffer();
                FloatVectorOperations::copy (t, src + pos, first);
                if (first < fftSize_)
                    FloatVectorOperations::copy (t + first, src, fftSize_ - first);
            }

            FloatVectorOperations::multiply (fft_.getTimeBuffer(), window_.data(), fftSize_);
            fft_.magnitudesSquared (magSq_.data());

            // Sum power across each band, then take the root: that makes a
            // band's reading independent of how many bins happen to fall
            // inside it, which is what lets broadband content read at the
            // right level.
            float* out = levels_.data() + (size_t) ch * kNumBands;
            const float* mag = magSq_.data();

            for (int b = 0; b < kNumBands; ++b)
            {
                const auto& e = bands_[(size_t) b];
                float power;

                if (e.hi == e.lo)
                {
                    // Band inside a single bin. The weight applies once — the
                    // old form multiplied the low and high weights together
                    // and so squared a fraction, under-reading every
                    // sub-bin-wide band.
                    power = e.wLo * mag[e.lo];
                }
                else
                {
                    power = e.wLo * mag[e.lo] + e.wHi * mag[e.hi];
                    for (int bin = e.lo + 1; bin < e.hi; ++bin)
                        power += mag[bin];
                }

                const float level = std::sqrt (power) * bandNorm_;

                // Averaged in power, not amplitude: it is power that is
                // additive for uncorrelated content, so this converges on the
                // right level for noise instead of biasing low.
                out[b] = (primed_ && alpha > 0.f)
                             ? std::sqrt (alpha * out[b] * out[b] + (1.f - alpha) * level * level)
                             : level;
            }
        }

        primed_ = true;
    }
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
        written_.store (0, std::memory_order_relaxed);
    }
    nextFrame_ = -1;
    std::fill (levels_.begin(), levels_.end(), 0.f);
    primed_ = false;
}
