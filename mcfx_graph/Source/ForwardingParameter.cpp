#include "ForwardingParameter.h"

namespace
{
    juce::String makeStableId (int slot)
    {
        return "p" + juce::String (slot).paddedLeft ('0', 3);
    }
}

ForwardingParameter::ForwardingParameter (int slotIndex)
    : slotIndex_ (slotIndex), stableId_ (makeStableId (slotIndex))
{
}

void ForwardingParameter::bind (juce::AudioProcessorParameter* innerParam,
                                 juce::Uuid nodeUuid,
                                 int paramIndex,
                                 const juce::String& displayName)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    boundNodeUuid_     = nodeUuid;
    boundParamIndex_   = paramIndex;
    boundDisplayName_  = displayName;

    innerParam_.store (innerParam, std::memory_order_release);

    // Seed cached value from the freshly bound inner so the DAW sees the
    // right state — important right after project restore.
    if (innerParam != nullptr)
        cachedValue_.store (innerParam->getValue(), std::memory_order_relaxed);

    // Drop any stale pending write left over from a previous binding.
    pendingDirty_.store (false, std::memory_order_release);
}

void ForwardingParameter::unbind()
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    innerParam_.store (nullptr, std::memory_order_release);
    boundNodeUuid_    = {};
    boundParamIndex_  = -1;
    boundDisplayName_.clear();
    pendingDirty_.store (false, std::memory_order_release);
}

void ForwardingParameter::updateFromInner (float newValue)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    cachedValue_.store (newValue, std::memory_order_relaxed);
    sendValueChangedMessageToListeners (newValue);
}

bool ForwardingParameter::consumePending (float& out) noexcept
{
    if (! pendingDirty_.exchange (false, std::memory_order_acquire))
        return false;
    out = pendingValue_.load (std::memory_order_relaxed);
    return true;
}

void ForwardingParameter::setValue (float newValue)
{
    cachedValue_.store  (newValue, std::memory_order_relaxed);
    pendingValue_.store (newValue, std::memory_order_relaxed);
    pendingDirty_.store (true,     std::memory_order_release);
}

float ForwardingParameter::getDefaultValue() const
{
    if (auto* p = innerParam_.load (std::memory_order_acquire))
        return p->getDefaultValue();
    return 0.0f;
}

juce::String ForwardingParameter::getName (int maximumStringLength) const
{
    if (boundDisplayName_.isNotEmpty())
        return boundDisplayName_.substring (0, maximumStringLength);
    if (auto* p = innerParam_.load (std::memory_order_acquire))
    {
        auto n = p->getName (maximumStringLength);
        if (n.isNotEmpty()) return n;
    }
    return ("Param " + juce::String (slotIndex_ + 1)).substring (0, maximumStringLength);
}

juce::String ForwardingParameter::getLabel() const
{
    if (auto* p = innerParam_.load (std::memory_order_acquire))
        return p->getLabel();
    return {};
}

juce::String ForwardingParameter::getText (float value, int maximumStringLength) const
{
    if (auto* p = innerParam_.load (std::memory_order_acquire))
        return p->getText (value, maximumStringLength);
    return juce::String (value, 3);
}

float ForwardingParameter::getValueForText (const juce::String& text) const
{
    if (auto* p = innerParam_.load (std::memory_order_acquire))
        return p->getValueForText (text);
    return text.getFloatValue();
}

int ForwardingParameter::getNumSteps() const
{
    if (auto* p = innerParam_.load (std::memory_order_acquire))
        return p->getNumSteps();
    return juce::AudioProcessor::getDefaultNumParameterSteps();
}

bool ForwardingParameter::isDiscrete() const
{
    auto* p = innerParam_.load (std::memory_order_acquire);
    return p != nullptr && p->isDiscrete();
}

bool ForwardingParameter::isBoolean() const
{
    auto* p = innerParam_.load (std::memory_order_acquire);
    return p != nullptr && p->isBoolean();
}
