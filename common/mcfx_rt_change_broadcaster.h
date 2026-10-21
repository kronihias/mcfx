/*
 ==============================================================================

 ChangeBroadcaster that is safe to notify from the audio thread.

 The legacy setParameter() plug-ins call sendChangeMessage() so the editor
 refreshes. Host automation reaches setParameter() on the audio thread
 (the VST3 wrapper applies it inside process()), and sendChangeMessage()
 posts to the message queue: a lock and possibly an allocation. Off the
 message thread this only raises a flag; a message-thread timer turns it
 into a change message.

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
    /** Message thread, ~30 Hz: apply whatever the audio thread could only
        flag (start a Timer, ...). Runs before a deferred change message. */
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
