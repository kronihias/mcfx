#include "BypassMuteWrapper.h"

juce::AudioProcessor::BusesProperties
BypassMuteWrapper::busesPropFromInner (juce::AudioProcessor& p)
{
    BusesProperties props;
    if (p.getBusCount (true) > 0)
        props = props.withInput  ("Input",
                                   p.getChannelLayoutOfBus (true, 0),
                                   p.getBus (true, 0)->isEnabled());
    if (p.getBusCount (false) > 0)
        props = props.withOutput ("Output",
                                   p.getChannelLayoutOfBus (false, 0),
                                   p.getBus (false, 0)->isEnabled());
    return props;
}

BypassMuteWrapper::BypassMuteWrapper (std::unique_ptr<juce::AudioProcessor> innerIn)
    : juce::AudioProcessor (busesPropFromInner (*innerIn)),
      inner_ (std::move (innerIn))
{
    // Subscribe to the inner so latency changes (e.g. an FFT lookahead
    // re-configured at runtime) propagate up to the graph.
    inner_->addListener (this);
    setLatencySamples (inner_->getLatencySamples());
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

    // Mirror the inner's reported latency so juce::AudioProcessorGraph (which
    // queries us, not the inner) can pad parallel paths to align them. Many
    // plug-ins only set their final latency during prepareToPlay.
    setLatencySamples (inner_->getLatencySamples());
}

void BypassMuteWrapper::audioProcessorChanged (juce::AudioProcessor*,
                                                const ChangeDetails& details)
{
    if (details.latencyChanged)
        setLatencySamples (inner_->getLatencySamples());
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
        const int nIn  = inner_->getMainBusNumInputChannels();
        const int nOut = inner_->getMainBusNumOutputChannels();
        for (int c = nIn; c < nOut; ++c)
            buffer.clear (c, 0, buffer.getNumSamples());
        return;
    }

    inner_->processBlock (buffer, midi);
}
