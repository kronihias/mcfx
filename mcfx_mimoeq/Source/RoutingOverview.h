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
    };

    RoutingOverviewComponent(const std::vector<PathKey>& paths,
                             const std::map<PathKey, int>& bandCounts,
                             int numChannels,
                             bool hasDiagonalBands,
                             Listener* listener);

    void updatePaths(const std::vector<PathKey>& paths,
                     const std::map<PathKey, int>& bandCounts);

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
        return cols * (kCellSize + kCellGap) - kCellGap + kMarginX * 2;
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

    bool isChannelOn(int ch1) const;
    Rectangle<int> getCellRect(int ch1) const;
    Rectangle<int> getAllButtonRect() const;
    Rectangle<int> getNoneButtonRect() const;
    int hitTestCell(Point<int> pt) const;  // returns 1-based ch, or -1

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelSelectorComponent)
};

#endif
