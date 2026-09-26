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
#include "../NativeNodes/FeedbackBus.h"
#include "GraphNode.h"
#include <map>
#include <unordered_map>
#include <functional>
#include <vector>

class GraphController : private juce::AudioProcessorListener,
                        private juce::AsyncUpdater
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

    /** Called whenever this level's total latency changes: after the graph
        re-plans its delay compensation (a node added, removed or rewired, or
        a node reporting a new latency). A SubgraphNode uses it to take on
        its inner graph's latency; the plug-in uses it to tell the host.
        Runs on whatever thread rebuilt the graph (normally the message
        thread, or the caller of prepareToPlay). */
    using LatencyListener = std::function<void()>;
    void setLatencyListener (LatencyListener cb) { latencyListener_ = std::move (cb); }

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

    /** Leave the node's latency out of delay compensation (and out of the
        latency reported upwards). See GraphNode::ignoreLatency. */
    void setNodeIgnoreLatency (const juce::Uuid& uuid, bool on);

    /** The node's own latency in samples, whether or not it is ignored. */
    int getNodeLatency (const juce::Uuid& uuid) const;

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
    /** Whether addConnection would accept this wire, without adding it. */
    bool canConnect       (const juce::Uuid& fromUuid, int fromCh,
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
    // Feedback links. A link pairs a FeedbackSendNode with a FeedbackReturnNode
    // so the loop closes through a shared FeedbackBus with one block of delay.
    //
    // A link is deliberately NOT a juce::AudioProcessorGraph connection: an edge
    // from the send back to the return would close a cycle, and the graph cannot
    // render cycles (the back edge silently reads an empty buffer). Keeping the
    // pair unconnected leaves the graph acyclic. The editor still draws the link
    // as a wire, because the user needs to see the loop they built — it is just
    // ours to draw, not the graph's to schedule.

    struct FeedbackLinkInfo
    {
        juce::Uuid sendUuid;
        juce::Uuid returnUuid;

        bool operator== (const FeedbackLinkInfo& o) const noexcept
        {
            return sendUuid == o.sendUuid && returnUuid == o.returnUuid;
        }
        bool operator!= (const FeedbackLinkInfo& o) const noexcept { return ! (*this == o); }
    };

    /** Pair a send with a return. Both UUIDs must name nodes of the matching
        kind, both must currently be unlinked (1:1 only — several sends into one
        return would need a summing rule we have not defined), and their channel
        counts must agree. Returns false otherwise. */
    bool addFeedbackLink    (const juce::Uuid& sendUuid, const juce::Uuid& returnUuid);
    bool removeFeedbackLink (const juce::Uuid& sendUuid, const juce::Uuid& returnUuid);
    std::vector<FeedbackLinkInfo> getAllFeedbackLinks() const;

    /** The link involving this node, if any. Either end resolves to the pair. */
    bool findFeedbackLinkFor (const juce::Uuid& nodeUuid, FeedbackLinkInfo& out) const;

    /** Why a proposed link would be refused, for the editor to show. Empty when
        the pair is legal. */
    juce::String describeFeedbackLinkRefusal (const juce::Uuid& sendUuid,
                                              const juce::Uuid& returnUuid) const;

    void clearAllFeedbackLinks();

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
        external observers like the parameter-forwarding pool or the node's
        open window can let go of it. The uuid passed in is still valid at the
        moment of the callback.

        Covers every nesting level: removing a subgraph first reports each
        node inside it (innermost first), and a subgraph's inner graph reports
        its own removals up to this listener. */
    using NodeAboutToBeRemovedListener = std::function<void (juce::Uuid)>;
    void setNodeAboutToBeRemovedListener (NodeAboutToBeRemovedListener cb);

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

    // Delay compensation. We listen to every node's (wrapper) processor and
    // to our own AudioProcessorGraph. A node reporting a new latency makes
    // the graph re-plan (juce::AudioProcessorGraph doesn't watch its nodes'
    // latency itself); the graph re-planning to a new total is passed on to
    // latencyListener_.
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged (juce::AudioProcessor* proc,
                                const juce::AudioProcessorListener::ChangeDetails& details) override;
    // Rebuild from the message thread, and never from inside a node's
    // latency notice: that can arrive while the graph is preparing its nodes.
    void handleAsyncUpdate() override;

    void watchNodeLatency   (juce::AudioProcessorGraph::NodeID nid);
    void unwatchNodeLatency (juce::AudioProcessorGraph::NodeID nid);

    /** Report a node about to go, and for a subgraph everything inside it
        first; then unhook the subgraph's inner graph from this one. */
    void announceRemoval (GraphNode& gn);

    /** Make a subgraph node's inner graph report its removals to this one. */
    void forwardRemovalsFrom (GraphNode& gn);
    void notifyTopologyChanged();

    /** (Re)allocate every linked bus for the current block size, and detach the
        buses of any link whose nodes went away. */
    void prepareFeedbackBuses();

    /** Drop any link that names a node which no longer exists, or whose ends no
        longer agree — called after node removal and processor replacement. */
    void pruneFeedbackLinks();

    void bindFeedbackLink   (const FeedbackLinkInfo& link);
    void unbindFeedbackLink (const FeedbackLinkInfo& link);

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

    std::vector<FeedbackLinkInfo> feedbackLinks_;
    // Bus per link, same index. Held here so both nodes can share one and it
    // outlives neither.
    std::vector<FeedbackBus::Ptr> feedbackBuses_;

    TopologyListener             topologyListener_;
    LatencyListener              latencyListener_;
    NodeAboutToBeRemovedListener nodeAboutToBeRemovedListener_;
    NodeAddedListener            nodeAddedListener_;
};
