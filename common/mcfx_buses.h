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

/** Rewrite any named multichannel layout to discreteChannels(N) so JUCE's
    VST3 wrapper skips its channel-reorder table.

    JUCE's VST3 wrapper maps VST3 SpeakerArrangement bitmasks to named JUCE
    AudioChannelSets (e.g. k222 <-> 22.2, k714 <-> 7.1.4). For named sets,
    the channel order inside AudioBuffer is determined by bit positions of
    the ChannelType enum, which does NOT match VST3's speaker order — so
    the wrapper applies a reorder table. On a 24-channel track in Reaper
    that reorder produces a swap (e.g. channel 20 <-> 24).

    For mcfx plugins every channel is treated generically, so we want a
    1:1 VST3-index <-> buffer-channel mapping. Discrete channel sets have
    no corresponding VST3 named arrangement, so `makeChannelIndices` falls
    back to bit-position order — which for discreteChannels(N) is just
    0,1,2,...,N-1. No reorder.

    Returns a layout identical to `in` but with every bus >= `minChannelsToRewrite`
    channels rewritten to discreteChannels. Small named layouts (mono,
    stereo, 5.1, 7.1) are left alone — they have identical VST3/JUCE orders
    so the reorder is a no-op, and keeping the name helps hosts that pick
    layouts by semantics. */
inline juce::AudioProcessor::BusesLayout
toDiscreteLayout (const juce::AudioProcessor::BusesLayout& in,
                  int minChannelsToRewrite = 3)
{
    auto rewrite = [minChannelsToRewrite] (juce::AudioChannelSet& cs)
    {
        const int n = cs.size();
        if (n >= minChannelsToRewrite && ! cs.isDiscreteLayout())
            cs = juce::AudioChannelSet::discreteChannels (n);
    };

    auto out = in;
    for (auto& cs : out.inputBuses)  rewrite (cs);
    for (auto& cs : out.outputBuses) rewrite (cs);
    return out;
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

/** applyBusLayouts override that rewrites any named multichannel layout
    (22.2, 7.1.4, etc.) to discreteChannels(N) before handing it to the
    base class — prevents JUCE's VST3 wrapper from reordering channels
    per the named layout's speaker positions.

    Declare this inside each plugin's AudioProcessor subclass (public). */
#define MCFX_MULTICHANEL_APPLY_BUS_LAYOUTS_OVERRIDE                            \
    bool applyBusLayouts (const BusesLayout& layouts) override                 \
    {                                                                          \
        return juce::AudioProcessor::applyBusLayouts (mcfx::toDiscreteLayout (layouts)); \
    }
