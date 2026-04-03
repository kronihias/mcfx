/*
 ==============================================================================

 Routing overview component for mcfx_mimoeq.

 ==============================================================================
 */

#include "RoutingOverview.h"

RoutingOverviewComponent::RoutingOverviewComponent(const std::vector<PathKey>& paths,
                                                     const std::map<PathKey, int>& bandCounts,
                                                     int numChannels,
                                                     bool hasDiagonalBands,
                                                     Listener* listener)
    : paths_(paths),
      bandCounts_(bandCounts),
      numChannels_(numChannels),
      hasDiagonalBands_(hasDiagonalBands),
      listener_(listener)
{
    setSize(kWidth, getRecommendedHeight(numChannels));
    setMouseCursor(MouseCursor::PointingHandCursor);
}

void RoutingOverviewComponent::updatePaths(const std::vector<PathKey>& paths,
                                            const std::map<PathKey, int>& bandCounts)
{
    paths_ = paths;
    bandCounts_ = bandCounts;
    hoveredWire_ = -1;
    repaint();
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

    // Draw wires with band count dots
    for (int i = 0; i < (int)paths_.size(); ++i)
    {
        auto wire = makeWirePath(paths_[i].first, paths_[i].second);
        bool isDiag = (paths_[i].first == paths_[i].second);
        bool isHovered = (i == hoveredWire_);
        bool isSelected = (paths_[i] == selectedPath_);

        Colour wireCol = isSelected ? Colours::white
                       : isDiag ? Colours::aquamarine : Colours::orange;
        float alpha = (isSelected || isHovered) ? 1.0f : 0.6f;
        float thickness = isSelected ? 3.0f : (isHovered ? 2.5f : 1.8f);

        g.setColour(wireCol.withAlpha(alpha));
        g.strokePath(wire, PathStrokeType(thickness));

        // Draw dots along wire representing band count
        auto it = bandCounts_.find(paths_[i]);
        int numBands = (it != bandCounts_.end()) ? it->second : 0;
        if (numBands > 0)
        {
            float wireLen = wire.getLength();
            int maxDots = jmin(numBands, 8); // cap at 8 dots to avoid clutter
            float dotRadius = (isSelected || isHovered) ? 4.0f : 2.5f;
            float spacing = wireLen / (float)(maxDots + 1);

            g.setColour(wireCol.withAlpha(isHovered ? 1.0f : 0.85f));
            for (int d = 1; d <= maxDots; ++d)
            {
                float dist = spacing * (float)d;
                auto pt = wire.getPointAlongPath(dist);
                g.fillEllipse(pt.x - dotRadius, pt.y - dotRadius,
                              dotRadius * 2.f, dotRadius * 2.f);
            }
        }
    }

    // Draw preview wire during drag
    if (dragFromInput_ > 0)
    {
        Point<float> end = dragEndPoint_;
        bool validDrop = (dragOverOutput_ > 0 && !pathExists(dragFromInput_, dragOverOutput_));

        // Snap to output port centre if hovering over one
        if (dragOverOutput_ > 0)
            end = getOutputPortLeft(dragOverOutput_);

        auto preview = makeWirePathToPoint(dragFromInput_, end);
        Colour previewCol = validDrop ? Colours::yellow : Colours::white.withAlpha(0.4f);
        float dashes[] = { 6.f, 4.f };
        g.setColour(previewCol);
        g.strokePath(preview, PathStrokeType(2.f, PathStrokeType::curved,
                                              PathStrokeType::rounded));

        // Highlight source input port
        auto srcRect = getInputPortRect(dragFromInput_);
        g.setColour(Colours::yellow.withAlpha(0.5f));
        g.fillRoundedRectangle(srcRect, 4.f);

        // Highlight target output port if valid
        if (validDrop)
        {
            auto tgtRect = getOutputPortRect(dragOverOutput_);
            g.setColour(Colours::yellow.withAlpha(0.5f));
            g.fillRoundedRectangle(tgtRect, 4.f);
        }
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

int RoutingOverviewComponent::hitTestInputPort(Point<float> pt) const
{
    for (int ch = 1; ch <= numChannels_; ++ch)
        if (getInputPortRect(ch).expanded(4.f).contains(pt))
            return ch;
    return -1;
}

int RoutingOverviewComponent::hitTestOutputPort(Point<float> pt) const
{
    for (int ch = 1; ch <= numChannels_; ++ch)
        if (getOutputPortRect(ch).expanded(4.f).contains(pt))
            return ch;
    return -1;
}

bool RoutingOverviewComponent::pathExists(int inCh, int outCh) const
{
    PathKey key { inCh, outCh };
    for (auto& p : paths_)
        if (p == key) return true;
    return false;
}

Path RoutingOverviewComponent::makeWirePathToPoint(int inCh, Point<float> end) const
{
    auto start = getInputPortRight(inCh);
    float dx = (end.x - start.x) * 0.4f;
    Path p;
    p.startNewSubPath(start);
    p.cubicTo(start.x + dx, start.y,
              end.x - dx,   end.y,
              end.x,        end.y);
    return p;
}

void RoutingOverviewComponent::mouseDown(const MouseEvent& e)
{
    // Check if clicking on an input port to start a drag
    int inputCh = hitTestInputPort(e.position);
    if (inputCh > 0)
    {
        dragFromInput_ = inputCh;
        dragEndPoint_ = e.position;
        dragOverOutput_ = -1;
        repaint();
        return;
    }

    // Otherwise, check for wire click (select path)
    int idx = hitTestWire(e.position);
    if (idx >= 0 && idx < (int)paths_.size() && listener_ != nullptr)
    {
        selectedPath_ = paths_[idx];
        repaint();
        listener_->routingPathSelected(paths_[idx].first, paths_[idx].second);
    }
}

void RoutingOverviewComponent::mouseDrag(const MouseEvent& e)
{
    if (dragFromInput_ < 1) return;

    dragEndPoint_ = e.position;
    dragOverOutput_ = hitTestOutputPort(e.position);

    // Auto-scroll the parent Viewport when dragging near edges
    if (auto* vp = findParentComponentOfClass<Viewport>())
    {
        auto posInVp = e.getEventRelativeTo(vp).position;
        int scrollMargin = 30;
        int scrollStep = 12;
        auto vpBounds = vp->getLocalBounds();

        if (posInVp.y < scrollMargin)
        {
            int newY = jmax(0, vp->getViewPositionY() - scrollStep);
            vp->setViewPosition(vp->getViewPositionX(), newY);
        }
        else if (posInVp.y > vpBounds.getHeight() - scrollMargin)
        {
            int maxY = getHeight() - vp->getViewHeight();
            int newY = jmin(maxY, vp->getViewPositionY() + scrollStep);
            vp->setViewPosition(vp->getViewPositionX(), newY);
        }
    }

    repaint();
}

void RoutingOverviewComponent::mouseUp(const MouseEvent& e)
{
    if (dragFromInput_ < 1) return;

    int outCh = hitTestOutputPort(e.position);
    int inCh = dragFromInput_;
    dragFromInput_ = -1;
    dragOverOutput_ = -1;
    repaint();

    if (outCh > 0 && listener_ != nullptr && !pathExists(inCh, outCh))
    {
        listener_->routingPathCreated(inCh, outCh);
        // Don't dismiss — let the user see the new wire or continue adding
    }
}

//==============================================================================
// ChannelSelectorComponent
//==============================================================================

ChannelSelectorComponent::ChannelSelectorComponent(int numChannels,
                                                     const std::set<int>& currentMask,
                                                     Listener* listener)
    : numChannels_(numChannels),
      mask_(currentMask),
      listener_(listener)
{
    setSize(getRecommendedWidth(numChannels), getRecommendedHeight(numChannels));
    setMouseCursor(MouseCursor::PointingHandCursor);
}

bool ChannelSelectorComponent::isChannelOn(int ch1) const
{
    return mask_.empty() || mask_.count(ch1) > 0;
}

Rectangle<int> ChannelSelectorComponent::getCellRect(int ch1) const
{
    int idx = ch1 - 1;
    int col = idx % kCols;
    int row = idx / kCols;
    int x = kMarginX + col * (kCellSize + kCellGap);
    int y = kTopArea + row * (kCellSize + kCellGap);
    return { x, y, kCellSize, kCellSize };
}

Rectangle<int> ChannelSelectorComponent::getAllButtonRect() const
{
    return { kMarginX, 22, 40, 16 };
}

Rectangle<int> ChannelSelectorComponent::getNoneButtonRect() const
{
    return { kMarginX + 48, 22, 45, 16 };
}

int ChannelSelectorComponent::hitTestCell(Point<int> pt) const
{
    for (int ch = 1; ch <= numChannels_; ++ch)
        if (getCellRect(ch).contains(pt))
            return ch;
    return -1;
}

void ChannelSelectorComponent::paint(Graphics& g)
{
    g.fillAll(Colour(0xff1e1e1e));

    // Title
    g.setColour(Colours::white.withAlpha(0.9f));
    g.setFont(Font(12.f, Font::bold));
    g.drawText("Diagonal Channels", 0, 2, getWidth(), 18, Justification::centred, false);

    // All / None buttons
    auto allRect = getAllButtonRect();
    auto noneRect = getNoneButtonRect();

    // Count active channels
    int activeCount = 0;
    for (int ch = 1; ch <= numChannels_; ++ch)
        if (isChannelOn(ch)) activeCount++;
    bool allOn = (activeCount == numChannels_);
    bool noneOn = (activeCount == 0);

    // All button
    g.setColour(allOn ? Colours::aquamarine.withAlpha(0.3f) : Colour(0xff2a2a2a));
    g.fillRoundedRectangle(allRect.toFloat(), 3.f);
    g.setColour(allOn ? Colours::aquamarine : Colours::white.withAlpha(0.6f));
    g.drawRoundedRectangle(allRect.toFloat(), 3.f, 1.f);
    g.setFont(Font(10.f, Font::bold));
    g.drawText("All", allRect, Justification::centred, false);

    // None button
    g.setColour(noneOn ? Colours::orange.withAlpha(0.3f) : Colour(0xff2a2a2a));
    g.fillRoundedRectangle(noneRect.toFloat(), 3.f);
    g.setColour(noneOn ? Colours::orange : Colours::white.withAlpha(0.6f));
    g.drawRoundedRectangle(noneRect.toFloat(), 3.f, 1.f);
    g.drawText("None", noneRect, Justification::centred, false);

    // Channel grid
    g.setFont(Font(11.f, Font::plain));
    for (int ch = 1; ch <= numChannels_; ++ch)
    {
        auto rect = getCellRect(ch).toFloat();
        bool on = isChannelOn(ch);
        bool hovered = (ch == hoveredCell_);

        // Background
        if (on)
            g.setColour(Colours::aquamarine.withAlpha(hovered ? 0.5f : 0.3f));
        else
            g.setColour(hovered ? Colour(0xff3a3a3a) : Colour(0xff2a2a2a));
        g.fillRoundedRectangle(rect, 4.f);

        // Border
        g.setColour(on ? Colours::aquamarine.withAlpha(0.8f) : Colour(0xff555555));
        g.drawRoundedRectangle(rect, 4.f, 1.f);

        // Channel number
        g.setColour(on ? Colours::white : Colours::white.withAlpha(0.35f));
        g.drawText(String(ch), rect.toNearestInt(), Justification::centred, false);
    }
}

void ChannelSelectorComponent::mouseMove(const MouseEvent& e)
{
    int newHovered = hitTestCell(e.position.toInt());
    if (newHovered != hoveredCell_)
    {
        hoveredCell_ = newHovered;
        repaint();
    }
}

void ChannelSelectorComponent::mouseDown(const MouseEvent& e)
{
    auto pt = e.position.toInt();

    if (getAllButtonRect().contains(pt))
    {
        mask_.clear(); // empty = all
        dragging_ = false;
        repaint();
        if (listener_) listener_->diagChannelMaskChanged(mask_);
        return;
    }

    if (getNoneButtonRect().contains(pt))
    {
        mask_ = { 0 }; // sentinel: no valid channels
        dragging_ = false;
        repaint();
        if (listener_) listener_->diagChannelMaskChanged(mask_);
        return;
    }

    int ch = hitTestCell(pt);
    if (ch < 1) return;

    // If currently "all" (empty), expand to explicit full set
    if (mask_.empty())
        for (int i = 1; i <= numChannels_; ++i)
            mask_.insert(i);

    mask_.erase(0); // remove sentinel

    // Toggle first cell and determine drag paint mode
    bool wasOn = mask_.count(ch) > 0;
    if (wasOn)
        mask_.erase(ch);
    else
        mask_.insert(ch);

    dragPaintOn_ = !wasOn; // dragging will paint cells to this state
    dragging_ = true;
    dragVisited_.clear();
    dragVisited_.insert(ch);

    // Normalize
    int validCount = 0;
    for (int c : mask_)
        if (c >= 1 && c <= numChannels_) validCount++;

    if (validCount == numChannels_)
        mask_.clear();
    else if (validCount == 0)
        mask_ = { 0 };

    repaint();
    if (listener_) listener_->diagChannelMaskChanged(mask_);
}

void ChannelSelectorComponent::mouseDrag(const MouseEvent& e)
{
    if (!dragging_) return;

    int ch = hitTestCell(e.position.toInt());
    if (ch < 1 || dragVisited_.count(ch) > 0) return;

    dragVisited_.insert(ch);

    // Expand "all" to explicit set if needed
    if (mask_.empty())
        for (int i = 1; i <= numChannels_; ++i)
            mask_.insert(i);

    mask_.erase(0);

    // Set this cell to the drag paint mode
    if (dragPaintOn_)
        mask_.insert(ch);
    else
        mask_.erase(ch);

    // Normalize
    int validCount = 0;
    for (int c : mask_)
        if (c >= 1 && c <= numChannels_) validCount++;

    if (validCount == numChannels_)
        mask_.clear();
    else if (validCount == 0)
        mask_ = { 0 };

    repaint();
    if (listener_) listener_->diagChannelMaskChanged(mask_);
}

void ChannelSelectorComponent::mouseUp(const MouseEvent&)
{
    dragging_ = false;
    dragVisited_.clear();
}
