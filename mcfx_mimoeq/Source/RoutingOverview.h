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
    };

    RoutingOverviewComponent(const std::vector<PathKey>& paths,
                             int numChannels,
                             bool hasDiagonalBands,
                             Listener* listener);

    void paint(Graphics& g) override;
    void mouseMove(const MouseEvent& e) override;
    void mouseDown(const MouseEvent& e) override;

    /** Recommended size based on channel count. */
    static int getRecommendedHeight(int numChannels)
    {
        return jmax(180, numChannels * 28 + 60);
    }

    static constexpr int kWidth = 300;

private:
    std::vector<PathKey> paths_;
    int numChannels_;
    bool hasDiagonalBands_;
    Listener* listener_;
    int hoveredWire_ = -1; // index into paths_

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
    int hitTestWire(Point<float> pt) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoutingOverviewComponent)
};

#endif
