/*
  ==============================================================================

   SubgraphNode — an AudioProcessor wrapping an inner GraphController, so the
   user can nest entire graphs inside a single node. Channel counts are fixed
   at construction; resize means swap-and-replace.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GraphController.h"
#include <memory>

class SubgraphNode : public juce::AudioProcessor
{
public:
    SubgraphNode (int numIn, int numOut);
    ~SubgraphNode() override;

    static constexpr const char* kTypeId = "subgraph";

    int getNumIn()  const noexcept { return numIn_; }
    int getNumOut() const noexcept { return numOut_; }

    GraphController& getInner() noexcept { return *inner_; }
    const GraphController& getInner() const noexcept { return *inner_; }

    //==============================================================================
    const juce::String getName() const override        { return "Subgraph"; }
    bool acceptsMidi()  const override                 { return false; }
    bool producesMidi() const override                 { return false; }
    double getTailLengthSeconds() const override       { return 0.0; }
    bool hasEditor() const override                    { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    int  getNumPrograms() override                     { return 1; }
    int  getCurrentProgram() override                  { return 0; }
    void setCurrentProgram (int) override              {}
    const juce::String getProgramName (int) override   { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override   {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    int getLatencySamples() const noexcept;

private:
    static juce::AudioProcessor::BusesProperties makeBuses (int numIn, int numOut);

    const int numIn_, numOut_;
    std::unique_ptr<GraphController> inner_;
};
