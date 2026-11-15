#include "BypassMuteWrapper.h"

juce::AudioProcessor::BusesProperties
BypassMuteWrapper::busesPropFromInner (juce::AudioProcessor& p)
{
    // One bus per direction, as wide as ALL of the inner's enabled buses
    // together. The graph sizes the buffer it hands us from this, and the
    // inner reads its buses back to back out of that one buffer — so a
    // plug-in that keeps aux buses on (or 32 stereo buses for 64 channels)
    // must be counted in full. Counting the main bus only gave such a
    // plug-in a buffer too small for it, and it ran off the end.
    BusesProperties props;

    for (const bool isInput : { true, false })
    {
        if (p.getBusCount (isInput) == 0) continue;

        const int total = isInput ? p.getTotalNumInputChannels()
                                  : p.getTotalNumOutputChannels();
        const auto mainLayout = p.getChannelLayoutOfBus (isInput, 0);

        // Keep the main bus' own layout where it is the whole story.
        const auto layout = (total == 0 || mainLayout.size() == total)
                                ? mainLayout
                                : juce::AudioChannelSet::discreteChannels (total);

        props.addBus (isInput, isInput ? "Input" : "Output", layout, total > 0);
    }

    return props;
}

BypassMuteWrapper::BypassMuteWrapper (std::unique_ptr<juce::AudioProcessor> innerIn)
    : juce::AudioProcessor (busesPropFromInner (*innerIn)),
      inner_ (std::move (innerIn))
{
    // Subscribe to the inner so latency changes (e.g. an FFT lookahead
    // re-configured at runtime) propagate up to the graph.
    inner_->addListener (this);
    reportLatency();
}

void BypassMuteWrapper::reportLatency()
{
    setLatencySamples (ignoreLatency_.load() ? 0 : inner_->getLatencySamples());
}

void BypassMuteWrapper::setIgnoreLatency (bool ignore)
{
    ignoreLatency_.store (ignore);
    reportLatency();   // a change reaches GraphController, which re-plans
}

BypassMuteWrapper::~BypassMuteWrapper()
{
    if (inner_ != nullptr)
        inner_->removeListener (this);
}

void BypassMuteWrapper::prepareToPlay (double sampleRate, int blockSize)
{
    // Mirror what JUCE hosts do before calling prepareToPlay: stash the
    // rate / buffer-size on the inner processor too, so its base-class
    // getSampleRate() / getBlockSize() return the real values. Without this,
    // editor code that introspects the inner (e.g. the Delay panel querying
    // d->getSampleRate() to convert samples ↔ ms) reads 0 and falls back to
    // a default.
    inner_->setRateAndBufferSizeDetails (sampleRate, blockSize);
    inner_->prepareToPlay (sampleRate, blockSize);

    // Our own width was fixed when the node was built. If the inner has
    // since grown (a state restore can switch buses back on), run it through
    // a buffer of its own size rather than let it overrun the graph's.
    const int innerChannels = juce::jmax (inner_->getTotalNumInputChannels(),
                                          inner_->getTotalNumOutputChannels());
    const int ownChannels   = juce::jmax (getTotalNumInputChannels(),
                                          getTotalNumOutputChannels());
    if (innerChannels > ownChannels)
        scratch_.setSize (innerChannels, blockSize, false, true, false);
    else
        scratch_.setSize (0, 0);

    // Mirror the inner's reported latency so juce::AudioProcessorGraph (which
    // queries us, not the inner) can pad parallel paths to align them. Many
    // plug-ins only set their final latency during prepareToPlay.
    reportLatency();
}

void BypassMuteWrapper::audioProcessorChanged (juce::AudioProcessor*,
                                                const juce::AudioProcessorListener::ChangeDetails& details)
{
    if (details.latencyChanged)
        reportLatency();
}

void BypassMuteWrapper::releaseResources()
{
    inner_->releaseResources();
}

void BypassMuteWrapper::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (muted_.load (std::memory_order_relaxed))
    {
        buffer.clear();
        return;
    }

    if (bypassed_.load (std::memory_order_relaxed))
    {
        // Pass input through to output. The graph allocates max(in, out) channels
        // for in-place processing; input data sits in [0, nIn). For wider outputs
        // we clear the extra channels so they don't carry stale scratch data.
        const int nIn  = getTotalNumInputChannels();
        const int nOut = juce::jmin (getTotalNumOutputChannels(), buffer.getNumChannels());
        for (int c = nIn; c < nOut; ++c)
            buffer.clear (c, 0, buffer.getNumSamples());
        return;
    }

    // Asked every block, not cached: this is the one thing standing between
    // a plug-in that changed its buses behind our back and an overrun.
    const int innerChannels = juce::jmax (inner_->getTotalNumInputChannels(),
                                          inner_->getTotalNumOutputChannels());

    if (buffer.getNumChannels() >= innerChannels)
    {
        inner_->processBlock (buffer, midi);
        return;
    }

    // The inner wants more channels than the graph gave us. Feed it what we
    // have, zero the rest, hand back what fits. If it grew since the last
    // prepareToPlay there is no buffer for that yet: stay silent until the
    // next one rather than hand it too little.
    const int numSamples = buffer.getNumSamples();
    if (scratch_.getNumChannels() < innerChannels || numSamples > scratch_.getNumSamples())
    {
        buffer.clear();
        return;
    }

    const int shared = buffer.getNumChannels();
    for (int c = 0; c < shared; ++c)
        scratch_.copyFrom (c, 0, buffer, c, 0, numSamples);
    for (int c = shared; c < innerChannels; ++c)
        scratch_.clear (c, 0, numSamples);

    juce::AudioBuffer<float> view (scratch_.getArrayOfWritePointers(),
                                   innerChannels, numSamples);
    inner_->processBlock (view, midi);

    for (int c = 0; c < shared; ++c)
        buffer.copyFrom (c, 0, scratch_, c, 0, numSamples);
}
