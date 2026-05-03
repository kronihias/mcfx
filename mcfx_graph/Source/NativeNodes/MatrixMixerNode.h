/*
  ==============================================================================

   MatrixMixerNode — NxM matrix mixer with independent input and output
   channel counts. Output channel j sums all input channels weighted by
   matrix[j,i].

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

class MatrixMixerNode : public juce::AudioProcessor
{
public:
    MatrixMixerNode (int numIn, int numOut);
    ~MatrixMixerNode() override = default;

    static constexpr const char* kTypeId = "matrix_mixer";

    int getNumIn()  const noexcept { return numIn_; }
    int getNumOut() const noexcept { return numOut_; }

    // Linear gain (not dB) at row=output, col=input.
    void  setMatrix (int outChannel, int inChannel, float gainLinear) noexcept;
    float getMatrix (int outChannel, int inChannel) const noexcept;

    /** Reset the matrix to a unity diagonal (passthrough as far as channels overlap). */
    void resetToIdentity();

    juce::var toVar() const;
    void      fromVar (const juce::var& v);

    //==============================================================================
    const juce::String getName() const override        { return "Matrix Mixer"; }
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
    static juce::AudioProcessor::BusesProperties makeBuses (int numIn, int numOut);

    inline int idx (int outCh, int inCh) const noexcept { return outCh * numIn_ + inCh; }

    const int numIn_, numOut_;
    std::vector<std::atomic<float>> matrix_; // size numOut_ * numIn_, row-major (out, in)
    juce::AudioBuffer<float> scratchIn_;     // input copy so we can write outputs in place
};
