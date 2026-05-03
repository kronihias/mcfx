#include "DelayNode.h"

juce::AudioProcessor::BusesProperties DelayNode::makeBuses (int numChannels)
{
    return BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::discreteChannels (numChannels), true)
        .withOutput ("Output", juce::AudioChannelSet::discreteChannels (numChannels), true);
}

DelayNode::DelayNode (int numChannels, float maxDelaySamples)
    : juce::AudioProcessor (makeBuses (juce::jmax (1, numChannels))),
      numChannels_ (juce::jmax (1, numChannels)),
      maxDelaySamples_ (juce::jmax (1.0f, maxDelaySamples)),
      targetSamples_ (numChannels_)
{
    for (auto& t : targetSamples_)
        t.store (0.0f, std::memory_order_relaxed);

    lines_.reserve (numChannels_);
    for (int c = 0; c < numChannels_; ++c)
    {
        auto line = std::make_unique<Line>();
        line->setMaximumDelayInSamples ((int) std::ceil (maxDelaySamples_) + 4);
        lines_.push_back (std::move (line));
    }
}

bool DelayNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().size()  == numChannels_
        && layouts.getMainOutputChannelSet().size() == numChannels_;
}

void DelayNode::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRate_ = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    for (auto& line : lines_)
    {
        line->prepare (spec);
        line->reset();
    }
}

void DelayNode::releaseResources()
{
    for (auto& line : lines_)
        line->reset();
}

double DelayNode::getTailLengthSeconds() const
{
    if (sampleRate_ <= 0.0) return 0.0;
    float maxTarget = 0.0f;
    for (auto& t : targetSamples_)
        maxTarget = juce::jmax (maxTarget, t.load (std::memory_order_relaxed));
    return (double) maxTarget / sampleRate_;
}

void DelayNode::setLinked (bool linked) noexcept
{
    linked_.store (linked, std::memory_order_relaxed);
    if (linked && numChannels_ > 0)
    {
        const float v = targetSamples_[0].load (std::memory_order_relaxed);
        for (int c = 1; c < numChannels_; ++c)
            targetSamples_[c].store (v, std::memory_order_relaxed);
    }
}

void DelayNode::setDelaySamples (int channel, float samples) noexcept
{
    if (channel < 0 || channel >= numChannels_) return;
    const float clamped = juce::jlimit (0.0f, maxDelaySamples_, samples);

    if (linked_.load (std::memory_order_relaxed))
        for (auto& t : targetSamples_)
            t.store (clamped, std::memory_order_relaxed);
    else
        targetSamples_[channel].store (clamped, std::memory_order_relaxed);
}

float DelayNode::getDelaySamples (int channel) const noexcept
{
    if (channel < 0 || channel >= numChannels_) return 0.0f;
    return targetSamples_[channel].load (std::memory_order_relaxed);
}

void DelayNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), numChannels_);

    for (int c = 0; c < numCh; ++c)
    {
        auto& line = *lines_[c];
        line.setDelay (targetSamples_[c].load (std::memory_order_relaxed));

        auto* data = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            line.pushSample (0, data[i]);
            data[i] = line.popSample (0);
        }
    }
}

juce::var DelayNode::toVar() const
{
    auto* obj = new juce::DynamicObject();

    obj->setProperty ("linked", linked_.load (std::memory_order_relaxed));

    juce::Array<juce::var> arr;
    arr.ensureStorageAllocated (numChannels_);
    for (int c = 0; c < numChannels_; ++c)
        arr.add (targetSamples_[c].load (std::memory_order_relaxed));
    obj->setProperty ("delaySamples", arr);
    obj->setProperty ("maxDelaySamples", maxDelaySamples_);

    return juce::var (obj);
}

void DelayNode::fromVar (const juce::var& v)
{
    if (auto* obj = v.getDynamicObject())
    {
        const bool linked = (bool) obj->getProperty ("linked");

        // Write per-channel values directly so the linked broadcast in
        // setDelaySamples doesn't overwrite per-channel data we just loaded.
        if (auto* arr = obj->getProperty ("delaySamples").getArray())
            for (int c = 0; c < numChannels_ && c < arr->size(); ++c)
                targetSamples_[c].store (
                    juce::jlimit (0.0f, maxDelaySamples_, (float) (double) (*arr)[c]),
                    std::memory_order_relaxed);

        linked_.store (linked, std::memory_order_relaxed);
    }
}
