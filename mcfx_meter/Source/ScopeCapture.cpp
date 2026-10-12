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

#include "ScopeCapture.h"

void ScopeCapture::prepare (double sampleRate, int numChannels)
{
    const int nCh = jmax (1, numChannels);
    if (sampleRate <= 0.0 || (sampleRate == sampleRate_ && nCh == numChannels_ && isReady()))
        return;

    ready_.store (false, std::memory_order_release);   // audio thread stops writing

    // About a second of history: the longest window is one second, and the
    // trigger search only runs on windows short enough to leave room for it.
    // Capped so 128 channels at a high rate stay at 64 MB rather than 256 —
    // the longest windows then just get clamped to what fits.
    const int wanted   = nextPowerOfTwo ((int) (sampleRate * 1.1));
    const int budget   = (16 * 1024 * 1024) / nCh;
    const int capacity = jmin (wanted, jmax (nextPowerOfTwo ((int) (sampleRate * 0.25)), budget));

    {
        const juce::SpinLock::ScopedLockType sl (ringLock_);
        ring_.setSize (nCh, capacity, false, true, false);
        ring_.clear();
        writePos_.store (0, std::memory_order_relaxed);
        written_.store (0, std::memory_order_relaxed);
    }

    sampleRate_  = sampleRate;
    numChannels_ = nCh;
    capacity_    = capacity;

    ready_.store (true, std::memory_order_release);
}

void ScopeCapture::push (const AudioSampleBuffer& buffer, int numSamples)
{
    if (! active_.load (std::memory_order_relaxed) || frozen_.load (std::memory_order_relaxed)
        || ! ready_.load (std::memory_order_acquire))
        return;

    // Skip the block rather than wait for a GUI read or a prepare().
    const juce::SpinLock::ScopedTryLockType sl (ringLock_);
    if (! sl.isLocked() || ring_.getNumSamples() <= 0)
        return;

    const int size = ring_.getNumSamples();
    const int nCh  = jmin (buffer.getNumChannels(), ring_.getNumChannels());
    const int n    = jmin (numSamples, size);
    const int skip = numSamples - n;
    const int wp   = writePos_.load (std::memory_order_relaxed);

    const int first = jmin (n, size - wp);
    const int rest  = n - first;

    for (int ch = 0; ch < nCh; ++ch)
    {
        const float* src = buffer.getReadPointer (ch) + skip;
        ring_.copyFrom (ch, wp, src, first);
        if (rest > 0)
            ring_.copyFrom (ch, 0, src + first, rest);
    }

    writePos_.store ((wp + n) % size, std::memory_order_release);
    written_.fetch_add (n, std::memory_order_relaxed);
}

int ScopeCapture::getNumValid() const
{
    return (int) jmin ((int64) capacity_, written_.load (std::memory_order_relaxed));
}

int64 ScopeCapture::read (AudioSampleBuffer& dest, int firstCh, int numCh, int numSamples, int endBack)
{
    numCh      = jmax (1, numCh);
    numSamples = jmax (1, numSamples);
    dest.setSize (numCh, numSamples, false, false, true);
    dest.clear();

    if (! isReady())
        return 0;

    const juce::SpinLock::ScopedLockType sl (ringLock_);

    const int size = ring_.getNumSamples();
    const int64 written = written_.load (std::memory_order_relaxed);
    if (size <= 0)
        return written;

    const int valid = (int) jmin ((int64) size, written);
    const int wp    = writePos_.load (std::memory_order_acquire);

    // Sample k of dest (0 = oldest) is (numSamples - 1 - k + endBack) samples
    // older than the newest. Only the part inside the valid history is copied;
    // the rest stays zero.
    for (int k = 0; k < numSamples; )
    {
        const int back = numSamples - 1 - k + endBack;
        if (back >= valid)
        {
            // Too old: jump to the first k that is young enough.
            k = jmax (k + 1, numSamples - valid + endBack);
            continue;
        }

        const int idx = ((wp - 1 - back) % size + size) % size;
        // Contiguous run up to the end of dest or the end of the ring.
        const int run = jmin (numSamples - k, size - idx);

        for (int c = 0; c < numCh; ++c)
        {
            const int src = firstCh + c;
            if (isPositiveAndBelow (src, ring_.getNumChannels()))
                dest.copyFrom (c, k, ring_, src, idx, run);
        }
        k += run;
    }

    return written;
}
