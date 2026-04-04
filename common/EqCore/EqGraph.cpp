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

#include "EqGraph.h"

// Distinct band colours similar to IEM MultiEQ
static const Colour bandColours[] = {
    Colour(0xff80c0a0),  // 1: muted green
    Colour(0xffb080d0),  // 2: purple
    Colour(0xff6090d0),  // 3: blue
    Colour(0xffa0d040),  // 4: lime green
    Colour(0xffe0c040),  // 5: yellow
    Colour(0xffe08040),  // 6: orange
    Colour(0xffe05050),  // 7: red
    Colour(0xff40c0c0),  // 8: cyan
    Colour(0xffc060a0),  // 9: pink
    Colour(0xff90b060),  // 10: olive
};

Colour EqGraph::getBandColour(int bandIndex)
{
    return bandColours[bandIndex % 10];
}

EqGraph::EqGraph()
{
    setWantsKeyboardFocus(true);
    startTimer(100);
}

EqGraph::~EqGraph()
{
    stopTimer();
}

int EqGraph::dbtoypos(float db_val) const
{
    float height = (float)getHeight() - 12.f;
    float dyn = maxdb_ - mindb_;
    return (int)(ymargin_ / 2 + (height - ymargin_) * (1.f - (db_val - mindb_) / dyn));
}

float EqGraph::ypostodb(int ypos) const
{
    float height = (float)getHeight() - 12.f;
    float dyn = maxdb_ - mindb_;
    return -((ypos - ymargin_ / 2) / (height - ymargin_) * dyn + mindb_);
}

int EqGraph::hztoxpos(float hz_val) const
{
    float width = (float)getWidth();
    return (int)(xmargin_ + (width - xmargin_) * (log(hz_val / minf_) / log(maxf_ / minf_)));
}

float EqGraph::xpostohz(int xpos) const
{
    float width = (float)getWidth();
    return minf_ * powf((maxf_ / minf_), ((xpos - xmargin_) / (width - xmargin_)));
}

void EqGraph::paint(Graphics& g)
{
    int width = getWidth();

    // Background
    g.setGradientFill(ColourGradient(Colour(0xff232338), width / 2.f, getHeight() / 2.f,
                                      Colour(0xff21222a), 2.5f, getHeight() / 2.f, true));
    g.fillRoundedRectangle(xmargin_ - 2, 0.f, width - xmargin_ + 4, (float)getHeight() - 12, 10.f);

    drawGrid(g);

    // Draw magnitude response
    if (chain_ != nullptr)
    {
        calcPaths();

        // Draw individual band filled areas
        for (int i = 0; i < (int)bandFills_.size(); ++i)
        {
            auto* band = chain_->getBand(i);
            if (band == nullptr)
                continue;

            Colour bandCol = getBandColour(i);
            bool enabled = band->isEnabled();
            float alpha;
            if (!enabled)
                alpha = (i == selectedBand_) ? 0.12f : 0.05f;
            else
                alpha = (i == selectedBand_) ? 0.25f : 0.10f;
            g.setColour(bandCol.withAlpha(alpha));
            g.fillPath(bandFills_[i]);
        }

        // Draw individual band response lines
        for (int i = 0; i < (int)bandPaths_.size(); ++i)
        {
            auto* band = chain_->getBand(i);
            if (band == nullptr)
                continue;

            Colour bandCol = getBandColour(i);
            bool enabled = band->isEnabled();
            float lineAlpha;
            if (!enabled)
                lineAlpha = (i == selectedBand_) ? 0.35f : 0.12f;
            else
                lineAlpha = (i == selectedBand_) ? 0.7f : 0.3f;
            g.setColour(bandCol.withAlpha(lineAlpha));
            float lineWidth = enabled ? 1.0f : 0.75f;
            g.strokePath(bandPaths_[i], PathStrokeType(lineWidth));
        }

        // Draw combined response line (white)
        g.setColour(Colour(0xddffffff));
        g.strokePath(pathMag_, PathStrokeType(2.f));
    }

    // Draw band handles
    drawBandHandles(g);
}

void EqGraph::drawGrid(Graphics& g)
{
    g.setColour(Colour(0x60ffffff));

    // dB grid lines
    float dyn = maxdb_ - mindb_;
    int numGridLines = (int)(dyn / gridDiv_) + 1;

    for (int i = 0; i < numGridLines; i++)
    {
        float db_val = maxdb_ - i * gridDiv_;
        int ypos = dbtoypos(db_val);

        String axislabel = String((int)db_val);
        axislabel << "dB";
        g.setFont(Font(FontOptions("Arial Rounded MT", 12.f, Font::plain)));
        g.drawText(axislabel, 0, ypos - 6, 32, 12, Justification::right, false);
    }

    // Freq lines
    for (float f = minf_; f <= maxf_; f += powf(10, floorf(log10(f))))
    {
        int xpos = hztoxpos(f);
        if (f == 50 || f == 100 || f == 500 || f == 1000 || f == 5000 || f == 10000)
        {
            String axislabel = String((int)f);
            g.setFont(Font(FontOptions("Arial Rounded MT", 12.f, Font::plain)));
            g.drawText(axislabel, xpos, getHeight() - 12, 45, 12, Justification::left, false);
        }
    }

    g.setColour(Colour(0x60ffffff));
    g.strokePath(pathGrid_, PathStrokeType(0.25f));
    g.setColour(Colour(0xffffffff));
    g.strokePath(pathGridW_, PathStrokeType(0.25f));
}

void EqGraph::drawBandHandles(Graphics& g)
{
    if (chain_ == nullptr)
        return;

    for (int i = 0; i < chain_->getNumBands(); ++i)
    {
        auto* band = chain_->getBand(i);
        if (band == nullptr)
            continue;

        auto type = band->getType();
        bool enabled = band->isEnabled();

        float handleX = 0.f, handleY = 0.f;
        bool showHandle = false;

        if (type == EqBandType::IIR && !band->hasRawCoefficients())
        {
            handleX = (float)hztoxpos(band->getFrequency());
            handleY = (float)dbtoypos(band->getGainDB());
            showHandle = true;
        }
        else if (type == EqBandType::Gain)
        {
            handleX = (float)(getWidth() / 2);
            handleY = (float)dbtoypos(band->getGainDB());
            showHandle = true;
        }

        if (showHandle)
        {
            float radius = (i == selectedBand_) ? 8.f : 6.f;

            Colour bandCol = getBandColour(i);

            if (!enabled)
            {
                // Disabled band: dimmed outline only, no fill
                Colour outlineCol = bandCol.withAlpha(i == selectedBand_ ? 0.6f : 0.25f);
                g.setColour(outlineCol);
                g.drawEllipse(handleX - radius, handleY - radius, radius * 2, radius * 2, 1.5f);
                g.setFont(Font(FontOptions(radius * 1.4f, Font::bold)));
                g.drawText(String(i + 1), (int)(handleX - radius), (int)(handleY - radius),
                           (int)(radius * 2), (int)(radius * 2), Justification::centred, false);
            }
            else
            {
                // Fill with band colour, brighter when selected
                Colour fillCol = (i == selectedBand_) ? bandCol : bandCol.withAlpha(0.8f);
                g.setColour(fillCol);
                g.fillEllipse(handleX - radius, handleY - radius, radius * 2, radius * 2);
                g.setColour(Colours::white);
                g.drawEllipse(handleX - radius, handleY - radius, radius * 2, radius * 2, 1.f);

                // Draw band number centered on the handle
                g.setColour(bandCol.getPerceivedBrightness() > 0.5f ? Colour(0xff1a1a1a) : Colours::white);
                g.setFont(Font(FontOptions(radius * 1.4f, Font::bold)));
                g.drawText(String(i + 1), (int)(handleX - radius), (int)(handleY - radius),
                           (int)(radius * 2), (int)(radius * 2), Justification::centred, false);
            }
        }
    }
}

void EqGraph::calcPaths()
{
    if (chain_ == nullptr)
        return;

    int width = getWidth();
    int numBands = chain_->getNumBands();
    float zeroY = (float)dbtoypos(0.f);

    pathMag_.clear();
    bandPaths_.resize(numBands);
    bandFills_.resize(numBands);

    for (int b = 0; b < numBands; ++b)
    {
        bandPaths_[b].clear();
        bandFills_[b].clear();
    }

    // First x position
    int startX = (int)xmargin_;
    float startHz = xpostohz(startX);

    // Combined response start
    auto resp = chain_->getFrequencyResponse(startHz);
    float mag = std::abs(resp);
    float db = (mag > 0.f) ? 20.f * log10f(mag) : mindb_;
    pathMag_.startNewSubPath((float)startX, (float)dbtoypos(db));

    // Per-band starts (compute for all bands, including disabled)
    for (int b = 0; b < numBands; ++b)
    {
        auto* band = chain_->getBand(b);
        if (band == nullptr)
            continue;

        auto bandResp = band->getFrequencyResponse(startHz, true);
        float bandMag = std::abs(bandResp);
        float bandDb = (bandMag > 0.f) ? 20.f * log10f(bandMag) : 0.f;
        float y = (float)dbtoypos(bandDb);
        bandPaths_[b].startNewSubPath((float)startX, y);
        bandFills_[b].startNewSubPath((float)startX, zeroY);
        bandFills_[b].lineTo((float)startX, y);
    }

    // Iterate across pixels
    for (int xPos = startX + 1; xPos < width; ++xPos)
    {
        float hz = xpostohz(xPos);

        // Combined
        resp = chain_->getFrequencyResponse(hz);
        mag = std::abs(resp);
        db = (mag > 0.f) ? 20.f * log10f(mag) : mindb_;
        pathMag_.lineTo((float)xPos, (float)dbtoypos(db));

        // Per-band (always compute, even disabled)
        for (int b = 0; b < numBands; ++b)
        {
            auto* band = chain_->getBand(b);
            if (band == nullptr)
                continue;

            auto bandResp = band->getFrequencyResponse(hz, true);
            float bandMag = std::abs(bandResp);
            float bandDb = (bandMag > 0.f) ? 20.f * log10f(bandMag) : 0.f;
            float y = (float)dbtoypos(bandDb);
            bandPaths_[b].lineTo((float)xPos, y);
            bandFills_[b].lineTo((float)xPos, y);
        }
    }

    // Close fills back to zero line
    for (int b = 0; b < numBands; ++b)
    {
        auto* band = chain_->getBand(b);
        if (band == nullptr)
            continue;

        bandFills_[b].lineTo((float)(width - 1), zeroY);
        bandFills_[b].closeSubPath();
    }
}

void EqGraph::resized()
{
    int width = getWidth();

    // Create grid paths
    pathGridW_.clear();
    pathGrid_.clear();

    float dyn = maxdb_ - mindb_;
    int numGridLines = (int)(dyn / gridDiv_) + 1;

    for (int i = 0; i < numGridLines; i++)
    {
        float db_val = maxdb_ - i * gridDiv_;
        int ypos = dbtoypos(db_val);

        if (db_val == 0.f)
        {
            pathGridW_.startNewSubPath(xmargin_, (float)ypos);
            pathGridW_.lineTo((float)width, (float)ypos);
        }
        else
        {
            pathGrid_.startNewSubPath(xmargin_, (float)ypos);
            pathGrid_.lineTo((float)width, (float)ypos);
        }
    }

    for (float f = minf_; f <= maxf_; f += powf(10, floorf(log10(f))))
    {
        int xpos = hztoxpos(f);
        if (f == 50 || f == 100 || f == 500 || f == 1000 || f == 5000 || f == 10000)
        {
            pathGridW_.startNewSubPath((float)xpos, (float)dbtoypos(maxdb_));
            pathGridW_.lineTo((float)xpos, (float)dbtoypos(mindb_));
        }
        else
        {
            pathGrid_.startNewSubPath((float)xpos, (float)dbtoypos(maxdb_));
            pathGrid_.lineTo((float)xpos, (float)dbtoypos(mindb_));
        }
    }
}

void EqGraph::timerCallback()
{
    repaint();
}

int EqGraph::findBandAtPosition(Point<int> pos) const
{
    if (chain_ == nullptr)
        return -1;

    int closest = -1;
    float closestDist = 15.f; // pixel threshold

    for (int i = 0; i < chain_->getNumBands(); ++i)
    {
        auto* band = chain_->getBand(i);
        if (band == nullptr)
            continue;

        float handleX = 0.f, handleY = 0.f;
        bool hasHandle = false;
        bool enabled = band->isEnabled();

        if (band->getType() == EqBandType::IIR && !band->hasRawCoefficients())
        {
            handleX = (float)hztoxpos(band->getFrequency());
            handleY = (float)dbtoypos(band->getGainDB());
            hasHandle = true;
        }
        else if (band->getType() == EqBandType::Gain)
        {
            handleX = (float)(getWidth() / 2);
            handleY = (float)dbtoypos(band->getGainDB());
            hasHandle = true;
        }

        if (hasHandle)
        {
            float dist = pos.getDistanceFrom(Point<int>((int)handleX, (int)handleY));
            if (dist < closestDist)
            {
                closestDist = dist;
                closest = i;
            }
        }
    }

    return closest;
}

void EqGraph::mouseDown(const MouseEvent& e)
{
    grabKeyboardFocus();
    dragBand_ = findBandAtPosition(e.getPosition());
    if (dragBand_ >= 0 && listener_ != nullptr)
        listener_->eqBandSelected(dragBand_);
}

bool EqGraph::keyPressed(const KeyPress& key)
{
    if (key.getTextCharacter() == 'e' || key.getTextCharacter() == 'E')
    {
        if (selectedBand_ >= 0 && listener_ != nullptr)
        {
            listener_->eqBandEnableToggled(selectedBand_);
            return true;
        }
    }
    if (key.getTextCharacter() == 'd' || key.getTextCharacter() == 'D'
        || key.isKeyCode(KeyPress::deleteKey) || key.isKeyCode(KeyPress::backspaceKey))
    {
        if (selectedBand_ >= 0 && listener_ != nullptr)
        {
            listener_->eqBandDeleteRequested(selectedBand_);
            return true;
        }
    }
    if (key.getModifiers().isCommandDown() && key.isKeyCode('Z'))
    {
        if (listener_ != nullptr)
        {
            if (key.getModifiers().isShiftDown())
                listener_->eqRedoRequested();
            else
                listener_->eqUndoRequested();
            return true;
        }
    }
    if (key.isKeyCode(KeyPress::upKey) || key.isKeyCode(KeyPress::downKey))
    {
        if (selectedBand_ >= 0 && chain_ != nullptr && listener_ != nullptr)
        {
            auto* band = chain_->getBand(selectedBand_);
            if (band != nullptr)
            {
                float step = key.getModifiers().isShiftDown() ? 1.0f : 0.25f;
                float delta = key.isKeyCode(KeyPress::upKey) ? step : -step;
                float newGain = jlimit(mindb_, maxdb_, band->getGainDB() + delta);
                listener_->eqBandDragged(selectedBand_, band->getFrequency(), newGain);
                return true;
            }
        }
    }
    if (key.isKeyCode(KeyPress::leftKey) || key.isKeyCode(KeyPress::rightKey))
    {
        if (key.getModifiers().isAltDown())
        {
            // Option + Left/Right: switch between bands
            if (chain_ != nullptr && listener_ != nullptr)
            {
                int numBands = chain_->getNumBands();
                if (numBands > 0)
                {
                    int newIndex;
                    if (key.isKeyCode(KeyPress::leftKey))
                        newIndex = (selectedBand_ <= 0) ? numBands - 1 : selectedBand_ - 1;
                    else
                        newIndex = (selectedBand_ < 0 || selectedBand_ >= numBands - 1) ? 0 : selectedBand_ + 1;
                    listener_->eqBandSelected(newIndex);
                    return true;
                }
            }
        }
        else
        {
            // Left/Right: adjust frequency
            if (selectedBand_ >= 0 && chain_ != nullptr && listener_ != nullptr)
            {
                auto* band = chain_->getBand(selectedBand_);
                if (band != nullptr)
                {
                    float freq = band->getFrequency();
                    // Logarithmic step: multiply/divide by a small factor
                    float factor = key.getModifiers().isShiftDown() ? 1.02f : 1.005f;
                    if (key.isKeyCode(KeyPress::leftKey))
                        freq /= factor;
                    else
                        freq *= factor;
                    freq = jlimit(20.f, 20000.f, freq);
                    listener_->eqBandDragged(selectedBand_, freq, band->getGainDB());
                    return true;
                }
            }
        }
    }
    return false;
}

void EqGraph::mouseDoubleClick(const MouseEvent& e)
{
    if (listener_ == nullptr)
        return;

    int bandAtPos = findBandAtPosition(e.getPosition());
    if (bandAtPos >= 0)
    {
        // Double-click on existing handle: toggle enable/disable
        listener_->eqBandEnableToggled(bandAtPos);
        return;
    }

    // Double-click on empty area: add new band at position
    float freq = jlimit(minf_, maxf_, xpostohz(e.getPosition().getX()));
    float gain = jlimit(mindb_, maxdb_, ypostodb(e.getPosition().getY()));
    listener_->eqBandDoubleClicked(freq, gain);
}

void EqGraph::mouseDrag(const MouseEvent& e)
{
    if (dragBand_ < 0 || chain_ == nullptr || listener_ == nullptr)
        return;

    auto* band = chain_->getBand(dragBand_);
    if (band == nullptr)
        return;

    float f = jlimit(minf_, maxf_, xpostohz(e.getPosition().getX()));
    float g = jlimit(mindb_, maxdb_, ypostodb(e.getPosition().getY()));

    listener_->eqBandDragged(dragBand_, f, g);
}

void EqGraph::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel)
{
    int bandIdx = findBandAtPosition(e.getPosition());
    if (bandIdx < 0)
        bandIdx = selectedBand_;

    if (bandIdx >= 0 && chain_ != nullptr && listener_ != nullptr)
    {
        auto* band = chain_->getBand(bandIdx);
        if (band != nullptr && band->getType() == EqBandType::IIR)
        {
            float newQ = jlimit(0.1f, 20.f, band->getQ() + wheel.deltaY * 2.f);
            listener_->eqBandQChanged(bandIdx, newQ);
        }
    }
}
