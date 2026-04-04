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

#include "IrInspectorComponent.h"
#include "PluginProcessor.h"
#include <cmath>

//==============================================================================
// IrPlotComponent
//==============================================================================

IrPlotComponent::IrPlotComponent()
{
}

void IrPlotComponent::setIR(const AudioBuffer<float>* irData, double sampleRate, int inCh, int outCh)
{
    currentIR = irData;
    sampleRate_ = sampleRate;
    inCh_ = inCh;
    outCh_ = outCh;
    computeFrequencyResponse();
    repaint();
}

void IrPlotComponent::clearIR()
{
    currentIR = nullptr;
    inCh_ = -1;
    outCh_ = -1;
    magnitudeDb_.clear();
    repaint();
}

void IrPlotComponent::computeFrequencyResponse()
{
    magnitudeDb_.clear();
    if (currentIR == nullptr || currentIR->getNumSamples() == 0)
        return;

    int numSamples = currentIR->getNumSamples();

    // use next power of 2 for FFT, minimum 256
    int fftOrder = (int)std::ceil(std::log2(std::max(numSamples, 256)));
    int fftSize = 1 << fftOrder;

    juce::dsp::FFT fft(fftOrder);

    // FFT needs 2*fftSize floats (interleaved real/imag)
    std::vector<float> fftData(fftSize * 2, 0.f);

    const float* irPtr = currentIR->getReadPointer(0);
    for (int i = 0; i < numSamples; ++i)
        fftData[i] = irPtr[i];

    fft.performFrequencyOnlyForwardTransform(fftData.data());

    // fftData now contains magnitudes for bins 0..fftSize/2
    int numBins = fftSize / 2 + 1;
    magnitudeDb_.resize(numBins);
    freqResolution_ = (float)sampleRate_ / (float)fftSize;

    for (int i = 0; i < numBins; ++i)
    {
        float mag = fftData[i];
        magnitudeDb_[i] = (mag > 1e-10f) ? 20.f * std::log10(mag) : -200.f;
    }
}

void IrPlotComponent::paint(Graphics& g)
{
    g.fillAll(Colour(0xff1a1a2e));

    if (currentIR == nullptr)
    {
        g.setColour(Colours::grey);
        g.setFont(14.f);
        g.drawText("Select an impulse response from the matrix",
                   getLocalBounds(), Justification::centred);
        return;
    }

    auto bounds = getLocalBounds().reduced(8);
    int halfHeight = bounds.getHeight() / 2;

    auto timeArea = bounds.removeFromTop(halfHeight).reduced(2);
    auto freqArea = bounds.reduced(2);

    drawTimeDomain(g, timeArea);
    drawFrequencyDomain(g, freqArea);
}

void IrPlotComponent::resized()
{
}

float IrPlotComponent::hzToX(float hz, float xLeft, float width, float minHz, float maxHz) const
{
    if (hz <= 0.f) hz = minHz;
    float logMin = std::log10(minHz);
    float logMax = std::log10(maxHz);
    float logHz = std::log10(hz);
    return xLeft + width * (logHz - logMin) / (logMax - logMin);
}

float IrPlotComponent::dbToY(float db, float yTop, float height, float minDb, float maxDb) const
{
    return yTop + height * (1.f - (db - minDb) / (maxDb - minDb));
}

void IrPlotComponent::drawDashedLine(Graphics& g, float x1, float y1, float x2, float y2)
{
    const float dashLen = 4.f, gapLen = 3.f;
    float dx = x2 - x1, dy = y2 - y1;
    float length = std::sqrt(dx * dx + dy * dy);
    if (length < 1.f) return;
    float ux = dx / length, uy = dy / length;
    float pos = 0.f;
    while (pos < length)
    {
        float end = jmin(pos + dashLen, length);
        g.drawLine(x1 + ux * pos, y1 + uy * pos, x1 + ux * end, y1 + uy * end, 0.5f);
        pos = end + gapLen;
    }
}

void IrPlotComponent::drawGrid(Graphics& g, Rectangle<int> area, bool isFreqDomain)
{
    g.setColour(Colour(0xff2a2a4a));
    g.fillRect(area);
    g.setColour(Colour(0xff3a3a5a));
    g.drawRect(area);

    if (isFreqDomain)
    {
        float minHz = 20.f;
        float maxHz = (float)(sampleRate_ / 2.0);

        // vertical grid lines at decade frequencies (dashed)
        float freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        for (float f : freqs)
        {
            if (f > maxHz) break;
            float x = hzToX(f, (float)area.getX(), (float)area.getWidth(), minHz, maxHz);

            g.setColour(Colour(0xff666688));
            drawDashedLine(g, x, (float)area.getY(), x, (float)area.getBottom());

            String label;
            if (f >= 1000.f)
                label = String(f / 1000.f, (f == 1000.f || f == 2000.f || f == 5000.f || f == 10000.f || f == 20000.f) ? 0 : 1) + "k";
            else
                label = String((int)f);
            g.setColour(Colours::white.withAlpha(0.7f));
            g.setFont(Font(FontOptions(10.f)));
            g.drawText(label, (int)x - 15, area.getBottom() - 14, 30, 14, Justification::centred);
        }

        // horizontal grid lines (dB, dashed)
        float minDb = -60.f, maxDb = 30.f;
        for (float db = -60.f; db <= 30.f; db += 10.f)
        {
            float y = dbToY(db, (float)area.getY(), (float)area.getHeight(), minDb, maxDb);

            g.setColour(Colour(0xff666688));
            if (db == 0.f)
                g.drawLine((float)area.getX(), y, (float)area.getRight(), y, 0.5f);
            else
                drawDashedLine(g, (float)area.getX(), y, (float)area.getRight(), y);

            if (db == 0.f || db == -20.f || db == -40.f || db == -60.f || db == 20.f)
            {
                g.setColour(Colours::white.withAlpha(0.7f));
                g.setFont(Font(FontOptions(10.f)));
                g.drawText(String((int)db) + "dB", area.getX() + 2, (int)y - 7, 42, 14, Justification::centredLeft);
            }
        }
    }
    else
    {
        // time domain: horizontal center line (solid)
        float cy = (float)area.getCentreY();
        g.setColour(Colour(0xff666688));
        g.drawLine((float)area.getX(), cy, (float)area.getRight(), cy, 0.5f);

        // time labels
        if (currentIR != nullptr)
        {
            int numSamples = currentIR->getNumSamples();
            double durationMs = 1000.0 * numSamples / sampleRate_;
            int numLabels = jmin(10, (int)(durationMs / 5.0) + 1);
            if (numLabels < 2) numLabels = 2;
            for (int i = 0; i <= numLabels; ++i)
            {
                float frac = (float)i / (float)numLabels;
                float x = area.getX() + frac * area.getWidth();

                g.setColour(Colour(0xff666688));
                drawDashedLine(g, x, (float)area.getY(), x, (float)area.getBottom());

                double ms = frac * durationMs;
                String label;
                if (durationMs > 2000.0)
                    label = String(ms / 1000.0, 2) + "s";
                else
                    label = String(ms, 1) + "ms";
                g.setColour(Colours::white.withAlpha(0.7f));
                g.setFont(Font(FontOptions(10.f)));
                g.drawText(label, (int)x - 20, area.getBottom() - 14, 40, 14, Justification::centred);
            }
        }
    }
}

void IrPlotComponent::drawTimeDomain(Graphics& g, Rectangle<int> area)
{
    // title
    g.setColour(Colours::white);
    g.setFont(11.f);
    g.drawText("Time Domain  (In " + String(inCh_ + 1) + " -> Out " + String(outCh_ + 1) + ")",
               area.getX(), area.getY() - 2, area.getWidth(), 16, Justification::centredLeft);
    area.removeFromTop(14);

    drawGrid(g, area, false);

    if (currentIR == nullptr || currentIR->getNumSamples() == 0)
        return;

    const float* data = currentIR->getReadPointer(0);
    int numSamples = currentIR->getNumSamples();

    // find peak for scaling
    float peak = 0.f;
    for (int i = 0; i < numSamples; ++i)
        peak = jmax(peak, std::abs(data[i]));
    if (peak < 1e-10f) peak = 1.f;

    float cx = (float)area.getX();
    float cy = (float)area.getCentreY();
    float halfH = (float)area.getHeight() * 0.45f;
    float w = (float)area.getWidth();

    // y-axis gridlines at peak and half-peak (positive and negative)
    float peakDb = 20.f * std::log10(peak);
    float halfPeakDb = 20.f * std::log10(peak * 0.5f);
    float levels[] = { 1.0f, 0.5f, -0.5f, -1.0f };
    for (float lvl : levels)
    {
        float y = cy - lvl * halfH;
        g.setColour(Colour(0xff666688));
        drawDashedLine(g, (float)area.getX(), y, (float)area.getRight(), y);

        g.setColour(Colours::white.withAlpha(0.55f));
        g.setFont(Font(FontOptions(9.f)));
        float db = (std::abs(lvl) > 0.75f) ? peakDb : halfPeakDb;
        String label = String(db, 1) + "dB";
        if (lvl < 0.f) label = "-" + String(std::abs(db), 1) + "dB";
        else label = String(db, 1) + "dB";
        g.drawText(label, area.getX() + 2, (int)y - 7, 48, 14, Justification::centredLeft);
    }

    Path path;
    bool started = false;

    for (int px = 0; px < (int)w; ++px)
    {
        int sampleStart = (int)((float)px / w * numSamples);
        int sampleEnd = jmin(numSamples, (int)(((float)px + 1.f) / w * numSamples));

        float minVal = 0.f, maxVal = 0.f;
        for (int s = sampleStart; s < sampleEnd; ++s)
        {
            minVal = jmin(minVal, data[s]);
            maxVal = jmax(maxVal, data[s]);
        }

        float yMin = cy - (maxVal / peak) * halfH;
        float yMax = cy - (minVal / peak) * halfH;

        if (!started)
        {
            path.startNewSubPath(cx + px, yMin);
            started = true;
        }
        path.lineTo(cx + px, yMin);
        if (yMax != yMin)
            path.lineTo(cx + px, yMax);
    }

    g.setColour(Colour(0xff44aaff));
    g.strokePath(path, PathStrokeType(1.f));

    // draw peak value label
    g.setColour(Colours::white);
    g.setFont(Font(FontOptions(10.f)));
    String peakStr = String(peakDb, 1) + " dB peak";
    g.drawText(peakStr, area.getRight() - 85, area.getY() + 2, 83, 14, Justification::centredRight);
}

void IrPlotComponent::drawFrequencyDomain(Graphics& g, Rectangle<int> area)
{
    g.setColour(Colours::white);
    g.setFont(11.f);
    g.drawText("Frequency Domain",
               area.getX(), area.getY() - 2, area.getWidth(), 16, Justification::centredLeft);
    area.removeFromTop(14);

    drawGrid(g, area, true);

    if (magnitudeDb_.empty())
        return;

    float minHz = 20.f;
    float maxHz = (float)(sampleRate_ / 2.0);
    float minDb = -60.f;
    float maxDb = 30.f;

    Path path;
    bool started = false;

    int numBins = (int)magnitudeDb_.size();

    for (int i = 1; i < numBins; ++i) // skip DC
    {
        float freq = i * freqResolution_;
        if (freq < minHz || freq > maxHz)
            continue;

        float x = hzToX(freq, (float)area.getX(), (float)area.getWidth(), minHz, maxHz);
        float db = jlimit(minDb, maxDb, magnitudeDb_[i]);
        float y = dbToY(db, (float)area.getY(), (float)area.getHeight(), minDb, maxDb);

        if (!started)
        {
            path.startNewSubPath(x, y);
            started = true;
        }
        else
        {
            path.lineTo(x, y);
        }
    }

    g.setColour(Colour(0xffff8844));
    g.strokePath(path, PathStrokeType(1.f));
}

//==============================================================================
// IrMatrixComponent
//==============================================================================

IrMatrixComponent::IrMatrixComponent()
{
}

void IrMatrixComponent::updateMatrix(ConvolverData& convData)
{
    numInputs_ = convData.getNumInputChannels();
    numOutputs_ = convData.getNumOutputChannels();

    matrix_.clear();
    matrix_.resize(numOutputs_, std::vector<CellInfo>(numInputs_));

    for (int i = 0; i < convData.getNumIRs(); ++i)
    {
        int in = convData.getInCh(i);
        int out = convData.getOutCh(i);
        if (in < numInputs_ && out < numOutputs_)
            matrix_[out][in].irIndex = i;
    }

    setSize(headerSize + numInputs_ * cellSize + 4,
            headerSize + numOutputs_ * cellSize + 4);
    repaint();
}

void IrMatrixComponent::setSelectedCell(int in, int out)
{
    selectedIn_ = in;
    selectedOut_ = out;
    repaint();
}

void IrMatrixComponent::paint(Graphics& g)
{
    g.fillAll(Colour(0xff1a1a2e));

    if (numInputs_ == 0 || numOutputs_ == 0)
    {
        g.setColour(Colours::grey);
        g.setFont(12.f);
        g.drawText("No IRs loaded", getLocalBounds(), Justification::centred);
        return;
    }

    g.setFont(9.f);

    // draw column headers (input channels)
    for (int in = 0; in < numInputs_; ++in)
    {
        int x = headerSize + in * cellSize;
        g.setColour(Colours::white);
        g.drawText(String(in + 1), x, 4, cellSize, headerSize - 6, Justification::centred);
    }

    // draw row headers (output channels)
    for (int out = 0; out < numOutputs_; ++out)
    {
        int y = headerSize + out * cellSize;
        g.setColour(Colours::white);
        g.drawText(String(out + 1), 2, y, headerSize - 4, cellSize, Justification::centredRight);
    }

    // draw header labels
    g.setColour(Colour(0xffaaaacc));
    g.setFont(8.f);
    g.drawText("In", headerSize, 0, numInputs_ * cellSize, 8, Justification::centred);

    // draw rotated "Out" label
    {
        Graphics::ScopedSaveState saveState(g);
        float pivotX = 6.f;
        float pivotY = headerSize + numOutputs_ * cellSize * 0.5f;
        g.addTransform(AffineTransform::rotation(-MathConstants<float>::halfPi, pivotX, pivotY));
        g.drawText("Out", (int)(pivotX - numOutputs_ * cellSize * 0.5f), (int)(pivotY - 4), numOutputs_ * cellSize, 8, Justification::centred);
    }

    // draw cells
    for (int out = 0; out < numOutputs_; ++out)
    {
        for (int in = 0; in < numInputs_; ++in)
        {
            int x = headerSize + in * cellSize;
            int y = headerSize + out * cellSize;
            Rectangle<int> cell(x, y, cellSize - 1, cellSize - 1);

            bool hasIR = matrix_[out][in].irIndex >= 0;
            bool isSelected = (in == selectedIn_ && out == selectedOut_);

            if (isSelected)
                g.setColour(Colour(0xff6688cc));
            else if (hasIR)
                g.setColour(Colour(0xff44aa66));
            else
                g.setColour(Colour(0xff2a2a3a));

            g.fillRect(cell);

            g.setColour(Colour(0xff3a3a5a));
            g.drawRect(cell);
        }
    }
}

void IrMatrixComponent::mouseMove(const MouseEvent& e)
{
    int in = (e.x - headerSize) / cellSize;
    int out = (e.y - headerSize) / cellSize;

    if (in >= 0 && in < numInputs_ && out >= 0 && out < numOutputs_)
    {
        hoveredIn_ = in;
        hoveredOut_ = out;
    }
    else
    {
        hoveredIn_ = -1;
        hoveredOut_ = -1;
    }
}

void IrMatrixComponent::mouseExit(const MouseEvent&)
{
    hoveredIn_ = -1;
    hoveredOut_ = -1;
}

void IrMatrixComponent::mouseDown(const MouseEvent& e)
{
    int in = (e.x - headerSize) / cellSize;
    int out = (e.y - headerSize) / cellSize;

    if (in >= 0 && in < numInputs_ && out >= 0 && out < numOutputs_)
    {
        if (matrix_[out][in].irIndex >= 0)
        {
            setSelectedCell(in, out);
            if (onCellClicked)
                onCellClicked(in, out);
        }
    }
}

//==============================================================================
// IrWireComponent
//==============================================================================

IrWireComponent::IrWireComponent()
{
    setMouseCursor(MouseCursor::PointingHandCursor);
}

void IrWireComponent::updateWires(ConvolverData& convData)
{
    numInputs_ = convData.getNumInputChannels();
    numOutputs_ = convData.getNumOutputChannels();
    wires_.clear();

    for (int i = 0; i < convData.getNumIRs(); ++i)
    {
        WireInfo w;
        w.inCh = convData.getInCh(i);
        w.outCh = convData.getOutCh(i);
        w.irIndex = i;
        wires_.push_back(w);
    }

    setSize(kWidth, getRecommendedHeight(numInputs_, numOutputs_));
    hoveredWire_ = -1;
    repaint();
}

void IrWireComponent::setSelectedWire(int inCh, int outCh)
{
    selectedIn_ = inCh;
    selectedOut_ = outCh;
    repaint();
}

int IrWireComponent::getRecommendedHeight(int numInputs, int numOutputs)
{
    int maxCh = jmax(numInputs, numOutputs);
    return jmax(180, maxCh * kPortSpacing + kMarginTop + 20);
}

Rectangle<float> IrWireComponent::getInputPortRect(int ch) const
{
    float y = (float)kMarginTop + (float)ch * (float)kPortSpacing;
    return { (float)kMarginX, y, (float)kPortW, (float)kPortH };
}

Rectangle<float> IrWireComponent::getOutputPortRect(int ch) const
{
    float y = (float)kMarginTop + (float)ch * (float)kPortSpacing;
    float x = (float)(getWidth() - kMarginX - kPortW);
    return { x, y, (float)kPortW, (float)kPortH };
}

Point<float> IrWireComponent::getInputPortRight(int ch) const
{
    auto r = getInputPortRect(ch);
    return { r.getRight(), r.getCentreY() };
}

Point<float> IrWireComponent::getOutputPortLeft(int ch) const
{
    auto r = getOutputPortRect(ch);
    return { r.getX(), r.getCentreY() };
}

Path IrWireComponent::makeWirePath(int inCh, int outCh) const
{
    auto start = getInputPortRight(inCh);
    auto end = getOutputPortLeft(outCh);
    float dx = (end.x - start.x) * 0.4f;

    Path p;
    p.startNewSubPath(start);
    p.cubicTo(start.x + dx, start.y,
              end.x - dx, end.y,
              end.x, end.y);
    return p;
}

int IrWireComponent::hitTestWire(Point<float> pt) const
{
    float bestDist = 7.f;
    int bestIdx = -1;

    for (int i = 0; i < (int)wires_.size(); ++i)
    {
        auto wire = makeWirePath(wires_[i].inCh, wires_[i].outCh);
        PathFlatteningIterator it(wire, AffineTransform(), 2.f);
        while (it.next())
        {
            float ax = it.x2 - it.x1, ay = it.y2 - it.y1;
            float bx = pt.x - it.x1, by = pt.y - it.y1;
            float len2 = ax * ax + ay * ay;
            float t = (len2 > 0.f) ? jlimit(0.f, 1.f, (bx * ax + by * ay) / len2) : 0.f;
            float ddx = bx - t * ax, ddy = by - t * ay;
            float dist = std::sqrt(ddx * ddx + ddy * ddy);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestIdx = i;
            }
        }
    }
    return bestIdx;
}

String IrWireComponent::getTooltip()
{
    if (hoveredWire_ >= 0 && hoveredWire_ < (int)wires_.size())
        return "In " + String(wires_[hoveredWire_].inCh + 1)
             + ", Out " + String(wires_[hoveredWire_].outCh + 1);
    return {};
}

void IrWireComponent::paint(Graphics& g)
{
    g.fillAll(Colour(0xff1a1a2e));

    if (numInputs_ == 0 || numOutputs_ == 0)
    {
        g.setColour(Colours::grey);
        g.setFont(12.f);
        g.drawText("No IRs loaded", getLocalBounds(), Justification::centred);
        return;
    }

    // Column labels
    g.setFont(Font(FontOptions(9.f, Font::bold)));
    g.setColour(Colours::aquamarine.withAlpha(0.6f));
    g.drawText("IN", kMarginX, 4, kPortW, 14, Justification::centred);
    g.setColour(Colours::orange.withAlpha(0.6f));
    g.drawText("OUT", getWidth() - kMarginX - kPortW, 4, kPortW, 14, Justification::centred);

    // Draw wires (behind ports)
    for (int i = 0; i < (int)wires_.size(); ++i)
    {
        bool isSelected = (wires_[i].inCh == selectedIn_ && wires_[i].outCh == selectedOut_);
        bool isHovered = (i == hoveredWire_);
        bool isDiag = (wires_[i].inCh == wires_[i].outCh);

        Colour wireCol;
        if (isSelected)
            wireCol = Colours::white;
        else if (isDiag)
            wireCol = Colours::aquamarine;
        else
            wireCol = Colours::orange;

        float alpha = (isSelected || isHovered) ? 1.0f : 0.6f;
        float thickness = isSelected ? 3.0f : (isHovered ? 2.5f : 1.8f);

        g.setColour(wireCol.withAlpha(alpha));
        auto wire = makeWirePath(wires_[i].inCh, wires_[i].outCh);
        g.strokePath(wire, PathStrokeType(thickness));
    }

    // Draw input ports
    g.setFont(Font(FontOptions(9.f)));
    for (int ch = 0; ch < numInputs_; ++ch)
    {
        auto r = getInputPortRect(ch);
        g.setColour(Colour(0xff3a3a3a));
        g.fillRoundedRectangle(r, 4.f);
        g.setColour(Colours::aquamarine.withAlpha(0.8f));
        g.drawRoundedRectangle(r, 4.f, 1.f);
        g.setColour(Colours::white);
        g.drawText(String(ch + 1), r.toNearestInt(), Justification::centred);
    }

    // Draw output ports
    for (int ch = 0; ch < numOutputs_; ++ch)
    {
        auto r = getOutputPortRect(ch);
        g.setColour(Colour(0xff3a3a3a));
        g.fillRoundedRectangle(r, 4.f);
        g.setColour(Colours::orange.withAlpha(0.8f));
        g.drawRoundedRectangle(r, 4.f, 1.f);
        g.setColour(Colours::white);
        g.drawText(String(ch + 1), r.toNearestInt(), Justification::centred);
    }
}

void IrWireComponent::mouseMove(const MouseEvent& e)
{
    int idx = hitTestWire(e.position);
    if (idx != hoveredWire_)
    {
        hoveredWire_ = idx;
        repaint();
    }
}

void IrWireComponent::mouseDown(const MouseEvent& e)
{
    int idx = hitTestWire(e.position);
    if (idx >= 0 && idx < (int)wires_.size())
    {
        selectedIn_ = wires_[idx].inCh;
        selectedOut_ = wires_[idx].outCh;
        repaint();
        if (onCellClicked)
            onCellClicked(wires_[idx].inCh, wires_[idx].outCh);
    }
}

void IrWireComponent::mouseExit(const MouseEvent&)
{
    if (hoveredWire_ >= 0)
    {
        hoveredWire_ = -1;
        repaint();
    }
}

//==============================================================================
// IrInspectorComponent
//==============================================================================

IrInspectorComponent::IrInspectorComponent(Mcfx_convolverAudioProcessor& proc)
    : processor_(proc)
{
    titleLabel_.setText("IR Inspector", dontSendNotification);
    titleLabel_.setFont(Font(FontOptions().withHeight(16.f).withStyle("Bold")));
    titleLabel_.setColour(Label::textColourId, Colours::white);
    titleLabel_.setJustificationType(Justification::centredLeft);
    addAndMakeVisible(titleLabel_);

    infoLabel_.setText("Click a wire or cell to inspect the impulse response", dontSendNotification);
    infoLabel_.setFont(Font(FontOptions().withHeight(11.f)));
    infoLabel_.setColour(Label::textColourId, Colour(0xffaaaacc));
    infoLabel_.setJustificationType(Justification::centredLeft);
    addAndMakeVisible(infoLabel_);

    // Set up both views with shared callback
    matrixComponent_.onCellClicked = [this](int in, int out) { onIrSelected(in, out); };
    wireComponent_.onCellClicked = [this](int in, int out) { onIrSelected(in, out); };

    // Restore saved view preference
    showingWires_ = processor_.irInspectorWireView;
    if (showingWires_)
    {
        leftViewport_.setViewedComponent(&wireComponent_, false);
        leftViewport_.setScrollBarsShown(true, false);
    }
    else
    {
        leftViewport_.setViewedComponent(&matrixComponent_, false);
        leftViewport_.setScrollBarsShown(true, true);
    }
    addAndMakeVisible(leftViewport_);

    addAndMakeVisible(plotComponent_);

    // Toggle button
    viewToggleBtn_.setButtonText(showingWires_ ? "Matrix View" : "Wire View");
    viewToggleBtn_.setColour(TextButton::buttonColourId, Colour(0xff2a2a3a));
    viewToggleBtn_.setColour(TextButton::textColourOffId, Colours::white.withAlpha(0.8f));
    viewToggleBtn_.onClick = [this]()
    {
        showingWires_ = !showingWires_;
        processor_.irInspectorWireView = showingWires_;
        if (showingWires_)
        {
            viewToggleBtn_.setButtonText("Matrix View");
            leftViewport_.setViewedComponent(&wireComponent_, false);
            leftViewport_.setScrollBarsShown(true, false);
        }
        else
        {
            viewToggleBtn_.setButtonText("Wire View");
            leftViewport_.setViewedComponent(&matrixComponent_, false);
            leftViewport_.setScrollBarsShown(true, true);
        }
        resized();
        repaint();
    };
    addAndMakeVisible(viewToggleBtn_);

    processor_.addChangeListener(this);

    refresh();
}

IrInspectorComponent::~IrInspectorComponent()
{
    processor_.removeChangeListener(this);
}

void IrInspectorComponent::paint(Graphics& g)
{
    g.fillAll(Colour(0xff16162a));
}

void IrInspectorComponent::resized()
{
    auto area = getLocalBounds().reduced(8);

    auto topRow = area.removeFromTop(24);
    viewToggleBtn_.setBounds(topRow.removeFromRight(85));
    topRow.removeFromRight(4);
    titleLabel_.setBounds(topRow);

    infoLabel_.setBounds(area.removeFromTop(18));
    area.removeFromTop(4);

    int leftWidth;
    if (showingWires_)
    {
        leftWidth = jmin(area.getWidth() / 3, IrWireComponent::kWidth + 20);
        leftWidth = jmax(leftWidth, 200);
    }
    else
    {
        leftWidth = jmin(area.getWidth() / 3, matrixComponent_.getWidth() + 20);
        leftWidth = jmax(leftWidth, 120);
    }

    auto leftArea = area.removeFromLeft(leftWidth);
    leftViewport_.setBounds(leftArea);

    area.removeFromLeft(8);
    plotComponent_.setBounds(area);
}

void IrInspectorComponent::changeListenerCallback(ChangeBroadcaster* source)
{
    refresh();
}

void IrInspectorComponent::refresh()
{
    auto& convData = processor_.getConvolverData();
    matrixComponent_.updateMatrix(convData);
    wireComponent_.updateWires(convData);
    plotComponent_.clearIR();
    matrixComponent_.setSelectedCell(-1, -1);
    wireComponent_.setSelectedWire(-1, -1);
    infoLabel_.setText("Click a wire or cell to inspect the impulse response", dontSendNotification);
    resized();
    repaint();
}

void IrInspectorComponent::onIrSelected(int inCh, int outCh)
{
    auto& convData = processor_.getConvolverData();
    for (int i = 0; i < convData.getNumIRs(); ++i)
    {
        if (convData.getInCh(i) == inCh && convData.getOutCh(i) == outCh)
        {
            auto* irBuf = convData.getIR(i);
            if (irBuf != nullptr)
            {
                plotComponent_.setIR(irBuf, convData.getSampleRate(), inCh, outCh);
                int len = convData.getLength(i);
                double durMs = 1000.0 * len / convData.getSampleRate();
                infoLabel_.setText("In " + String(inCh + 1) + " -> Out " + String(outCh + 1)
                                   + "  |  " + String(len) + " samples  |  "
                                   + String(durMs, 1) + " ms",
                                   dontSendNotification);
            }
            // Sync selection on both views
            matrixComponent_.setSelectedCell(inCh, outCh);
            wireComponent_.setSelectedWire(inCh, outCh);
            return;
        }
    }
}

//==============================================================================
// IrInspectorWindow
//==============================================================================

IrInspectorWindow::IrInspectorWindow(Mcfx_convolverAudioProcessor& processor)
    : DocumentWindow("IR Inspector - mcfx_convolver",
                     Colour(0xff16162a),
                     DocumentWindow::closeButton | DocumentWindow::minimiseButton),
      processor_(processor)
{
    setContentOwned(new IrInspectorComponent(processor), false);
    setResizable(true, true);
    setResizeLimits(600, 400, 1600, 1200);
    setUsingNativeTitleBar(true);

    auto saved = processor_.irInspectorBounds;
    if (saved.isEmpty())
    {
        // First open: center on the display
        setSize(900, 550);
        centreWithSize(getWidth(), getHeight());
    }
    else
    {
        setBounds(saved);
    }

    setVisible(true);
}

IrInspectorWindow::~IrInspectorWindow()
{
    saveBounds();
}

void IrInspectorWindow::closeButtonPressed()
{
    saveBounds();
    setVisible(false);
}

void IrInspectorWindow::moved()
{
    DocumentWindow::moved();
    saveBounds();
}

void IrInspectorWindow::resized()
{
    DocumentWindow::resized();
    saveBounds();
}

void IrInspectorWindow::saveBounds()
{
    if (isVisible())
        processor_.irInspectorBounds = getBounds();
}

bool IrInspectorWindow::keyPressed(const KeyPress& key)
{
    if (key == KeyPress::escapeKey)
    {
        closeButtonPressed();
        return true;
    }
    return DocumentWindow::keyPressed(key);
}
