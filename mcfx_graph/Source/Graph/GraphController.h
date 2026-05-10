/*
  ==============================================================================

   GraphController — wraps a juce::AudioProcessorGraph plus two
   AudioGraphIOProcessor terminals representing the host's I/O, and tracks a
   parallel collection of GraphNode metadata indexed by stable UUIDs.

   Connections in juce::AudioProcessorGraph use ephemeral NodeIDs that are
   not stable across save/load. The controller maintains a UUID <-> NodeID map
   so connections can be saved and restored using stable identifiers.

   Threading: structural mutations (add/remove node, add/remove connection)
   are message-thread only. Use ScopedSuspend to wrap multi-step changes
   (JSON load, host channel-count change) so the audio thread sees a stable
   graph throughout. Single mutations are fine without suspension because
   juce::AudioProcessorGraph holds an internal lock.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GraphNode.h"
#include <map>
#include <unordered_map>
#include <functional>

class GraphController
{
public:
    GraphController();
    ~GraphController();

    //==============================================================================
    // Outer plugin lifecycle
    void prepareToPlay (double sampleRate, int blockSize, int numIn, int numOut);
    void releaseResources();
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

    int getLatencySamples() const;

    //==============================================================================
    // IO terminals
    juce::Uuid getInputTerminalUuid()  const { return inputTerminalUuid_; }
    juce::Uuid getOutputTerminalUuid() const { return outputTerminalUuid_; }

    int  getInputChannelCount()  const { return inputChannelCount_; }
    int  getOutputChannelCount() const { return outputChannelCount_; }

    //==============================================================================
    // Node management. addNode takes ownership of the AudioProcessor (transfers
    // it into the inner juce::AudioProcessorGraph). Returns the assigned UUID.
    // The (optional) preset uuid lets callers restore from JSON with the same
    // UUID that was saved.
    juce::Uuid addNode (std::unique_ptr<juce::AudioProcessor> proc,
                        NodeKind kind,
                        const juce::String& displayName,
                        int channelCountIn,
                        int channelCountOut,
                        juce::Point<int> editorPosition = { 100, 100 },
                        juce::Uuid presetUuid = {});

    void removeNode (const juce::Uuid& uuid);
    void clearAllUserNodes();

    /** Swap the processor of an existing node, keeping its UUID and editor
        position. Connections that still fit the new in/out channel counts are
        preserved; the rest are dropped. Returns false if the UUID is unknown
        or refers to a terminal. */
    bool replaceNodeProcessor (const juce::Uuid& uuid,
                               std::unique_ptr<juce::AudioProcessor> newProc,
                               NodeKind newKind,
                               const juce::String& newDisplayName,
                               int newChannelCountIn,
                               int newChannelCountOut,
                               std::unique_ptr<juce::PluginDescription> newPluginDesc = {});

    GraphNode* getNode (const juce::Uuid& uuid) const;
    GraphNode* getNodeByNodeId (juce::AudioProcessorGraph::NodeID nid) const;
    juce::Array<GraphNode*> getAllUserNodes() const;
    juce::Array<GraphNode*> getAllNodesIncludingTerminals() const;

    void setNodePosition (const juce::Uuid& uuid, juce::Point<int> pos);

    /** Update the user-customizable display name shown in the node tile,
        properties panel, and (for subgraphs) the breadcrumb. Empty string
        reverts to the processor-derived name. No-op on terminals. */
    void setNodeDisplayName (const juce::Uuid& uuid, const juce::String& newName);

    /** Bypass: input passes through to output via JUCE's per-node bypass.
        For asymmetric channel layouts the JUCE engine maps as best it can. */
    void setNodeBypassed (const juce::Uuid& uuid, bool on);

    /** Mute: temporarily disconnects incoming connections so no audio
        reaches the node. Combined with bypass, output is silence.
        Disconnected wires are stashed and restored on unmute. */
    void setNodeMuted (const juce::Uuid& uuid, bool on);

    //==============================================================================
    // Connections. Returns true on success.
    bool addConnection    (const juce::Uuid& fromUuid, int fromCh,
                           const juce::Uuid& toUuid,   int toCh);
    bool removeConnection (const juce::Uuid& fromUuid, int fromCh,
                           const juce::Uuid& toUuid,   int toCh);
    bool isConnected      (const juce::Uuid& fromUuid, int fromCh,
                           const juce::Uuid& toUuid,   int toCh) const;

    struct ConnectionInfo
    {
        juce::Uuid fromUuid;  int fromCh;
        juce::Uuid toUuid;    int toCh;

        bool operator== (const ConnectionInfo& other) const noexcept
        {
            return fromUuid == other.fromUuid && fromCh == other.fromCh
                && toUuid   == other.toUuid   && toCh   == other.toCh;
        }
        bool operator!= (const ConnectionInfo& other) const noexcept { return ! (*this == other); }
    };
    std::vector<ConnectionInfo> getAllConnections() const;

    /** Replace all connections to/from the input/output terminals with a
        straight 1:1 passthrough on every channel. Useful as a default. */
    void connectIOPassthrough();

    /** Drop every connection that doesn't involve a terminal — called when
        deserializing a saved graph or when starting a fresh graph. */
    void clearAllConnections();

    //==============================================================================
    /** RAII helper that pauses processing on the supplied AudioProcessor while
        a structural change is made, then resumes. Pass the OUTER plugin's
        AudioProcessor (the one whose suspendProcessing controls audio-thread
        entry into our graph). */
    class ScopedSuspend
    {
    public:
        ScopedSuspend (juce::AudioProcessor& outer) : outer_ (outer)
        {
            wasSuspended_ = false;
            outer_.suspendProcessing (true);
        }
        ~ScopedSuspend()
        {
            outer_.suspendProcessing (false);
        }
    private:
        juce::AudioProcessor& outer_;
        bool wasSuspended_;
    };

    //==============================================================================
    // Listener: notified whenever the graph topology changes (node added/removed,
    // connection added/removed, terminal channel count changed). Editor uses
    // this to redraw.
    using TopologyListener = std::function<void()>;
    void setTopologyListener (TopologyListener cb) { topologyListener_ = std::move (cb); }

    /** Fired BEFORE a node's processor is destroyed (remove or replace), so
        external observers like the parameter-forwarding pool can drop their
        cached pointers. The uuid passed in is still valid at the moment of
        the callback. */
    using NodeAboutToBeRemovedListener = std::function<void (juce::Uuid)>;
    void setNodeAboutToBeRemovedListener (NodeAboutToBeRemovedListener cb)
    {
        nodeAboutToBeRemovedListener_ = std::move (cb);
    }

    /** Fired AFTER a user node is added (or its processor swapped via
        replaceNodeProcessor). Lets external observers attach themselves —
        e.g., the outer plug-in attaches an AudioProcessorListener to capture
        parameter changes for undo. */
    using NodeAddedListener = std::function<void (juce::Uuid)>;
    void setNodeAddedListener (NodeAddedListener cb)
    {
        nodeAddedListener_ = std::move (cb);
    }

    juce::AudioProcessorGraph& getGraph() noexcept { return *graph_; }

private:
    void rebuildIOTerminals (int numIn, int numOut);
    void notifyTopologyChanged();

    bool resolveEndpoint (const juce::Uuid& uuid,
                          juce::AudioProcessorGraph::NodeID& outId) const;

    std::unique_ptr<juce::AudioProcessorGraph> graph_;

    juce::AudioProcessorGraph::Node::Ptr inputTerminalNode_;
    juce::AudioProcessorGraph::Node::Ptr outputTerminalNode_;

    juce::Uuid inputTerminalUuid_;
    juce::Uuid outputTerminalUuid_;

    int inputChannelCount_ = 0;
    int outputChannelCount_ = 0;
    double sampleRate_ = 0.0;
    int blockSize_ = 0;

    juce::OwnedArray<GraphNode> userNodes_;
    GraphNode inputTerminalMeta_;
    GraphNode outputTerminalMeta_;

    // juce::String / juce::Uuid don't ship std::hash by default, so we key by
    // string and use std::map (operator< on juce::String is fine).
    std::map<juce::String, GraphNode*> uuidToNode_;
    std::unordered_map<juce::uint32, GraphNode*> nodeIdToNode_;

    TopologyListener             topologyListener_;
    NodeAboutToBeRemovedListener nodeAboutToBeRemovedListener_;
    NodeAddedListener            nodeAddedListener_;
};
