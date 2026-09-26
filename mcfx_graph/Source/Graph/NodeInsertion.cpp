#include "NodeInsertion.h"
#include <algorithm>

namespace
{
    using ConnectionInfo = GraphController::ConnectionInfo;

    /** The wires from `from` to `to`, by destination channel, then source. */
    std::vector<ConnectionInfo> wiresBetween (const GraphController& controller,
                                              const juce::Uuid& from,
                                              const juce::Uuid& to)
    {
        std::vector<ConnectionInfo> wires;
        for (const auto& c : controller.getAllConnections())
            if (c.fromUuid == from && c.toUuid == to)
                wires.push_back (c);

        std::sort (wires.begin(), wires.end(), [] (const ConnectionInfo& a, const ConnectionInfo& b)
        {
            if (a.toCh != b.toCh) return a.toCh < b.toCh;
            return a.fromCh < b.fromCh;
        });
        return wires;
    }
}

juce::String NodeInsertion::describeRefusal (const GraphController& controller,
                                             const juce::Uuid& node,
                                             const juce::Uuid& from,
                                             const juce::Uuid& to)
{
    auto* gn = controller.getNode (node);
    if (gn == nullptr)
        return "Unknown node.";
    if (gn->kind == NodeKind::InputTerminal || gn->kind == NodeKind::OutputTerminal)
        return "The Input and Output terminals can't be inserted.";
    if (node == from || node == to)
        return "A node can't be inserted into its own wire.";
    if (gn->channelCountIn <= 0 || gn->channelCountOut <= 0)
        return "Only a node with both inputs and outputs can be inserted.";

    for (const auto& c : controller.getAllConnections())
        if (c.fromUuid == node || c.toUuid == node)
            return "Only a node that isn't wired yet can be inserted.";

    GraphController::FeedbackLinkInfo link;
    if (controller.findFeedbackLinkFor (node, link))
        return "A linked feedback node can't be inserted.";

    if (wiresBetween (controller, from, to).empty())
        return "There is no wire there.";

    return {};
}

int NodeInsertion::insert (GraphController& controller,
                           const juce::Uuid& node,
                           const juce::Uuid& from,
                           const juce::Uuid& to)
{
    if (describeRefusal (controller, node, from, to).isNotEmpty())
        return 0;

    auto* gn = controller.getNode (node);
    const auto wires = wiresBetween (controller, from, to);
    const int n = juce::jmin ((int) wires.size(), gn->channelCountIn, gn->channelCountOut);

    // Check every new wire is legal before removing anything, so a refusal
    // from the graph (e.g. a channel past a plug-in's real bus) can't leave
    // the chain half-rewired.
    for (int j = 0; j < n; ++j)
    {
        const auto& w = wires[(std::size_t) j];
        if (! controller.canConnect (w.fromUuid, w.fromCh, node, j)
            || ! controller.canConnect (node, j, w.toUuid, w.toCh))
            return 0;
    }

    for (int j = 0; j < n; ++j)
    {
        const auto& w = wires[(std::size_t) j];
        controller.removeConnection (w.fromUuid, w.fromCh, w.toUuid, w.toCh);
        controller.addConnection    (w.fromUuid, w.fromCh, node, j);
        controller.addConnection    (node, j, w.toUuid, w.toCh);
    }
    return n;
}
