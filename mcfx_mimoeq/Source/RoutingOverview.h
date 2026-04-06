/*
 ==============================================================================

 Routing overview component for mcfx_mimoeq.
 Shows input/output connections as wires between two columns.

 ==============================================================================
 */

#ifndef ROUTINGOVERVIEW_H_INCLUDED
#define ROUTINGOVERVIEW_H_INCLUDED

#include "JuceHeader.h"
#include <vector>
#include <utility>

using PathKey = std::pair<int, int>;

class RoutingOverviewComponent : public Component
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void routingPathSelected(int inCh, int outCh) = 0;
        virtual void routingPathCreated(int inCh, int outCh) = 0;
        virtual void routingPathRemoved(int inCh, int outCh) = 0;
    };

    RoutingOverviewComponent(const std::vector<PathKey>& paths,
                             const std::map<PathKey, int>& bandCounts,
                             int numChannels,
                             bool hasDiagonalBands,
                             Listener* listener);

    void updatePaths(const std::vector<PathKey>& paths,
                     const std::map<PathKey, int>& bandCounts);
    void setSelectedPath(PathKey path) { selectedPath_ = path; repaint(); }

    void paint(Graphics& g) override;
    void mouseMove(const MouseEvent& e) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;

    /** Recommended size based on channel count. */
    static int getRecommendedHeight(int numChannels)
    {
        return jmax(180, numChannels * 28 + 60);
    }

    static constexpr int kWidth = 300;

private:
    std::vector<PathKey> paths_;
    std::map<PathKey, int> bandCounts_;
    int numChannels_;
    bool hasDiagonalBands_;
    Listener* listener_;
    int hoveredWire_ = -1;      // index into paths_
    PathKey selectedPath_ { -1, -1 };
    int dragFromInput_ = -1;     // 1-based input channel being dragged from, -1 = not dragging
    int dragOverOutput_ = -1;    // 1-based output channel cursor is over during drag
    Point<float> dragEndPoint_;  // current mouse position during drag

    // Layout helpers
    static constexpr int kPortW = 36;
    static constexpr int kPortH = 22;
    static constexpr int kMarginTop = 36;
    static constexpr int kMarginX = 16;

    Rectangle<float> getInputPortRect(int ch) const;
    Rectangle<float> getOutputPortRect(int ch) const;
    Point<float> getInputPortRight(int ch) const;   // wire start
    Point<float> getOutputPortLeft(int ch) const;    // wire end

    Path makeWirePath(int inCh, int outCh) const;
    Path makeWirePathToPoint(int inCh, Point<float> end) const;
    int hitTestWire(Point<float> pt) const;
    int hitTestInputPort(Point<float> pt) const;   // returns 1-based ch or -1
    int hitTestOutputPort(Point<float> pt) const;  // returns 1-based ch or -1
    bool pathExists(int inCh, int outCh) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoutingOverviewComponent)
};

//==============================================================================
/** Matrix view for routing — inputs as rows, outputs as columns.
    Cells with existing paths show a coloured background and band count.
    Click to select existing paths or create new ones. */
class RoutingMatrixComponent : public Component,
                               public TooltipClient
{
public:
    RoutingMatrixComponent(const std::vector<PathKey>& paths,
                           const std::map<PathKey, int>& bandCounts,
                           int numChannels,
                           bool hasDiagonalBands,
                           RoutingOverviewComponent::Listener* listener);

    void updatePaths(const std::vector<PathKey>& paths,
                     const std::map<PathKey, int>& bandCounts);
    void setSelectedPath(PathKey path) { selectedPath_ = path; repaint(); }

    void paint(Graphics& g) override;
    void mouseMove(const MouseEvent& e) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseDoubleClick(const MouseEvent& e) override;
    void mouseExit(const MouseEvent& e) override;

    String getTooltip() override
    {
        if (hoveredCell_.first >= 1 && hoveredCell_.second >= 1)
            return "In " + String(hoveredCell_.first) + ", Out " + String(hoveredCell_.second);
        return {};
    }

    static int getRecommendedWidth(int numChannels)
    {
        return kMarginLeft + numChannels * kCellSize + kMarginRight;
    }

    static int getRecommendedHeight(int numChannels)
    {
        return kMarginTop + numChannels * kCellSize + kMarginBottom;
    }

private:
    std::vector<PathKey> paths_;
    std::map<PathKey, int> bandCounts_;
    int numChannels_;
    bool hasDiagonalBands_;
    RoutingOverviewComponent::Listener* listener_;
    PathKey selectedPath_ { -1, -1 };
    PathKey hoveredCell_ { -1, -1 };
    TooltipWindow tooltipWindow_ { this, 200 };

    static constexpr int kCellSize = 28;
    static constexpr int kMarginLeft = 40;   // space for row labels
    static constexpr int kMarginTop = 50;    // space for title + col labels
    static constexpr int kMarginRight = 8;
    static constexpr int kMarginBottom = 8;

    Rectangle<int> getCellRect(int inCh, int outCh) const;
    PathKey hitTestCell(Point<int> pt) const;
    bool pathExists(int inCh, int outCh) const;
    int getBandCount(int inCh, int outCh) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoutingMatrixComponent)
};

//==============================================================================
/** Scrollable wrapper that holds both the wire view and matrix view
    with a toggle button to switch between them. */
class ScrollableRoutingOverview : public Component,
                                   public Button::Listener
{
public:
    ScrollableRoutingOverview(const std::vector<PathKey>& paths,
                              const std::map<PathKey, int>& bandCounts,
                              int numChannels,
                              bool hasDiagonalBands,
                              RoutingOverviewComponent::Listener* listener)
        : numChannels_(numChannels),
          paths_(paths),
          bandCounts_(bandCounts),
          hasDiagonalBands_(hasDiagonalBands),
          listener_(listener)
    {
        overview_ = std::make_unique<RoutingOverviewComponent>(paths, bandCounts,
                                                                numChannels, hasDiagonalBands, listener);
        matrix_ = std::make_unique<RoutingMatrixComponent>(paths, bandCounts,
                                                            numChannels, hasDiagonalBands, listener);

        viewport_.setViewedComponent(overview_.get(), false);
        viewport_.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport_);

        toggleBtn_.setButtonText("Matrix");
        toggleBtn_.setColour(TextButton::buttonColourId, Colour(0xff2a2a2a));
        toggleBtn_.setColour(TextButton::textColourOffId, Colours::white.withAlpha(0.8f));
        toggleBtn_.addListener(this);
        addAndMakeVisible(toggleBtn_);

        constrainer_.setMinimumSize(200, 150);
        constrainer_.setMaximumSize(1200, 900);
        resizer_ = std::make_unique<ResizableCornerComponent>(this, &constrainer_);
        addAndMakeVisible(resizer_.get());

        showingMatrix_ = false;
        updateSize();
    }

    void resized() override
    {
        auto area = getLocalBounds();
        toggleBtn_.setBounds(area.removeFromTop(kToggleBarH).reduced(4, 2));
        viewport_.setBounds(area);

        const int resizerSize = 16;
        resizer_->setBounds(getWidth() - resizerSize, getHeight() - resizerSize,
                            resizerSize, resizerSize);

        if (onSizeChanged)
            onSizeChanged(getWidth(), getHeight());
    }

    void buttonClicked(Button*) override
    {
        showingMatrix_ = !showingMatrix_;

        if (showingMatrix_)
        {
            toggleBtn_.setButtonText("Wires");
            viewport_.setViewedComponent(matrix_.get(), false);
            viewport_.setScrollBarsShown(true, true);
        }
        else
        {
            toggleBtn_.setButtonText("Matrix");
            viewport_.setViewedComponent(overview_.get(), false);
            viewport_.setScrollBarsShown(true, false);
        }

        updateSize();
        repaint();
        if (onViewChanged)
            onViewChanged(showingMatrix_);
    }

    RoutingOverviewComponent* getOverview() { return overview_.get(); }
    RoutingMatrixComponent* getMatrix() { return matrix_.get(); }
    bool isShowingMatrix() const { return showingMatrix_; }

    std::function<void(bool)> onViewChanged;       // called with true=matrix, false=wires
    std::function<void(int, int)> onSizeChanged;   // called with (width, height) on resize

    void setShowMatrix(bool matrix)
    {
        if (matrix == showingMatrix_) return;
        showingMatrix_ = matrix;
        if (showingMatrix_)
        {
            toggleBtn_.setButtonText("Wires");
            viewport_.setViewedComponent(matrix_.get(), false);
            viewport_.setScrollBarsShown(true, true);
        }
        else
        {
            toggleBtn_.setButtonText("Matrix");
            viewport_.setViewedComponent(overview_.get(), false);
            viewport_.setScrollBarsShown(true, false);
        }
        updateSize();
    }

    void updatePaths(const std::vector<PathKey>& paths,
                     const std::map<PathKey, int>& bandCounts)
    {
        paths_ = paths;
        bandCounts_ = bandCounts;
        overview_->updatePaths(paths, bandCounts);
        matrix_->updatePaths(paths, bandCounts);
    }

    static constexpr int kMaxHeight = 600;

private:
    static constexpr int kToggleBarH = 24;

    void updateSize()
    {
        int contentW, contentH;

        if (showingMatrix_)
        {
            contentW = RoutingMatrixComponent::getRecommendedWidth(numChannels_);
            contentH = RoutingMatrixComponent::getRecommendedHeight(numChannels_);
        }
        else
        {
            contentW = RoutingOverviewComponent::kWidth;
            contentH = RoutingOverviewComponent::getRecommendedHeight(numChannels_);
        }

        int cappedH = jmin(contentH, kMaxHeight);
        bool needsVScroll = contentH > kMaxHeight;
        bool needsHScroll = showingMatrix_ && contentW > kMaxWidth;
        int w = jmin(contentW, kMaxWidth) + (needsVScroll ? 14 : 0);
        int h = cappedH + kToggleBarH + (needsHScroll ? 14 : 0);
        setSize(w, h);
    }

    static constexpr int kMaxWidth = 500;

    int numChannels_;
    std::vector<PathKey> paths_;
    std::map<PathKey, int> bandCounts_;
    bool hasDiagonalBands_;
    RoutingOverviewComponent::Listener* listener_;
    bool showingMatrix_ = false;

    std::unique_ptr<RoutingOverviewComponent> overview_;
    std::unique_ptr<RoutingMatrixComponent> matrix_;
    Viewport viewport_;
    TextButton toggleBtn_;
    ComponentBoundsConstrainer constrainer_;
    std::unique_ptr<ResizableCornerComponent> resizer_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScrollableRoutingOverview)
};

//==============================================================================
/** Channel selector grid for diagonal EQ — visual popup with clickable channel
    boxes and All/None buttons. */
class ChannelSelectorComponent : public Component
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void diagChannelMaskChanged(const std::set<int>& mask) = 0;
    };

    ChannelSelectorComponent(int numChannels,
                              const std::set<int>& currentMask,
                              Listener* listener);

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;
    void mouseMove(const MouseEvent& e) override;

    static constexpr int kCellSize = 32;
    static constexpr int kCellGap = 4;
    static constexpr int kCols = 8;
    static constexpr int kMarginX = 12;
    static constexpr int kTopArea = 40;  // title + All/None buttons
    static constexpr int kBottomPad = 8;

    static int getRecommendedWidth(int numChannels)
    {
        int cols = jmin(numChannels, kCols);
        int w = cols * (kCellSize + kCellGap) - kCellGap + kMarginX * 2;
        return jmax(w, 160);  // minimum width for title + All/None buttons
    }

    static int getRecommendedHeight(int numChannels)
    {
        int rows = (numChannels + kCols - 1) / kCols;
        return kTopArea + rows * (kCellSize + kCellGap) - kCellGap + kBottomPad;
    }

private:
    int numChannels_;
    std::set<int> mask_;          // empty = all
    Listener* listener_;
    int hoveredCell_ = -1;        // 1-based channel, -1 = none, 0 = special buttons
    bool dragging_ = false;        // true while drag-painting cells
    bool dragPaintOn_ = false;     // whether drag paints cells on or off
    std::set<int> dragVisited_;    // cells already toggled during this drag

    bool isChannelOn(int ch1) const;
    Rectangle<int> getCellRect(int ch1) const;
    Rectangle<int> getAllButtonRect() const;
    Rectangle<int> getNoneButtonRect() const;
    int hitTestCell(Point<int> pt) const;  // returns 1-based ch, or -1

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelSelectorComponent)
};

#endif
