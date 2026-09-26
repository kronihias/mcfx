/*
  ==============================================================================

   SubgraphConversion — "Convert to subgraph": move a set of nodes into a new
   SubgraphNode in the same graph, keeping every connection intact.

   Connections inside the set move with the nodes. A connection that crosses
   the boundary is split at the subgraph: each distinct outside source feeding
   the set becomes one subgraph input, and each distinct inside source feeding
   the outside becomes one subgraph output. A source that fans out to several
   pins across the boundary therefore uses a single port, and the audio at
   every pin is unchanged.

   Feedback pairs move only as a pair: a send/return link can't reach across
   the subgraph boundary (each GraphController owns its own links), so a
   selection that splits one is refused.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GraphController.h"
#include <vector>

namespace SubgraphConversion
{
    /** Why `selection` can't become a subgraph, or empty if it can. Terminals
        and unknown UUIDs in the selection are ignored. */
    juce::String describeRefusal (const GraphController& controller,
                                  const std::vector<juce::Uuid>& selection);

    struct Result
    {
        // Explicitly null: juce::Uuid's default constructor makes a random one.
        juce::Uuid   subgraphUuid = juce::Uuid::null();   // null on failure
        juce::String error;          // set on failure

        bool succeeded() const noexcept { return ! subgraphUuid.isNull(); }
    };

    /** Replace the selected nodes with one SubgraphNode holding them.

        The inner graph is built completely before the outer one is touched,
        from the same per-node serialization copy / paste and undo use. If any
        node can't be rebuilt (e.g. a plug-in that no longer loads), nothing is
        changed and the error says so. Hosted plug-ins are re-instantiated from
        their saved state, so their editor windows close.

        Message thread only. The caller should hold a ScopedSuspend on the
        outer plug-in across the call. */
    Result convert (GraphController& controller,
                    const std::vector<juce::Uuid>& selection,
                    juce::AudioPluginFormatManager& formatManager,
                    const juce::KnownPluginList* knownPluginList);
}
