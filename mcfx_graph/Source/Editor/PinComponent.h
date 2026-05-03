/*
  ==============================================================================

   PinComponent — one input or output pin on a NodeComponent. Forwards mouse
   drags to its parent GraphEditorComponent so it can draw a temporary wire
   and connect to another pin on release.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class GraphEditorComponent;

class PinComponent : public juce::Component
{
public:
    enum class Direction { Input, Output };

    PinComponent (GraphEditorComponent& editor,
                  const juce::Uuid& nodeUuid,
                  Direction dir,
                  int channelIndex);

    void paint (juce::Graphics& g) override;
    void mouseDown  (const juce::MouseEvent& e) override;
    void mouseDrag  (const juce::MouseEvent& e) override;
    void mouseUp    (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit  (const juce::MouseEvent& e) override;

    /** Externally-driven highlight state, used during drag-to-connect to
        light up the candidate target pin under the cursor. */
    void setDragTargetHighlight (bool on);

    juce::Uuid getNodeUuid()    const noexcept { return nodeUuid_; }
    Direction  getDirection()   const noexcept { return dir_; }
    int        getChannelIndex() const noexcept { return channelIndex_; }
    bool       isInput()  const noexcept { return dir_ == Direction::Input; }
    bool       isOutput() const noexcept { return dir_ == Direction::Output; }

    juce::Point<int> getCenterInGraphCoords() const;

private:
    GraphEditorComponent& editor_;
    juce::Uuid nodeUuid_;
    Direction  dir_;
    int        channelIndex_;
    bool       hovered_ = false;
    bool       dragTargetHighlight_ = false;
};
