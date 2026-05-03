#include "SubgraphNode.h"

juce::AudioProcessor::BusesProperties SubgraphNode::makeBuses (int numIn, int numOut)
{
    return BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::discreteChannels (numIn),  true)
        .withOutput ("Output", juce::AudioChannelSet::discreteChannels (numOut), true);
}

SubgraphNode::SubgraphNode (int numIn, int numOut)
    : juce::AudioProcessor (makeBuses (juce::jmax (1, numIn), juce::jmax (1, numOut))),
      numIn_  (juce::jmax (1, numIn)),
      numOut_ (juce::jmax (1, numOut)),
      inner_ (std::make_unique<GraphController>())
{
}

SubgraphNode::~SubgraphNode() = default;

bool SubgraphNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().size()  == numIn_
        && layouts.getMainOutputChannelSet().size() == numOut_;
}

void SubgraphNode::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    inner_->prepareToPlay (sampleRate, samplesPerBlock, numIn_, numOut_);
}

void SubgraphNode::releaseResources()
{
    inner_->releaseResources();
}

void SubgraphNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    inner_->processBlock (buffer, midi);
}

int SubgraphNode::getLatencySamples() const noexcept
{
    return inner_->getLatencySamples();
}
