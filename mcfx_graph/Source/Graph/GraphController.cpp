#include "GraphController.h"
#include "BypassMuteWrapper.h"

using AGProc = juce::AudioProcessorGraph;

GraphController::GraphController()
{
    graph_ = std::make_unique<AGProc>();

    // Create the audio I/O terminals once. Their channel counts come from the
    // parent graph's setPlayConfigDetails — we just have to add the nodes.
    auto in  = std::make_unique<AGProc::AudioGraphIOProcessor> (
                  AGProc::AudioGraphIOProcessor::audioInputNode);
    auto out = std::make_unique<AGProc::AudioGraphIOProcessor> (
                  AGProc::AudioGraphIOProcessor::audioOutputNode);

    inputTerminalNode_  = graph_->addNode (std::move (in));
    outputTerminalNode_ = graph_->addNode (std::move (out));

    inputTerminalUuid_  = juce::Uuid();
    outputTerminalUuid_ = juce::Uuid();

    inputTerminalMeta_.uuid    = inputTerminalUuid_;
    inputTerminalMeta_.nodeId  = inputTerminalNode_->nodeID;
    inputTerminalMeta_.processor = inputTerminalNode_->getProcessor();
    inputTerminalMeta_.kind    = NodeKind::InputTerminal;
    inputTerminalMeta_.displayName = "Input";
    inputTerminalMeta_.editorPosition = { 60, 240 };

    outputTerminalMeta_.uuid    = outputTerminalUuid_;
    outputTerminalMeta_.nodeId  = outputTerminalNode_->nodeID;
    outputTerminalMeta_.processor = outputTerminalNode_->getProcessor();
    outputTerminalMeta_.kind    = NodeKind::OutputTerminal;
    outputTerminalMeta_.displayName = "Output";
    outputTerminalMeta_.editorPosition = { 720, 240 };

    uuidToNode_[inputTerminalUuid_.toString()]  = &inputTerminalMeta_;
    uuidToNode_[outputTerminalUuid_.toString()] = &outputTerminalMeta_;
    nodeIdToNode_[inputTerminalNode_->nodeID.uid]  = &inputTerminalMeta_;
    nodeIdToNode_[outputTerminalNode_->nodeID.uid] = &outputTerminalMeta_;
}

GraphController::~GraphController()
{
    // unique_ptr<AGProc> destroys all nodes in turn.
}

//==============================================================================

void GraphController::prepareToPlay (double sampleRate, int blockSize, int numIn, int numOut)
{
    sampleRate_ = sampleRate;
    blockSize_  = blockSize;

    if (numIn != inputChannelCount_ || numOut != outputChannelCount_)
        rebuildIOTerminals (numIn, numOut);

    graph_->setPlayConfigDetails (numIn, numOut, sampleRate, blockSize);
    graph_->prepareToPlay (sampleRate, blockSize);

    inputTerminalMeta_.channelCountIn   = 0;
    inputTerminalMeta_.channelCountOut  = numIn;
    outputTerminalMeta_.channelCountIn  = numOut;
    outputTerminalMeta_.channelCountOut = 0;
}

void GraphController::releaseResources()
{
    graph_->releaseResources();
}

void GraphController::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    graph_->processBlock (buffer, midi);
}

int GraphController::getLatencySamples() const
{
    return graph_->getLatencySamples();
}

//==============================================================================

void GraphController::rebuildIOTerminals (int numIn, int numOut)
{
    // Drop connections that reference now-out-of-range channels on either
    // terminal so the graph stays internally consistent.
    auto allConns = graph_->getConnections();
    for (const auto& c : allConns)
    {
        const bool srcIsInput  = c.source.nodeID      == inputTerminalNode_->nodeID;
        const bool dstIsOutput = c.destination.nodeID == outputTerminalNode_->nodeID;
        const bool dstIsInput  = c.destination.nodeID == inputTerminalNode_->nodeID;
        const bool srcIsOutput = c.source.nodeID      == outputTerminalNode_->nodeID;

        if (srcIsInput  && c.source.channelIndex      >= numIn)  graph_->removeConnection (c);
        else if (dstIsOutput && c.destination.channelIndex >= numOut) graph_->removeConnection (c);
        else if (dstIsInput  && c.destination.channelIndex >= numIn)  graph_->removeConnection (c);
        else if (srcIsOutput && c.source.channelIndex      >= numOut) graph_->removeConnection (c);
    }

    inputChannelCount_  = numIn;
    outputChannelCount_ = numOut;

    notifyTopologyChanged();
}

void GraphController::connectIOPassthrough()
{
    const int n = juce::jmin (inputChannelCount_, outputChannelCount_);
    for (int c = 0; c < n; ++c)
    {
        graph_->addConnection ({{ inputTerminalNode_->nodeID,  c },
                                { outputTerminalNode_->nodeID, c }});
    }
    notifyTopologyChanged();
}

//==============================================================================

juce::Uuid GraphController::addNode (std::unique_ptr<juce::AudioProcessor> proc,
                                     NodeKind kind,
                                     const juce::String& displayName,
                                     int channelCountIn,
                                     int channelCountOut,
                                     juce::Point<int> editorPosition,
                                     juce::Uuid presetUuid)
{
    if (proc == nullptr) return {};

    if (sampleRate_ > 0.0 && blockSize_ > 0)
        proc->prepareToPlay (sampleRate_, blockSize_);

    // Wrap the user processor so bypass/mute can be toggled per-node without
    // touching connections. The wrapper is what the AudioProcessorGraph
    // actually owns; gn->processor still points at the inner so editor code
    // (dynamic_cast<GainNode*>(node.processor) etc.) keeps working.
    auto* rawInner = proc.get();
    auto wrapped   = std::make_unique<BypassMuteWrapper> (std::move (proc));
    auto nodePtr   = graph_->addNode (std::move (wrapped));
    if (nodePtr == nullptr) return {};

    auto* gn = new GraphNode();
    gn->uuid           = presetUuid.isNull() ? juce::Uuid() : presetUuid;
    gn->nodeId         = nodePtr->nodeID;
    gn->processor      = rawInner;
    gn->kind           = kind;
    gn->channelCountIn = channelCountIn;
    gn->channelCountOut= channelCountOut;
    gn->editorPosition = editorPosition;
    gn->displayName    = displayName;
    userNodes_.add (gn);

    uuidToNode_[gn->uuid.toString()] = gn;
    nodeIdToNode_[nodePtr->nodeID.uid] = gn;

    if (nodeAddedListener_) nodeAddedListener_ (gn->uuid);
    notifyTopologyChanged();
    return gn->uuid;
}

void GraphController::removeNode (const juce::Uuid& uuid)
{
    auto it = uuidToNode_.find (uuid.toString());
    if (it == uuidToNode_.end()) return;

    GraphNode* gn = it->second;
    if (gn == &inputTerminalMeta_ || gn == &outputTerminalMeta_) return; // can't remove terminals

    if (nodeAboutToBeRemovedListener_) nodeAboutToBeRemovedListener_ (gn->uuid);

    graph_->removeNode (gn->nodeId);
    nodeIdToNode_.erase (gn->nodeId.uid);
    uuidToNode_.erase (it);

    for (int i = 0; i < userNodes_.size(); ++i)
    {
        if (userNodes_.getUnchecked (i) == gn)
        {
            userNodes_.remove (i);
            break;
        }
    }

    notifyTopologyChanged();
}

void GraphController::clearAllUserNodes()
{
    for (auto* gn : userNodes_)
    {
        if (nodeAboutToBeRemovedListener_) nodeAboutToBeRemovedListener_ (gn->uuid);
        graph_->removeNode (gn->nodeId);
        nodeIdToNode_.erase (gn->nodeId.uid);
        uuidToNode_.erase (gn->uuid.toString());
    }
    userNodes_.clear();
    notifyTopologyChanged();
}

bool GraphController::replaceNodeProcessor (const juce::Uuid& uuid,
                                             std::unique_ptr<juce::AudioProcessor> newProc,
                                             NodeKind newKind,
                                             const juce::String& newDisplayName,
                                             int newChannelCountIn,
                                             int newChannelCountOut,
                                             std::unique_ptr<juce::PluginDescription> newPluginDesc)
{
    auto* gn = getNode (uuid);
    if (gn == nullptr || newProc == nullptr) return false;
    if (gn == &inputTerminalMeta_ || gn == &outputTerminalMeta_) return false;

    // Snapshot connections that touch this node before we tear it out.
    struct SavedConn
    {
        juce::Uuid otherUuid;
        int        otherChannel;
        int        thisChannel;
        bool       thisIsSource;   // this node was the connection source
    };
    std::vector<SavedConn> saved;
    for (const auto& c : graph_->getConnections())
    {
        if (c.source.nodeID == gn->nodeId)
        {
            if (auto* dst = getNodeByNodeId (c.destination.nodeID))
                saved.push_back ({ dst->uuid, c.destination.channelIndex,
                                   c.source.channelIndex, true });
        }
        else if (c.destination.nodeID == gn->nodeId)
        {
            if (auto* src = getNodeByNodeId (c.source.nodeID))
                saved.push_back ({ src->uuid, c.source.channelIndex,
                                   c.destination.channelIndex, false });
        }
    }

    const auto savedPosition = gn->editorPosition;

    // Drop any pointers external observers (e.g. forwarding-parameter slots)
    // hold to the old processor before we destroy it.
    if (nodeAboutToBeRemovedListener_) nodeAboutToBeRemovedListener_ (gn->uuid);

    // Tear out the old processor.
    graph_->removeNode (gn->nodeId);
    nodeIdToNode_.erase (gn->nodeId.uid);

    if (sampleRate_ > 0.0 && blockSize_ > 0)
        newProc->prepareToPlay (sampleRate_, blockSize_);

    // Wrap the new processor so bypass/mute work the same way as for nodes
    // added via addNode().
    auto* rawInner = newProc.get();
    auto wrapped   = std::make_unique<BypassMuteWrapper> (std::move (newProc));
    auto nodePtr   = graph_->addNode (std::move (wrapped));
    if (nodePtr == nullptr)
    {
        // Failure path: we already removed the old node, so drop it from
        // metadata too. Caller's UUID becomes invalid — propagate via false.
        for (int i = 0; i < userNodes_.size(); ++i)
            if (userNodes_.getUnchecked (i) == gn)
            {
                userNodes_.remove (i);
                break;
            }
        uuidToNode_.erase (uuid.toString());
        notifyTopologyChanged();
        return false;
    }

    // Re-apply bypass/mute on the freshly-created wrapper so a channel-count
    // change (which triggers replaceNodeProcessor) doesn't silently re-enable
    // the node.
    if (auto* w = dynamic_cast<BypassMuteWrapper*> (nodePtr->getProcessor()))
    {
        w->setBypassedFlag (gn->bypassed);
        w->setMutedFlag    (gn->muted);
    }

    // Update metadata in place. UUID stays the same; NodeID is new.
    gn->nodeId           = nodePtr->nodeID;
    gn->processor        = rawInner;
    gn->kind             = newKind;
    gn->displayName      = newDisplayName;
    gn->channelCountIn   = newChannelCountIn;
    gn->channelCountOut  = newChannelCountOut;
    gn->editorPosition   = savedPosition;
    gn->pluginDescription = std::move (newPluginDesc);

    nodeIdToNode_[nodePtr->nodeID.uid] = gn;

    if (nodeAddedListener_) nodeAddedListener_ (gn->uuid);

    // Re-apply connections that still fit. Out-of-range channels become
    // invalid and are silently dropped.
    for (const auto& s : saved)
    {
        if (s.thisIsSource)
        {
            if (s.thisChannel < newChannelCountOut)
                addConnection (uuid, s.thisChannel, s.otherUuid, s.otherChannel);
        }
        else
        {
            if (s.thisChannel < newChannelCountIn)
                addConnection (s.otherUuid, s.otherChannel, uuid, s.thisChannel);
        }
    }

    notifyTopologyChanged();
    return true;
}

GraphNode* GraphController::getNode (const juce::Uuid& uuid) const
{
    auto it = uuidToNode_.find (uuid.toString());
    return it != uuidToNode_.end() ? it->second : nullptr;
}

GraphNode* GraphController::getNodeByNodeId (juce::AudioProcessorGraph::NodeID nid) const
{
    auto it = nodeIdToNode_.find (nid.uid);
    return it != nodeIdToNode_.end() ? it->second : nullptr;
}

juce::Array<GraphNode*> GraphController::getAllUserNodes() const
{
    juce::Array<GraphNode*> out;
    for (auto* gn : userNodes_) out.add (gn);
    return out;
}

juce::Array<GraphNode*> GraphController::getAllNodesIncludingTerminals() const
{
    juce::Array<GraphNode*> out;
    out.add (const_cast<GraphNode*> (&inputTerminalMeta_));
    for (auto* gn : userNodes_) out.add (gn);
    out.add (const_cast<GraphNode*> (&outputTerminalMeta_));
    return out;
}

void GraphController::setNodePosition (const juce::Uuid& uuid, juce::Point<int> pos)
{
    if (auto* gn = getNode (uuid))
        gn->editorPosition = pos;
}

void GraphController::setNodeBypassed (const juce::Uuid& uuid, bool on)
{
    auto* gn = getNode (uuid);
    if (gn == nullptr || gn == &inputTerminalMeta_ || gn == &outputTerminalMeta_) return;

    gn->bypassed = on;
    if (auto* node = graph_->getNodeForId (gn->nodeId))
        if (auto* w = dynamic_cast<BypassMuteWrapper*> (node->getProcessor()))
            w->setBypassedFlag (on);

    notifyTopologyChanged();
}

void GraphController::setNodeMuted (const juce::Uuid& uuid, bool on)
{
    auto* gn = getNode (uuid);
    if (gn == nullptr || gn == &inputTerminalMeta_ || gn == &outputTerminalMeta_) return;

    gn->muted = on;
    if (auto* node = graph_->getNodeForId (gn->nodeId))
        if (auto* w = dynamic_cast<BypassMuteWrapper*> (node->getProcessor()))
            w->setMutedFlag (on);

    notifyTopologyChanged();
}

//==============================================================================

bool GraphController::resolveEndpoint (const juce::Uuid& uuid,
                                       juce::AudioProcessorGraph::NodeID& outId) const
{
    if (auto* gn = getNode (uuid))
    {
        outId = gn->nodeId;
        return true;
    }
    return false;
}

bool GraphController::addConnection (const juce::Uuid& fromUuid, int fromCh,
                                     const juce::Uuid& toUuid,   int toCh)
{
    AGProc::NodeID fromId, toId;
    if (! resolveEndpoint (fromUuid, fromId)) return false;
    if (! resolveEndpoint (toUuid,   toId))   return false;

    AGProc::Connection c {{ fromId, fromCh }, { toId, toCh }};
    if (! graph_->canConnect (c)) return false;

    if (graph_->addConnection (c))
    {
        notifyTopologyChanged();
        return true;
    }
    return false;
}

bool GraphController::removeConnection (const juce::Uuid& fromUuid, int fromCh,
                                        const juce::Uuid& toUuid,   int toCh)
{
    AGProc::NodeID fromId, toId;
    if (! resolveEndpoint (fromUuid, fromId)) return false;
    if (! resolveEndpoint (toUuid,   toId))   return false;

    if (graph_->removeConnection ({{ fromId, fromCh }, { toId, toCh }}))
    {
        notifyTopologyChanged();
        return true;
    }
    return false;
}

bool GraphController::isConnected (const juce::Uuid& fromUuid, int fromCh,
                                   const juce::Uuid& toUuid,   int toCh) const
{
    AGProc::NodeID fromId, toId;
    if (! resolveEndpoint (fromUuid, fromId)) return false;
    if (! resolveEndpoint (toUuid,   toId))   return false;

    return graph_->isConnected ({{ fromId, fromCh }, { toId, toCh }});
}

std::vector<GraphController::ConnectionInfo> GraphController::getAllConnections() const
{
    std::vector<ConnectionInfo> out;
    for (const auto& c : graph_->getConnections())
    {
        auto* src = getNodeByNodeId (c.source.nodeID);
        auto* dst = getNodeByNodeId (c.destination.nodeID);
        if (src == nullptr || dst == nullptr) continue;

        out.push_back ({ src->uuid, c.source.channelIndex,
                         dst->uuid, c.destination.channelIndex });
    }
    return out;
}

void GraphController::clearAllConnections()
{
    for (const auto& c : graph_->getConnections())
        graph_->removeConnection (c);
    notifyTopologyChanged();
}

//==============================================================================

void GraphController::notifyTopologyChanged()
{
    if (topologyListener_)
        topologyListener_();
}
