/*
  ==============================================================================

   NodeInsertion — drop a node onto a wire to put it into the signal chain
   (Option / Alt + drag in the editor).

   The node goes into every wire between the same two nodes, not just the one
   under the cursor, so a multichannel link is rerouted as a whole: the j-th
   wire, in order of destination channel, becomes
       from.ch -> node input j,   node output j -> to.ch
   Wires beyond the node's channel count stay direct.

   Only a node with no wires of its own can be inserted. That keeps the result
   obvious, and a node with nothing attached can't close a cycle.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GraphController.h"

namespace NodeInsertion
{
    /** Why `node` can't go into the wires from `from` to `to`, or empty if it
        can. */
    juce::String describeRefusal (const GraphController& controller,
                                  const juce::Uuid& node,
                                  const juce::Uuid& from,
                                  const juce::Uuid& to);

    /** Put `node` into the wires from `from` to `to`. Returns how many wires
        now run through it; 0 means nothing changed. Message thread only. */
    int insert (GraphController& controller,
                const juce::Uuid& node,
                const juce::Uuid& from,
                const juce::Uuid& to);
}
