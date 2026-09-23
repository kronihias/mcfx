/*
  ==============================================================================

   FeedbackBus — the one block of shared state between a FeedbackSendNode and
   the FeedbackReturnNode it is linked to.

   Why a shared buffer rather than a wire: a juce::AudioProcessorGraph
   connection from the send back to the return would close a cycle, and
   AudioProcessorGraph does not render cycles. RenderSequenceBuilder cannot
   find a buffer for the back edge and silently substitutes the read-only
   empty buffer (see the "if not found, this is probably a feedback loop"
   branch in juce_AudioProcessorGraph.cpp), so the loop carries nothing at
   all. Leaving the pair unconnected keeps the graph a DAG and closes the
   loop out-of-band, here.

   Double-buffered on purpose. With no edge between them, nothing constrains
   whether the graph schedules the send before the return within a block, and
   relying on that order would be a latent bug. Instead the send writes the
   front buffer, the return reads the back one, and the owning GraphController
   flips them once per block after the graph has rendered. The return then
   emits exactly what the send saw one block earlier, whatever order JUCE
   picked.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>

class FeedbackBus
{
public:
    using Ptr = std::shared_ptr<FeedbackBus>;

    void prepare (int numChannels, int blockSize)
    {
        numChannels_ = juce::jmax (0, numChannels);
        blockSize_   = juce::jmax (0, blockSize);
        for (auto& b : buf_)
        {
            b.setSize (numChannels_, blockSize_, false, true, true);
            b.clear();
        }
        written_[0] = written_[1] = false;
        w_ = 0;
    }

    void release()
    {
        for (auto& b : buf_)
            b.setSize (0, 0);
        written_[0] = written_[1] = false;
        numChannels_ = blockSize_ = 0;
    }

    int getNumChannels() const noexcept { return numChannels_; }

    /** Audio thread, from FeedbackSendNode::processBlock. */
    void write (const juce::AudioBuffer<float>& src, int numSamples) noexcept
    {
        auto& dst = buf_[w_];
        if (dst.getNumChannels() == 0 || dst.getNumSamples() == 0)
            return;

        const int ch = juce::jmin (dst.getNumChannels(), src.getNumChannels());
        const int n  = juce::jmin (numSamples, dst.getNumSamples());

        for (int c = 0; c < ch; ++c)
            dst.copyFrom (c, 0, src, c, 0, n);

        // Anything the send didn't cover — a narrower send, or a short final
        // block — would otherwise still hold the previous pass and loop
        // forever as a stale fragment. Zero it.
        for (int c = 0; c < ch; ++c)
            if (n < dst.getNumSamples())
                dst.clear (c, n, dst.getNumSamples() - n);
        for (int c = ch; c < dst.getNumChannels(); ++c)
            dst.clear (c, 0, dst.getNumSamples());

        written_[w_] = true;
    }

    /** Audio thread, from FeedbackReturnNode::processBlock. Emits the block the
        send wrote last time round, or silence when it never ran — not linked,
        muted, bypassed or removed. Silence rather than a repeat matters: a
        stale block handed back every period is an oscillator, and it would
        drone on after the user muted the very node feeding it. */
    void read (juce::AudioBuffer<float>& dst, int numSamples) const noexcept
    {
        const int r = w_ ^ 1;
        if (! written_[r])
        {
            dst.clear();
            return;
        }

        const auto& src = buf_[r];
        const int ch = juce::jmin (dst.getNumChannels(), src.getNumChannels());
        const int n  = juce::jmin (numSamples, src.getNumSamples());

        for (int c = 0; c < ch; ++c)
            dst.copyFrom (c, 0, src, c, 0, n);
        for (int c = 0; c < ch; ++c)
            if (n < dst.getNumSamples())
                dst.clear (c, n, dst.getNumSamples() - n);
        for (int c = ch; c < dst.getNumChannels(); ++c)
            dst.clear (c, 0, dst.getNumSamples());
    }

    /** Audio thread, once per block, AFTER the graph has rendered. */
    void flip() noexcept
    {
        w_ ^= 1;
        // The half we just turned to is the one about to be written. Mark it
        // unwritten so that if the send doesn't run this block, the return
        // reads silence next time instead of replaying two blocks ago.
        written_[w_] = false;
    }

private:
    juce::AudioBuffer<float> buf_[2];
    bool written_[2] { false, false };
    int  w_ = 0;
    int  numChannels_ = 0;
    int  blockSize_   = 0;

    JUCE_LEAK_DETECTOR (FeedbackBus)
};
