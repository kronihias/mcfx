/*
  ==============================================================================

   PluginInstanceFactory — wraps juce::AudioPluginFormatManager::createPluginInstance
   with the same bus-disabling and main-bus channel probing tricks
   mcfx_anything uses, so hosted plugins behave consistently regardless of
   how many sidechain / aux buses they advertise.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>

namespace PluginInstanceFactory
{
    /** Disable every input/output bus except the main bus on `instance`.
        Reuses mcfx_anything's strategy (PluginProcessor.cpp:574+). */
    void disableNonMainBuses (juce::AudioPluginInstance& instance);

    /** Return the channel counts that the plugin's main input bus accepts
        from a candidate list, in declaration order. Used to populate a UX
        dropdown of supported layouts. */
    std::vector<int> probeAvailableMainBusSizes (juce::AudioPluginInstance& instance,
                                                 const std::vector<int>& candidates =
                                                     { 1, 2, 4, 6, 8, 10, 12, 16, 24, 32, 64 });

    /** Try to set the plugin's main bus channel count to N. Returns true on
        success. If N is unsupported, the plugin's previous layout is left
        untouched. */
    bool trySetMainBusChannels (juce::AudioPluginInstance& instance, int numChannels);

    /** Convenience: createPluginInstance + disableNonMainBuses + optional
        main-bus channel sizing. Returns nullptr on failure (errorOut populated). */
    std::unique_ptr<juce::AudioPluginInstance>
    create (juce::AudioPluginFormatManager& formatManager,
            const juce::PluginDescription& desc,
            double sampleRate,
            int blockSize,
            int requestedMainChannels,    // <= 0 means "leave plugin's natural channel count"
            juce::String& errorOut);
}
