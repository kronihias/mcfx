#include "MutePhaseNode.h"

juce::AudioProcessor::BusesProperties MutePhaseNode::makeBuses (int numChannels)
{
    return BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::discreteChannels (numChannels), true)
        .withOutput ("Output", juce::AudioChannelSet::discreteChannels (numChannels), true);
}

MutePhaseNode::MutePhaseNode (int numChannels)
    : juce::AudioProcessor (makeBuses (juce::jmax (1, numChannels))),
      numChannels_ (juce::jmax (1, numChannels)),
      mute_ (numChannels_),
      invert_ (numChannels_),
      muteRamp_ (numChannels_)
{
    for (auto& m : mute_)   m.store (0, std::memory_order_relaxed);
    for (auto& i : invert_) i.store (0, std::memory_order_relaxed);

    muteParams_.reserve ((size_t) numChannels_);
    invertParams_.reserve ((size_t) numChannels_);
    for (int c = 0; c < numChannels_; ++c)
    {
        auto* m = new juce::AudioParameterBool (
            juce::ParameterID { "mute_" + juce::String (c), 1 },
            "Mute " + juce::String (c + 1), false);
        auto* i = new juce::AudioParameterBool (
            juce::ParameterID { "invert_" + juce::String (c), 1 },
            "Invert " + juce::String (c + 1), false);
        addParameter (m);
        addParameter (i);
        m->addListener (this);
        i->addListener (this);
        muteParams_.push_back (m);
        invertParams_.push_back (i);
    }
}

MutePhaseNode::~MutePhaseNode()
{
    for (auto* p : muteParams_)   p->removeListener (this);
    for (auto* p : invertParams_) p->removeListener (this);
}

void MutePhaseNode::parameterValueChanged (int parameterIndex, float)
{
    // Parameters were added in mute/invert pairs, so the index maps straight
    // back to a channel and which of the two switches it is.
    const int channel = parameterIndex / 2;
    if (! juce::isPositiveAndBelow (channel, numChannels_))
        return;

    if (applyingParam_.exchange (true))
        return;
    if (parameterIndex % 2 == 0)
        setMute   (channel, muteParams_[(size_t) channel]->get());
    else
        setInvert (channel, invertParams_[(size_t) channel]->get());
    applyingParam_.store (false);
}

bool MutePhaseNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().size()  == numChannels_
        && layouts.getMainOutputChannelSet().size() == numChannels_;
}

void MutePhaseNode::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    for (auto& s : muteRamp_)
    {
        s.reset (sampleRate, 0.005); // 5 ms ramp
        s.setCurrentAndTargetValue (1.0f);
    }
}

void MutePhaseNode::setLinked (bool linked) noexcept
{
    linked_.store (linked, std::memory_order_relaxed);
    if (linked && numChannels_ > 0)
    {
        const auto m = mute_  [0].load (std::memory_order_relaxed);
        const auto i = invert_[0].load (std::memory_order_relaxed);
        for (int c = 1; c < numChannels_; ++c)
        {
            mute_  [c].store (m, std::memory_order_relaxed);
            invert_[c].store (i, std::memory_order_relaxed);
        }
    }
}

void MutePhaseNode::setMute (int channel, bool m) noexcept
{
    if (channel < 0 || channel >= numChannels_) return;
    const uint8_t v = m ? 1 : 0;

    const bool linked = linked_.load (std::memory_order_relaxed);

    if (linked)
        for (auto& a : mute_)
            a.store (v, std::memory_order_relaxed);
    else
        mute_[channel].store (v, std::memory_order_relaxed);

    if (! applyingParam_.load() && ! muteParams_.empty())
    {
        applyingParam_.store (true);
        if (linked)
            for (auto* p : muteParams_) *p = m;
        else
            *muteParams_[(size_t) channel] = m;
        applyingParam_.store (false);
    }
}

bool MutePhaseNode::getMute (int channel) const noexcept
{
    return channel >= 0 && channel < numChannels_
            && mute_[channel].load (std::memory_order_relaxed) != 0;
}

void MutePhaseNode::setInvert (int channel, bool i) noexcept
{
    if (channel < 0 || channel >= numChannels_) return;
    const uint8_t v = i ? 1 : 0;

    const bool linked = linked_.load (std::memory_order_relaxed);

    if (linked)
        for (auto& a : invert_)
            a.store (v, std::memory_order_relaxed);
    else
        invert_[channel].store (v, std::memory_order_relaxed);

    if (! applyingParam_.load() && ! invertParams_.empty())
    {
        applyingParam_.store (true);
        if (linked)
            for (auto* p : invertParams_) *p = i;
        else
            *invertParams_[(size_t) channel] = i;
        applyingParam_.store (false);
    }
}

bool MutePhaseNode::getInvert (int channel) const noexcept
{
    return channel >= 0 && channel < numChannels_
            && invert_[channel].load (std::memory_order_relaxed) != 0;
}

void MutePhaseNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), numChannels_);

    for (int c = 0; c < numCh; ++c)
    {
        const bool mute   = mute_[c].load   (std::memory_order_relaxed) != 0;
        const bool invert = invert_[c].load (std::memory_order_relaxed) != 0;

        muteRamp_[c].setTargetValue (mute ? 0.0f : 1.0f);

        const float sign = invert ? -1.0f : 1.0f;
        auto* data = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i)
            data[i] *= sign * muteRamp_[c].getNextValue();
    }
}

juce::var MutePhaseNode::toVar() const
{
    auto* obj = new juce::DynamicObject();

    obj->setProperty ("linked", linked_.load (std::memory_order_relaxed));

    juce::Array<juce::var> mu, inv;
    mu.ensureStorageAllocated (numChannels_);
    inv.ensureStorageAllocated (numChannels_);
    for (int c = 0; c < numChannels_; ++c)
    {
        mu.add  (mute_[c].load   (std::memory_order_relaxed) != 0);
        inv.add (invert_[c].load (std::memory_order_relaxed) != 0);
    }
    obj->setProperty ("mute",   mu);
    obj->setProperty ("invert", inv);

    return juce::var (obj);
}

void MutePhaseNode::fromVar (const juce::var& v)
{
    if (auto* obj = v.getDynamicObject())
    {
        const bool linked = (bool) obj->getProperty ("linked");

        // Write per-channel values directly so the linked broadcast in
        // setMute/setInvert doesn't overwrite per-channel data we just loaded.
        if (auto* mu = obj->getProperty ("mute").getArray())
            for (int c = 0; c < numChannels_ && c < mu->size(); ++c)
                mute_[c].store ((bool) (*mu)[c] ? 1 : 0, std::memory_order_relaxed);

        if (auto* inv = obj->getProperty ("invert").getArray())
            for (int c = 0; c < numChannels_ && c < inv->size(); ++c)
                invert_[c].store ((bool) (*inv)[c] ? 1 : 0, std::memory_order_relaxed);

        linked_.store (linked, std::memory_order_relaxed);
    }
}
