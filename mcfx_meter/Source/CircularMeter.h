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

#ifndef CIRCULARMETER_H_INCLUDED
#define CIRCULARMETER_H_INCLUDED

#include "JuceHeader.h"
#include <vector>

/** All channels' level meters arranged around a ring, one component drawing the
    lot rather than N rotated widgets.

    Two things it buys over the bar view: an outlier reads as a break in the
    ring's symmetry rather than as one bar among a hundred similar ones, and the
    footprint stays square whatever the channel count, instead of growing 16 px
    per channel until it is wider than the screen.

    It shows the same quantities as the bars — RMS wedge, peak tick, peak-hold
    tick — through the same `iec_scale` curve and the same offset, so a level
    reads identically in both views. */
class CircularMeter : public Component
{
public:
    /** What a channel's angle around the ring means. Only channel order exists
        today; the indirection is here so a physical speaker layout can be added
        without touching any of the drawing code. */
    enum class AngleMapping { ChannelOrder };

    CircularMeter();
    ~CircularMeter() override = default;

    /** Rebuild for a channel count. Cheap; safe to call when nothing changed. */
    void setNumChannels (int numChannels);
    int  getNumChannels() const { return numCh_; }

    /** Linear values, exactly as MeterComponent::setValue takes them. */
    void setValue (int channel, float rms, float peak, float peakHold);

    void setOffset (int offsetDb);
    void setPeakHoldVisible (bool shouldBeVisible);

    /** Called with a 0-based channel when the user clicks its wedge. */
    std::function<void (int)> onChannelReset;

    /** Called when the user clicks the hub or outside the ring. The bar view
        resets every channel on a click anywhere off a strip; this component
        covers the whole area below the control strip, so without this the
        editor's own mouseDown never sees those clicks. */
    std::function<void()> onResetAll;

    /** Angle of a channel's wedge centre, in JUCE's convention: radians
        clockwise from 12 o'clock. Channel 0 sits at the top. */
    static float channelAngle (int channel, int numChannels,
                               AngleMapping mapping = AngleMapping::ChannelOrder);

    /** Which channel a point falls on, or -1 for the hub or outside the ring.
        Public because it is the exact inverse of channelAngle(), and worth being
        able to assert on. */
    int channelAt (Point<float> p) const;

    /** Radius a level is drawn at, matching the bar view's iec_scale mapping.
        Clamped to the outer radius: iec_scale keeps climbing above 0 dB (1.15 at
        +6 dB), which would otherwise draw outside the ring. */
    float radiusForDb (float db) const;
    float getOuterRadius() const { return outerR_; }
    float getInnerRadius() const { return innerR_; }

    void paint (Graphics&) override;
    void resized() override;
    void mouseMove (const MouseEvent&) override;
    void mouseExit (const MouseEvent&) override;
    void mouseUp (const MouseEvent&) override;

private:
    struct ChannelLevel { float rmsDb = -200.f, peakDb = -200.f, holdDb = -200.f; };

    /** True for the rows of pure white the bar's scale marks occupy in the
        gradient image — they must not become radial gradient stops. */
    static bool isScaleTickRow (Colour c);

    void buildGradient();

    std::vector<ChannelLevel> levels_;
    std::vector<float> angles_;        // pre-computed; paint does no trig
    int   numCh_   = 0;
    int   offset_  = 0;
    bool  peakHold_ = false;
    int   hovered_ = -1;

    // Geometry, recomputed on resize.
    Point<float> centre_;
    float outerR_ = 0.f, innerR_ = 0.f;

    // Radial gradient sampled from the bar view's own gradient image, so a wedge
    // shows the colour the bar shows at the same height.
    ColourGradient fill_;
    Image gradientImage_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CircularMeter)
};

#endif // CIRCULARMETER_H_INCLUDED
