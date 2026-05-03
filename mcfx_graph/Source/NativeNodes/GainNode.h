/*
  ==============================================================================

   GainNode — per-channel dB gain. linked=true keeps every channel at the
   master value; linked=false lets each channel have its own gain.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

class GainNode : public juce::AudioProcessor
{
public:
    explicit GainNode (int numChannels);
    ~GainNode() override = default;

    static constexpr const char* kTypeId = "gain";

    int getNumChannels() const noexcept { return numChannels_; }

    void  setLinked (bool linked) noexcept;
    bool  isLinked()  const noexcept       { return linked_.load (std::memory_order_relaxed); }

    // Per-channel dB. When linked, channel 0 is the master; setters write to
    // every channel so reads stay consistent regardless of which channel
    // the caller asked for.
    void  setGainDb (int channel, float db) noexcept;
    float getGainDb (int channel) const noexcept;

    juce::var toVar() const;
    void      fromVar (const juce::var& v);

    //==============================================================================
    // AudioProcessor
    const juce::String getName() const override        { return "Gain"; }
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
    void releaseResources() override                   {}
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

private:
    static juce::AudioProcessor::BusesProperties makeBuses (int numChannels);

    const int numChannels_;
    std::atomic<bool> linked_ { true };
    std::vector<std::atomic<float>> gainsDb_;          // per-channel target gains in dB
    std::vector<juce::SmoothedValue<float>> smoothed_; // linear gain, smoothed (audio-thread only)
};
