/*
  ==============================================================================

   PluginInstanceFactory — wraps juce::AudioPluginFormatManager::createPluginInstance
   with bus set-up and main-bus channel probing, so hosted plugins behave
   consistently regardless of how many sidechain / aux buses they advertise.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>

namespace PluginInstanceFactory
{
    /** Decide which of the plug-in's buses stay enabled. Per direction:

        - one or two buses: the second is taken for a sidechain / aux and
          switched off, so the node carries the main bus only;
        - three or more: the plug-in spreads its channels over several buses
          (e.g. 32 stereo buses for 64 channels), so all are switched on.

        The plug-in may refuse either change. Whatever ends up enabled is
        what the node exposes: size it with getTotalNumInputChannels() /
        getTotalNumOutputChannels(), never with the main bus alone. */
    void configureBuses (juce::AudioPluginInstance& instance);

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

    /** Convenience: createPluginInstance + configureBuses + optional
        main-bus channel sizing. Returns nullptr on failure (errorOut populated). */
    std::unique_ptr<juce::AudioPluginInstance>
    create (juce::AudioPluginFormatManager& formatManager,
            const juce::PluginDescription& desc,
            double sampleRate,
            int blockSize,
            int requestedMainChannels,    // <= 0 means "leave plugin's natural channel count"
            juce::String& errorOut);
}
