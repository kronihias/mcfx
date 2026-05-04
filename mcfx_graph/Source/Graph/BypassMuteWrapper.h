/*
  ==============================================================================

   BypassMuteWrapper — thin AudioProcessor that wraps another AudioProcessor
   and adds atomic bypass / mute toggles handled in processBlock. Every node
   added to mcfx_graph's AudioProcessorGraph is wrapped in this so the
   per-node bypass / mute toggles are O(1) audio-thread flag flips that
   leave the graph topology (including all wires) untouched.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>

class BypassMuteWrapper : public juce::AudioProcessor,
                           private juce::AudioProcessorListener
{
public:
    explicit BypassMuteWrapper (std::unique_ptr<juce::AudioProcessor> innerIn);
    ~BypassMuteWrapper() override;

    juce::AudioProcessor* getInnerProcessor() const noexcept { return inner_.get(); }

    void setBypassedFlag (bool b) noexcept { bypassed_.store (b, std::memory_order_relaxed); }
    void setMutedFlag    (bool m) noexcept { muted_   .store (m, std::memory_order_relaxed); }
    bool isBypassedFlag() const noexcept   { return bypassed_.load (std::memory_order_relaxed); }
    bool isMutedFlag()    const noexcept   { return muted_   .load (std::memory_order_relaxed); }

    //==============================================================================
    // AudioProcessor overrides — almost everything just forwards to inner.

    const juce::String getName() const override                 { return inner_->getName(); }
    bool acceptsMidi()  const override                          { return inner_->acceptsMidi(); }
    bool producesMidi() const override                          { return inner_->producesMidi(); }
    bool isMidiEffect() const override                          { return inner_->isMidiEffect(); }
    double getTailLengthSeconds() const override                { return inner_->getTailLengthSeconds(); }

    bool hasEditor() const override                             { return inner_->hasEditor(); }
    juce::AudioProcessorEditor* createEditor() override         { return inner_->createEditor(); }

    int  getNumPrograms() override                              { return inner_->getNumPrograms(); }
    int  getCurrentProgram() override                           { return inner_->getCurrentProgram(); }
    void setCurrentProgram (int i) override                     { inner_->setCurrentProgram (i); }
    const juce::String getProgramName (int i) override          { return inner_->getProgramName (i); }
    void changeProgramName (int i, const juce::String& n) override { inner_->changeProgramName (i, n); }

    void getStateInformation (juce::MemoryBlock& m) override    { inner_->getStateInformation (m); }
    void setStateInformation (const void* d, int s) override    { inner_->setStateInformation (d, s); }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return inner_->checkBusesLayoutSupported (l);
    }

    void prepareToPlay (double sampleRate, int blockSize) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

private:
    static BusesProperties busesPropFromInner (juce::AudioProcessor& p);

    // AudioProcessorListener — only used to mirror the inner's reported
    // latency onto the wrapper, so juce::AudioProcessorGraph can pad
    // parallel paths to align them (the graph queries the wrapper, not
    // the inner). Parameter-change events are intentionally ignored here;
    // those are handled by Mcfx_graphAudioProcessor's listener attached to
    // the inner directly.
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged (juce::AudioProcessor*, const juce::AudioProcessorListener::ChangeDetails&) override;

    std::unique_ptr<juce::AudioProcessor> inner_;
    std::atomic<bool> bypassed_ { false };
    std::atomic<bool> muted_    { false };
};
