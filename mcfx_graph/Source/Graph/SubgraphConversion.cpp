#include "SubgraphConversion.h"
#include "GraphSerializer.h"
#include "SubgraphNode.h"
#include <algorithm>
#include <limits>
#include <map>
#include <set>

namespace
{
    using ConnectionInfo = GraphController::ConnectionInfo;

    /** One end of a wire: (node, channel). */
    struct Port
    {
        juce::Uuid uuid;
        int        channel = 0;

        bool operator< (const Port& o) const noexcept
        {
            if (uuid != o.uuid) return uuid < o.uuid;
            return channel < o.channel;
        }
        bool operator== (const Port& o) const noexcept
        {
            return uuid == o.uuid && channel == o.channel;
        }
    };

    // Where the inner I/O terminals go, relative to the moved nodes' bounding
    // box (world coordinates; a node tile is roughly 200 px wide).
    constexpr int kInputTerminalGap  = 260;
    constexpr int kOutputTerminalGap = 320;

    bool isTerminal (const GraphNode& gn)
    {
        return gn.kind == NodeKind::InputTerminal || gn.kind == NodeKind::OutputTerminal;
    }

    /** The user nodes named in `selection`, without duplicates or terminals. */
    std::vector<GraphNode*> collectNodes (const GraphController& controller,
                                          const std::vector<juce::Uuid>& selection)
    {
        std::vector<GraphNode*> nodes;
        std::set<juce::Uuid> seen;
        for (const auto& uuid : selection)
        {
            auto* gn = controller.getNode (uuid);
            if (gn == nullptr || isTerminal (*gn)) continue;
            if (seen.insert (uuid).second)
                nodes.push_back (gn);
        }
        return nodes;
    }

    /** Order ports top-to-bottom by their node's position on the canvas, then
        by channel, so the subgraph's pins come out in the order the wires
        arrived in. */
    void sortPortsByLayout (std::vector<Port>& ports, const GraphController& controller)
    {
        std::sort (ports.begin(), ports.end(), [&controller] (const Port& a, const Port& b)
        {
            const auto* na = controller.getNode (a.uuid);
            const auto* nb = controller.getNode (b.uuid);
            const auto pa = na != nullptr ? na->editorPosition : juce::Point<int>();
            const auto pb = nb != nullptr ? nb->editorPosition : juce::Point<int>();
            if (pa.y != pb.y) return pa.y < pb.y;
            if (pa.x != pb.x) return pa.x < pb.x;
            if (a.uuid != b.uuid) return a.uuid < b.uuid;
            return a.channel < b.channel;
        });
    }

    juce::var makeEnd (const juce::String& uuid, int channel)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("uuid",    uuid);
        obj->setProperty ("channel", channel);
        return juce::var (obj);
    }

    juce::var makeConnection (const juce::String& fromUuid, int fromCh,
                              const juce::String& toUuid,   int toCh)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("from", makeEnd (fromUuid, fromCh));
        obj->setProperty ("to",   makeEnd (toUuid,   toCh));
        return juce::var (obj);
    }

    juce::var makePosition (juce::Point<int> p)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("x", p.x);
        obj->setProperty ("y", p.y);
        return juce::var (obj);
    }
}

//==============================================================================

juce::String SubgraphConversion::describeRefusal (const GraphController& controller,
                                                  const std::vector<juce::Uuid>& selection)
{
    const auto nodes = collectNodes (controller, selection);
    if (nodes.empty())
        return "Select at least one node (the Input and Output terminals can't be moved).";

    std::set<juce::Uuid> inside;
    for (auto* gn : nodes) inside.insert (gn->uuid);

    for (const auto& link : controller.getAllFeedbackLinks())
    {
        const bool sendIn   = inside.count (link.sendUuid)   > 0;
        const bool returnIn = inside.count (link.returnUuid) > 0;
        if (sendIn != returnIn)
            return "A feedback send and its return can't be split between the subgraph "
                   "and the outside. Select both nodes of the pair, or neither.";
    }

    return {};
}

SubgraphConversion::Result SubgraphConversion::convert (
    GraphController& controller,
    const std::vector<juce::Uuid>& selection,
    juce::AudioPluginFormatManager& formatManager,
    const juce::KnownPluginList* knownPluginList)
{
    Result result;

    if (auto refusal = describeRefusal (controller, selection); refusal.isNotEmpty())
    {
        result.error = refusal;
        return result;
    }

    const auto nodes = collectNodes (controller, selection);
    std::set<juce::Uuid> inside;
    for (auto* gn : nodes) inside.insert (gn->uuid);

    //--------------------------------------------------------------------------
    // Classify every wire against the boundary.
    std::vector<ConnectionInfo> internal, incoming, outgoing;
    for (const auto& c : controller.getAllConnections())
    {
        const bool fromIn = inside.count (c.fromUuid) > 0;
        const bool toIn   = inside.count (c.toUuid)   > 0;
        if (fromIn && toIn)       internal.push_back (c);
        else if (toIn)            incoming.push_back (c);
        else if (fromIn)          outgoing.push_back (c);
    }

    // One subgraph input per distinct outside source, one output per distinct
    // inside source.
    std::vector<Port> inputs, outputs;
    for (const auto& c : incoming)
    {
        const Port p { c.fromUuid, c.fromCh };
        if (std::find (inputs.begin(), inputs.end(), p) == inputs.end())
            inputs.push_back (p);
    }
    for (const auto& c : outgoing)
    {
        const Port p { c.fromUuid, c.fromCh };
        if (std::find (outputs.begin(), outputs.end(), p) == outputs.end())
            outputs.push_back (p);
    }
    sortPortsByLayout (inputs,  controller);
    sortPortsByLayout (outputs, controller);

    std::map<Port, int> inputIndex, outputIndex;
    for (int i = 0; i < (int) inputs.size();  ++i) inputIndex[inputs[(std::size_t) i]]   = i;
    for (int i = 0; i < (int) outputs.size(); ++i) outputIndex[outputs[(std::size_t) i]] = i;

    // SubgraphNode needs at least one channel each way; an unused spare pin
    // is harmless.
    const int numIn  = juce::jmax (1, (int) inputs.size());
    const int numOut = juce::jmax (1, (int) outputs.size());

    //--------------------------------------------------------------------------
    // The inner graph, in the saved-graph format. The nodes keep their UUIDs
    // and positions, so the subgraph opens looking like the part of the canvas
    // it came from.
    int minX = std::numeric_limits<int>::max(), minY = minX;
    int maxX = std::numeric_limits<int>::min(), maxY = maxX;
    for (auto* gn : nodes)
    {
        minX = juce::jmin (minX, gn->editorPosition.x);
        minY = juce::jmin (minY, gn->editorPosition.y);
        maxX = juce::jmax (maxX, gn->editorPosition.x);
        maxY = juce::jmax (maxY, gn->editorPosition.y);
    }
    const int midY = (minY + maxY) / 2;

    juce::Array<juce::var> nodesArr, connsArr, fbArr;
    for (auto* gn : nodes)
    {
        auto v = GraphSerializer::nodeVarFromGraphNode (*gn);
        if (v.isVoid())
        {
            result.error = "Could not save \"" + gn->displayName + "\" for the move.";
            return result;
        }
        nodesArr.add (v);
    }

    for (const auto& c : internal)
        connsArr.add (makeConnection (c.fromUuid.toString(), c.fromCh,
                                      c.toUuid.toString(),   c.toCh));
    for (const auto& c : incoming)
        connsArr.add (makeConnection (GraphSerializer::kInputSentinel,
                                      inputIndex[{ c.fromUuid, c.fromCh }],
                                      c.toUuid.toString(), c.toCh));
    // Several outside pins fed from one inside source share an output: wire
    // each output inside only once.
    for (const auto& src : outputs)
        connsArr.add (makeConnection (src.uuid.toString(), src.channel,
                                      GraphSerializer::kOutputSentinel, outputIndex[src]));

    for (const auto& link : controller.getAllFeedbackLinks())
    {
        if (inside.count (link.sendUuid) == 0) continue;   // pairs move whole (checked above)
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("send",   link.sendUuid.toString());
        obj->setProperty ("return", link.returnUuid.toString());
        fbArr.add (juce::var (obj));
    }

    auto* innerObj = new juce::DynamicObject();
    innerObj->setProperty ("nodes",       nodesArr);
    innerObj->setProperty ("connections", connsArr);
    if (! fbArr.isEmpty())
        innerObj->setProperty ("feedbackLinks", fbArr);
    innerObj->setProperty ("inputTerminalPosition",
                           makePosition ({ minX - kInputTerminalGap, midY }));
    innerObj->setProperty ("outputTerminalPosition",
                           makePosition ({ maxX + kOutputTerminalGap, midY }));
    const juce::var innerVar (innerObj);

    //--------------------------------------------------------------------------
    // Build the subgraph off to the side. Same steps as loading a saved
    // subgraph: size the inner terminals first, or every wire to them fails.
    auto sub = std::make_unique<SubgraphNode> (numIn, numOut);
    sub->getInner().prepareToPlay (48000.0, 512, numIn, numOut);

    juce::String loadError;
    GraphSerializer::graphFromVar (innerVar, sub->getInner(), formatManager,
                                   knownPluginList, &loadError);

    if (sub->getInner().getAllUserNodes().size() != (int) nodes.size())
    {
        result.error = "Not every node could be rebuilt inside the subgraph "
                       "(is a plug-in missing?), so nothing was changed.";
        if (loadError.isNotEmpty())
            result.error << "\n\n" << loadError;
        return result;
    }

    //--------------------------------------------------------------------------
    // Swap it in: add the subgraph, drop the originals (which takes their
    // wires with them), then reconnect across the boundary.
    const auto subUuid = controller.addNode (std::move (sub), NodeKind::Subgraph, "Subgraph",
                                             numIn, numOut, { minX, minY });
    if (subUuid.isNull())
    {
        result.error = "Could not add the subgraph node.";
        return result;
    }

    for (auto* gn : nodes)
        controller.removeNode (gn->uuid);   // gn is gone after this

    for (int i = 0; i < (int) inputs.size(); ++i)
        controller.addConnection (inputs[(std::size_t) i].uuid, inputs[(std::size_t) i].channel,
                                  subUuid, i);

    for (const auto& c : outgoing)
        controller.addConnection (subUuid, outputIndex[{ c.fromUuid, c.fromCh }],
                                  c.toUuid, c.toCh);

    result.subgraphUuid = subUuid;
    return result;
}
