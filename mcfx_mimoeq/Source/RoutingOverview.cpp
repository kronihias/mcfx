/*
 ==============================================================================

 Routing overview component for mcfx_mimoeq.

 ==============================================================================
 */

#include "RoutingOverview.h"

RoutingOverviewComponent::RoutingOverviewComponent(const std::vector<PathKey>& paths,
                                                     int numChannels,
                                                     bool hasDiagonalBands,
                                                     Listener* listener)
    : paths_(paths),
      numChannels_(numChannels),
      hasDiagonalBands_(hasDiagonalBands),
      listener_(listener)
{
    setSize(kWidth, getRecommendedHeight(numChannels));
    setMouseCursor(MouseCursor::PointingHandCursor);
}

Rectangle<float> RoutingOverviewComponent::getInputPortRect(int ch) const
{
    float y = (float)kMarginTop + (float)(ch - 1) * 28.f;
    return { (float)kMarginX, y, (float)kPortW, (float)kPortH };
}

Rectangle<float> RoutingOverviewComponent::getOutputPortRect(int ch) const
{
    float y = (float)kMarginTop + (float)(ch - 1) * 28.f;
    float x = (float)getWidth() - (float)kMarginX - (float)kPortW;
    return { x, y, (float)kPortW, (float)kPortH };
}

Point<float> RoutingOverviewComponent::getInputPortRight(int ch) const
{
    auto r = getInputPortRect(ch);
    return { r.getRight(), r.getCentreY() };
}

Point<float> RoutingOverviewComponent::getOutputPortLeft(int ch) const
{
    auto r = getOutputPortRect(ch);
    return { r.getX(), r.getCentreY() };
}

Path RoutingOverviewComponent::makeWirePath(int inCh, int outCh) const
{
    auto start = getInputPortRight(inCh);
    auto end   = getOutputPortLeft(outCh);
    float dx = (end.x - start.x) * 0.4f;

    Path p;
    p.startNewSubPath(start);
    p.cubicTo(start.x + dx, start.y,
              end.x - dx,   end.y,
              end.x,        end.y);
    return p;
}

int RoutingOverviewComponent::hitTestWire(Point<float> pt) const
{
    float bestDist = 7.f; // threshold in pixels
    int bestIdx = -1;

    for (int i = 0; i < (int)paths_.size(); ++i)
    {
        auto wire = makeWirePath(paths_[i].first, paths_[i].second);
        // Flatten to line segments and find closest distance
        PathFlatteningIterator it(wire, AffineTransform(), 2.f);
        while (it.next())
        {
            // Distance from point to line segment (it.x1,y1)-(it.x2,y2)
            float ax = it.x2 - it.x1, ay = it.y2 - it.y1;
            float bx = pt.x - it.x1,  by = pt.y - it.y1;
            float len2 = ax * ax + ay * ay;
            float t = (len2 > 0.f) ? jlimit(0.f, 1.f, (bx * ax + by * ay) / len2) : 0.f;
            float dx = bx - t * ax, dy = by - t * ay;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestIdx = i;
            }
        }
    }
    return bestIdx;
}

void RoutingOverviewComponent::paint(Graphics& g)
{
    g.fillAll(Colour(0xff1e1e1e));

    // Title
    g.setColour(Colours::white.withAlpha(0.9f));
    g.setFont(Font(13.f, Font::bold));
    g.drawText("Routing Overview", getLocalBounds().removeFromTop(28),
               Justification::centred, false);

    // Diagonal indicator
    if (hasDiagonalBands_)
    {
        g.setColour(Colours::aquamarine.withAlpha(0.7f));
        g.setFont(Font(10.f, Font::italic));
        g.drawText("Diagonal EQ active on all channels",
                   4, 16, getWidth() - 8, 16, Justification::centred, false);
    }

    // Draw port labels
    g.setFont(Font(12.f, Font::plain));
    for (int ch = 1; ch <= numChannels_; ++ch)
    {
        auto inRect  = getInputPortRect(ch);
        auto outRect = getOutputPortRect(ch);

        // Input port
        g.setColour(Colour(0xff3a3a3a));
        g.fillRoundedRectangle(inRect, 4.f);
        g.setColour(Colours::aquamarine.withAlpha(0.8f));
        g.drawRoundedRectangle(inRect, 4.f, 1.f);
        g.setColour(Colours::white.withAlpha(0.9f));
        g.drawText(String(ch), inRect, Justification::centred, false);

        // Output port
        g.setColour(Colour(0xff3a3a3a));
        g.fillRoundedRectangle(outRect, 4.f);
        g.setColour(Colours::orange.withAlpha(0.8f));
        g.drawRoundedRectangle(outRect, 4.f, 1.f);
        g.setColour(Colours::white.withAlpha(0.9f));
        g.drawText(String(ch), outRect, Justification::centred, false);
    }

    // Column labels
    g.setFont(Font(10.f, Font::plain));
    g.setColour(Colours::aquamarine.withAlpha(0.6f));
    g.drawText("IN", kMarginX, kMarginTop - 14, kPortW, 12, Justification::centred, false);
    g.setColour(Colours::orange.withAlpha(0.6f));
    float outX = (float)getWidth() - (float)kMarginX - (float)kPortW;
    g.drawText("OUT", (int)outX, kMarginTop - 14, kPortW, 12, Justification::centred, false);

    // Draw wires
    for (int i = 0; i < (int)paths_.size(); ++i)
    {
        auto wire = makeWirePath(paths_[i].first, paths_[i].second);
        bool isDiag = (paths_[i].first == paths_[i].second);
        bool isHovered = (i == hoveredWire_);

        Colour wireCol = isDiag ? Colours::aquamarine : Colours::orange;
        float alpha = isHovered ? 1.0f : 0.6f;
        float thickness = isHovered ? 3.0f : 1.8f;

        g.setColour(wireCol.withAlpha(alpha));
        g.strokePath(wire, PathStrokeType(thickness));
    }
}

void RoutingOverviewComponent::mouseMove(const MouseEvent& e)
{
    int newHovered = hitTestWire(e.position);
    if (newHovered != hoveredWire_)
    {
        hoveredWire_ = newHovered;
        repaint();
    }
}

void RoutingOverviewComponent::mouseDown(const MouseEvent& e)
{
    int idx = hitTestWire(e.position);
    if (idx >= 0 && idx < (int)paths_.size() && listener_ != nullptr)
    {
        listener_->routingPathSelected(paths_[idx].first, paths_[idx].second);

        // Close the callout box by finding the parent CallOutBox
        if (auto* callout = findParentComponentOfClass<CallOutBox>())
            callout->dismiss();
    }
}
