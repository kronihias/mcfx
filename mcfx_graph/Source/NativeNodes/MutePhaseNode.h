/*
  ==============================================================================

   MutePhaseNode — per-channel mute and polarity (phase) invert.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

class MutePhaseNode : public juce::AudioProcessor
{
public:
    explicit MutePhaseNode (int numChannels);
    ~MutePhaseNode() override = default;

    static constexpr const char* kTypeId = "mute_phase";

    int  getNumChannels() const noexcept { return numChannels_; }

    void setLinked (bool linked) noexcept;
    bool isLinked() const noexcept       { return linked_.load (std::memory_order_relaxed); }

    // When linked, setMute / setInvert write the same value to every channel.
    void setMute    (int channel, bool m)  noexcept;
    bool getMute    (int channel) const noexcept;
    void setInvert  (int channel, bool i)  noexcept;
    bool getInvert  (int channel) const noexcept;

    juce::var toVar() const;
    void      fromVar (const juce::var& v);

    //==============================================================================
    const juce::String getName() const override        { return "Mute / Invert"; }
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
    std::atomic<bool> linked_ { false };
    // Use uint8_t because std::vector<std::atomic<bool>> can't be resized
    // (atomic isn't movable). uint8_t with explicit atomic ops works fine.
    std::vector<std::atomic<uint8_t>> mute_;
    std::vector<std::atomic<uint8_t>> invert_;
    std::vector<juce::SmoothedValue<float>> muteRamp_; // 0..1 multiplier, smoothed for click-free mute
};
