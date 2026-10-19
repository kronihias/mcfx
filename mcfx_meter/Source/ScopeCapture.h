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

#ifndef SCOPECAPTURE_H_INCLUDED
#define SCOPECAPTURE_H_INCLUDED

#include "JuceHeader.h"
#include <atomic>

/** Raw sample history of every channel, for the oscilloscope view.

    Same threading as MultiBandAnalyser: the audio thread only copies, behind a
    gate that is closed unless the scope is on screen, and the buffer is not
    allocated until the scope is first shown.

    Pausing is freezing: push() stops writing, so the ring itself is the
    snapshot and nothing has to be copied to inspect it. */
class ScopeCapture
{
public:
    ScopeCapture() = default;

    /** Allocates, so GUI thread. Cheap no-op when nothing changed. */
    void prepare (double sampleRate, int numChannels);

    void setActive (bool shouldBeActive) { active_.store (shouldBeActive, std::memory_order_relaxed); }
    bool isActive() const                { return active_.load (std::memory_order_relaxed); }

    void setFrozen (bool shouldBeFrozen) { frozen_.store (shouldBeFrozen, std::memory_order_relaxed); }
    bool isFrozen() const                { return frozen_.load (std::memory_order_relaxed); }

    /** Audio thread: append a block. */
    void push (const AudioSampleBuffer& buffer, int numSamples);

    /** GUI thread: copy `numSamples` samples of channels firstCh.. into dest
        (dest is resized to numCh x numSamples). The last sample copied is
        `endBack` samples older than the newest one in the ring. Anything older
        than the ring's history reads as silence.

        Returns how many samples had been written in total at the moment of the
        copy, so the caller can pin what it read to an absolute sample index —
        which is what keeps a paused view still even if one last block slipped
        in between the read and the freeze. */
    int64 read (AudioSampleBuffer& dest, int firstCh, int numCh, int numSamples, int endBack);

    /** Same, but the last sample copied is the one with absolute index
        `lastAbs` (0 = the first sample written since prepare()), resolved
        under the ring lock. Use this when the position was worked out from an
        earlier getNumWritten(): blocks pushed since then don't shift what is
        read. Samples not written yet read as silence. */
    int64 readEndingAt (AudioSampleBuffer& dest, int firstCh, int numCh, int numSamples, int64 lastAbs);

    /** Total samples written since prepare(). */
    int64 getNumWritten() const { return written_.load (std::memory_order_relaxed); }

    /** Samples per channel the ring holds. */
    int    getCapacity() const   { return capacity_; }
    /** How many of those have actually been written since prepare(). */
    int    getNumValid() const;
    double getSampleRate() const { return sampleRate_; }
    int    getNumChannels() const { return numChannels_; }
    bool   isReady() const       { return ready_.load (std::memory_order_acquire); }

private:
    int64 readImpl (AudioSampleBuffer& dest, int firstCh, int numCh, int numSamples,
                    int64 end, bool endIsAbsolute);

    double sampleRate_  = 0.0;
    int    numChannels_ = 0;
    int    capacity_    = 0;

    std::atomic<bool> ready_  { false };
    std::atomic<bool> active_ { false };
    std::atomic<bool> frozen_ { false };

    AudioSampleBuffer     ring_;
    std::atomic<int>      writePos_ { 0 };
    std::atomic<int64>    written_  { 0 };
    juce::SpinLock        ringLock_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopeCapture)
};

#endif // SCOPECAPTURE_H_INCLUDED
