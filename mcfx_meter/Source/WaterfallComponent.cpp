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

#include "WaterfallComponent.h"

namespace
{
    // Axis gutters. The left one holds the dB scale; the channel numbers sit in
    // their own column just inside it, which is why it is wider than the text.
    constexpr float kLeftAxis   = 52.f;
    constexpr float kRightPad   = 12.f;
    constexpr float kTopPad     = 10.f;
    constexpr float kBottomAxis = 22.f;

    // Share of the width given to the depth axis. Fixing the *displacement*
    // rather than the projection angle is what keeps the plot inside its bounds
    // at any channel count: at 128 channels a fixed 40-degree axis would need
    // ~600 px of depth on its own and leave nothing for the spectrum.
    constexpr float kDepthWidthShare = 0.30f;

    // A ridge is this many depth-steps tall, so a full-scale one occludes about
    // three rows behind it — enough to read as depth, not so much that the front
    // channel hides the plot.
    constexpr float kRidgeStepsTall = 3.5f;
    constexpr float kRidgeMinPx     = 26.f;
    constexpr float kRidgeMaxShare  = 0.45f;

    constexpr float kFreqLoHz = 20.f;
    constexpr float kFreqHiHz = 20000.f;

    const float kGridFreqs[] = { 20.f, 50.f, 100.f, 200.f, 500.f,
                                 1000.f, 2000.f, 5000.f, 10000.f, 20000.f };

    /** Local rather than EqCore's shared formatter: mcfx_meter deliberately does
        not depend on EqCore, which would drag in the whole EQ chain for this. */
    String freqLabel (float hz)
    {
        if (hz >= 1000.f)
        {
            String s (hz / 1000.f, 1);
            if (s.endsWith (".0")) s = s.dropLastCharacters (2);
            return s + "k";
        }
        return String ((int) hz);
    }

    /** Cool to hot, so a loud band glows along an otherwise quiet ridge. */
    Colour levelColour (float t)
    {
        t = jlimit (0.f, 1.f, t);
        return Colour::fromHSV ((1.f - t) * 0.68f, 0.82f, 0.45f + 0.55f * t, 1.f);
    }
}

WaterfallComponent::WaterfallComponent()
{
    setOpaque (false);
    setInterceptsMouseClicks (true, false);

    // Band centres never move, so their x positions are computed once here
    // rather than through a std::log per band per channel per frame.
    const int n = MultiBandAnalyser::kNumBands;
    bandX_.resize ((size_t) n);
    for (int b = 0; b < n; ++b)
        bandX_[(size_t) b] = freqToNorm (MultiBandAnalyser::getBandCentreHz
                                             (MultiBandAnalyser::kLowestBand + b));

    levelNorm_.assign ((size_t) n, 0.f);
    pts_.resize ((size_t) n);
}

void WaterfallComponent::setNumChannels (int numChannels)
{
    const int n = jmax (0, numChannels);
    if (n == numCh_)
        return;

    numCh_ = n;
    hovered_ = -1;
    rebuildLayout();
    repaint();
}

void WaterfallComponent::setOffset (int offsetDb)
{
    if (offsetDb != offset_) { offset_ = offsetDb; repaint(); }
}

void WaterfallComponent::setFloorDb (float db)
{
    const float f = jlimit (-140.f, -20.f, db);
    if (! approximatelyEqual (f, floorDb_)) { floorDb_ = f; repaint(); }
}

float WaterfallComponent::freqToNorm (float hz)
{
    const float h = jlimit (kFreqLoHz, kFreqHiHz, hz);
    return std::log (h / kFreqLoHz) / std::log (kFreqHiHz / kFreqLoHz);
}

float WaterfallComponent::levelToNorm (float db) const
{
    return jlimit (0.f, 1.f, (db - (float) offset_ - floorDb_) / (-floorDb_));
}

Point<float> WaterfallComponent::project (float freqNorm, float levelNorm, int channel) const
{
    const float c = (float) channel;
    return { layout_.originX + freqNorm * layout_.plotW + c * layout_.dx,
             layout_.originY - levelNorm * layout_.ridgeH - c * layout_.dy };
}

void WaterfallComponent::resized()
{
    rebuildLayout();
}

void WaterfallComponent::rebuildLayout()
{
    Layout L;
    L.numCh = numCh_;

    const float availW = jmax (40.f, (float) getWidth()  - kLeftAxis - kRightPad);
    const float availH = jmax (40.f, (float) getHeight() - kTopPad   - kBottomAxis);

    L.originX = kLeftAxis;
    L.originY = kTopPad + availH;

    if (numCh_ <= 1)
    {
        L.plotW  = availW;
        L.ridgeH = availH * 0.8f;
        L.dx = L.dy = 0.f;
        layout_ = L;
        return;
    }

    const float depthW = kDepthWidthShare * availW;
    L.plotW = availW - depthW;
    L.dx    = depthW / (float) (numCh_ - 1);

    // Solve (n-1)*step + kRidgeStepsTall*step == availH, then clamp the ridge and
    // give whatever is left to the depth axis. Past ~64 channels the ridge hits
    // its floor and the rows compress instead, which is the right trade: a 26 px
    // ridge still shows a shape, a 2 px one does not.
    float step  = availH / ((float) numCh_ + kRidgeStepsTall - 1.f);
    float ridge = kRidgeStepsTall * step;

    const float ridgeMax = kRidgeMaxShare * availH;
    const float ridgeMin = jmin (kRidgeMinPx, availH * 0.5f);

    if (ridge > ridgeMax)       ridge = ridgeMax;
    else if (ridge < ridgeMin)  ridge = ridgeMin;

    step = (availH - ridge) / (float) (numCh_ - 1);

    L.ridgeH = ridge;
    L.dy     = step;

    layout_ = L;
}

void WaterfallComponent::paintGrid (Graphics& g) const
{
    const int n = jmax (1, numCh_);
    const int back = n - 1;

    // --- floor lines, one per decade marker, receding into depth ---
    const bool sparseLabels = layout_.plotW < 300.f;
    int idx = 0;
    for (float hz : kGridFreqs)
    {
        const float fn = freqToNorm (hz);
        const auto  a  = project (fn, 0.f, 0);
        const auto  b  = project (fn, 0.f, back);

        g.setColour (Colours::white.withAlpha (0.09f));
        g.drawLine (a.x, a.y, b.x, b.y, 0.6f);

        if (! sparseLabels || (idx % 2) == 0)
        {
            g.setColour (Colours::white.withAlpha (0.45f));
            g.setFont (Font (FontOptions (10.f, Font::plain)));
            g.drawText (freqLabel (hz), (int) (a.x - 20.f), (int) (a.y + 4.f), 40, 14,
                        Justification::centred, false);
        }
        ++idx;
    }

    // --- front baseline and the two receding edges ---
    {
        const auto f0 = project (0.f, 0.f, 0);
        const auto f1 = project (1.f, 0.f, 0);
        const auto b0 = project (0.f, 0.f, back);
        g.setColour (Colours::white.withAlpha (0.28f));
        g.drawLine (f0.x, f0.y, f1.x, f1.y, 0.8f);
        g.setColour (Colours::white.withAlpha (0.18f));
        g.drawLine (f0.x, f0.y, b0.x, b0.y, 0.8f);
    }

    // --- dB scale up the front-left edge. Past ~64 channels the ridge is only
    //     its minimum tall, and a 20 dB grid would stack five labels inside
    //     26 px of it, so the step follows the space actually available. ---
    const float dbStep = layout_.ridgeH >= 150.f ? 20.f
                       : layout_.ridgeH >= 80.f  ? 40.f
                                                 : -floorDb_;
    g.setFont (Font (FontOptions (10.f, Font::plain)));
    for (float db = 0.f; db >= floorDb_ - 0.5f; db -= dbStep)
    {
        const float ln = (db - floorDb_) / (-floorDb_);
        const auto  p  = project (0.f, ln, 0);
        g.setColour (Colours::white.withAlpha (0.22f));
        g.drawLine (p.x - 4.f, p.y, p.x, p.y, 0.8f);
        g.setColour (Colours::white.withAlpha (0.45f));
        g.drawText (String ((int) (db + (float) offset_)),
                    2, (int) (p.y - 7.f), 26, 14,
                    Justification::centredRight, false);
    }

    // --- resolution floor: below it the bands are narrower than one FFT bin,
    //     so adjacent bands share bins and the curve is a staircase. Showing
    //     where that starts beats interpolating it away. ---
    if (analyser_ != nullptr)
    {
        const float fHz = analyser_->getResolutionFloorHz();
        if (fHz > kFreqLoHz && fHz < kFreqHiHz)
        {
            const float fn = freqToNorm (fHz);
            const auto  a  = project (fn, 0.f, 0);
            const auto  b  = project (fn, 0.f, back);
            g.setColour (Colours::orange.withAlpha (0.22f));
            g.drawLine (a.x, a.y, b.x, b.y, 1.f);
        }
    }
}

void WaterfallComponent::paintRidges (Graphics& g)
{
    if (analyser_ == nullptr || ! analyser_->isReady() || numCh_ <= 0)
        return;

    const int nBands = MultiBandAnalyser::kNumBands;
    const float resFloorNorm = freqToNorm (analyser_->getResolutionFloorHz());
    const float denom = jmax (1.f, (float) (numCh_ - 1));

    // Back to front: each fill occludes what is behind it, which is exact here
    // because the ridges are parallel planes that cannot intersect.
    for (int ch = numCh_ - 1; ch >= 0; --ch)
    {
        const float* bands = analyser_->getChannelBands (ch);
        if (bands == nullptr)
            continue;

        const float depth = (float) ch / denom;          // 1 = furthest away

        skirt_.clear();
        skirt_.preallocateSpace ((nBands + 3) * 3);

        for (int b = 0; b < nBands; ++b)
        {
            const float ln = levelToNorm (Decibels::gainToDecibels (bands[b], -200.f));
            levelNorm_[(size_t) b] = ln;
            pts_[(size_t) b] = project (bandX_[(size_t) b], ln, ch);

            if (b == 0)
            {
                skirt_.startNewSubPath (project (bandX_[0], 0.f, ch));
                skirt_.lineTo (pts_[0]);
            }
            else
            {
                skirt_.lineTo (pts_[(size_t) b]);
            }
        }

        skirt_.lineTo (project (bandX_[(size_t) (nBands - 1)], 0.f, ch));
        skirt_.closeSubPath();

        // The fill is what hides the ridges behind. Slightly translucent so the
        // stack still reads as depth rather than as a wall.
        g.setColour (Colour::fromFloatRGBA (0.05f, 0.055f, 0.075f, 0.90f)
                         .brighter (0.16f * depth));
        g.fillPath (skirt_);

        // Stroke segment by segment so one hot band glows on an otherwise cool
        // ridge — that is what makes an outlier findable at 128 channels.
        // Measured: batching these into per-colour paths and calling strokePath
        // is *slower* than drawLine, which has a fast path for a single segment.
        const float alpha = (ch == hovered_) ? 1.f : (0.95f - 0.45f * depth);
        const float thick = (ch == hovered_) ? 1.8f : 1.1f;

        for (int b = 1; b < nBands; ++b)
        {
            const auto& pA = pts_[(size_t) (b - 1)];
            const auto& pB = pts_[(size_t) b];

            // Below the resolution floor adjacent bands share FFT bins, so the
            // curve there is a staircase rather than detail. Dim it rather than
            // interpolating it away.
            const float dim = (bandX_[(size_t) b] < resFloorNorm) ? 0.45f : 1.f;

            g.setColour (levelColour (0.5f * (levelNorm_[(size_t) (b - 1)]
                                              + levelNorm_[(size_t) b]))
                             .withAlpha (alpha * dim));
            g.drawLine (pA.x, pA.y, pB.x, pB.y, thick);
        }
    }
}

int WaterfallComponent::channelAt (Point<float> p) const
{
    if (numCh_ <= 0 || analyser_ == nullptr || ! analyser_->isReady())
        return -1;

    int best = -1;
    float bestDy = 1.0e9f;

    for (int ch = 0; ch < numCh_; ++ch)
    {
        // Which frequency this x would be on *this* channel's ridge.
        const float fn = (p.x - layout_.originX - (float) ch * layout_.dx) / jmax (1.f, layout_.plotW);
        if (fn < 0.f || fn > 1.f)
            continue;

        const float* bands = analyser_->getChannelBands (ch);
        if (bands == nullptr)
            continue;

        const int b = jlimit (0, MultiBandAnalyser::kNumBands - 1,
                              roundToInt (fn * (float) (MultiBandAnalyser::kNumBands - 1)));
        const float y = project (fn, levelToNorm (Decibels::gainToDecibels (bands[b], -200.f)), ch).y;
        const float d = std::abs (y - p.y);

        if (d < bestDy) { bestDy = d; best = ch; }
    }

    return bestDy < 14.f ? best : -1;
}

void WaterfallComponent::mouseMove (const MouseEvent& e)
{
    const int h = channelAt (e.position);
    if (h != hovered_) { hovered_ = h; repaint(); }
}

void WaterfallComponent::mouseExit (const MouseEvent&)
{
    if (hovered_ != -1) { hovered_ = -1; repaint(); }
}

void WaterfallComponent::paint (Graphics& g)
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    paintGrid (g);
    paintRidges (g);

    // --- channel labels along the receding left edge ---
    if (numCh_ > 0)
    {
        // Thin the numbers out as the rows compress: at 128 channels they are
        // under 4 px apart, so labelling every ridge would just be a grey bar.
        const int every = numCh_ <= 16 ? 1 : (numCh_ <= 64 ? 4 : 8);
        g.setFont (Font (FontOptions (9.f, Font::plain)));

        for (int ch = 0; ch < numCh_; ++ch)
        {
            if (ch != 0 && ch != numCh_ - 1 && (ch % every) != 0)
                continue;

            const auto p = project (0.f, 0.f, ch);
            g.setColour (Colours::white.withAlpha (ch == hovered_ ? 1.f : 0.4f));
            g.drawText (String (ch + 1), (int) (p.x - 24.f), (int) (p.y - 7.f), 20, 14,
                        Justification::centredRight, false);
        }
    }

    if (hovered_ >= 0)
    {
        g.setColour (Colours::white);
        g.setFont (Font (FontOptions (12.f, Font::bold)));
        g.drawText ("ch " + String (hovered_ + 1), getWidth() - 90, 4, 82, 16,
                    Justification::centredRight, false);
    }
}
