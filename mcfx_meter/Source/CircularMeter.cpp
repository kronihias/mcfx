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

    auto& L = levels_[(size_t) channel];
    L.rmsDb  = Decibels::gainToDecibels (rms,      -200.f);
    L.peakDb = Decibels::gainToDecibels (peak,     -200.f);
    L.holdDb = Decibels::gainToDecibels (peakHold, -200.f);
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

void CircularMeter::resized()
{
    auto b = getLocalBounds().toFloat();
    centre_ = b.getCentre();
    outerR_ = jmax (10.f, 0.5f * jmin (b.getWidth(), b.getHeight()) - 26.f);
    innerR_ = outerR_ * kInnerFraction;
    buildGradient();
}

void CircularMeter::buildGradient()
{
    // Sample the bar view's vertical gradient into a radial one, so a wedge is
    // coloured the way the bar is at the same height.
    fill_ = ColourGradient (Colours::black, centre_, Colours::black,
                            centre_ + Point<float> (outerR_, 0.f), true);
    fill_.clearColours();

    const int h = jmax (1, gradientImage_.getHeight());
    const int steps = 12;
    for (int i = 0; i <= steps; ++i)
    {
        const float t = (float) i / (float) steps;                 // 0 = hub, 1 = rim
        const float prop = innerR_ / jmax (1.f, outerR_) + t * (1.f - innerR_ / jmax (1.f, outerR_));
        // The image runs top = full scale, bottom = silence, so invert.
        const int y = jlimit (0, h - 1, (int) ((1.f - t) * (float) (h - 1)));
        fill_.addColour (jlimit (0.0, 1.0, (double) prop),
                         gradientImage_.isValid() ? gradientImage_.getPixelAt (0, y)
                                                  : Colours::green);
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

void CircularMeter::mouseUp (const MouseEvent& e)
{
    const int ch = channelAt (e.position);
    if (ch >= 0 && onChannelReset != nullptr)
        onChannelReset (ch);
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
    g.setFont (Font (FontOptions (10.f, Font::plain)));
    for (float db : kRingDb)
    {
        const float r = radiusForDb (db);
        const bool zero = std::abs (db) < 0.01f;
        g.setColour (Colours::white.withAlpha (zero ? 0.40f : 0.13f));
        g.drawEllipse (centre_.x - r, centre_.y - r, r * 2.f, r * 2.f, zero ? 1.2f : 0.6f);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawText (String ((int) db), (int) (centre_.x + 3.f), (int) (centre_.y - r - 6.f),
                    30, 12, Justification::centredLeft, false);
    }

    // --- per-channel wedges ---
    const float thick = tickThickness (outerR_ - innerR_);
    const bool  labelAll = numCh_ <= 24;
    const int   labelEvery = numCh_ <= 24 ? 1 : (numCh_ <= 64 ? 4 : 8);

    for (int ch = 0; ch < numCh_; ++ch)
    {
        const float a  = angles_[(size_t) ch];
        const float a0 = a - halfW;
        const float a1 = a + halfW;
        const auto& L  = levels_[(size_t) ch];

        // Track: the "off" part of the scale, so an idle channel still reads.
        {
            Path p;
            p.addPieSegment (box, a0, a1, innerR_ / outerR_);
            g.setColour (Colours::white.withAlpha (ch == hovered_ ? 0.14f : 0.07f));
            g.fillPath (p);
        }

        // RMS wedge.
        const float rRms = radiusForDb (L.rmsDb);
        if (rRms > innerR_ + 0.5f)
        {
            Path p;
            p.addPieSegment (Rectangle<float> (centre_.x - rRms, centre_.y - rRms,
                                               rRms * 2.f, rRms * 2.f),
                             a0, a1, innerR_ / rRms);
            g.setGradientFill (fill_);
            g.fillPath (p);
        }

        // Peak tick — the ring's answer to the bar's 2 px marker.
        {
            const float r = radiusForDb (L.peakDb);
            if (r > innerR_)
            {
                Path p;
                p.addPieSegment (Rectangle<float> (centre_.x - r, centre_.y - r, r * 2.f, r * 2.f),
                                 a0, a1, jmax (0.f, (r - thick) / r));
                g.setColour (L.peakDb - (float) offset_ > 0.f ? Colours::red : Colours::white);
                g.fillPath (p);
            }
        }

        // Peak hold, drawn last so it stays visible in the busy outer rim.
        if (peakHold_)
        {
            const float r = radiusForDb (L.holdDb);
            if (r > innerR_)
            {
                Path p;
                p.addPieSegment (Rectangle<float> (centre_.x - r, centre_.y - r, r * 2.f, r * 2.f),
                                 a0, a1, jmax (0.f, (r - thick) / r));
                g.setColour (L.holdDb - (float) offset_ > 0.f ? Colours::red : Colours::yellow);
                g.fillPath (p);
            }
        }

        // Group separators every GROUP_CHANNELS, keeping the bar view's
        // count-in-fours affordance.
        if (numCh_ > 8 && ch % 4 == 0)
        {
            const float sa = a - per * 0.5f;
            const auto  p0 = centre_ + Point<float> (std::sin (sa), -std::cos (sa)) * innerR_;
            const auto  p1 = centre_ + Point<float> (std::sin (sa), -std::cos (sa)) * (outerR_ + 3.f);
            g.setColour (Colours::white.withAlpha (0.16f));
            g.drawLine ({ p0, p1 }, 0.6f);
        }

        // Channel numbers outside the rim, thinned as the ring fills.
        if (labelAll || ch % labelEvery == 0)
        {
            const auto lp = centre_ + Point<float> (std::sin (a), -std::cos (a)) * (outerR_ + 13.f);
            g.setColour (Colours::white.withAlpha (ch == hovered_ ? 1.f : 0.55f));
            g.setFont (Font (FontOptions (numCh_ > 64 ? 9.f : 11.f, Font::plain)));
            g.drawText (String (ch + 1), (int) lp.x - 14, (int) lp.y - 7, 28, 14,
                        Justification::centred, false);
        }
    }

    // --- hub readout: at high counts this answers "which wedge is that?"
    //     better than any label scheme could ---
    if (hovered_ >= 0)
    {
        const auto& L = levels_[(size_t) hovered_];
        g.setColour (Colours::white);
        g.setFont (Font (FontOptions (15.f, Font::bold)));
        g.drawText ("ch " + String (hovered_ + 1),
                    (int) (centre_.x - innerR_), (int) (centre_.y - innerR_ * 0.55f),
                    (int) (innerR_ * 2.f), 18, Justification::centred, false);
        g.setFont (Font (FontOptions (13.f, Font::plain)));
        g.setColour (Colours::white.withAlpha (0.8f));
        g.drawText (L.rmsDb <= -199.f ? "-inf" : String (L.rmsDb - (float) offset_, 1) + " dB",
                    (int) (centre_.x - innerR_), (int) (centre_.y - 2.f),
                    (int) (innerR_ * 2.f), 18, Justification::centred, false);
        g.setColour (Colours::white.withAlpha (0.5f));
        g.setFont (Font (FontOptions (11.f, Font::plain)));
        g.drawText (L.peakDb <= -199.f ? "" : "pk " + String (L.peakDb - (float) offset_, 1),
                    (int) (centre_.x - innerR_), (int) (centre_.y + 17.f),
                    (int) (innerR_ * 2.f), 16, Justification::centred, false);
    }
}
