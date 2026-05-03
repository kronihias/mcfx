/*
  ==============================================================================

   GraphSerializer — round-trip a GraphController to/from juce::var (JSON).

   Hosted-plugin nodes are loaded synchronously through an
   AudioPluginFormatManager passed by the caller. If a plugin description
   can't be resolved, the node is silently skipped and connections referring
   to it are also skipped — the rest of the graph still loads.

   The IO terminals are referenced via the sentinel UUIDs "__input__" and
   "__output__" in the JSON, since the controller's terminal UUIDs are
   regenerated each session.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GraphController.h"

class GraphSerializer
{
public:
    static constexpr int kCurrentVersion = 1;

    static constexpr const char* kInputSentinel  = "__input__";
    static constexpr const char* kOutputSentinel = "__output__";

    /** Serialize a controller's user nodes + connections to a juce::var. */
    static juce::var graphToVar (const GraphController& controller);

    /** Top-level: wraps graphToVar in a root object with version and rootGraph. */
    static juce::var topLevelToVar (const GraphController& controller);

    /** Restore. The format manager is used to resolve hosted-plugin descriptions
        on Plugin nodes. Returns true if at least the structure was parsed
        (individual plugin nodes that fail to load are silently skipped). */
    static bool topLevelFromVar (const juce::var& v,
                                 GraphController& controller,
                                 juce::AudioPluginFormatManager& formatManager,
                                 juce::String* errorOut = nullptr);

    /** Like topLevelFromVar but takes an already-extracted rootGraph object
        (used recursively for subgraphs). */
    static bool graphFromVar (const juce::var& v,
                              GraphController& controller,
                              juce::AudioPluginFormatManager& formatManager,
                              juce::String* errorOut = nullptr);

    // Sentinel <-> Uuid helpers — public so the in-place undo fast path in
    // PluginProcessor can re-resolve connection endpoints using the same
    // mapping the regular load path uses.
    static juce::String uuidOrSentinel   (const juce::Uuid& u, const GraphController& c);
    static juce::Uuid   uuidFromSentinel (const juce::String& s, const GraphController& c);
};
