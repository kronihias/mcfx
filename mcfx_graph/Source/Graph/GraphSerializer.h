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

   Plugin nodes are saved using JUCE's location-independent identifier
   (PluginDescription::createIdentifierString()) plus a few diagnostic
   fields (format / name / manufacturer). On load, the identifier is
   resolved against the local KnownPluginList, so the same JSON works
   across machines as long as the same plugin is installed locally —
   no absolute file paths are stored.

   Older JSON files that still contain a "pluginDescriptionXml" payload
   are read back transparently for backward compatibility.

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

    /** Restore. The format manager is used to instantiate hosted plugins;
        the KnownPluginList (when provided) is used to resolve cross-machine
        plugin identifiers to a locally-installed file path. Returns true if
        at least the structure was parsed (individual plugin nodes that fail
        to load are silently skipped). */
    static bool topLevelFromVar (const juce::var& v,
                                 GraphController& controller,
                                 juce::AudioPluginFormatManager& formatManager,
                                 const juce::KnownPluginList* knownPluginList = nullptr,
                                 juce::String* errorOut = nullptr);

    /** Like topLevelFromVar but takes an already-extracted rootGraph object
        (used recursively for subgraphs). */
    static bool graphFromVar (const juce::var& v,
                              GraphController& controller,
                              juce::AudioPluginFormatManager& formatManager,
                              const juce::KnownPluginList* knownPluginList = nullptr,
                              juce::String* errorOut = nullptr);

    // Sentinel <-> Uuid helpers — public so the in-place undo fast path in
    // PluginProcessor can re-resolve connection endpoints using the same
    // mapping the regular load path uses.
    static juce::String uuidOrSentinel   (const juce::Uuid& u, const GraphController& c);
    static juce::Uuid   uuidFromSentinel (const juce::String& s, const GraphController& c);

    //==============================================================================
    // Per-node helpers — exposed so GraphClipboard (copy/paste) can produce JSON
    // entries with the same schema as graphToVar / graphFromVar.

    /** Serialize one user node to a JSON node-object (same schema as the entries
        inside graphToVar's "nodes" array). Returns an invalid var for terminals. */
    static juce::var nodeVarFromGraphNode (const GraphNode& gn);

    /** Construct an AudioProcessor (and optional PluginDescription for plugin
        nodes) from a node-object. Returns nullptr on failure (unknown plugin,
        invalid type, etc.). For plugin nodes, ownership of the description is
        returned via outPluginDesc.

        When knownPluginList is supplied, plugin nodes are resolved by their
        cross-machine identifier (or by name+format as a fallback) — this is
        what makes saved JSON portable across machines. When null, we fall
        back to whatever PluginDescription we can read from the JSON, which
        only works on the machine that wrote the file. */
    static std::unique_ptr<juce::AudioProcessor> buildProcessorFromNodeVar (
        const juce::var& nodeObj,
        juce::AudioPluginFormatManager& formatManager,
        std::unique_ptr<juce::PluginDescription>& outPluginDesc,
        const juce::KnownPluginList* knownPluginList = nullptr);
};
