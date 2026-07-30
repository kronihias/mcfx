/*
 ==============================================================================

 This file is part of the mcfx (Multichannel Effects) plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com

 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)

 Details of these licenses can be found at: www.gnu.org/licenses

 mcfx is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 ==============================================================================
 */

#ifndef DOTMATRIXCOMPONENT_H_INCLUDED
#define DOTMATRIXCOMPONENT_H_INCLUDED

#include "JuceHeader.h"
#include <vector>

/** One colour-mapped dot per channel, in a grid.

    The most compact of the four views, and the one to leave open in a corner:
    it answers "is there signal on every channel, and roughly how much" at a
    glance and at a size the others cannot reach. A bar needs its full height to
    say anything; a dot says it with a colour.

    Level maps through the same `iec_scale` curve and the same offset as the
    bars, and takes its colours from the same gradient image, so a dot is the
    colour the bar's tip would be at that level. Silence is a dark outline
    rather than nothing at all, so a dead channel reads as present-but-silent
    instead of as a gap in the grid. */
class DotMatrixComponent : public Component
{
public:
    DotMatrixComponent();
    ~DotMatrixComponent() override = default;

    void setNumChannels (int numChannels);
    int  getNumChannels() const { return numCh_; }

    /** Linear values, exactly as MeterComponent::setValue takes them. */
    void setValue (int channel, float rms, float peak, float peakHold);

    /** True once any setValue() since the last call moved a level visibly.
        Clears on read. Lets the editor's timer skip repainting an idle grid —
        this view's stated use is sitting in a corner, mostly idle. */
    bool takeDirty();

    void setOffset (int offsetDb);
    void setPeakHoldVisible (bool shouldBeVisible);

    /** Sticky highlight, -1 for none. Its level is printed in full at the
        bottom of the view, which is the point: the dots answer "is it there",
        the readout answers "how much, exactly". */
    void setSelectedChannel (int channel);
    int  getSelectedChannel() const { return selected_; }

    /** Fired when a click changes the selection, so a host control can follow.
        Passes -1 when a click off the dots clears it. */
    std::function<void (int)> onChannelSelected;

    /** A single click selects; resetting is the double click, so the two do not
        fight over the same gesture. Off the dots, double click resets all. */
    std::function<void (int)> onChannelReset;
    std::function<void()>     onResetAll;

    /** Grid shape for a channel count and aspect. Public because the layout is
        the whole design of this view and worth asserting on directly. */
    struct Grid { int cols = 1, rows = 1; float cell = 0.f, originX = 0.f, originY = 0.f; };
    static Grid computeGrid (int numChannels, float width, float height);
    Grid getGrid() const { return grid_; }

    /** Centre of a channel's dot, and the inverse of that mapping. */
    Point<float> dotCentre (int channel) const;
    int channelAt (Point<float> p) const;

    void paint (Graphics&) override;
    void resized() override;
    void mouseMove (const MouseEvent&) override;
    void mouseExit (const MouseEvent&) override;
    void mouseUp (const MouseEvent&) override;
    void mouseDoubleClick (const MouseEvent&) override;

private:
    struct ChannelLevel { float rmsDb = -200.f, peakDb = -200.f, holdDb = -200.f; };

    Colour colourForDb (float db) const;

    std::vector<ChannelLevel> levels_;
    std::vector<Colour> ramp_;         // gradient image sampled once per rebuild
    int   numCh_    = 0;
    int   offset_   = 0;
    bool  peakHold_ = false;
    bool  dirty_    = true;
    int   hovered_  = -1;
    int   selected_ = -1;

    Grid  grid_;
    Image gradientImage_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DotMatrixComponent)
};

#endif // DOTMATRIXCOMPONENT_H_INCLUDED
