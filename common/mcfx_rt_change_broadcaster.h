/*
 ==============================================================================

 ChangeBroadcaster that is safe to notify from the audio thread.

 Host automation can reach setParameter() on the audio thread (VST3 applies
 it inside process()), where sendChangeMessage() would lock and allocate.
 Off the message thread this only raises a flag; a timer sends the message.

 ==============================================================================
 */

#pragma once

#include <JuceHeader.h>
#include <atomic>

namespace mcfx
{

class RtChangeBroadcaster : public juce::ChangeBroadcaster,
                            private juce::Timer
{
public:
    RtChangeBroadcaster()           { startTimerHz (30); }
    ~RtChangeBroadcaster() override { stopTimer(); }

    /** sendChangeMessage() on the message thread, a deferred one anywhere else. */
    void sendChangeMessageRtSafe() noexcept
    {
        if (juce::MessageManager::existsAndIsCurrentThread())
            sendChangeMessage();
        else
            pending_.store (true, std::memory_order_release);
    }

protected:
    /** Message thread, ~30 Hz: do work the audio thread could only flag. */
    virtual void handleDeferredMessageThreadWork() {}

private:
    void timerCallback() override
    {
        handleDeferredMessageThreadWork();
        if (pending_.exchange (false, std::memory_order_acquire))
            sendChangeMessage();
    }

    std::atomic<bool> pending_ { false };
};

} // namespace mcfx
