#include "SubgraphNode.h"
#include "GraphSerializer.h"

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

std::unique_ptr<SubgraphNode> SubgraphNode::withChannelCounts (
    int numIn, int numOut,
    juce::AudioPluginFormatManager& formatManager,
    const juce::KnownPluginList* knownPluginList) const
{
    const auto saved = GraphSerializer::graphToVar (*inner_);

    auto resized = std::make_unique<SubgraphNode> (numIn, numOut);
    // Size the new inner terminals before restoring, as loading a saved
    // subgraph does; wires to channels past the new widths then fail
    // canConnect and are dropped, and the rest go back as they were.
    resized->inner_->prepareToPlay (48000.0, 512, resized->numIn_, resized->numOut_);
    GraphSerializer::graphFromVar (saved, *resized->inner_, formatManager, knownPluginList);
    return resized;
}

int SubgraphNode::getLatencySamples() const noexcept
{
    return inner_->getLatencySamples();
}
