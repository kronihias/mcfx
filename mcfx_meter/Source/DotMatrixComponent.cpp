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

#include "DotMatrixComponent.h"
#include "meter.h"          // MeterComponent's gradient resources + iec_scale

extern float iec_scale (float dB);

namespace
{
    constexpr float kDotFill    = 0.38f;   // dot radius as a fraction of the cell
    constexpr int   kRampSize   = 128;
    // Below this a channel counts as silent and draws as an outline only.
    constexpr float kSilenceDb  = -90.f;
    // Strip along the bottom for the selected channel's numbers. Taken out of
    // the grid rather than drawn over it, or the readout covers the last row.
    constexpr float kReadoutH   = 18.f;
}

DotMatrixComponent::DotMatrixComponent()
{
    setOpaque (false);
    setInterceptsMouseClicks (true, false);

    gradientImage_ = ImageCache::getFromMemory (MeterComponent::meter_gradient_png,
                                                MeterComponent::meter_gradient_pngSize);

    // Sample the bar's gradient once. Its scale marks are baked in as rows of
    // pure white, which would show up here as bogus bright levels, so those
    // rows are stepped over the same way the ring does it.
    ramp_.resize ((size_t) kRampSize);
    const int h = jmax (1, gradientImage_.getHeight());

    for (int i = 0; i < kRampSize; ++i)
    {
        const float t = (float) i / (float) (kRampSize - 1);       // 0 = silent
        const int y0 = jlimit (0, h - 1, (int) ((1.f - t) * (float) (h - 1)));

        Colour c = gradientImage_.isValid() ? gradientImage_.getPixelAt (0, y0) : Colours::green;
        for (int d = 1; d < h && gradientImage_.isValid()
                        && c.getRed() == 255 && c.getGreen() == 255 && c.getBlue() == 255; ++d)
        {
            const int up = y0 - d, dn = y0 + d;
            if (up >= 0)      c = gradientImage_.getPixelAt (0, up);
            else if (dn < h)  c = gradientImage_.getPixelAt (0, dn);
        }
        ramp_[(size_t) i] = c.withAlpha (1.f);
    }
}

void DotMatrixComponent::setNumChannels (int numChannels)
{
    const int n = jmax (0, numChannels);
    if (n == numCh_)
        return;

    numCh_ = n;
    levels_.assign ((size_t) n, ChannelLevel{});
    hovered_ = -1;
    resized();
    repaint();
}

void DotMatrixComponent::setValue (int channel, float rms, float peak, float peakHold)
{
    if (! isPositiveAndBelow (channel, numCh_))
        return;

    auto& L = levels_[(size_t) channel];
    L.rmsDb  = Decibels::gainToDecibels (rms,      -200.f);
    L.peakDb = Decibels::gainToDecibels (peak,     -200.f);
    L.holdDb = Decibels::gainToDecibels (peakHold, -200.f);
}

void DotMatrixComponent::setOffset (int offsetDb)
{
    if (offsetDb != offset_) { offset_ = offsetDb; repaint(); }
}

void DotMatrixComponent::setPeakHoldVisible (bool shouldBeVisible)
{
    if (shouldBeVisible != peakHold_) { peakHold_ = shouldBeVisible; repaint(); }
}

DotMatrixComponent::Grid DotMatrixComponent::computeGrid (int numChannels, float width, float height)
{
    Grid g;
    if (numChannels <= 0 || width <= 0.f || height <= 0.f)
        return g;

    // Pick the column count whose cell is largest: the grid should follow the
    // window's aspect, so a wide strip lays out in a row or two and a square
    // window in a square block, rather than always being sqrt(n) wide.
    float best = -1.f;
    for (int cols = 1; cols <= numChannels; ++cols)
    {
        const int rows = (numChannels + cols - 1) / cols;
        const float cell = jmin (width / (float) cols, height / (float) rows);
        if (cell > best) { best = cell; g.cols = cols; g.rows = rows; }
    }

    g.cell = best;
    // Centre the block in whichever direction has slack.
    g.originX = 0.5f * (width  - g.cell * (float) g.cols);
    g.originY = 0.5f * (height - g.cell * (float) g.rows);
    return g;
}

void DotMatrixComponent::resized()
{
    grid_ = computeGrid (numCh_, (float) getWidth(),
                         jmax (10.f, (float) getHeight() - kReadoutH));
}

Point<float> DotMatrixComponent::dotCentre (int channel) const
{
    if (! isPositiveAndBelow (channel, numCh_) || grid_.cols <= 0)
        return {};

    const int col = channel % grid_.cols;
    const int row = channel / grid_.cols;
    return { grid_.originX + ((float) col + 0.5f) * grid_.cell,
             grid_.originY + ((float) row + 0.5f) * grid_.cell };
}

int DotMatrixComponent::channelAt (Point<float> p) const
{
    if (numCh_ <= 0 || grid_.cell <= 0.f)
        return -1;

    const int col = (int) std::floor ((p.x - grid_.originX) / grid_.cell);
    const int row = (int) std::floor ((p.y - grid_.originY) / grid_.cell);
    if (col < 0 || col >= grid_.cols || row < 0 || row >= grid_.rows)
        return -1;

    const int ch = row * grid_.cols + col;
    return isPositiveAndBelow (ch, numCh_) ? ch : -1;
}

Colour DotMatrixComponent::colourForDb (float db) const
{
    const float s = jlimit (0.f, 1.f, iec_scale (db - (float) offset_));
    return ramp_[(size_t) jlimit (0, kRampSize - 1, (int) (s * (float) (kRampSize - 1)))];
}

void DotMatrixComponent::mouseMove (const MouseEvent& e)
{
    const int h = channelAt (e.position);
    if (h != hovered_) { hovered_ = h; repaint(); }
}

void DotMatrixComponent::mouseExit (const MouseEvent&)
{
    if (hovered_ != -1) { hovered_ = -1; repaint(); }
}

void DotMatrixComponent::setSelectedChannel (int channel)
{
    const int c = isPositiveAndBelow (channel, numCh_) ? channel : -1;
    if (c != selected_) { selected_ = c; repaint(); }
}

void DotMatrixComponent::mouseUp (const MouseEvent& e)
{
    // Selecting, not resetting: the selection has to survive the mouse leaving,
    // so it cannot be the hover, and a click is the obvious way to set it.
    const int ch = channelAt (e.position);
    if (ch != selected_)
    {
        setSelectedChannel (ch);
        if (onChannelSelected != nullptr)
            onChannelSelected (ch);
    }
}

void DotMatrixComponent::mouseDoubleClick (const MouseEvent& e)
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

void DotMatrixComponent::paint (Graphics& g)
{
    if (numCh_ <= 0 || grid_.cell <= 0.f)
        return;

    const float r = grid_.cell * kDotFill;
    // Numbers only while they would still be readable inside a dot.
    const bool  labels   = grid_.cell >= 26.f;
    const float labelPt  = jlimit (8.f, 13.f, grid_.cell * 0.30f);

    for (int ch = 0; ch < numCh_; ++ch)
    {
        const auto  c  = dotCentre (ch);
        const auto& L  = levels_[(size_t) ch];
        const bool  on = L.peakDb > kSilenceDb;

        const auto box = Rectangle<float> (c.x - r, c.y - r, r * 2.f, r * 2.f);

        if (on)
        {
            g.setColour (colourForDb (L.rmsDb));
            g.fillEllipse (box);

            // Peak as a ring around the dot, so a channel that is loud but
            // quiet on average still reads — the pair is what the bar view
            // shows as body plus tick.
            const float pk = jlimit (0.f, 1.f, iec_scale (L.peakDb - (float) offset_));
            g.setColour ((L.peakDb - (float) offset_ > 0.f ? Colours::red
                                                           : Colours::white).withAlpha (0.55f + 0.45f * pk));
            g.drawEllipse (box.expanded (1.5f), jmax (1.f, r * 0.14f));

            if (peakHold_ && L.holdDb > kSilenceDb)
            {
                g.setColour (L.holdDb - (float) offset_ > 0.f ? Colours::red : Colours::yellow);
                g.drawEllipse (box.expanded (r * 0.28f), jmax (1.f, r * 0.10f));
            }
        }
        else
        {
            // Present but silent: an outline, not a hole in the grid.
            g.setColour (Colours::white.withAlpha (0.16f));
            g.drawEllipse (box, 1.f);
        }

        if (ch == selected_)
        {
            g.setColour (Colours::aquamarine);
            g.drawEllipse (box.expanded (r * 0.45f), jmax (1.5f, r * 0.12f));
        }
        else if (ch == hovered_)
        {
            g.setColour (Colours::white.withAlpha (0.75f));
            g.drawEllipse (box.expanded (r * 0.45f), 1.f);
        }

        if (labels)
        {
            g.setFont (Font (FontOptions (labelPt, Font::plain)));
            // Dark text on a lit dot, light on an unlit one.
            g.setColour (on ? Colours::black.withAlpha (0.65f)
                            : Colours::white.withAlpha (0.45f));
            g.drawText (String (ch + 1), box, Justification::centred, false);
        }
    }

    // Numbers for one channel, bottom-left where they never cover the grid.
    // The hover previews; the selection is what stays once the mouse leaves.
    const int readout = hovered_ >= 0 ? hovered_ : selected_;

    if (isPositiveAndBelow (readout, numCh_))
    {
        const auto& L = levels_[(size_t) readout];
        auto dbText = [this] (float db)
        {
            return db <= -199.f ? String ("-inf")
                                : String (db - (float) offset_, 1) + " dB";
        };

        String s;
        s << "ch " << (readout + 1)
          << "    rms " << dbText (L.rmsDb)
          << "    pk "  << dbText (L.peakDb);
        if (peakHold_)
            s << "    hold " << dbText (L.holdDb);

        g.setFont (Font (FontOptions (12.f, Font::bold)));
        g.setColour (readout == selected_ && hovered_ < 0 ? Colours::aquamarine : Colours::white);
        g.drawText (s, 4, (int) ((float) getHeight() - kReadoutH), getWidth() - 8, (int) kReadoutH,
                    Justification::centredLeft, false);
    }
}
