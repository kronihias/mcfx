/*
 ==============================================================================

 Shared multichannel bus helpers for the mcfx plug-in suite.

 VST3's SpeakerArrangement is a uint64 bitmask — max 64 speakers per bus.
 Bus 1 uses canonicalChannelSet so the host negotiates the track channel
 count correctly for 1–64 channels.  A second (disabled) bus is declared
 so that hosts which support it can activate 128-channel mode (2 × 64).
 Intermediate totals (65–127) are rejected because hosts cannot negotiate
 them reliably across two buses.

 ==============================================================================
 */

#pragma once

#include <JuceHeader.h>

namespace mcfx
{

static constexpr int kMaxChPerBus = 64;

/** Validate a (possibly two-bus) layout: each bus ≤ 64 ch,
    total in == total out, 1 ≤ total ≤ maxChannels.
    Accepts 1–64 (bus 1 only) or exactly maxChannels (both buses full). */
inline bool isMultichannelLayoutSupported (const juce::AudioProcessor::BusesLayout& layouts,
                                           int maxChannels)
{
    int totalIn  = 0;
    int totalOut = 0;

    for (auto& cs : layouts.inputBuses)
    {
        if (cs.size() > kMaxChPerBus)
            return false;
        totalIn += cs.size();
    }

    for (auto& cs : layouts.outputBuses)
    {
        if (cs.size() > kMaxChPerBus)
            return false;
        totalOut += cs.size();
    }

    if (totalIn != totalOut || totalIn < 1 || totalIn > maxChannels)
        return false;

    // Reject intermediate multi-bus totals (65–127): hosts expand both
    // buses to max rather than splitting, so only 1–64 or 128 work.
    if (totalIn > kMaxChPerBus && totalIn < maxChannels)
        return false;

    return true;
}

} // namespace mcfx

/** BusesProperties for the multichannel build.
    Bus 1: canonicalChannelSet(2) — host negotiates 1–64 channels.
    Bus 2: disabled — host may activate for 128-channel mode (2 × 64).
    BusesProperties is protected, so this macro must be expanded inside
    a subclass constructor initialiser list. */
#define MCFX_MULTICHANEL_BUSES \
    BusesProperties() \
        .withInput  ("Input",    juce::AudioChannelSet::canonicalChannelSet (2), true) \
        .withInput  ("Input 2",  juce::AudioChannelSet::discreteChannels (0), false) \
        .withOutput ("Output",   juce::AudioChannelSet::canonicalChannelSet (2), true) \
        .withOutput ("Output 2", juce::AudioChannelSet::discreteChannels (0), false)

/** No-op — reserved for future per-plugin bus overrides. */
#define MCFX_MULTICHANEL_BUS_OVERRIDES
