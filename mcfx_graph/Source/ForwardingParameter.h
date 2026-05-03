/*
  ==============================================================================

   ForwardingParameter — one slot in a fixed-size pool of proxy parameters
   exposed to the DAW. Each slot may be dynamically bound to one inner-plugin
   AudioProcessorParameter. Host-driven writes are deposited lock-free and
   forwarded to the bound inner parameter at the next processBlock.

   The slot count must NEVER change after construction (DAWs cache parameter
   layout at first scan and bad things happen if it shifts).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>

class ForwardingParameter : public juce::HostedAudioProcessorParameter
{
public:
    explicit ForwardingParameter (int slotIndex);

    //==============================================================================
    // Message-thread API.

    /** Bind this slot to an inner parameter. The pointer must remain valid
        until either unbind() is called or this slot's owning processor is
        destroyed. nodeUuid + paramIndex are kept for serialization so the
        binding can be re-established after a project reload. */
    void bind (juce::AudioProcessorParameter* innerParam,
               juce::Uuid nodeUuid,
               int paramIndex,
               const juce::String& displayName);

    void unbind();

    /** Audio-thread safe: returns the currently-bound inner parameter (or
        nullptr). */
    juce::AudioProcessorParameter* getInnerParameter() const noexcept
    {
        return innerParam_.load (std::memory_order_acquire);
    }

    bool       isBound()       const noexcept { return getInnerParameter() != nullptr; }
    juce::Uuid getNodeUuid()   const noexcept { return boundNodeUuid_; }
    int        getParamIndex() const noexcept { return boundParamIndex_; }
    int        getSlotIndex()  const noexcept { return slotIndex_; }

    /** Mirror an inner-parameter movement back to the host so the DAW can
        record automation. Call from the GUI sync timer when a non-host
        actor (e.g. plugin GUI knob) changed the bound param's value. */
    void updateFromInner (float newValue);

    //==============================================================================
    // Audio-thread API.

    /** If a host-driven write is pending, store it in `out` and return true. */
    bool consumePending (float& out) noexcept;

    //==============================================================================
    // HostedAudioProcessorParameter / AudioProcessorParameter
    juce::String getParameterID() const override { return stableId_; }

    float        getValue() const override
    {
        return cachedValue_.load (std::memory_order_relaxed);
    }
    void         setValue (float newValue) override;
    float        getDefaultValue() const override;
    juce::String getName (int maximumStringLength) const override;
    juce::String getLabel() const override;
    juce::String getText (float value, int maximumStringLength) const override;
    float        getValueForText (const juce::String& text) const override;
    int          getNumSteps() const override;
    bool         isDiscrete() const override;
    bool         isBoolean() const override;
    bool         isAutomatable() const override { return true; }

private:
    const int          slotIndex_;
    const juce::String stableId_;     // "p000" .. "pNNN" — STABLE for processor lifetime

    std::atomic<float> cachedValue_  { 0.0f };
    std::atomic<float> pendingValue_ { 0.0f };
    std::atomic<bool>  pendingDirty_ { false };

    // Audio thread reads this; message thread writes it during bind/unbind.
    std::atomic<juce::AudioProcessorParameter*> innerParam_ { nullptr };

    // For serialization. Touched from message thread only.
    juce::Uuid   boundNodeUuid_;
    int          boundParamIndex_ = -1;
    juce::String boundDisplayName_;
};
