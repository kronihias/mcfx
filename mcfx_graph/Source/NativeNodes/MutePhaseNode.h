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
    ~MutePhaseNode() override;

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

    // Two host-visible switches per channel, so mcfx_graph's forwarding pool
    // can expose them. The atomics below stay the authority for the audio
    // thread; these mirror them both ways. Mute and invert alternate
    // (mute 1, invert 1, mute 2, ...) so a channel's pair sits together.
    // valueChanged() is what juce calls from AudioParameterBool::setValue(),
    // which is how a host write actually arrives — Listener only fires for
    // setValueNotifyingHost.
    struct SwitchParam : juce::AudioParameterBool
    {
        SwitchParam (MutePhaseNode& o, int ch, bool inv,
                     const juce::ParameterID& id, const juce::String& nm)
            : juce::AudioParameterBool (id, nm, false), owner (o), channel (ch), isInvert (inv) {}
        void valueChanged (bool newValue) override { owner.applyParamChange (channel, isInvert, newValue); }
        MutePhaseNode& owner;
        const int  channel;
        const bool isInvert;
    };
    void applyParamChange (int channel, bool isInvert, bool on) noexcept;

    std::vector<SwitchParam*> muteParams_, invertParams_;
    std::atomic<bool> applyingParam_ { false };

    const int numChannels_;
    std::atomic<bool> linked_ { false };
    // Use uint8_t because std::vector<std::atomic<bool>> can't be resized
    // (atomic isn't movable). uint8_t with explicit atomic ops works fine.
    std::vector<std::atomic<uint8_t>> mute_;
    std::vector<std::atomic<uint8_t>> invert_;
    std::vector<juce::SmoothedValue<float>> muteRamp_; // 0..1 multiplier, smoothed for click-free mute
};
