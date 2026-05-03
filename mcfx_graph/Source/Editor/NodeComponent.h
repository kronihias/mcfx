/*
  ==============================================================================

   NodeComponent — visual representation of one node in the graph. Owns
   PinComponents for input and output channels. Draggable; right-click for
   context menu; double-click opens plugin editor or descends into subgraph.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PinComponent.h"
#include "../Graph/GraphNode.h"

class GraphEditorComponent;
class GraphController;

class NodeComponent : public juce::Component
{
public:
    NodeComponent (GraphEditorComponent& editor, GraphNode& node);

    void rebuildPins();

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    GraphNode&       getNode()       noexcept { return node_; }
    const GraphNode& getNode() const noexcept { return node_; }

    PinComponent* findPin (PinComponent::Direction dir, int channelIndex) const;

private:
    void showContextMenu();
    void openPluginEditor();
    void openNativePropertiesWindow();
    void descendIntoSubgraph();
    void showChangeChannelCountPopup();
    void changeNativeNodeChannelCount (int newChIn, int newChOut);
    void changePluginNodeMainBusChannels (int newMainCh);

    GraphEditorComponent& editor_;
    GraphNode& node_;

    juce::OwnedArray<PinComponent> inputPins_;
    juce::OwnedArray<PinComponent> outputPins_;

    juce::ComponentDragger dragger_;

    // Drag tracking — used to commit an undo snapshot on mouseUp only when
    // a click actually moved the node, so plain selection clicks don't
    // pollute the undo history.
    juce::Point<int> dragStartPosition_;
    bool             didDrag_ = false;
};
