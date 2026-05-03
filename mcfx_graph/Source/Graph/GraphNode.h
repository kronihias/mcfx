/*
  ==============================================================================

   GraphNode — metadata wrapper around one node in the graph. Tracks the
   stable cross-session UUID, the ephemeral juce::AudioProcessorGraph::NodeID,
   the editor position, the channel configuration, and (for hosted-plugin
   nodes) the cached PluginDescription.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>

enum class NodeKind
{
    Plugin,
    Gain,
    MutePhase,
    MatrixMixer,
    Delay,
    Subgraph,
    InputTerminal,   // graph IO — references AudioGraphIOProcessor (audioInputNode)
    OutputTerminal   // graph IO — references AudioGraphIOProcessor (audioOutputNode)
};

inline juce::String nodeKindToString (NodeKind k)
{
    switch (k)
    {
        case NodeKind::Plugin:         return "plugin";
        case NodeKind::Gain:           return "gain";
        case NodeKind::MutePhase:      return "mute_phase";
        case NodeKind::MatrixMixer:    return "matrix_mixer";
        case NodeKind::Delay:          return "delay";
        case NodeKind::Subgraph:       return "subgraph";
        case NodeKind::InputTerminal:  return "input_terminal";
        case NodeKind::OutputTerminal: return "output_terminal";
    }
    return "unknown";
}

inline NodeKind nodeKindFromString (const juce::String& s)
{
    if (s == "plugin")          return NodeKind::Plugin;
    if (s == "gain")            return NodeKind::Gain;
    if (s == "mute_phase")      return NodeKind::MutePhase;
    if (s == "matrix_mixer")    return NodeKind::MatrixMixer;
    if (s == "delay")           return NodeKind::Delay;
    if (s == "subgraph")        return NodeKind::Subgraph;
    if (s == "input_terminal")  return NodeKind::InputTerminal;
    if (s == "output_terminal") return NodeKind::OutputTerminal;
    return NodeKind::Plugin;
}

struct GraphNode
{
    juce::Uuid uuid;
    juce::AudioProcessorGraph::NodeID nodeId;
    juce::AudioProcessor* processor = nullptr; // owned by the graph
    NodeKind kind = NodeKind::Plugin;

    int channelCountIn = 0;
    int channelCountOut = 0;

    juce::Point<int> editorPosition { 100, 100 };
    juce::String displayName;

    // Per-node toggles. `bypassed` routes the node's input to its output
    // (channel-by-channel up to the smaller side), skipping processing.
    // `muted` zeroes the node's output. Both are honoured by the AudioProcessorGraph
    // engine via the Node's bypass switch + a follow-up clear pass for mute.
    bool bypassed = false;
    bool muted    = false;

    // Plugin-specific. Held only for nodes of kind Plugin so we can serialize
    // the PluginDescription back into the JSON.
    std::unique_ptr<juce::PluginDescription> pluginDescription;
};
