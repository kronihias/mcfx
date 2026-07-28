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

#ifndef WATERFALLCOMPONENT_H_INCLUDED
#define WATERFALLCOMPONENT_H_INCLUDED

#include "JuceHeader.h"
#include "MultiBandAnalyser.h"

/** Every channel's live spectrum, drawn as a ridge receding into depth.

    Unlike the usual waterfall, depth here is the channel index, not time. That
    makes the view answer "which channel sounds wrong, and at what frequency" —
    a question neither the bars nor the ring can answer at all.

    The projection is **oblique parallel, not perspective**. Under perspective
    both the dB and the frequency scale would shrink with depth, so a back ridge
    could not be read against the same axes as a front one and equal levels on
    channel 1 and channel 64 would look different — which defeats the point.
    Parallel projection keeps both scales depth-invariant: one dB grid and one
    frequency grid serve every ridge. It is also a fixed affine map with no
    per-point divide.

    Hidden-surface removal is the painter's algorithm, back to front, with each
    ridge *filled* down to its own baseline. Because the ridges are parallel,
    non-intersecting surfaces at constant depth, that ordering is exact rather
    than an approximation. Stroke-only ridges turn to hash past ~32 channels;
    the fill is what does the occluding. */
class WaterfallComponent : public Component
{
public:
    WaterfallComponent();
    ~WaterfallComponent() override = default;

    /** Borrowed, not owned — it lives on the processor and outlives the editor. */
    void setAnalyser (MultiBandAnalyser* analyser) { analyser_ = analyser; }

    void setNumChannels (int numChannels);
    int  getNumChannels() const { return numCh_; }

    /** Same meaning as the bar view's offset: shifts the level scale. */
    void setOffset (int offsetDb);

    /** Levels below this read as the floor. Deliberately a **linear dB** scale
        and not the bars' iec_scale: splitting a signal across 61 bands puts
        typical band levels around -35..-50 dB, exactly where iec_scale flattens
        to ~0.0025 per dB. That would crush the ridge shape, which is the thing
        being read here. */
    void setFloorDb (float db);
    float getFloorDb() const { return floorDb_; }

    /** Where each ridge sits and how big it is. Recomputed on resize and on a
        channel-count change; public because the projection is worth asserting
        on without a window. */
    struct Layout
    {
        float originX = 0.f, originY = 0.f;   // channel 0's baseline, left end
        float plotW   = 0.f;                  // frequency axis length, px
        float ridgeH  = 0.f;                  // full-scale ridge height, px
        float dx      = 0.f, dy = 0.f;        // per-channel depth step, px
        int   numCh   = 0;
    };
    Layout getLayout() const { return layout_; }

    /** freqNorm and levelNorm are both 0..1; channel 0 is at the front. */
    Point<float> project (float freqNorm, float levelNorm, int channel) const;

    /** Log position of a frequency along the x axis, 0..1. */
    static float freqToNorm (float hz);

    /** dB to 0..1 against the current floor and offset. */
    float levelToNorm (float db) const;

    /** Sticky highlight, -1 for none. The selected ridge is redrawn on top of
        the painter's-algorithm pass, so it stays readable even when channels in
        front of it would otherwise bury it. */
    void setSelectedChannel (int channel);
    int  getSelectedChannel() const { return selected_; }

    /** Fired when a click changes the selection, so a host control can follow. */
    std::function<void (int)> onChannelSelected;

    /** Which channel number on the depth axis a point falls on, or -1.
        Public because it is the inverse of where paint() puts those labels. */
    int channelLabelAt (Point<float> p) const;

    void paint (Graphics&) override;
    void resized() override;
    void mouseMove (const MouseEvent&) override;
    void mouseExit (const MouseEvent&) override;
    void mouseDown (const MouseEvent&) override;

private:
    void rebuildLayout();
    void paintGrid (Graphics&) const;
    void paintRidges (Graphics&);

    /** Which channel's ridge a point is nearest, judged along the depth axis. */
    int channelAt (Point<float> p) const;

    /** Label thinning, shared by paint() and the hit-test so they cannot
        disagree about which numbers are on screen. */
    int labelEvery() const;

    /** One ridge: filled skirt, then the level-coloured stroke. */
    void paintRidge (Graphics&, int channel, bool highlighted);

    MultiBandAnalyser* analyser_ = nullptr;

    int   numCh_   = 0;
    int   offset_  = 0;
    float floorDb_ = -80.f;
    int   hovered_  = -1;
    int   selected_ = -1;

    Layout layout_;

    // Scratch, so paint() allocates nothing per frame.
    std::vector<float> bandX_;          // freqToNorm per band — constant, so cached
    std::vector<float> levelNorm_;
    std::vector<Point<float>> pts_;
    Path skirt_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaterfallComponent)
};

#endif // WATERFALLCOMPONENT_H_INCLUDED
