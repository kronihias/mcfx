#include "PluginInstanceFactory.h"

namespace PluginInstanceFactory
{

void disableNonMainBuses (juce::AudioPluginInstance& instance)
{
    auto layout = instance.getBusesLayout();
    bool changed = false;

    for (int b = 1; b < layout.inputBuses.size(); ++b)
    {
        if (! layout.inputBuses.getReference (b).isDisabled())
        {
            layout.inputBuses.getReference (b) = juce::AudioChannelSet::disabled();
            changed = true;
        }
    }
    for (int b = 1; b < layout.outputBuses.size(); ++b)
    {
        if (! layout.outputBuses.getReference (b).isDisabled())
        {
            layout.outputBuses.getReference (b) = juce::AudioChannelSet::disabled();
            changed = true;
        }
    }

    if (changed)
        instance.setBusesLayout (layout);
}

namespace
{
    /** Build a candidate BusesLayout for `instance` whose main in/out are set
        to `channelSet`. Returns std::nullopt if the instance has no buses. */
    std::optional<juce::AudioProcessor::BusesLayout>
    buildMainLayout (const juce::AudioPluginInstance& instance,
                     juce::AudioChannelSet channelSet)
    {
        auto layout = instance.getBusesLayout();
        if (layout.inputBuses.size() == 0 && layout.outputBuses.size() == 0)
            return std::nullopt;
        if (layout.inputBuses.size()  > 0) layout.inputBuses.getReference  (0) = channelSet;
        if (layout.outputBuses.size() > 0) layout.outputBuses.getReference (0) = channelSet;
        return layout;
    }

    /** Returns ambisonic(k) when the channel count matches (k+1)^2, else null.
        JUCE's VST3 host can translate ambisonic layouts to kAmbi*OrderACN
        SpeakerArrangements — that's the only way to push 16 / 25 / 36 / 49 /
        64 channels to a VST3 plugin via the standard JUCE host API. */
    std::optional<juce::AudioChannelSet> ambisonicForCount (int n)
    {
        for (int k = 1; k <= 7; ++k)
            if ((k + 1) * (k + 1) == n)
                return juce::AudioChannelSet::ambisonic (k);
        return std::nullopt;
    }

    /** Try to apply `cs` and verify the plugin actually accepted it. JUCE's
        VST3 host wrapper already verifies setBusArrangements via a readback
        of getActualArrangements, but we additionally check the channel count
        directly because some plug-ins return success while keeping their
        previous layout. */
    bool applyAndVerify (juce::AudioPluginInstance& instance,
                         juce::AudioChannelSet cs,
                         int expectedChannels)
    {
        if (auto layout = buildMainLayout (instance, cs))
            if (instance.setBusesLayout (*layout))
                if (instance.getMainBusNumInputChannels()  == expectedChannels
                 && instance.getMainBusNumOutputChannels() == expectedChannels)
                    return true;
        return false;
    }
}

bool trySetMainBusChannels (juce::AudioPluginInstance& instance, int numChannels)
{
    if (numChannels <= 0) return false;

    // VST3 plug-ins must be INACTIVE when their bus arrangements change
    // (IComponent::setBusArrangements is only valid in the inactive state).
    // JUCE's VST3 host wrapper silently ignores layout changes on an active
    // plugin — that's why VST3 versions of mcfx_mimoeq couldn't be resized
    // even though the AU version worked. Release the plugin first; the
    // caller (or the surrounding graph) re-prepares it afterwards.
    instance.releaseResources();

    // The order matters because JUCE's VST3 host can only translate certain
    // AudioChannelSets into a VST3 SpeakerArrangement. For 1-8 channels the
    // canonical named layouts (mono / stereo / 5.0 / 7.1.4 / ...) cover it.
    // For 4 / 9 / 16 / 25 / 36 / 49 / 64 channels, JUCE maps to kAmbi*OrderACN
    // SpeakerArrangements via ambisonic(k). For everything else, only AU /
    // VST2 hosts accept discreteChannels — VST3 will reject the layout
    // because there is no VST3 SpeakerArrangement that fits.
    if (applyAndVerify (instance, juce::AudioChannelSet::canonicalChannelSet (numChannels),
                        numChannels))
        return true;

    if (auto a = ambisonicForCount (numChannels))
        if (applyAndVerify (instance, *a, numChannels))
            return true;

    if (applyAndVerify (instance, juce::AudioChannelSet::discreteChannels (numChannels),
                        numChannels))
        return true;

    return false;
}

std::vector<int> probeAvailableMainBusSizes (juce::AudioPluginInstance& instance,
                                             const std::vector<int>& candidates)
{
    // Real probe: actually try to apply each candidate and verify the plugin
    // accepted it. checkBusesLayoutSupported / isBusesLayoutSupported is
    // unreliable — Waves plugins (and others) advertise broad support but
    // refuse the change in setBusArrangements. Doing the real apply with a
    // round-trip read of getMainBusNumChannels gives us the truth.
    //
    // We save and restore the original layout so the live plugin's state
    // looks unchanged when this returns.
    auto savedLayout = instance.getBusesLayout();
    instance.releaseResources();

    std::vector<int> ok;

    for (int n : candidates)
    {
        if (applyAndVerify (instance, juce::AudioChannelSet::canonicalChannelSet (n), n))
        { ok.push_back (n); continue; }

        if (auto a = ambisonicForCount (n))
            if (applyAndVerify (instance, *a, n))
            { ok.push_back (n); continue; }

        if (applyAndVerify (instance, juce::AudioChannelSet::discreteChannels (n), n))
        { ok.push_back (n); continue; }
    }

    // Restore the layout we found at entry, regardless of which candidate
    // we left it on. If the plugin won't even accept its old layout back,
    // we leave it at whatever the last successful candidate was.
    instance.setBusesLayout (savedLayout);

    return ok;
}

std::unique_ptr<juce::AudioPluginInstance>
create (juce::AudioPluginFormatManager& formatManager,
        const juce::PluginDescription& desc,
        double sampleRate,
        int blockSize,
        int requestedMainChannels,
        juce::String& errorOut)
{
    auto inst = formatManager.createPluginInstance (desc, sampleRate, blockSize, errorOut);
    if (inst == nullptr) return nullptr;

    disableNonMainBuses (*inst);

    if (requestedMainChannels > 0)
        trySetMainBusChannels (*inst, requestedMainChannels);

    return inst;
}

} // namespace PluginInstanceFactory
