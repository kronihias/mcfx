/*
  ==============================================================================

   DelayNode — per-channel sample-accurate delay (fractional via Lagrange3rd
   interpolation). Reports getLatencySamples() = 0 because delays here are
   intentional and should NOT be compensated by the host.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <vector>
#include <memory>

class DelayNode : public juce::AudioProcessor
{
public:
    DelayNode (int numChannels, float maxDelaySamples = 48000.0f);
    ~DelayNode() override = default;

    static constexpr const char* kTypeId = "delay";

    int   getNumChannels() const noexcept    { return numChannels_; }
    float getMaxDelaySamples() const noexcept { return maxDelaySamples_; }

    void  setLinked (bool linked) noexcept;
    bool  isLinked() const noexcept          { return linked_.load (std::memory_order_relaxed); }

    // When linked, setDelaySamples writes the same value to every channel.
    void  setDelaySamples (int channel, float samples) noexcept;
    float getDelaySamples (int channel) const noexcept;

    juce::var toVar() const;
    void      fromVar (const juce::var& v);

    //==============================================================================
    const juce::String getName() const override        { return "Delay"; }
    bool acceptsMidi()  const override                 { return false; }
    bool producesMidi() const override                 { return false; }
    double getTailLengthSeconds() const override;
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

    int getLatencySamples() const noexcept { return 0; } // delays are intentional

private:
    static juce::AudioProcessor::BusesProperties makeBuses (int numChannels);

    using Line = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>;

    const int   numChannels_;
    const float maxDelaySamples_;
    double      sampleRate_ = 48000.0;
    std::atomic<bool> linked_ { false };
    std::vector<std::atomic<float>> targetSamples_;
    std::vector<std::unique_ptr<Line>> lines_;
};
