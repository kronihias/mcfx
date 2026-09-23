/*
  ==============================================================================

   FeedbackSendNode / FeedbackReturnNode implementation.

  ==============================================================================
*/

#include "FeedbackNodes.h"

//==============================================================================
juce::AudioProcessor::BusesProperties FeedbackSendNode::makeBuses (int numChannels)
{
    return BusesProperties().withInput ("In",
                                        juce::AudioChannelSet::discreteChannels (numChannels),
                                        true);
}

FeedbackSendNode::FeedbackSendNode (int numChannels)
    : juce::AudioProcessor (makeBuses (juce::jmax (1, numChannels))),
      numChannels_ (juce::jmax (1, numChannels))
{
}

bool FeedbackSendNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannels() == numChannels_
        && layouts.getMainOutputChannels() == 0;
}

void FeedbackSendNode::prepareToPlay (double, int samplesPerBlock)
{
    // The bus is sized by the GraphController when the link is made or the
    // graph is prepared; nothing to do per-node beyond refusing to assume a
    // block size we were not given.
    juce::ignoreUnused (samplesPerBlock);
}

void FeedbackSendNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (bus_ != nullptr)
        bus_->write (buffer, buffer.getNumSamples());

    // A send has no outputs, but the graph hands us a buffer sized to
    // max(in, out) and a later node may be handed the same one. Leave it as
    // found — the graph clears what it needs to.
}

//==============================================================================
juce::AudioProcessor::BusesProperties FeedbackReturnNode::makeBuses (int numChannels)
{
    return BusesProperties().withOutput ("Out",
                                         juce::AudioChannelSet::discreteChannels (numChannels),
                                         true);
}

FeedbackReturnNode::FeedbackReturnNode (int numChannels)
    : juce::AudioProcessor (makeBuses (juce::jmax (1, numChannels))),
      numChannels_ (juce::jmax (1, numChannels))
{
}

bool FeedbackReturnNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannels() == numChannels_
        && layouts.getMainInputChannels() == 0;
}

void FeedbackReturnNode::prepareToPlay (double, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
}

void FeedbackReturnNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (bus_ == nullptr)
    {
        // Unlinked: emit silence rather than whatever the shared graph buffer
        // last held, which would otherwise leak an unrelated node's output.
        buffer.clear();
        return;
    }

    bus_->read (buffer, buffer.getNumSamples());
}
