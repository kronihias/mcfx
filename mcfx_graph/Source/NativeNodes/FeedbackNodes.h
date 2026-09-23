/*
  ==============================================================================

   FeedbackSendNode / FeedbackReturnNode — an explicit, user-placed pair that
   closes a feedback loop with exactly one block of delay.

   The send takes N inputs and produces nothing; the return produces N outputs
   from nothing. They are paired by a link the user draws in the editor, which
   is deliberately NOT a juce::AudioProcessorGraph connection — see FeedbackBus
   for why a real edge cannot work. The graph therefore stays acyclic and the
   loop is closed through the shared bus.

   Both report zero latency: the block of delay is the point of the node, not
   something the host should compensate for. Same reasoning as DelayNode.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "FeedbackBus.h"

//==============================================================================
class FeedbackSendNode : public juce::AudioProcessor
{
public:
    explicit FeedbackSendNode (int numChannels);

    static constexpr const char* kTypeId = "feedback_send";

    int getNumChannels() const noexcept { return numChannels_; }

    /** Message thread, and only with the graph's processing suspended — the
        caller uses GraphController's scoped pause, so no atomics here. */
    void setBus (FeedbackBus::Ptr bus) { bus_ = std::move (bus); }
    const FeedbackBus::Ptr& getBus() const noexcept { return bus_; }

    //==============================================================================
    const juce::String getName() const override         { return "Fb Send"; }
    bool acceptsMidi()  const override                  { return false; }
    bool producesMidi() const override                  { return false; }
    double getTailLengthSeconds() const override        { return 0.0; }
    bool hasEditor() const override                     { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    int  getNumPrograms() override                      { return 1; }
    int  getCurrentProgram() override                   { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override   {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

private:
    static BusesProperties makeBuses (int numChannels);

    int numChannels_;
    FeedbackBus::Ptr bus_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FeedbackSendNode)
};

//==============================================================================
class FeedbackReturnNode : public juce::AudioProcessor
{
public:
    explicit FeedbackReturnNode (int numChannels);

    static constexpr const char* kTypeId = "feedback_return";

    int getNumChannels() const noexcept { return numChannels_; }

    void setBus (FeedbackBus::Ptr bus) { bus_ = std::move (bus); }
    const FeedbackBus::Ptr& getBus() const noexcept { return bus_; }

    //==============================================================================
    const juce::String getName() const override         { return "Fb Return"; }
    bool acceptsMidi()  const override                  { return false; }
    bool producesMidi() const override                  { return false; }
    double getTailLengthSeconds() const override        { return 0.0; }
    bool hasEditor() const override                     { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    int  getNumPrograms() override                      { return 1; }
    int  getCurrentProgram() override                   { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override   {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

private:
    static BusesProperties makeBuses (int numChannels);

    int numChannels_;
    FeedbackBus::Ptr bus_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FeedbackReturnNode)
};
