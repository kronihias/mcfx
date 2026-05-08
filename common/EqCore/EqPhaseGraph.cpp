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

#include "EqPhaseGraph.h"

#include <cmath>

EqPhaseGraph::EqPhaseGraph()
{
    setOpaque (false);
    startTimerHz (24);   // light refresh; phase only changes when the chain changes
}

EqPhaseGraph::~EqPhaseGraph() = default;

int EqPhaseGraph::hzToXpos (float hz) const
{
    float t = (std::log10 (hz) - std::log10 (minf_))
            / (std::log10 (maxf_) - std::log10 (minf_));
    return (int) (xmargin_ + t * ((float) getWidth() - xmargin_));
}

int EqPhaseGraph::degToYpos (float deg) const
{
    float dyn = maxDeg_ - minDeg_;
    return (int) (ymargin_ / 2 + ((float) getHeight() - ymargin_)
                                  * (1.f - (deg - minDeg_) / dyn));
}

float EqPhaseGraph::xposToHz (int xpos) const
{
    float plotW = (float) getWidth() - xmargin_;
    if (plotW <= 0.f) return minf_;
    float t = ((float) xpos - xmargin_) / plotW;
    return minf_ * std::pow (maxf_ / minf_, t);
}

void EqPhaseGraph::mouseMove (const MouseEvent& e)
{
    int x = e.getPosition().getX();
    if (x != hoverX_) { hoverX_ = x; repaint(); }
}

void EqPhaseGraph::mouseExit (const MouseEvent&)
{
    if (hoverX_ != -1) { hoverX_ = -1; repaint(); }
}

void EqPhaseGraph::rebuildGrid()
{
    int width = getWidth();
    pathGrid_.clear();
    pathGridW_.clear();

    // Degree grid
    float firstDeg = std::ceil (minDeg_ / degStep_) * degStep_;
    for (float d = firstDeg; d <= maxDeg_ + 0.001f; d += degStep_)
    {
        int y = degToYpos (d);
        if (std::abs (d) < 0.01f)
        {
            pathGridW_.startNewSubPath (xmargin_, (float) y);
            pathGridW_.lineTo ((float) width, (float) y);
        }
        else
        {
            pathGrid_.startNewSubPath (xmargin_, (float) y);
            pathGrid_.lineTo ((float) width, (float) y);
        }
    }

    // Frequency grid (every step within each decade)
    auto isMajor = [] (float f) {
        return f == 50 || f == 100 || f == 500 || f == 1000 || f == 5000 || f == 10000;
    };
    for (float f = minf_; f <= maxf_; f += std::pow (10.f, std::floor (std::log10 (f))))
    {
        int x = hzToXpos (f);
        if (isMajor (f))
        {
            pathGridW_.startNewSubPath ((float) x, (float) degToYpos (maxDeg_));
            pathGridW_.lineTo            ((float) x, (float) degToYpos (minDeg_));
        }
        else
        {
            pathGrid_.startNewSubPath ((float) x, (float) degToYpos (maxDeg_));
            pathGrid_.lineTo            ((float) x, (float) degToYpos (minDeg_));
        }
    }
}

void EqPhaseGraph::resized()
{
    rebuildGrid();
}

void EqPhaseGraph::drawGrid (Graphics& g)
{
    g.setColour (Colour (0x60ffffff));

    // Degree labels. The degree sign U+00B0 must go in as proper UTF-8 (two bytes
    // 0xC2 0xB0) — appending the bare 0xB0 byte produces invalid UTF-8 that JUCE
    // renders as a stray digit (the "1800" rendering bug we saw earlier).
    static const String kDegSign = String::fromUTF8 ("\xc2\xb0");
    float firstDeg = std::ceil (minDeg_ / degStep_) * degStep_;
    for (float d = firstDeg; d <= maxDeg_ + 0.001f; d += degStep_)
    {
        int y = degToYpos (d);
        g.setFont (Font (FontOptions ("Arial Rounded MT", 12.f, Font::plain)));
        g.drawText (String ((int) d) + kDegSign, 0, y - 6, (int) xmargin_ - 2, 12,
                    Justification::right, false);
    }

    // Frequency labels (only the major ones)
    for (float f = minf_; f <= maxf_; f += std::pow (10.f, std::floor (std::log10 (f))))
    {
        if (! (f == 50 || f == 100 || f == 500 || f == 1000 || f == 5000 || f == 10000))
            continue;
        int x = hzToXpos (f);
        g.setFont (Font (FontOptions ("Arial Rounded MT", 12.f, Font::plain)));
        g.drawText (String ((int) f), x, getHeight() - 12, 45, 12,
                    Justification::left, false);
    }

    g.setColour (Colour (0x60ffffff));
    g.strokePath (pathGrid_,  PathStrokeType (0.25f));
    g.setColour (Colour (0xffffffff));
    g.strokePath (pathGridW_, PathStrokeType (0.25f));
}

void EqPhaseGraph::calcPath()
{
    pathPhase_.clear();
    if (chain_ == nullptr) return;

    int width  = getWidth();
    int xStart = (int) xmargin_;
    int xEnd   = width - 1;
    int npts   = jmax (32, (xEnd - xStart) + 1);

    bool started = false;
    for (int i = 0; i < npts; ++i)
    {
        float t = (float) i / (float) (npts - 1);
        float f = minf_ * std::pow (maxf_ / minf_, t);

        auto H = chain_->getFrequencyResponse ((double) f);
        if (! std::isfinite (H.real()) || ! std::isfinite (H.imag())) continue;
        if (std::abs (H) < 1e-12f) continue;

        // std::arg returns the principal value in [-π, π], so deg is naturally in
        // [-180, 180]. We deliberately keep the polyline continuous across the wraps
        // — the near-vertical connection line at each ±180 boundary is the standard
        // phase-plot convention (MATLAB / scipy / etc.) and visually conveys the
        // phase rotation rate without requiring separate sub-paths.
        float deg = (float) (std::arg (H) * 180.0 / juce::MathConstants<double>::pi);
        float x = (float) hzToXpos (f);
        float y = (float) degToYpos (deg);

        if (! started) { pathPhase_.startNewSubPath (x, y); started = true; }
        else            pathPhase_.lineTo (x, y);
    }
}

void EqPhaseGraph::paint (Graphics& g)
{
    int width = getWidth();

    // Background — same gradient as EqGraph
    g.setGradientFill (ColourGradient (Colour (0xff232338), width / 2.f, getHeight() / 2.f,
                                         Colour (0xff21222a), 2.5f, getHeight() / 2.f, true));
    g.fillRoundedRectangle (xmargin_ - 2, 0.f,
                              width - xmargin_ + 4, (float) getHeight() - 12, 10.f);

    drawGrid (g);

    if (chain_ != nullptr)
    {
        calcPath();
        g.setColour (Colour (0xddffffff));
        g.strokePath (pathPhase_, PathStrokeType (1.6f));
    }

    // Hover readout — bottom-right of the plot, freq + phase under the cursor.
    if (hoverX_ >= (int) xmargin_ && chain_ != nullptr)
    {
        float f = xposToHz (hoverX_);
        if (f >= minf_ && f <= maxf_)
        {
            auto H = chain_->getFrequencyResponse ((double) f);
            float deg = (float) (std::arg (H) * 180.0 / juce::MathConstants<double>::pi);

            String text;
            if (f >= 1000.f) text << String (f / 1000.f, 2) << " kHz   ";
            else              text << String (f, 1)         << " Hz    ";
            text << String ((int) std::round (deg)) << String::fromUTF8 ("\xc2\xb0");

            Font fnt (FontOptions ("Arial Rounded MT", 11.f, Font::plain));
            g.setFont (fnt);
            int textW = GlyphArrangement::getStringWidthInt (fnt, text);
            int textH = 14;
            int textX = getWidth() - textW - 6;
            int textY = getHeight() - textH - 14;
            g.setColour (Colour (0xa0000000));
            g.fillRoundedRectangle ((float) textX - 4, (float) textY - 1,
                                      (float) textW + 8, (float) textH + 2, 3.f);
            g.setColour (Colours::white.withAlpha (0.85f));
            g.drawText (text, textX, textY, textW, textH, Justification::right, false);
        }
    }
}

void EqPhaseGraph::timerCallback()
{
    repaint();
}
