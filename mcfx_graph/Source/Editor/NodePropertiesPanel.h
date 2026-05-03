/*
  ==============================================================================

   NodePropertiesPanel — side panel that shows editable properties for the
   currently-selected node. Different sub-panels are built per node kind.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Graph/GraphNode.h"

class GraphEditorComponent;

class NodePropertiesPanel : public juce::Component
{
public:
    explicit NodePropertiesPanel (GraphEditorComponent& editor);
    ~NodePropertiesPanel() override;

    void setSelectedNode (GraphNode* node);

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Build the per-kind body component for `node`. Used both by the side
        panel (embedded in a viewport) and by the floating parameter window
        opened on a native node's double-click. Returns nullptr for terminals
        and for nodes whose processor is missing. */
    static std::unique_ptr<juce::Component>
    createBodyForNode (GraphEditorComponent& editor, GraphNode& node);

private:
    void rebuildContent();

    GraphEditorComponent& editor_;
    GraphNode* selectedNode_ = nullptr;

    juce::Label titleLabel_;
    juce::Label subtitleLabel_;

    std::unique_ptr<juce::Component> body_;       // per-kind content (in viewport)
    std::unique_ptr<juce::Viewport>  viewport_;
};
