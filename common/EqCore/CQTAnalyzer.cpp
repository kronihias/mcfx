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

namespace
{
    /** Frame length, fixed at 32768. It sets both the cost of a frame and how far
        down constant-Q holds before the window caps and Q starts to fall — at
        48 kHz, 0.68 s and a floor of 50 Hz.

        Doubling it would carry constant-Q down to 25 Hz, but at double the cost
        and double the latency, now that the windows are centred. 50 Hz is a
        good trade — below it Q tapers off gradually rather than falling off a
        cliff.

        Held constant across sample rates so the cost is too; the floor rises in
        proportion (100 Hz at 96 kHz, 200 Hz at 192 kHz). */
    constexpr int kFFTOrder = 15;                    // 32768
}

void CQTAnalyzer::prepare (double sampleRate)
{
    if (sampleRate <= 0.0 || sampleRate == sampleRate_)
        return;

    ready_ = false;                    // stop the audio thread using the old ring

    sampleRate_ = sampleRate;

    mcfx::ConstantQ::Config cfg;
    cfg.sampleRate    = sampleRate;
    cfg.fftOrder      = kFFTOrder;
    cfg.fMin          = kFMin;
    cfg.fMax          = kFMax;
    cfg.binsPerOctave = kBinsPerOctave;
    cfg.resolutionBinsPerOctave = kResolutionPerOctave;
    cfg.alignment     = mcfx::ConstantQ::Alignment::Centred;
    cq_.prepare (cfg);                 // the expensive part, no lock held

    magnitude_.assign ((size_t) jmax (1, cq_.getNumBins()), 0.f);

    {
        const juce::SpinLock::ScopedLockType sl (ringLock_);
        ring_.assign ((size_t) cq_.getFrameSize(), 0.f);
        writePos_.store (0, std::memory_order_relaxed);
    }

    ready_ = cq_.isReady();
}

double CQTAnalyzer::getLatencySeconds() const
{
    return sampleRate_ > 0.0 ? (double) cq_.getCentreFromEnd() / sampleRate_ : 0.0;
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
    const int wp   = writePos_.load (std::memory_order_relaxed);
    const int n    = jmin (numSamples, size);
    const int skip = numSamples - n;          // oversized block: keep the newest

    // Two contiguous segments, never a per-sample wrap; the averaged mode mixes
    // straight into the ring with vector ops instead of a per-sample channel sum.
    const int first = jmin (n, size - wp);
    const int rest  = n - first;

    auto writeSegment = [&] (int dst, int srcOffset, int len)
    {
        if (len <= 0)
            return;
        if (channel >= 1 && channel <= numCh)
        {
            FloatVectorOperations::copy (ring_.data() + dst,
                                         buffer.getReadPointer (channel - 1) + srcOffset, len);
        }
        else
        {
            const float scale = 1.f / (float) numCh;
            FloatVectorOperations::copyWithMultiply (ring_.data() + dst,
                                                     buffer.getReadPointer (0) + srcOffset,
                                                     scale, len);
            for (int ch = 1; ch < numCh; ++ch)
                FloatVectorOperations::addWithMultiply (ring_.data() + dst,
                                                        buffer.getReadPointer (ch) + srcOffset,
                                                        scale, len);
        }
    };

    writeSegment (wp, skip, first);
    writeSegment (0, skip + first, rest);

    writePos_.store ((wp + n) % size, std::memory_order_release);
}

void CQTAnalyzer::compute()
{
    if (! ready_)
        return;

    // Copy the ring oldest-to-newest so the frame ends at the write head. The
    // audio thread may advance mid-copy; for a display that is harmless.
    const int frameSize = cq_.getFrameSize();
    float* frame = cq_.getTimeBuffer();
    {
        // Two memcpys rather than a per-sample modulo: the lock is held for
        // microseconds instead of the length of a 32k scalar loop, during
        // which the audio thread's try-lock would drop its block.
        const juce::SpinLock::ScopedLockType sl (ringLock_);
        const int size = jmin ((int) ring_.size(), frameSize);
        const int wp = writePos_.load (std::memory_order_acquire);
        const int first = size - wp;
        FloatVectorOperations::copy (frame, ring_.data() + wp, first);
        if (wp > 0)
            FloatVectorOperations::copy (frame + first, ring_.data(), wp);
        if (size < frameSize)
            FloatVectorOperations::clear (frame + size, frameSize - size);
    }

    // No smoothing here: the spectrogram already smooths per display column,
    // and compounding the two would blur away the fast treble response the
    // short constant-Q windows exist to provide.
    cq_.transform (magnitude_.data());
}

float CQTAnalyzer::getMagnitude (double freqHz) const
{
    if (! ready_)
        return 0.f;
    return cq_.magnitudeAt (magnitude_.data(), freqHz);
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
