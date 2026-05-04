/*
  ==============================================================================

   GraphClipboard — copy/paste a selection of nodes (with internal connections
   preserved) via the system clipboard.

   The clipboard payload is a small JSON envelope that reuses the same per-node
   schema as GraphSerializer, so a copied node round-trips with full state
   (plugin parameters, native-node data, bypass/mute, subgraph contents).

   - Copy: serialize selected non-terminal nodes plus the connections whose
     BOTH endpoints are in the selection. External wires (touching a node not
     in the selection, including I/O terminals) are dropped.

   - Paste: deserialize into a target controller with FRESH UUIDs; pasted nodes
     are offset to a target world position. Returns the new UUIDs so the caller
     can re-select them.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GraphController.h"
#include <optional>

namespace GraphClipboard
{
    /** Serialize the selection to the system clipboard.
        Returns true iff at least one non-terminal node was copied. */
    bool copySelection (const GraphController& controller,
                        const std::vector<juce::Uuid>& selectedNodes);

    /** Restore from the system clipboard into the active controller, using
        fresh UUIDs and offsetting positions so the bounding-box top-left lands
        at pasteOriginWorld (or anchor + (30, 30) if not provided).

        knownPluginList is used to resolve plugin nodes via their cross-machine
        identifier; pass null to skip the lookup (only useful inside one host
        instance).

        Returns the UUIDs of the newly-created nodes (empty on failure or
        wrong-format clipboard). */
    std::vector<juce::Uuid> pasteFromClipboard (
        GraphController& controller,
        juce::AudioPluginFormatManager& formatManager,
        const juce::KnownPluginList* knownPluginList = nullptr,
        std::optional<juce::Point<int>> pasteOriginWorld = std::nullopt);

    /** True when the system clipboard currently holds an mcfx_graph payload —
        used to enable / disable the Paste menu item without consuming the
        clipboard. */
    bool clipboardHasGraphData();
}
