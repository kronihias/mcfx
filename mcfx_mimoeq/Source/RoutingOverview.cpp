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
                                                     const std::set<int>& diagMask,
                                                     Listener* listener)
    : paths_(paths),
      bandCounts_(bandCounts),
      numChannels_(numChannels),
      hasDiagonalBands_(hasDiagonalBands),
      diagMask_(diagMask),
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
    g.setFont(Font(FontOptions(13.f, Font::bold)));
    g.drawText("Routing Overview", getLocalBounds().removeFromTop(28),
               Justification::centred, false);

    // Draw port labels and diagonal indicators
    g.setFont(Font(FontOptions(12.f, Font::plain)));
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

        // "D" box right after input port if diagonal is active for this channel
        bool diagActive = hasDiagonalBands_
                          && (diagMask_.empty() || diagMask_.count(ch) > 0);
        if (diagActive)
        {
            float dX = inRect.getRight() + 3.f;
            float dY = inRect.getY() + 2.f;
            float dW = 18.f;
            float dH = inRect.getHeight() - 4.f;
            Rectangle<float> dRect(dX, dY, dW, dH);
            g.setColour(Colours::aquamarine.withAlpha(0.25f));
            g.fillRoundedRectangle(dRect, 3.f);
            g.setColour(Colours::aquamarine.withAlpha(0.7f));
            g.drawRoundedRectangle(dRect, 3.f, 1.f);
            g.setFont(Font(FontOptions(10.f, Font::bold)));
            g.drawText("D", dRect, Justification::centred, false);
            g.setFont(Font(FontOptions(12.f, Font::plain)));
        }

        // Output port
        g.setColour(Colour(0xff3a3a3a));
        g.fillRoundedRectangle(outRect, 4.f);
        g.setColour(Colours::orange.withAlpha(0.8f));
        g.drawRoundedRectangle(outRect, 4.f, 1.f);
        g.setColour(Colours::white.withAlpha(0.9f));
        g.drawText(String(ch), outRect, Justification::centred, false);
    }

    // Column labels
    g.setFont(Font(FontOptions(10.f, Font::plain)));
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

int RoutingOverviewComponent::hitTestDiagBox(Point<float> pt) const
{
    if (!hasDiagonalBands_)
        return -1;

    for (int ch = 1; ch <= numChannels_; ++ch)
    {
        bool diagActive = diagMask_.empty() || diagMask_.count(ch) > 0;
        if (!diagActive)
            continue;

        auto inRect = getInputPortRect(ch);
        float dX = inRect.getRight() + 3.f;
        float dY = inRect.getY() + 2.f;
        Rectangle<float> dRect(dX, dY, 18.f, inRect.getHeight() - 4.f);
        if (dRect.expanded(2.f).contains(pt))
            return ch;
    }
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
    // Check if clicking on a "D" box — switch to diagonal mode
    if (hitTestDiagBox(e.position) > 0 && listener_ != nullptr)
    {
        listener_->routingDiagonalRequested();
        return;
    }

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
// RoutingMatrixComponent
//==============================================================================

RoutingMatrixComponent::RoutingMatrixComponent(const std::vector<PathKey>& paths,
                                                 const std::map<PathKey, int>& bandCounts,
                                                 int numChannels,
                                                 bool hasDiagonalBands,
                                                 const std::set<int>& diagMask,
                                                 RoutingOverviewComponent::Listener* listener)
    : paths_(paths),
      bandCounts_(bandCounts),
      numChannels_(numChannels),
      hasDiagonalBands_(hasDiagonalBands),
      diagMask_(diagMask),
      listener_(listener)
{
    setSize(getRecommendedWidth(numChannels), getRecommendedHeight(numChannels));
    setMouseCursor(MouseCursor::PointingHandCursor);
}

void RoutingMatrixComponent::updatePaths(const std::vector<PathKey>& paths,
                                          const std::map<PathKey, int>& bandCounts)
{
    paths_ = paths;
    bandCounts_ = bandCounts;
    hoveredCell_ = { -1, -1 };
    repaint();
}

Rectangle<int> RoutingMatrixComponent::getCellRect(int inCh, int outCh) const
{
    int x = kMarginLeft + (outCh - 1) * kCellSize;
    int y = kMarginTop + (inCh - 1) * kCellSize;
    return { x, y, kCellSize, kCellSize };
}

PathKey RoutingMatrixComponent::hitTestCell(Point<int> pt) const
{
    for (int in = 1; in <= numChannels_; ++in)
        for (int out = 1; out <= numChannels_; ++out)
            if (getCellRect(in, out).contains(pt))
                return { in, out };
    return { -1, -1 };
}

bool RoutingMatrixComponent::pathExists(int inCh, int outCh) const
{
    PathKey key { inCh, outCh };
    for (auto& p : paths_)
        if (p == key) return true;
    return false;
}

int RoutingMatrixComponent::getBandCount(int inCh, int outCh) const
{
    auto it = bandCounts_.find({ inCh, outCh });
    return (it != bandCounts_.end()) ? it->second : 0;
}

void RoutingMatrixComponent::paint(Graphics& g)
{
    g.fillAll(Colour(0xff1e1e1e));

    // Title
    g.setColour(Colours::white.withAlpha(0.9f));
    g.setFont(Font(FontOptions(13.f, Font::bold)));
    g.drawText("Routing Matrix", getLocalBounds().removeFromTop(28),
               Justification::centred, false);

    // Column labels (outputs)
    for (int out = 1; out <= numChannels_; ++out)
    {
        bool highlighted = (hoveredCell_.second == out);
        g.setFont(Font(FontOptions(10.f, highlighted ? Font::bold : Font::plain)));
        g.setColour(Colours::orange.withAlpha(highlighted ? 1.0f : 0.7f));
        auto cellR = getCellRect(1, out);
        g.drawText(String(out),
                   cellR.getX(), kMarginTop - 14, kCellSize, 12,
                   Justification::centred, false);
    }

    // "OUT" label above columns
    g.setColour(Colours::orange.withAlpha(0.5f));
    g.setFont(Font(FontOptions(9.f, Font::bold)));
    g.drawText("OUT", kMarginLeft, kMarginTop - 26, numChannels_ * kCellSize, 12,
               Justification::centred, false);

    // "IN" label to the left of rows
    g.setColour(Colours::aquamarine.withAlpha(0.5f));
    g.setFont(Font(FontOptions(9.f, Font::bold)));
    g.drawText("IN", 2, kMarginTop, kMarginLeft - 4, numChannels_ * kCellSize,
               Justification::centredTop, false);

    // Row labels (inputs) — mark with "D" if diagonal EQ is active
    for (int in = 1; in <= numChannels_; ++in)
    {
        bool highlighted = (hoveredCell_.first == in);
        bool diagActive = hasDiagonalBands_
                          && (diagMask_.empty() || diagMask_.count(in) > 0);
        g.setFont(Font(FontOptions(10.f, highlighted ? Font::bold : Font::plain)));
        g.setColour(Colours::aquamarine.withAlpha(highlighted ? 1.0f : 0.7f));
        auto cellR = getCellRect(in, 1);

        String label = String(in);
        if (diagActive)
            label << " D";
        g.drawText(label,
                   2, cellR.getY(), kMarginLeft - 4, kCellSize,
                   Justification::centredRight, false);
    }

    // Draw cells
    g.setFont(Font(FontOptions(10.f, Font::bold)));
    for (int in = 1; in <= numChannels_; ++in)
    {
        for (int out = 1; out <= numChannels_; ++out)
        {
            auto rect = getCellRect(in, out).toFloat().reduced(1.f);
            bool exists = pathExists(in, out);
            bool isDiag = (in == out);
            bool isHovered = (hoveredCell_.first == in && hoveredCell_.second == out);
            bool isSelected = (selectedPath_.first == in && selectedPath_.second == out);
            int bands = getBandCount(in, out);

            // Cell background
            if (exists)
            {
                Colour baseCol = isDiag ? Colours::aquamarine : Colours::orange;
                float alpha = isSelected ? 0.55f : (isHovered ? 0.4f : 0.25f);
                g.setColour(baseCol.withAlpha(alpha));
            }
            else
            {
                g.setColour(isHovered ? Colour(0xff3a3a3a) : Colour(0xff262626));
            }
            g.fillRoundedRectangle(rect, 3.f);

            // Cell border
            if (isSelected)
                g.setColour(Colours::white.withAlpha(0.9f));
            else if (exists)
                g.setColour((isDiag ? Colours::aquamarine : Colours::orange).withAlpha(0.5f));
            else
                g.setColour(Colour(0xff3a3a3a));
            g.drawRoundedRectangle(rect, 3.f, isSelected ? 1.5f : 0.8f);

            // Band count text
            if (exists && bands > 0)
            {
                g.setColour(isSelected ? Colours::white : Colours::white.withAlpha(0.85f));
                g.drawText(String(bands), rect.toNearestInt(), Justification::centred, false);
            }
        }
    }
}

void RoutingMatrixComponent::mouseMove(const MouseEvent& e)
{
    auto cell = hitTestCell(e.position.toInt());
    if (cell != hoveredCell_)
    {
        hoveredCell_ = cell;
        repaint();
    }
}

void RoutingMatrixComponent::mouseExit(const MouseEvent&)
{
    if (hoveredCell_.first >= 0)
    {
        hoveredCell_ = { -1, -1 };
        repaint();
    }
}

void RoutingMatrixComponent::mouseDown(const MouseEvent& e)
{
    auto cell = hitTestCell(e.position.toInt());
    if (cell.first < 1 || cell.second < 1) return;
    if (listener_ == nullptr) return;

    if (pathExists(cell.first, cell.second))
    {
        selectedPath_ = cell;
        repaint();
        listener_->routingPathSelected(cell.first, cell.second);
    }
    else
    {
        listener_->routingPathCreated(cell.first, cell.second);
        selectedPath_ = cell;
    }
}

void RoutingMatrixComponent::mouseDoubleClick(const MouseEvent& e)
{
    auto cell = hitTestCell(e.position.toInt());
    if (cell.first < 1 || cell.second < 1) return;
    if (listener_ == nullptr) return;

    if (pathExists(cell.first, cell.second))
    {
        listener_->routingPathRemoved(cell.first, cell.second);
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
    g.setFont(Font(FontOptions(12.f, Font::bold)));
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
    g.setFont(Font(FontOptions(10.f, Font::bold)));
    g.drawText("All", allRect, Justification::centred, false);

    // None button
    g.setColour(noneOn ? Colours::orange.withAlpha(0.3f) : Colour(0xff2a2a2a));
    g.fillRoundedRectangle(noneRect.toFloat(), 3.f);
    g.setColour(noneOn ? Colours::orange : Colours::white.withAlpha(0.6f));
    g.drawRoundedRectangle(noneRect.toFloat(), 3.f, 1.f);
    g.drawText("None", noneRect, Justification::centred, false);

    // Channel grid
    g.setFont(Font(FontOptions(11.f, Font::plain)));
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
