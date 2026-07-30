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

#include "CircularMeter.h"
#include "meter.h"          // MeterComponent's gradient resources + iec_scale

extern float iec_scale (float dB);

namespace
{
    // Fraction of the outer radius taken by the hub. Leaves room for the
    // hovered-channel readout and stops the wedges converging to a point.
    constexpr float kInnerFraction = 0.34f;
    // Reference rings, matching the values MeterScaleComponent labels.
    const float kRingDb[] = { 0.f, -10.f, -20.f, -30.f, -40.f, -60.f };

    /** Bars draw a 2 px tick on a 163 px scale; keep the ticks proportional so
        they stay readable as the ring grows. */
    float tickThickness (float span) { return jlimit (1.5f, 4.f, span * 2.f / 163.f); }
}

CircularMeter::CircularMeter()
{
    setOpaque (false);
    setInterceptsMouseClicks (true, false);
    gradientImage_ = ImageCache::getFromMemory (MeterComponent::meter_gradient_png,
                                                MeterComponent::meter_gradient_pngSize);
}

float CircularMeter::channelAngle (int channel, int numChannels, AngleMapping mapping)
{
    if (numChannels <= 0)
        return 0.f;

    switch (mapping)
    {
        case AngleMapping::ChannelOrder:
        default:
            // JUCE measures angles clockwise from 12 o'clock, which is already
            // what we want: channel 0 at the top, ascending clockwise.
            return MathConstants<float>::twoPi * (float) channel / (float) numChannels;
    }
}

void CircularMeter::setNumChannels (int numChannels)
{
    const int n = jmax (0, numChannels);
    if (n == numCh_)
        return;

    numCh_ = n;
    levels_.assign ((size_t) n, ChannelLevel{});
    angles_.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
        angles_[(size_t) i] = channelAngle (i, n);

    hovered_ = -1;
    repaint();
}

void CircularMeter::setValue (int channel, float rms, float peak, float peakHold)
{
    if (! isPositiveAndBelow (channel, numCh_))
        return;

    const float rmsDb  = Decibels::gainToDecibels (rms,      -200.f);
    const float peakDb = Decibels::gainToDecibels (peak,     -200.f);
    const float holdDb = Decibels::gainToDecibels (peakHold, -200.f);

    auto& L = levels_[(size_t) channel];
    // 0.05 dB is well under a pixel anywhere on the ring.
    if (std::abs (rmsDb  - L.rmsDb)  > 0.05f
     || std::abs (peakDb - L.peakDb) > 0.05f
     || std::abs (holdDb - L.holdDb) > 0.05f)
        dirty_ = true;

    L.rmsDb  = rmsDb;
    L.peakDb = peakDb;
    L.holdDb = holdDb;
}

bool CircularMeter::takeDirty()
{
    const bool d = dirty_;
    dirty_ = false;
    return d;
}

void CircularMeter::setOffset (int offsetDb)
{
    if (offsetDb != offset_) { offset_ = offsetDb; repaint(); }
}

void CircularMeter::setPeakHoldVisible (bool shouldBeVisible)
{
    if (shouldBeVisible != peakHold_) { peakHold_ = shouldBeVisible; repaint(); }
}

float CircularMeter::radiusForDb (float db) const
{
    // Same curve and same offset as the bar view, so a level reads identically
    // in both. iec_scale exceeds 1 above 0 dB (1.15 at +6), so clamp — otherwise
    // a hot channel would draw outside the ring.
    const float s = jlimit (0.f, 1.f, iec_scale (db - (float) offset_));
    return innerR_ + s * (outerR_ - innerR_);
}

float CircularMeter::radiusForScaleDb (float scaleDb) const
{
    // No offset here: see the header. The two agree by construction — a signal
    // sitting at `offset` dBFS reaches iec_scale(0), which is the gradation
    // labelled 0 + offset.
    const float s = jlimit (0.f, 1.f, iec_scale (scaleDb));
    return innerR_ + s * (outerR_ - innerR_);
}

void CircularMeter::resized()
{
    auto b = getLocalBounds().toFloat();
    centre_ = b.getCentre();
    outerR_ = jmax (10.f, 0.5f * jmin (b.getWidth(), b.getHeight()) - 26.f);
    innerR_ = outerR_ * kInnerFraction;
    buildGradient();
}

bool CircularMeter::isScaleTickRow (Colour c)
{
    // The gradient image has the bar's scale marks painted into it as rows of
    // pure white. In a bar they read as thin lines; interpolated between
    // gradient stops they smear into wide white bands instead. The ring draws
    // its own reference circles, so these rows are skipped entirely.
    return c.getRed() == 255 && c.getGreen() == 255 && c.getBlue() == 255;
}

void CircularMeter::buildGradient()
{
    // Sample the bar view's vertical gradient into a radial one, so a wedge is
    // coloured the way the bar is at the same height.
    fill_ = ColourGradient (Colours::black, centre_, Colours::black,
                            centre_ + Point<float> (outerR_, 0.f), true);
    fill_.clearColours();

    if (! gradientImage_.isValid())
    {
        fill_.addColour (0.0, Colours::green);
        fill_.addColour (1.0, Colours::green);
        return;
    }

    const int h = jmax (1, gradientImage_.getHeight());
    const float hubProp = innerR_ / jmax (1.f, outerR_);
    const int steps = 24;

    for (int i = 0; i <= steps; ++i)
    {
        const float t = (float) i / (float) steps;                 // 0 = hub, 1 = rim
        const float prop = hubProp + t * (1.f - hubProp);
        // The image runs top = full scale, bottom = silence, so invert.
        const int y0 = jlimit (0, h - 1, (int) ((1.f - t) * (float) (h - 1)));

        // Walk off a scale-mark row to the nearest real ramp colour.
        Colour c = gradientImage_.getPixelAt (0, y0);
        for (int d = 1; d < h && isScaleTickRow (c); ++d)
        {
            const int up = y0 - d, dn = y0 + d;
            if (up >= 0 && ! isScaleTickRow (gradientImage_.getPixelAt (0, up)))
                c = gradientImage_.getPixelAt (0, up);
            else if (dn < h && ! isScaleTickRow (gradientImage_.getPixelAt (0, dn)))
                c = gradientImage_.getPixelAt (0, dn);
        }

        fill_.addColour (jlimit (0.0, 1.0, (double) prop), c.withAlpha (1.f));
    }
}

int CircularMeter::channelAt (Point<float> p) const
{
    if (numCh_ <= 0)
        return -1;

    const auto d = p - centre_;
    const float r = d.getDistanceFromOrigin();
    if (r < innerR_ || r > outerR_)
        return -1;

    // Back to JUCE's clockwise-from-top convention.
    float a = std::atan2 (d.x, -d.y);
    if (a < 0.f) a += MathConstants<float>::twoPi;

    const float per = MathConstants<float>::twoPi / (float) numCh_;
    return jlimit (0, numCh_ - 1, (int) std::floor ((a + per * 0.5f) / per) % numCh_);
}

void CircularMeter::mouseMove (const MouseEvent& e)
{
    const int h = channelAt (e.position);
    if (h != hovered_) { hovered_ = h; repaint(); }
}

void CircularMeter::mouseExit (const MouseEvent&)
{
    if (hovered_ != -1) { hovered_ = -1; repaint(); }
}

void CircularMeter::setSelectedChannel (int channel)
{
    const int c = isPositiveAndBelow (channel, numCh_) ? channel : -1;
    if (c != selected_) { selected_ = c; repaint(); }
}

void CircularMeter::mouseUp (const MouseEvent& e)
{
    // A click selects; resetting moved to the double click so the two gestures
    // do not fight. Clicking off the wedges clears the selection.
    const int ch = channelAt (e.position);
    if (ch != selected_)
    {
        setSelectedChannel (ch);
        if (onChannelSelected != nullptr)
            onChannelSelected (ch);
    }
}

void CircularMeter::mouseDoubleClick (const MouseEvent& e)
{
    const int ch = channelAt (e.position);

    if (ch >= 0)
    {
        if (onChannelReset != nullptr)
            onChannelReset (ch);
    }
    else if (onResetAll != nullptr)
    {
        onResetAll();
    }
}

void CircularMeter::paint (Graphics& g)
{
    if (numCh_ <= 0 || outerR_ <= 0.f)
        return;

    const float per   = MathConstants<float>::twoPi / (float) numCh_;
    // Gap between wedges shrinks as the ring fills up, vanishing when there is
    // no room for it.
    const float gap   = jmin (per * 0.18f, 0.035f);
    const float halfW = jmax (per * 0.5f - gap * 0.5f, per * 0.30f);
    const auto  box   = Rectangle<float> (centre_.x - outerR_, centre_.y - outerR_,
                                          outerR_ * 2.f, outerR_ * 2.f);

    // --- reference rings, at the same dB values the bar scale labels ---
    //     Labels are deliberately not drawn here: the wedges paint over this,
    //     and outside the rim they collide with the channel numbers. They go on
    //     top at the end instead.
    for (float db : kRingDb)
    {
        const float r = radiusForScaleDb (db);
        const bool zero = std::abs (db) < 0.01f;
        g.setColour (Colours::white.withAlpha (zero ? 0.40f : 0.13f));
        g.drawEllipse (centre_.x - r, centre_.y - r, r * 2.f, r * 2.f, zero ? 1.2f : 0.6f);
    }

    // --- per-channel wedges, accumulated into per-fill paths ---
    //     The wedges of different channels never overlap, so batching by fill
    //     draws the same picture while setting up each fill once per frame
    //     instead of once per channel — the gradient in particular rebuilds its
    //     lookup table on every setGradientFill.
    const float thick = tickThickness (outerR_ - innerR_);
    const bool  labelAll = numCh_ <= 24;
    const int   labelEvery = numCh_ <= 24 ? 1 : (numCh_ <= 64 ? 4 : 8);

    trackPath_.clear();
    rmsPath_.clear();
    tickPath_.clear();
    tickHotPath_.clear();
    holdPath_.clear();
    holdHotPath_.clear();
    trackPath_.preallocateSpace (numCh_ * 48);
    rmsPath_.preallocateSpace (numCh_ * 48);

    for (int ch = 0; ch < numCh_; ++ch)
    {
        const float a  = angles_[(size_t) ch];
        const float a0 = a - halfW;
        const float a1 = a + halfW;
        const auto& L  = levels_[(size_t) ch];

        // Track: the "off" part of the scale, so an idle channel still reads.
        // The hovered and selected tracks are brighter, so they get their own
        // fill — there are at most two of them.
        if (ch == selected_ || ch == hovered_)
        {
            Path p;
            p.addPieSegment (box, a0, a1, innerR_ / outerR_);
            g.setColour (Colours::white.withAlpha (ch == selected_ ? 0.22f : 0.14f));
            g.fillPath (p);
        }
        else
        {
            trackPath_.addPieSegment (box, a0, a1, innerR_ / outerR_);
        }

        // RMS wedge.
        const float rRms = radiusForDb (L.rmsDb);
        if (rRms > innerR_ + 0.5f)
            rmsPath_.addPieSegment (Rectangle<float> (centre_.x - rRms, centre_.y - rRms,
                                                      rRms * 2.f, rRms * 2.f),
                                    a0, a1, innerR_ / rRms);

        // Peak tick — the ring's answer to the bar's 2 px marker.
        {
            const float r = radiusForDb (L.peakDb);
            if (r > innerR_)
                (L.peakDb - (float) offset_ > 0.f ? tickHotPath_ : tickPath_)
                    .addPieSegment (Rectangle<float> (centre_.x - r, centre_.y - r, r * 2.f, r * 2.f),
                                    a0, a1, jmax (0.f, (r - thick) / r));
        }

        // Peak hold, in its own pass below so it stays visible in the busy
        // outer rim.
        if (peakHold_)
        {
            const float r = radiusForDb (L.holdDb);
            if (r > innerR_)
                (L.holdDb - (float) offset_ > 0.f ? holdHotPath_ : holdPath_)
                    .addPieSegment (Rectangle<float> (centre_.x - r, centre_.y - r, r * 2.f, r * 2.f),
                                    a0, a1, jmax (0.f, (r - thick) / r));
        }
    }

    g.setColour (Colours::white.withAlpha (0.07f));
    g.fillPath (trackPath_);
    g.setGradientFill (fill_);
    g.fillPath (rmsPath_);
    g.setColour (Colours::white);
    g.fillPath (tickPath_);
    g.setColour (Colours::red);
    g.fillPath (tickHotPath_);
    if (peakHold_)
    {
        g.setColour (Colours::yellow);
        g.fillPath (holdPath_);
        g.setColour (Colours::red);
        g.fillPath (holdHotPath_);
    }

    // Group separators every GROUP_CHANNELS, keeping the bar view's
    // count-in-fours affordance. They live in the gaps between wedges, so
    // drawing them after the fills changes nothing visually.
    if (numCh_ > 8)
    {
        g.setColour (Colours::white.withAlpha (0.16f));
        for (int ch = 0; ch < numCh_; ch += 4)
        {
            const float sa = angles_[(size_t) ch] - per * 0.5f;
            const auto  p0 = centre_ + Point<float> (std::sin (sa), -std::cos (sa)) * innerR_;
            const auto  p1 = centre_ + Point<float> (std::sin (sa), -std::cos (sa)) * (outerR_ + 3.f);
            g.drawLine ({ p0, p1 }, 0.6f);
        }
    }

    // Channel numbers outside the rim, thinned as the ring fills. The
    // selected one is always numbered, or the selection has no anchor.
    {
        const float pt = numCh_ > 64 ? 9.f : 11.f;
        const Font plainFont (FontOptions (pt, Font::plain));
        const Font boldFont  (FontOptions (pt, Font::bold));
        g.setFont (plainFont);

        for (int ch = 0; ch < numCh_; ++ch)
        {
            if (! (labelAll || ch % labelEvery == 0 || ch == selected_))
                continue;

            const float a  = angles_[(size_t) ch];
            const auto  lp = centre_ + Point<float> (std::sin (a), -std::cos (a)) * (outerR_ + 13.f);
            g.setColour (ch == selected_ ? Colours::aquamarine
                                         : Colours::white.withAlpha (ch == hovered_ ? 1.f : 0.55f));
            if (ch == selected_)
                g.setFont (boldFont);
            g.drawText (String (ch + 1), (int) lp.x - 14, (int) lp.y - 7, 28, 14,
                        Justification::centred, false);
            if (ch == selected_)
                g.setFont (plainFont);
        }
    }

    // --- dB scale, on top of the wedges so it stays readable whatever the
    //     levels are doing. Each label sits just inside its own ring, on the
    //     vertical, over a dark plate. ---
    g.setFont (Font (FontOptions (10.f, Font::plain)));

    // Thinned by the space actually between the rings, not by a fixed list:
    // iec_scale bunches the quiet end up, and in a small window the whole scale
    // is only a few tens of pixels wide, so labelling all of them would stack
    // them on top of each other.
    float lastLabelR = 1.0e9f;

    for (float db : kRingDb)
    {
        const float r = radiusForScaleDb (db);
        if (r < innerR_ + 7.f)              // too close to the hub to place
            continue;
        if (std::abs (db) > 0.01f && lastLabelR - r < 13.f)
            continue;                       // 0 dB is always labelled
        lastLabelR = r;

        const auto plate = Rectangle<float> (centre_.x - 13.f, centre_.y - r + 1.f, 26.f, 12.f);
        g.setColour (Colours::black.withAlpha (0.62f));
        g.fillRoundedRectangle (plate, 2.f);
        g.setColour (Colours::white.withAlpha (std::abs (db) < 0.01f ? 0.85f : 0.6f));
        g.drawText (String ((int) db + offset_), plate, Justification::centred, false);
    }

    // --- hub readout: at high counts this answers "which wedge is that?"
    //     better than any label scheme could ---
    // The hover previews; the selection is what stays once the mouse leaves.
    const int readout = hovered_ >= 0 ? hovered_ : selected_;

    if (isPositiveAndBelow (readout, numCh_))
    {
        const auto& L = levels_[(size_t) readout];
        // True dBFS, no offset: it shifts the scale's labels, not the signal,
        // so applying it here too would count it twice. See radiusForScaleDb().
        auto dbText = [] (float db)
        {
            return db <= -199.f ? String ("-inf") : String (db, 1) + " dB";
        };
        const int x = (int) (centre_.x - innerR_), w = (int) (innerR_ * 2.f);

        g.setColour (readout == selected_ && hovered_ < 0 ? Colours::aquamarine : Colours::white);
        g.setFont (Font (FontOptions (15.f, Font::bold)));
        g.drawText ("ch " + String (readout + 1),
                    x, (int) (centre_.y - innerR_ * 0.62f), w, 18, Justification::centred, false);

        g.setFont (Font (FontOptions (13.f, Font::plain)));
        g.setColour (Colours::white.withAlpha (0.85f));
        g.drawText ("rms " + dbText (L.rmsDb),
                    x, (int) (centre_.y - 12.f), w, 18, Justification::centred, false);

        g.setColour (Colours::white.withAlpha (0.6f));
        g.setFont (Font (FontOptions (12.f, Font::plain)));
        g.drawText ("pk " + dbText (L.peakDb),
                    x, (int) (centre_.y + 6.f), w, 16, Justification::centred, false);

        if (peakHold_)
        {
            g.setColour (Colours::yellow.withAlpha (0.7f));
            g.drawText ("hold " + dbText (L.holdDb),
                        x, (int) (centre_.y + 22.f), w, 16, Justification::centred, false);
        }
    }
}
