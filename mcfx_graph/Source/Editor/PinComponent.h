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

    /** Channel index reserved for the feedback-link pin carried by
        FeedbackSend / FeedbackReturn nodes. It is not an audio channel: a link
        pairs the two nodes so they can share a bus, and must never become a
        juce::AudioProcessorGraph connection (that would close a cycle the
        graph cannot render). Using a sentinel channel keeps the pin in the
        existing input/output lists, so hit-testing, hover and drag all work
        unchanged — only the drop handler branches. */
    static constexpr int kLinkChannel = -1;

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
    bool       isLink()   const noexcept { return channelIndex_ == kLinkChannel; }
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
