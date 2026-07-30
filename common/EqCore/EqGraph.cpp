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

#include <array>

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
    startTimer(kRefreshIdleMs);
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
    float fraction = (ypos - ymargin_ / 2) / (height - ymargin_);
    return maxdb_ - fraction * dyn;
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

    // Draw spectrum analyzer (or rolling spectrogram) behind the EQ curves
    if (analyzerOn_ && (inputAnalyzer_ != nullptr || outputAnalyzer_ != nullptr))
    {
        // The transform runs here, on the GUI thread — pushBuffer() only
        // copies. A cheap no-op until half a window of new audio has arrived.
        if (inputAnalyzer_ != nullptr)
            inputAnalyzer_->update();
        if (outputAnalyzer_ != nullptr)
            outputAnalyzer_->update();

        if (spectrogramMode_)
        {
            Rectangle<int> plot((int)xmargin_, (int)(ymargin_ / 2.f),
                                 width - (int)xmargin_,
                                 getHeight() - 12 - (int)(ymargin_ / 2.f));
            drawSpectrogram(g, plot);
        }
        else
        {
            calcAnalyzerPaths();

            // Input spectrum: semi-transparent white, thin
            if (inputAnalyzer_ != nullptr)
            {
                g.setColour(Colour(0x44ffffff));
                g.strokePath(pathAnalyzerIn_, PathStrokeType(0.75f));
            }

            // Output spectrum: yellow, slightly thicker
            if (outputAnalyzer_ != nullptr)
            {
                g.setColour(Colour(0x99ffdd00));
                g.strokePath(pathAnalyzerOut_, PathStrokeType(1.0f));
            }
        }
    }

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

    // Hover readout — bottom-right of the plot, shows freq + magnitude under the cursor.
    if (hoverX_ >= (int) xmargin_ && chain_ != nullptr)
    {
        float f = xpostohz (hoverX_);
        if (f >= minf_ && f <= maxf_)
        {
            auto H = chain_->getFrequencyResponse ((double) f);
            float mag = std::abs (H);
            float magDb = (mag > 1e-12f) ? 20.f * std::log10 (mag) : -200.f;

            String text;
            if (f >= 1000.f) text << String (f / 1000.f, 2) << " kHz   ";
            else              text << String (f, 1)         << " Hz    ";
            text << String (magDb, 1) << " dB";

            Font fnt (FontOptions ("Arial Rounded MT", 11.f, Font::plain));
            g.setFont (fnt);
            int textW = GlyphArrangement::getStringWidthInt (fnt, text);
            int textH = 14;
            int textX = getWidth() - textW - 6;
            int textY = getHeight() - textH - 14;   // above the bottom freq labels
            g.setColour (Colour (0xa0000000));
            g.fillRoundedRectangle ((float) textX - 4, (float) textY - 1,
                                      (float) textW + 8, (float) textH + 2, 3.f);
            g.setColour (Colours::white.withAlpha (0.85f));
            g.drawText (text, textX, textY, textW, textH, Justification::right, false);
        }
    }
}

void EqGraph::drawGrid(Graphics& g)
{
    g.setColour(Colour(0x60ffffff));

    // dB grid labels — anchored to absolute multiples of gridDiv_
    float firstDb = std::ceil(mindb_ / gridDiv_) * gridDiv_;
    for (float db_val = firstDb; db_val <= maxdb_; db_val += gridDiv_)
    {
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
        if (isMajorFreqGridline(f))
        {
            String axislabel = formatFreqAxisLabel(f);
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

            // Dynamic EQ: faint bracket from the static gain to the range limit.
            // Applies to peak/shelf IIR bands and to a broadband Gain compressor.
            bool dynActive = band->supportsDynamic() && band->isDynamicActive();
            if (dynActive)
            {
                float yStatic = (float)dbtoypos(band->getGainDB());
                float yRange  = (float)dbtoypos(band->getGainDB() + band->getDynRangeDB());
                g.setColour(bandCol.withAlpha(enabled ? 0.4f : 0.2f));
                g.drawLine(handleX, yStatic, handleX, yRange, 1.5f);
                g.drawLine(handleX - 4.f, yRange, handleX + 4.f, yRange, 1.5f);
            }

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

            // Dynamic EQ: live "current gain" dot at static gain + current offset.
            if (dynActive && liveDynOffsetProvider_)
            {
                float yLive = (float)dbtoypos(band->getGainDB() + liveDynOffsetProvider_(i));
                float r = 3.5f;
                g.setColour(bandCol.brighter(0.3f));
                g.fillEllipse(handleX - r, yLive - r, r * 2.f, r * 2.f);
                g.setColour(Colours::white.withAlpha(0.85f));
                g.drawEllipse(handleX - r, yLive - r, r * 2.f, r * 2.f, 1.f);
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

    // Live dynamic gain offset (dB) per band — drives the curve so the displayed
    // response moves with the dynamic action. 0 for non-dynamic bands.
    std::vector<float> dynOffset((size_t)jmax(0, numBands), 0.f);
    for (int b = 0; b < numBands; ++b)
    {
        auto* band = chain_->getBand(b);
        if (band != nullptr && liveDynOffsetProvider_ != nullptr
            && band->supportsDynamic() && band->isDynamicActive())
            dynOffset[(size_t)b] = liveDynOffsetProvider_(b);
    }

    int startX = (int)xmargin_;
    bool first = true;

    // Iterate across pixels, accumulating the combined response from the per-band
    // (dynamic-aware) responses so both curves track the live gain.
    for (int xPos = startX; xPos < width; ++xPos)
    {
        float hz = xpostohz(xPos);

        std::complex<float> combined(1.f, 0.f);
        for (int b = 0; b < numBands; ++b)
        {
            auto* band = chain_->getBand(b);
            if (band == nullptr)
                continue;

            auto bandResp = band->getFrequencyResponse(hz, true, dynOffset[(size_t)b]);
            float bandMag = std::abs(bandResp);
            float bandDb = (bandMag > 0.f) ? 20.f * log10f(bandMag) : 0.f;
            float y = (float)dbtoypos(bandDb);

            if (first)
            {
                bandPaths_[b].startNewSubPath((float)xPos, y);
                bandFills_[b].startNewSubPath((float)xPos, zeroY);
                bandFills_[b].lineTo((float)xPos, y);
            }
            else
            {
                bandPaths_[b].lineTo((float)xPos, y);
                bandFills_[b].lineTo((float)xPos, y);
            }

            if (band->isEnabled())
                combined *= bandResp;
        }

        float cmag = std::abs(combined);
        float cdb = (cmag > 0.f) ? 20.f * log10f(cmag) : mindb_;
        float cy = (float)dbtoypos(cdb);
        if (first) pathMag_.startNewSubPath((float)xPos, cy);
        else       pathMag_.lineTo((float)xPos, cy);

        first = false;
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

void EqGraph::calcAnalyzerPaths()
{
    int width = getWidth();
    int startX = (int)xmargin_;
    const float magFloor = 1e-10f;

    // For auto-normalize: find the peak magnitude across both analyzers
    float peakMag = magFloor;
    if (analyzerAutoNormalize_)
    {
        for (int xPos = startX; xPos < width; xPos += 2) // sample every 2 pixels for speed
        {
            float hz = xpostohz(xPos), hzNext = xpostohz(xPos + 2);
            if (inputAnalyzer_ != nullptr)
                peakMag = jmax(peakMag, inputAnalyzer_->getMaxMagnitude(hz, hzNext));
            if (outputAnalyzer_ != nullptr)
                peakMag = jmax(peakMag, outputAnalyzer_->getMaxMagnitude(hz, hzNext));
        }
    }

    float offset = analyzerAutoNormalize_ ? -20.f * log10f(peakMag) : analyzerOffset_;

    auto buildPath = [&](SpectrumAnalyzer* analyzer, Path& path)
    {
        path.clear();
        if (analyzer == nullptr)
            return;

        bool started = false;
        for (int xPos = startX; xPos < width; ++xPos)
        {
            // Peak over the pixel's own frequency span, not a single sample of it:
            // high up the log axis one pixel covers many bins.
            float mag = jmax(magFloor, analyzer->getMaxMagnitude(xpostohz(xPos),
                                                                 xpostohz(xPos + 1)));
            float db = 20.f * log10f(mag) + offset;
            float y = (float)dbtoypos(db);

            if (!started)
            {
                path.startNewSubPath((float)xPos, y);
                started = true;
            }
            else
            {
                path.lineTo((float)xPos, y);
            }
        }
    };

    buildPath(inputAnalyzer_, pathAnalyzerIn_);
    buildPath(outputAnalyzer_, pathAnalyzerOut_);
}

//==============================================================================
// Spectrogram (rolling waterfall)
//==============================================================================
void EqGraph::setSpectrogramMode(bool on)
{
    if (on == spectrogramMode_)
        return;
    spectrogramMode_ = on;
    if (on && ! spectro_.isValid())
    {
        spectro_ = Image(Image::RGB, kSpecW, kSpecH, true);
        spectro_.clear(spectro_.getBounds(), Colour(0xff0a0c10));
        specSmoothDb_.assign(kSpecW, -300.f);
        specWrite_ = 0;
    }
    repaint();
}

void EqGraph::writeSpectroRow()
{
    SpectrumAnalyzer* an = spectroPost_ ? outputAnalyzer_ : inputAnalyzer_;
    if (an == nullptr || ! spectro_.isValid())
        return;

    // Prefer the constant-Q analyzer: its bins are log-spaced like this display,
    // so the bass gets the long window it needs while the treble keeps a short
    // one. A linear-bin FFT can only do one or the other. Kernels are built on
    // first use here (GUI thread) rather than in prepareToPlay, so a session
    // that never opens the spectrogram never pays for them.
    const bool useCQT = (cqt_ != nullptr);
    if (useCQT)
    {
        cqt_->prepare(an->getSampleRate());
        cqt_->compute();
    }

    // Column frequencies never change (minf_/maxf_ are fixed), so the pow per
    // column per row is paid once.
    if (spectroHz_.empty())
    {
        const double ratio = (double)maxf_ / (double)minf_;
        spectroHz_.resize(kSpecW);
        for (int i = 0; i < kSpecW; ++i)
            spectroHz_[(size_t)i] = (double)minf_ * std::pow(ratio, (double)i / kSpecW);
    }

    float col[kSpecW];
    float peak = -250.f;
    for (int i = 0; i < kSpecW; ++i)
    {
        const double hz = spectroHz_[(size_t)i];
        const float  mag = useCQT && cqt_->isReady() ? cqt_->getMagnitude(hz)
                                                     : an->getMagnitude(hz);
        const float  db = 20.f * std::log10(mag + 1e-12f);
        // light per-column smoothing (fast attack / slow release) for a soft look
        float& s = specSmoothDb_[(size_t)i];
        if (s < -250.f) s = db;
        else            s += (db - s) * (db > s ? 0.5f : 0.25f);
        col[i] = s;
        peak = jmax(peak, s);
    }

    // Level mapping matches the spectrum curve: auto-normalize pins the current
    // peak to the top of the scale, otherwise the user's offset shifts an
    // absolute one. Normalizing per row (the old behaviour) made every row reach
    // full scale by construction, so a quiet passage looked as hot as a loud one
    // and the top colour was always present; on an absolute scale the level over
    // time is what the picture actually shows.
    const float offset  = analyzerAutoNormalize_ ? -peak : analyzerOffset_;
    const float floorDb = -kSpectroRangeDb;         // top of the scale is 0 dB

    // The colour ramp — blue (low) through to red (high), with a 0.55 gamma on
    // brightness. One-dimensional, so 256 quantized entries replace a pow plus
    // an HSV conversion per pixel; the raw line pointer replaces
    // setPixelColour's per-pixel format dispatch (the image is created as
    // Image::RGB above, so the rows are PixelRGB).
    static const auto rampLUT = []
    {
        std::array<PixelARGB, 256> a;
        for (size_t i = 0; i < a.size(); ++i)
        {
            const float v   = (float)i / (float)(a.size() - 1);
            const float hue = 0.66f * (1.f - v);
            const float bri = std::pow(v, 0.55f);
            a[i] = Colour::fromHSV(hue, 0.82f, bri, 1.f).getPixelARGB();
        }
        return a;
    }();

    // The row is written through the real pixel stride, not an assumed one:
    // an Image::RGB created with the default (native) image type is stored as
    // 32-bit ARGB on macOS, where CoreGraphics has no 24-bit layout — only the
    // software backend gives 3-byte rows.
    Image::BitmapData bd(spectro_, Image::BitmapData::writeOnly);
    uint8* row = bd.getLinePointer(specWrite_);
    for (int i = 0; i < kSpecW; ++i)
    {
        const float v = (col[i] + offset - floorDb) / kSpectroRangeDb;
        const PixelARGB& p = rampLUT[(size_t)jlimit(0, 255, (int)(v * 255.f + 0.5f))];
        if (bd.pixelStride == 4)
            reinterpret_cast<PixelARGB*>(row)[i].set(p);
        else
            reinterpret_cast<PixelRGB*>(row)[i].set(p);
    }

    specWrite_ = (specWrite_ + 1) % kSpecH;         // newest scrolls in at the bottom
}

void EqGraph::setSpectroSpanSeconds(float seconds)
{
    spectroSpanSec_ = jlimit(kSpanMinSec, kSpanMaxSec, seconds);

    // A row can be written at most once per refresh tick, so that rate sets the
    // shortest span the stored rows can cover. Below it, keep writing every tick
    // and simply show fewer of the newest rows; above it, show them all and
    // write less often.
    const float tickRate = 1000.f / (float) kRefreshFastMs;      // rows/s at most
    const float fullSpan = (float) kSpecH / tickRate;            // span if every tick writes

    if (spectroSpanSec_ <= fullSpan)
    {
        rowsPerTick_ = 1.f;
        visibleRows_ = jlimit(1, kSpecH, roundToInt(spectroSpanSec_ * tickRate));
    }
    else
    {
        rowsPerTick_ = fullSpan / spectroSpanSec_;
        visibleRows_ = kSpecH;
    }
}

void EqGraph::drawSpectrogram(Graphics& g, Rectangle<int> plot)
{
    if (! spectro_.isValid())
        return;
    Graphics::ScopedSaveState save(g);
    g.reduceClipRegion(plot);

    // Draw the newest visibleRows_ rows, oldest at the top. They end at
    // specWrite_ and may wrap around the start of the ring image.
    const int rows  = jlimit(1, kSpecH, visibleRows_);
    const int start = ((specWrite_ - rows) % kSpecH + kSpecH) % kSpecH;
    const int first = jmin(rows, kSpecH - start);               // before wrapping
    const int hTop  = roundToInt(plot.getHeight() * (float) first / rows);

    g.drawImage(spectro_, plot.getX(), plot.getY(), plot.getWidth(), hTop,
                0, start, kSpecW, first);
    if (first < rows)
        g.drawImage(spectro_, plot.getX(), plot.getY() + hTop, plot.getWidth(),
                    plot.getHeight() - hTop, 0, 0, kSpecW, rows - first);
}

void EqGraph::rebuildGridPaths()
{
    int width = getWidth();

    pathGridW_.clear();
    pathGrid_.clear();

    // dB grid lines — anchored to absolute multiples of gridDiv_
    float firstDb = std::ceil(mindb_ / gridDiv_) * gridDiv_;
    for (float db_val = firstDb; db_val <= maxdb_; db_val += gridDiv_)
    {
        int ypos = dbtoypos(db_val);

        // Use a small epsilon to detect 0 dB reliably with float math
        if (std::abs(db_val) < 0.01f)
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

    // Frequency grid lines
    for (float f = minf_; f <= maxf_; f += powf(10, floorf(log10(f))))
    {
        int xpos = hztoxpos(f);
        if (isMajorFreqGridline(f))
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

void EqGraph::resized()
{
    rebuildGridPaths();
}

void EqGraph::timerCallback()
{
    // Refresh fast only while something is actually animating — the spectrum
    // analyzer or a live dynamic band. Otherwise idle slowly to save CPU; GUI
    // edits and mouse moves repaint the graph directly, so interactivity is
    // unaffected by the idle rate.
    bool live = analyzerOn_;
    if (! live && chain_ != nullptr)
    {
        for (int b = 0; b < chain_->getNumBands(); ++b)
        {
            auto* band = chain_->getBand(b);
            if (band != nullptr && band->isDynamicActive()) { live = true; break; }
        }
    }

    const int desired = live ? kRefreshFastMs : kRefreshIdleMs;
    if (getTimerInterval() != desired)
        startTimer(desired);

    if (spectrogramMode_ && analyzerOn_)
    {
        // Fractional scheduler: at long spans a row is written only every few
        // ticks, so the stored rows stretch further back in time.
        rowAccum_ += rowsPerTick_;
        if (rowAccum_ >= 1.f)
        {
            rowAccum_ -= std::floor(rowAccum_);
            writeSpectroRow();
        }
    }

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
    draggingYAxis_ = false;

    if (dragBand_ >= 0)
    {
        if (listener_ != nullptr)
            listener_->eqBandSelected(dragBand_);
    }
    else
    {
        // No band handle hit — start Y-axis pan drag
        draggingYAxis_ = true;
        dragStartMinDb_ = mindb_;
        dragStartMaxDb_ = maxdb_;
        dragStartY_ = e.getPosition().getY();
    }
}

void EqGraph::mouseUp(const MouseEvent&)
{
    dragBand_ = -1;
    draggingYAxis_ = false;
}

void EqGraph::mouseMove(const MouseEvent& e)
{
    int x = e.getPosition().getX();
    if (x != hoverX_)
    {
        hoverX_ = x;
        repaint();
    }
}

void EqGraph::mouseExit(const MouseEvent&)
{
    if (hoverX_ != -1)
    {
        hoverX_ = -1;
        repaint();
    }
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
    hoverX_ = e.getPosition().getX();
    if (draggingYAxis_)
    {
        // Pan the Y-axis: translate dB range by drag distance
        float height = (float)getHeight() - 12.f;
        float dyn = dragStartMaxDb_ - dragStartMinDb_;
        float pixelsPerDb = (height - ymargin_) / dyn;
        float deltaDb = (e.getPosition().getY() - dragStartY_) / pixelsPerDb;

        mindb_ = dragStartMinDb_ + deltaDb;
        maxdb_ = dragStartMaxDb_ + deltaDb;
        rebuildGridPaths();
        repaint();
        return;
    }

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
    // If pointer is near a band handle, adjust Q as before
    int bandIdx = findBandAtPosition(e.getPosition());

    if (bandIdx >= 0 && chain_ != nullptr && listener_ != nullptr)
    {
        auto* band = chain_->getBand(bandIdx);
        if (band != nullptr && band->getType() == EqBandType::IIR)
        {
            float newQ = jlimit(0.1f, 20.f, band->getQ() + wheel.deltaY * 2.f);
            listener_->eqBandQChanged(bandIdx, newQ);
            return;
        }
    }

    // No band handle — zoom the Y-axis range around the pointer position
    float dyn = maxdb_ - mindb_;
    float zoomFactor = 1.f - wheel.deltaY * 0.15f;  // scroll up = zoom in (smaller range)
    zoomFactor = jlimit(0.8f, 1.25f, zoomFactor);

    // Clamp: minimum 6 dB range, maximum 200 dB range
    float newDyn = jlimit(6.f, 200.f, dyn * zoomFactor);

    // Zoom around the dB value at the mouse Y position
    float mouseDb = ypostodb(e.getPosition().getY());
    float ratio = (mouseDb - mindb_) / dyn;  // 0..1 position of mouse in current range

    mindb_ = mouseDb - ratio * newDyn;
    maxdb_ = mouseDb + (1.f - ratio) * newDyn;

    // Snap gridDiv_ to reasonable values based on range
    if (newDyn <= 12.f)       gridDiv_ = 1.f;
    else if (newDyn <= 24.f)  gridDiv_ = 3.f;
    else if (newDyn <= 60.f)  gridDiv_ = 6.f;
    else if (newDyn <= 120.f) gridDiv_ = 12.f;
    else                      gridDiv_ = 24.f;

    rebuildGridPaths();
    repaint();
}
