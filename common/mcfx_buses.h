/*
 ==============================================================================

 Shared multichannel bus helpers for the mcfx plug-in suite.

 VST3's SpeakerArrangement is a uint64 bitmask — max 64 speakers per bus.
 AU and Standalone use integer channel counts with no such limit.

 The multichannel build uses a single bus starting at stereo; the host
 negotiates the channel count via isBusesLayoutSupported:
   - VST3:  up to 64 channels
   - AU:    up to NUM_CHANNELS (128)
   - Other: up to NUM_CHANNELS (128)

 ==============================================================================
 */

#pragma once

#include <JuceHeader.h>

namespace mcfx
{

static constexpr int kMaxChPerBus = 64;

/** Validate a single-bus layout with format-aware channel limit.
    VST3 is capped at 64 (SpeakerArrangement bitmask); other formats
    allow up to maxChannels. */
inline bool isMultichannelLayoutSupported (const juce::AudioProcessor::BusesLayout& layouts,
                                           int maxChannels,
                                           juce::AudioProcessor::WrapperType wrapper)
{
    int totalIn  = 0;
    int totalOut = 0;

    for (auto& cs : layouts.inputBuses)
        totalIn += cs.size();

    for (auto& cs : layouts.outputBuses)
        totalOut += cs.size();

    int limit = (wrapper == juce::AudioProcessor::wrapperType_VST3)
                    ? juce::jmin (maxChannels, kMaxChPerBus)
                    : maxChannels;

    return totalIn == totalOut && totalIn >= 1 && totalIn <= limit;
}

} // namespace mcfx

/** Single-bus BusesProperties for the multichannel build.
    Starts at stereo; the host negotiates the channel count.
    BusesProperties is protected, so this macro must be expanded inside
    a subclass constructor initialiser list. */
#define MCFX_MULTICHANEL_BUSES \
    BusesProperties() \
        .withInput  ("Input",  juce::AudioChannelSet::canonicalChannelSet (2), true) \
        .withOutput ("Output", juce::AudioChannelSet::canonicalChannelSet (2), true)
