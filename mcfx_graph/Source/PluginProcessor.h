/*
 ==============================================================================

 mcfx_graph — flexible plugin graph / patchbay.

 The outer AudioProcessor wraps a GraphController (juce::AudioProcessorGraph
 plus IO terminals + UUID/NodeID tracking + JSON-friendly serialization).
 Native nodes (gain, mute/phase, matrix mixer, delay) and hosted plugin nodes can
 be added; multiple sources feeding one input are summed by the graph.

 ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "mcfx_buses.h"
#include "Graph/GraphController.h"
#include "Graph/GraphSerializer.h"
#include "Hosting/PluginListManager.h"
#include "ForwardingParameter.h"
#include <map>

class Mcfx_graphAudioProcessor : public juce::AudioProcessor,
                                 public juce::ChangeBroadcaster,
                                 private juce::AsyncUpdater,
                                 private juce::Timer,
                                 private juce::AudioProcessorListener
{
public:
    Mcfx_graphAudioProcessor();
    ~Mcfx_graphAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

   #if MCFX_MULTICHANNEL_BUILD
    MCFX_MULTICHANEL_APPLY_BUS_LAYOUTS_OVERRIDE
   #endif

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override         { return JucePlugin_Name; }
    bool acceptsMidi()  const override                  { return false; }
    bool producesMidi() const override                  { return false; }
    bool silenceInProducesSilenceOut() const override   { return false; }
    double getTailLengthSeconds() const override        { return 0.0; }

    int getNumPrograms() override                       { return 1; }
    int getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    GraphController& getGraph() { return *graph_; }
    PluginListManager& getPluginList() { return pluginList_; }

    /** Export the current graph to a juce::var (JSON-ready). */
    juce::var exportToVar() const;

    /** Import a graph definition from a juce::var. Returns true on success. */
    bool importFromVar (const juce::var& v, juce::String* errorOut = nullptr);

    /** Save / load a .json file containing the graph definition. */
    bool saveToJsonFile (const juce::File& file, juce::String* errorOut = nullptr) const;
    bool loadFromJsonFile (const juce::File& file, juce::String* errorOut = nullptr);

    //==============================================================================
    // Snapshot-based undo/redo. The editor calls commitHistorySnapshot() after
    // every user-driven topology change; undo / redo restore from the history.
    void commitHistorySnapshot();
    bool canUndo() const;
    bool canRedo() const;
    bool undo();
    bool redo();

    //==============================================================================
    // Parameter exposure. The plugin exposes a fixed pool of 256 forwarding
    // parameters to the DAW; each may be bound to one inner-plugin parameter.
    static constexpr int kForwardingParameterCount = 256;
    int  getForwardingParameterCount() const noexcept { return kForwardingParameterCount; }
    ForwardingParameter* getForwardingParameter (int slot) const noexcept
    {
        return (slot >= 0 && slot < forwardingParameters_.size())
                 ? forwardingParameters_.getUnchecked (slot) : nullptr;
    }

    /** Bind the next free slot to (nodeUuid, paramIndex). Returns the slot
        index, or -1 if no slot is free or the binding is invalid. */
    int  exposeParameter   (juce::Uuid nodeUuid, int paramIndex);

    /** Unbind whichever slot (if any) is bound to (nodeUuid, paramIndex). */
    void unexposeParameter (juce::Uuid nodeUuid, int paramIndex);

    /** Returns the slot bound to (nodeUuid, paramIndex), or -1. */
    int  getExposedSlotFor (juce::Uuid nodeUuid, int paramIndex) const;

    /** Drop every binding referencing the given node — called when a node is
        about to be removed or its processor swapped. */
    void unbindAllForNode (juce::Uuid nodeUuid);

    //==============================================================================
    // Editor UI preferences (persisted in JSON so they survive reload).
    int  getPropertyPanelWidth() const noexcept     { return propertyPanelWidth_; }
    void setPropertyPanelWidth (int w) noexcept     { propertyPanelWidth_ = juce::jmax (0, w); }

    //==============================================================================
    // Plugin GUI window registry. NodeComponent::openPluginEditor() registers
    // the freshly-opened DocumentWindow here so that the slow undo/redo path
    // (which destroys and recreates plugin instances) can close any open
    // plugin GUI BEFORE the underlying processor is freed — otherwise the
    // floating editor's destructor races with the processor destruction and
    // crashes on plug-ins that don't tolerate being unloaded mid-show.
    void registerPluginWindow   (juce::Uuid nodeUuid, juce::DocumentWindow* dw);
    void unregisterPluginWindow (juce::DocumentWindow* dw);

private:
    void closePluginWindowFor (juce::Uuid nodeUuid);

    void handleAsyncUpdate() override;
    void timerCallback() override;

    // AudioProcessorListener — fires when any hosted node's parameters change.
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override;
    void audioProcessorChanged (juce::AudioProcessor*, const juce::AudioProcessorListener::ChangeDetails&) override;
    void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int) override;
    void audioProcessorParameterChangeGestureEnd   (juce::AudioProcessor*, int) override;

    void scheduleDebouncedHistoryCommit();

    void rebindAllSlotsAfterRestore();
    juce::AudioProcessorParameter* lookupInnerParameter (juce::Uuid nodeUuid, int paramIndex) const;

    // Fast-path undo: if the incoming JSON's structure (set of UUIDs, kinds,
    // channel counts, plugin identities) matches the live graph, apply state
    // changes in place — no plugin recreation, no GUI window churn. Returns
    // false (and leaves the graph untouched) when any structural difference
    // is found, in which case importFromVar falls through to the slow path.
    bool tryFastUpdate (const juce::var& v);

    std::unique_ptr<GraphController> graph_;
    PluginListManager pluginList_;

    // Fixed-size pool of automatable parameters exposed to the DAW.
    // The AudioProcessor's parameter tree takes ownership of each pointer
    // (via addParameter); this Array is just a non-owning indexed cache so
    // we can poke individual slots without scanning the parameter tree.
    juce::Array<ForwardingParameter*> forwardingParameters_;

    // Editor UI prefs persisted alongside the graph in JSON.
    int propertyPanelWidth_ = 280;

    // Debounced history-commit epoch — bumped on each parameter change so
    // a deferred lambda only commits if no further changes arrived during
    // the wait. Prevents 60-events-per-second slider drags from spamming
    // the undo stack.
    std::atomic<int> paramDirtyEpoch_ { 0 };

    // Pending JSON to apply on the message thread (set by setStateInformation).
    juce::SpinLock pendingLock_;
    juce::var pendingState_;
    bool hasPendingState_ = false;

    // Snapshot-based undo/redo. The stack always has the current state at
    // historyIndex_; undo decrements the index, redo increments it.
    std::vector<juce::var> history_;
    int historyIndex_ = -1;
    // Counter (not bool) so it can survive across deferred MessageManager
    // callAsync invocations — see importFromVar for the FIFO-flush trick.
    std::atomic<int> suppressHistoryDepth_ { 0 };
    static constexpr int kMaxHistory = 64;

    // Open plugin GUI windows keyed by node UUID. Owned externally (each
    // window deletes itself on close button), but tracked here so we can
    // close and free them before the underlying plugin processor goes away.
    std::map<juce::String, juce::DocumentWindow*> pluginWindows_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Mcfx_graphAudioProcessor)
};

//==============================================================================
// JUCE plugin entrypoint
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
