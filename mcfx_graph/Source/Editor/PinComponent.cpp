#include "PinComponent.h"
#include "GraphEditorComponent.h"

PinComponent::PinComponent (GraphEditorComponent& editor,
                            const juce::Uuid& nodeUuid,
                            Direction dir,
                            int channelIndex)
    : editor_ (editor),
      nodeUuid_ (nodeUuid),
      dir_ (dir),
      channelIndex_ (channelIndex)
{
    setSize (12, 12);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setInterceptsMouseClicks (true, false);
}

void PinComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (1.0f);

    const auto baseColour = (dir_ == Direction::Input ? juce::Colours::lightblue
                                                      : juce::Colours::orange);

    if (dragTargetHighlight_)
    {
        // Outer glow when the pin is the candidate target of an in-progress drag
        g.setColour (juce::Colours::yellow.withAlpha (0.45f));
        g.fillEllipse (getLocalBounds().toFloat().expanded (3.0f));
    }

    const bool lit = hovered_ || dragTargetHighlight_;

    if (isLink())
    {
        // A link pin is not an audio pin, and must not look like one or users
        // will try to wire it to a channel. Diamond, in the feedback colour.
        const auto c = area.getCentre();
        juce::Path diamond;
        diamond.startNewSubPath (c.x, area.getY());
        diamond.lineTo (area.getRight(), c.y);
        diamond.lineTo (c.x, area.getBottom());
        diamond.lineTo (area.getX(), c.y);
        diamond.closeSubPath();

        const auto linkColour = juce::Colour (0xff9b6bff);
        g.setColour (lit ? linkColour.brighter (0.5f) : linkColour);
        g.fillPath (diamond);
        g.setColour (lit ? juce::Colours::white : juce::Colours::black);
        g.strokePath (diamond, juce::PathStrokeType (lit ? 1.5f : 1.0f));
        return;
    }

    g.setColour (lit ? baseColour.brighter (0.5f) : baseColour);
    g.fillEllipse (area);
    g.setColour (lit ? juce::Colours::white : juce::Colours::black);
    g.drawEllipse (area, lit ? 1.5f : 1.0f);
}

juce::Point<int> PinComponent::getCenterInGraphCoords() const
{
    return editor_.getLocalPoint (this, getLocalBounds().getCentre());
}

void PinComponent::setDragTargetHighlight (bool on)
{
    if (dragTargetHighlight_ != on)
    {
        dragTargetHighlight_ = on;
        repaint();
    }
}

void PinComponent::mouseEnter (const juce::MouseEvent&)
{
    hovered_ = true;
    repaint();
}

void PinComponent::mouseExit (const juce::MouseEvent&)
{
    hovered_ = false;
    repaint();
}

void PinComponent::mouseDown (const juce::MouseEvent&)
{
    editor_.beginPinDrag (*this);
}

void PinComponent::mouseDrag (const juce::MouseEvent& e)
{
    editor_.updatePinDrag (editor_.getLocalPoint (this, e.getPosition()));
}

void PinComponent::mouseUp (const juce::MouseEvent& e)
{
    editor_.endPinDrag (editor_.getLocalPoint (this, e.getPosition()),
                        e.mods.isShiftDown());
}
