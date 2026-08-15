#include "GainNode.h"

juce::AudioProcessor::BusesProperties GainNode::makeBuses (int numChannels)
{
    return BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::discreteChannels (numChannels), true)
        .withOutput ("Output", juce::AudioChannelSet::discreteChannels (numChannels), true);
}

GainNode::GainNode (int numChannels)
    : juce::AudioProcessor (makeBuses (juce::jmax (1, numChannels))),
      numChannels_ (juce::jmax (1, numChannels)),
      gainsDb_ (numChannels_),
      smoothed_ (numChannels_)
{
    for (auto& g : gainsDb_)
        g.store (0.0f, std::memory_order_relaxed);

    // Range matches the panel's slider so a value set either way reads the
    // same. Parameter ids are per-channel and never renumbered, so a saved
    // exposure keeps pointing at the same channel.
    gainParams_.reserve ((size_t) numChannels_);
    for (int c = 0; c < numChannels_; ++c)
    {
        auto* p = new juce::AudioParameterFloat (
            juce::ParameterID { "gain_" + juce::String (c), 1 },
            "Gain " + juce::String (c + 1),
            juce::NormalisableRange<float> (-120.0f, 40.0f, 0.1f), 0.0f);
        addParameter (p);
        p->addListener (this);
        gainParams_.push_back (p);
    }
}

GainNode::~GainNode()
{
    for (auto* p : gainParams_)
        p->removeListener (this);
}

void GainNode::parameterValueChanged (int parameterIndex, float)
{
    if (! juce::isPositiveAndBelow (parameterIndex, (int) gainParams_.size()))
        return;

    // Re-entrancy guard: setGainDb writes the parameters back, and when
    // linked it writes every channel's.
    if (applyingParam_.exchange (true))
        return;
    setGainDb (parameterIndex, gainParams_[(size_t) parameterIndex]->get());
    applyingParam_.store (false);
}

bool GainNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().size()  == numChannels_
        && layouts.getMainOutputChannelSet().size() == numChannels_;
}

void GainNode::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    for (auto& s : smoothed_)
    {
        s.reset (sampleRate, 0.02);  // 20 ms ramp
        s.setCurrentAndTargetValue (1.0f);
    }
}

void GainNode::setLinked (bool linked) noexcept
{
    linked_.store (linked, std::memory_order_relaxed);
    if (linked && numChannels_ > 0)
    {
        const float db = gainsDb_[0].load (std::memory_order_relaxed);
        for (int c = 1; c < numChannels_; ++c)
            gainsDb_[c].store (db, std::memory_order_relaxed);
    }
}

void GainNode::setGainDb (int channel, float db) noexcept
{
    if (channel < 0 || channel >= numChannels_) return;

    const bool linked = linked_.load (std::memory_order_relaxed);

    if (linked)
        for (auto& g : gainsDb_)
            g.store (db, std::memory_order_relaxed);
    else
        gainsDb_[channel].store (db, std::memory_order_relaxed);

    // Mirror into the host-visible parameters, unless we are being called
    // from one of them.
    if (! applyingParam_.load() && ! gainParams_.empty())
    {
        applyingParam_.store (true);
        if (linked)
            for (auto* p : gainParams_)
                *p = db;
        else
            *gainParams_[(size_t) channel] = db;
        applyingParam_.store (false);
    }
}

float GainNode::getGainDb (int channel) const noexcept
{
    if (channel < 0 || channel >= numChannels_) return 0.0f;
    return gainsDb_[channel].load (std::memory_order_relaxed);
}

void GainNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), numChannels_);

    for (int c = 0; c < numCh; ++c)
    {
        const float targetDb = gainsDb_[c].load (std::memory_order_relaxed);
        smoothed_[c].setTargetValue (juce::Decibels::decibelsToGain (targetDb, -100.0f));

        auto* data = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i)
            data[i] *= smoothed_[c].getNextValue();
    }
}

juce::var GainNode::toVar() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("linked", linked_.load (std::memory_order_relaxed));

    juce::Array<juce::var> dbs;
    dbs.ensureStorageAllocated (numChannels_);
    for (int c = 0; c < numChannels_; ++c)
        dbs.add (gainsDb_[c].load (std::memory_order_relaxed));
    obj->setProperty ("gainsDb", dbs);

    return juce::var (obj);
}

void GainNode::fromVar (const juce::var& v)
{
    if (auto* obj = v.getDynamicObject())
    {
        const bool linked = obj->hasProperty ("linked")
                                ? (bool) obj->getProperty ("linked")
                                : true;

        const auto* arr = obj->getProperty ("gainsDb").getArray();
        if (arr != nullptr)
        {
            for (int c = 0; c < numChannels_ && c < arr->size(); ++c)
                gainsDb_[c].store ((float) (double) (*arr)[c], std::memory_order_relaxed);
        }
        // Apply linked AFTER setting gains so the rebroadcast in setLinked
        // doesn't overwrite per-channel data we just loaded.
        linked_.store (linked, std::memory_order_relaxed);
    }
}
