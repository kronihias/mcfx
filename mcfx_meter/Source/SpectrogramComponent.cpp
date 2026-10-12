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

#include "SpectrogramComponent.h"

namespace
{
    constexpr float kSpanSec[] = { 2.f, 5.f, 10.f, 30.f, 60.f };
    constexpr float kTopDb[]   = { 0.f, -10.f, -20.f, -30.f, -40.f };
    constexpr float kRangeDb[] = { 48.f, 72.f, 96.f, 120.f };
    template <typename T, size_t N> constexpr int countOf (const T (&)[N]) { return (int) N; }

    constexpr int   kToolbarH = 22;
    constexpr int   kPlotTop  = 28;
    constexpr float kMarginL  = 46.f, kMarginR = 4.f, kMarginB = 16.f;
    constexpr float kLaneGap  = 6.f;

    constexpr float kAvgTauSec   = 1.f;     // the strip's average
    constexpr float kPeakHoldSec = 2.f;     // then it falls...
    constexpr float kPeakFallDbS = 20.f;    // ...at this rate
    constexpr float kFloorDb     = -200.f;

    // Inferno-like: dark, through purple and red, to pale yellow. Monotonic in
    // lightness, so the level reads correctly in greyscale and on a projector
    // that washes colours out.
    const uint8 kLutAnchors[][3] = {
        {   0,   0,   4 }, {  31,  12,  72 }, {  85,  15, 109 }, { 136,  34, 106 },
        { 186,  54,  85 }, { 227,  89,  51 }, { 249, 140,  10 }, { 249, 201,  50 },
        { 252, 255, 164 } };

    const Colour kAccent (0xffff9a3d);
}

//==============================================================================
SpectrogramComponent::SpectrogramComponent()
{
    setOpaque (false);
    settings_ = &ownSettings_;

    // 256-entry colour table from the anchors above.
    lut_.resize (256);
    const int nA = (int) (sizeof (kLutAnchors) / sizeof (kLutAnchors[0]));
    for (int i = 0; i < 256; ++i)
    {
        const float t = (float) i / 255.f * (float) (nA - 1);
        const int a = jmin (nA - 2, (int) t);
        const float f = t - (float) a;
        auto mix = [&] (int k) { return (uint8) roundToInt ((1.f - f) * kLutAnchors[a][k] + f * kLutAnchors[a + 1][k]); };
        lut_[(size_t) i] = Colour (mix (0), mix (1), mix (2));
    }

    auto setupCombo = [this] (ComboBox& cb, const String& tip)
    {
        addAndMakeVisible (cb);
        cb.setTooltip (tip);
        cb.addListener (this);
    };

    setupCombo (cbFirst_, "First channel analysed.");
    setupCombo (cbLast_,  "Last channel analysed (at most " + String (kMaxChannels) + " at once). "
                          "Set it to the first one for a single channel.");

    setupCombo (cbLayout_, "Average the channels' power into one picture, or give each its own lane.");
    cbLayout_.addItem ("average", 1);
    cbLayout_.addItem ("stacked", 2);

    setupCombo (cbSpan_, "How much history is visible.");
    for (int i = 0; i < countOf (kSpanSec); ++i)
        cbSpan_.addItem (String ((int) kSpanSec[i]) + " s", i + 1);

    setupCombo (cbTop_, "Level at the top of the colour scale (dBFS; a full-scale sine is 0).");
    for (int i = 0; i < countOf (kTopDb); ++i)
        cbTop_.addItem ("top " + String ((int) kTopDb[i]) + " dB", i + 1);

    setupCombo (cbRange_, "Levels this far below the top fade to black.");
    for (int i = 0; i < countOf (kRangeDb); ++i)
        cbRange_.addItem ("range " + String ((int) kRangeDb[i]) + " dB", i + 1);

    addAndMakeVisible (btnPause_);
    btnPause_.setButtonText ("pause");
    btnPause_.setClickingTogglesState (true);
    btnPause_.setColour (TextButton::buttonOnColourId, Colour (0xffb03030));
    btnPause_.setTooltip ("Stop scrolling to read the picture. Hover for frequency, note, level and time.");
    btnPause_.addListener (this);

    syncControlsFromSettings();
}

void SpectrogramComponent::setSettings (SpectroSettings* settings)
{
    settings_ = settings != nullptr ? settings : &ownSettings_;
    clampSettings();
    syncControlsFromSettings();
    resetHistory();
}

void SpectrogramComponent::setNumChannels (int numChannels)
{
    const int n = jmax (1, numChannels);
    if (n != numCh_ || cbFirst_.getNumItems() != n)
    {
        numCh_ = n;
        cbFirst_.clear (dontSendNotification);
        cbLast_.clear (dontSendNotification);
        for (int ch = 0; ch < n; ++ch)
        {
            cbFirst_.addItem ("ch " + String (ch + 1), ch + 1);
            cbLast_.addItem  ("to " + String (ch + 1), ch + 1);
        }
        clampSettings();
        syncControlsFromSettings();
        resetHistory();
    }
}

void SpectrogramComponent::clampSettings()
{
    auto& s = *settings_;
    s.firstCh  = jlimit (0, numCh_ - 1, s.firstCh);
    s.lastCh   = jlimit (s.firstCh, jmin (numCh_ - 1, s.firstCh + kMaxChannels - 1), s.lastCh);
    s.spanIdx  = jlimit (0, countOf (kSpanSec) - 1, s.spanIdx);
    s.topIdx   = jlimit (0, countOf (kTopDb) - 1, s.topIdx);
    s.rangeIdx = jlimit (0, countOf (kRangeDb) - 1, s.rangeIdx);
}

void SpectrogramComponent::syncControlsFromSettings()
{
    cbFirst_.setSelectedId (settings_->firstCh + 1, dontSendNotification);
    cbLast_.setSelectedId (settings_->lastCh + 1, dontSendNotification);
    cbLayout_.setSelectedId (settings_->stacked ? 2 : 1, dontSendNotification);
    cbSpan_.setSelectedId (settings_->spanIdx + 1, dontSendNotification);
    cbTop_.setSelectedId (settings_->topIdx + 1, dontSendNotification);
    cbRange_.setSelectedId (settings_->rangeIdx + 1, dontSendNotification);
}

int SpectrogramComponent::numShown() const { return settings_->lastCh - settings_->firstCh + 1; }
int SpectrogramComponent::numLanes() const { return settings_->stacked ? numShown() : 1; }

float SpectrogramComponent::topDb() const   { return kTopDb[settings_->topIdx]; }
float SpectrogramComponent::rangeDb() const { return kRangeDb[settings_->rangeIdx]; }

Colour SpectrogramComponent::colourFor (float db) const
{
    const float t = (db - (topDb() - rangeDb())) / rangeDb();
    return lut_[(size_t) jlimit (0, 255, roundToInt (t * 255.f))];
}

float SpectrogramComponent::rowHz (int row) const
{
    return fMin_ * std::pow (fMax_ / fMin_, ((float) row + 0.5f) / (float) kRows);
}

//==============================================================================
void SpectrogramComponent::prepareAnalysis (double sr)
{
    sr_ = sr;

    // ~6 Hz bins whatever the rate: 8192 points at 48 kHz.
    const int order = jlimit (12, 15, 13 + roundToInt (std::log2 (sr / 48000.0)));
    fft_.prepare (order);
    const int N = fft_.getSize();

    window_.resize ((size_t) N);
    double sum = 0.0;
    for (int i = 0; i < N; ++i)
    {
        const double t = MathConstants<double>::twoPi * i / (double) (N - 1);
        const double w = 0.35875 - 0.48829 * std::cos (t)
                       + 0.14128 * std::cos (2.0 * t) - 0.01168 * std::cos (3.0 * t);
        window_[(size_t) i] = (float) w;
        sum += w;
    }

    // Unnormalised DFT: a sine of amplitude A peaks at A * N * wg / 2 in its
    // bin, so this turns bin power into A^2 and a full-scale sine reads 0 dB.
    const double wg = sum / (double) N;
    ampNorm_ = (float) (4.0 / ((double) N * wg * (double) N * wg));

    magSq_.assign ((size_t) fft_.getNumBins(), 0.f);
    accum_.assign ((size_t) fft_.getNumBins(), 0.f);

    fMin_ = 20.f;
    fMax_ = (float) jmin (20000.0, 0.45 * sr);

    // Which bins each display row covers. Rows narrower than a bin (the low
    // end, on a log axis) interpolate instead, so they do not show as blocky
    // steps of the same bin repeated.
    const double binHz = sr / (double) N;
    const int maxBin = fft_.getNumBins() - 1;
    rowBins_.assign ((size_t) kRows, RowBins{});
    for (int r = 0; r < kRows; ++r)
    {
        const double lo = fMin_ * std::pow ((double) fMax_ / fMin_, (double) r / kRows) / binHz;
        const double hi = fMin_ * std::pow ((double) fMax_ / fMin_, (double) (r + 1) / kRows) / binHz;
        auto& rb = rowBins_[(size_t) r];
        rb.lo = jlimit (0, maxBin, (int) std::ceil (lo));
        rb.hi = jlimit (0, maxBin, (int) std::floor (hi));
        rb.centre = (float) jlimit (0.0, (double) maxBin - 1.0, std::sqrt (lo * hi));
        rb.interp = rb.hi < rb.lo;
    }
}

void SpectrogramComponent::resetHistory()
{
    lanes_.resize ((size_t) numLanes());
    for (auto& l : lanes_)
    {
        l.image = Image (Image::RGB, kCols, kRows, false);
        l.image.clear (l.image.getBounds(), lut_[0]);
        l.db.assign ((size_t) kCols * kRows, kFloorDb);
        l.inst.assign ((size_t) kRows, kFloorDb);
        l.avgPow.assign ((size_t) kRows, 0.f);
        l.peak.assign ((size_t) kRows, kFloorDb);
        l.peakAge.assign ((size_t) kRows, 0.f);
    }
    colCount_ = 0;
    nextColAbs_ = -1.0;
    repaint();
}

void SpectrogramComponent::tick()
{
    if (capture_ == nullptr || ! capture_->isReady() || ! isShowing())
        return;

    const double sr = capture_->getSampleRate();
    if (sr != sr_ || ! fft_.isReady())
    {
        prepareAnalysis (sr);
        resetHistory();
    }

    if (paused_)
        return;

    const int    N       = fft_.getSize();
    const int64  written = capture_->getNumWritten();
    const double hop     = kSpanSec[settings_->spanIdx] * sr / (double) kCols;

    // Start from now, and start over from now if we fell so far behind that
    // the frames we owe have already left the ring.
    if (nextColAbs_ < 0.0 || (double) written - nextColAbs_ > (double) (capture_->getCapacity() - N))
        nextColAbs_ = (double) written;

    int produced = 0;
    while (nextColAbs_ <= (double) written && produced < 64)
    {
        produceColumn ((int64) nextColAbs_, written);
        nextColAbs_ += hop;
        ++produced;
    }

    if (produced > 0)
        repaint();
}

void SpectrogramComponent::produceColumn (int64 endAbs, int64 written)
{
    const int N = fft_.getSize();
    const int shown = numShown();
    capture_->read (frame_, settings_->firstCh, shown, N, (int) jmax ((int64) 0, written - endAbs));

    const float hopSec    = kSpanSec[settings_->spanIdx] / (float) kCols;
    const float avgAlpha  = std::exp (-hopSec / kAvgTauSec);
    const int   col       = (int) (colCount_ % kCols);

    for (int li = 0; li < (int) lanes_.size(); ++li)
    {
        auto& lane = lanes_[(size_t) li];
        const int c0 = settings_->stacked ? li : 0;
        const int c1 = settings_->stacked ? li + 1 : shown;

        // Power average across channels: what "the level of these channels at
        // this frequency" means for content that is not coherent between them.
        std::fill (accum_.begin(), accum_.end(), 0.f);
        for (int c = c0; c < c1 && c < frame_.getNumChannels(); ++c)
        {
            FloatVectorOperations::multiply (fft_.getTimeBuffer(), frame_.getReadPointer (c), window_.data(), N);
            fft_.magnitudesSquared (magSq_.data());
            FloatVectorOperations::add (accum_.data(), magSq_.data(), (int) accum_.size());
        }
        const float scale = ampNorm_ / (float) jmax (1, c1 - c0);

        Image::BitmapData bd (lane.image, col, 0, 1, kRows, Image::BitmapData::writeOnly);

        for (int r = 0; r < kRows; ++r)
        {
            const auto& rb = rowBins_[(size_t) r];
            float p;
            if (rb.interp)
            {
                const int i = (int) rb.centre;
                const float f = rb.centre - (float) i;
                p = accum_[(size_t) i] + f * (accum_[(size_t) i + 1] - accum_[(size_t) i]);
            }
            else
            {
                // Max, not mean: a tone keeps its level however many bins the
                // row spans, so the picture and the strip agree with a meter.
                p = 0.f;
                for (int b = rb.lo; b <= rb.hi; ++b)
                    p = jmax (p, accum_[(size_t) b]);
            }
            p *= scale;

            const float db = 10.f * std::log10 (jmax (p, 1.0e-20f));
            lane.db[(size_t) col * kRows + (size_t) r] = db;
            bd.setPixelColour (0, kRows - 1 - r, colourFor (db));

            // The strip alongside: instantaneous, average (in power), held peak.
            lane.inst[(size_t) r] = db;
            lane.avgPow[(size_t) r] = colCount_ == 0 ? p : avgAlpha * lane.avgPow[(size_t) r] + (1.f - avgAlpha) * p;

            float& pk = lane.peak[(size_t) r];
            float& age = lane.peakAge[(size_t) r];
            if (db >= pk) { pk = db; age = 0.f; }
            else
            {
                age += hopSec;
                if (age > kPeakHoldSec)
                    pk = jmax (db, pk - kPeakFallDbS * hopSec);
            }
        }
    }

    ++colCount_;
}

void SpectrogramComponent::recolourAll()
{
    for (auto& lane : lanes_)
    {
        Image::BitmapData bd (lane.image, Image::BitmapData::writeOnly);
        for (int c = 0; c < kCols; ++c)
            for (int r = 0; r < kRows; ++r)
            {
                bd.setPixelColour (c, kRows - 1 - r, colourFor (lane.db[(size_t) c * kRows + (size_t) r]));
            }
    }
    repaint();
}

//==============================================================================
void SpectrogramComponent::setPaused (bool shouldBePaused)
{
    if (paused_ == shouldBePaused)
        return;

    paused_ = shouldBePaused;
    // Resuming carries on from now. The paused stretch is not back-filled: it
    // has mostly left the ring by then anyway.
    if (! paused_)
        nextColAbs_ = -1.0;

    btnPause_.setToggleState (paused_, dontSendNotification);
    btnPause_.setButtonText (paused_ ? "run" : "pause");
    repaint();
}

void SpectrogramComponent::visibilityChanged()
{
    if (! isVisible())
        setPaused (false);
    nextColAbs_ = -1.0;    // never try to catch up across a time it was hidden
}

void SpectrogramComponent::comboBoxChanged (ComboBox* cb)
{
    const auto before = *settings_;

    if      (cb == &cbFirst_)  { settings_->firstCh = cbFirst_.getSelectedId() - 1;
                                 settings_->lastCh = jmax (settings_->lastCh, settings_->firstCh); }
    else if (cb == &cbLast_)   { settings_->lastCh = cbLast_.getSelectedId() - 1;
                                 settings_->firstCh = jmin (settings_->firstCh, settings_->lastCh); }
    else if (cb == &cbLayout_) settings_->stacked  = cbLayout_.getSelectedId() == 2;
    else if (cb == &cbSpan_)   settings_->spanIdx  = cbSpan_.getSelectedId() - 1;
    else if (cb == &cbTop_)    settings_->topIdx   = cbTop_.getSelectedId() - 1;
    else if (cb == &cbRange_)  settings_->rangeIdx = cbRange_.getSelectedId() - 1;

    // Keep the range within the analysis budget by moving the other end.
    if (settings_->lastCh - settings_->firstCh + 1 > kMaxChannels)
    {
        if (cb == &cbFirst_) settings_->lastCh  = settings_->firstCh + kMaxChannels - 1;
        else                 settings_->firstCh = settings_->lastCh - kMaxChannels + 1;
    }

    clampSettings();
    syncControlsFromSettings();

    const auto& s = *settings_;
    if (s.firstCh != before.firstCh || s.lastCh != before.lastCh
        || s.stacked != before.stacked || s.spanIdx != before.spanIdx)
        resetHistory();             // the old picture means something else now
    else if (s.topIdx != before.topIdx || s.rangeIdx != before.rangeIdx)
        recolourAll();              // same data, new scale: keep the history
}

void SpectrogramComponent::buttonClicked (Button* b)
{
    if (b == &btnPause_)
        setPaused (btnPause_.getToggleState());
}

//==============================================================================
void SpectrogramComponent::resized()
{
    int x = 0;
    auto place = [&] (Component& c, int w, int gap = 4)
    {
        c.setBounds (x, 0, w, kToolbarH);
        x += w + gap;
    };
    place (cbFirst_, 66);
    place (cbLast_, 66, 10);
    place (cbLayout_, 82, 10);
    place (cbSpan_, 62);
    place (cbTop_, 92);
    place (cbRange_, 112, 8);
    place (btnPause_, 58);
}

Rectangle<float> SpectrogramComponent::plotArea() const
{
    return Rectangle<float> (kMarginL, (float) kPlotTop,
                             jmax (10.f, (float) getWidth() - kMarginL - kMarginR),
                             jmax (10.f, (float) getHeight() - (float) kPlotTop - kMarginB));
}

Rectangle<float> SpectrogramComponent::laneArea (int lane) const
{
    const auto p = plotArea();
    const int n = jmax (1, (int) lanes_.size());
    const float h = (p.getHeight() - kLaneGap * (float) (n - 1)) / (float) n;
    return { p.getX(), p.getY() + (h + kLaneGap) * (float) lane, p.getWidth(), jmax (1.f, h) };
}

Rectangle<float> SpectrogramComponent::stripArea (int lane) const
{
    const auto l = laneArea (lane);
    const float w = jlimit (60.f, 160.f, l.getWidth() * 0.15f);
    return l.withLeft (l.getRight() - w);
}

Rectangle<float> SpectrogramComponent::historyArea (int lane) const
{
    const auto l = laneArea (lane);
    return l.withRight (stripArea (lane).getX() - 6.f);
}

float SpectrogramComponent::hzToY (float hz, Rectangle<float> r) const
{
    const float t = std::log (hz / fMin_) / std::log (fMax_ / fMin_);
    return r.getBottom() - t * r.getHeight();
}

float SpectrogramComponent::yToHz (float y, Rectangle<float> r) const
{
    const float t = (r.getBottom() - y) / r.getHeight();
    return fMin_ * std::pow (fMax_ / fMin_, t);
}

String SpectrogramComponent::formatHz (float hz)
{
    return hz >= 1000.f ? String (hz / 1000.f, hz >= 10000.f ? 1 : 2) + " kHz"
                        : (hz >= 100.f ? String (roundToInt (hz)) : String (hz, 1)) + " Hz";
}

String SpectrogramComponent::noteName (float hz)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const float midi = 69.f + 12.f * std::log2 (hz / 440.f);
    const int n = roundToInt (midi);
    const int cents = roundToInt ((midi - (float) n) * 100.f);
    String s = String (names[((n % 12) + 12) % 12]) + String (n / 12 - 1);
    if (cents != 0) s << (cents > 0 ? " +" : " ") << cents << "c";
    return s;
}

//==============================================================================
void SpectrogramComponent::paint (Graphics& g)
{
    const float span  = kSpanSec[settings_->spanIdx];
    const float floorDb = topDb() - rangeDb();
    const int   filled = (int) jmin ((int64) kCols, colCount_);
    const int   newest = colCount_ > 0 ? (int) ((colCount_ - 1) % kCols) : -1;
    static const float gridHz[] = { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f };

    g.setImageResamplingQuality (Graphics::lowResamplingQuality);

    for (int li = 0; li < (int) lanes_.size(); ++li)
    {
        const auto& lane = lanes_[(size_t) li];
        const auto h = historyArea (li);
        const auto s = stripArea (li);

        // History: the column ring drawn oldest-to-newest, newest at the right
        // edge. Not yet full, it grows in from the right.
        g.setColour (lut_[0]);
        g.fillRect (h);

        if (newest >= 0)
        {
            const float cw = h.getWidth() / (float) kCols;
            auto blit = [&] (int c0, int c1, float xRight)   // cols c0..c1 inclusive, ending at xRight
            {
                if (c1 < c0) return;
                const float w = cw * (float) (c1 - c0 + 1);
                g.drawImage (lane.image, roundToInt (xRight - w), roundToInt (h.getY()),
                             roundToInt (w), roundToInt (h.getHeight()),
                             c0, 0, c1 - c0 + 1, kRows);
            };
            blit (0, newest, h.getRight());
            if (filled == kCols)
                blit (newest + 1, kCols - 1, h.getRight() - cw * (float) (newest + 1));
        }

        // Strip: the current spectrum against the same frequency axis.
        g.setColour (Colour (0xe0101010));
        g.fillRect (s);

        auto dbToX = [&] (float db) { return s.getX() + jlimit (0.f, 1.f, (db - floorDb) / rangeDb()) * s.getWidth(); };
        auto rowY  = [&] (int r)    { return s.getBottom() - ((float) r + 0.5f) / (float) kRows * s.getHeight(); };

        g.setColour (Colours::white.withAlpha (0.07f));
        for (float d = topDb(); d > floorDb; d -= 12.f)
            g.drawVerticalLine ((int) dbToX (d), s.getY(), s.getBottom());

        Path avg, inst, peak;
        avg.startNewSubPath (s.getX(), s.getBottom());
        for (int r = 0; r < kRows; ++r)
        {
            const float y = rowY (r);
            const float aDb = 10.f * std::log10 (jmax (lane.avgPow[(size_t) r], 1.0e-20f));
            avg.lineTo (dbToX (aDb), y);
            const Point<float> pi (dbToX (lane.inst[(size_t) r]), y), pp (dbToX (lane.peak[(size_t) r]), y);
            if (r == 0) { inst.startNewSubPath (pi); peak.startNewSubPath (pp); }
            else        { inst.lineTo (pi);          peak.lineTo (pp); }
        }
        avg.lineTo (s.getX(), s.getY());
        avg.closeSubPath();

        g.setColour (Colour (0xff3d7bd9).withAlpha (0.45f));
        g.fillPath (avg);
        g.setColour (Colours::white.withAlpha (0.75f));
        g.strokePath (inst, PathStrokeType (1.f));
        g.setColour (kAccent.withAlpha (0.9f));
        g.strokePath (peak, PathStrokeType (1.f));

        // Shared frequency grid, across history and strip alike.
        g.setFont (Font (FontOptions (10.f)));
        for (float f : gridHz)
        {
            if (f < fMin_ || f > fMax_) continue;
            const float y = hzToY (f, h);
            g.setColour (Colours::white.withAlpha (0.12f));
            g.drawHorizontalLine ((int) y, h.getX(), h.getRight());
            g.drawHorizontalLine ((int) y, s.getX(), s.getRight());
            if (h.getHeight() >= 70.f)
            {
                g.setColour (Colours::white.withAlpha (0.7f));
                g.drawText (f >= 1000.f ? String ((int) (f / 1000.f)) + "k" : String ((int) f),
                            0, (int) y - 6, (int) kMarginL - 4, 12, Justification::centredRight);
            }
        }

        // Lane label.
        String label = numShown() == 1 || settings_->stacked
                         ? "ch " + String (settings_->firstCh + li + 1)
                         : "ch " + String (settings_->firstCh + 1) + "-" + String (settings_->lastCh + 1) + " avg";
        g.setFont (Font (FontOptions (11.f)));
        const int lw = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label) + 10;
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillRect ((int) h.getX() + 3, (int) h.getY() + 3, lw, 14);
        g.setColour (Colours::white.withAlpha (0.85f));
        g.drawText (label, (int) h.getX() + 3, (int) h.getY() + 3, lw, 14, Justification::centred);

        g.setColour (Colours::white.withAlpha (0.25f));
        g.drawRect (h, 1.f);
        g.drawRect (s, 1.f);
    }

    // Bottom axes: time under the history, the strip's dB scale under it.
    if (! lanes_.empty())
    {
        const auto h = historyArea ((int) lanes_.size() - 1);
        const auto s = stripArea ((int) lanes_.size() - 1);
        g.setFont (Font (FontOptions (10.f)));
        g.setColour (Colours::white.withAlpha (0.75f));
        const int y = (int) h.getBottom() + 2;
        for (int i = 0; i <= 4; ++i)
        {
            const float x = h.getX() + h.getWidth() * (float) i / 4.f;
            const float t = -span * (1.f - (float) i / 4.f);
            const String txt = i == 4 ? String ("now")
                                     : (std::abs (t) < 10.f ? String (t, 1) : String (roundToInt (t))) + " s";
            const auto j = i == 0 ? Justification::centredLeft : (i == 4 ? Justification::centredRight : Justification::centred);
            g.drawText (txt, i == 0 ? (int) x : (i == 4 ? (int) x - 60 : (int) x - 30), y, 60, 12, j);
        }
        g.drawText (String ((int) floorDb), (int) s.getX(), y, 40, 12, Justification::centredLeft);
        g.drawText (String ((int) topDb()) + " dB", (int) s.getRight() - 50, y, 50, 12, Justification::centredRight);
    }

    if (paused_)
    {
        const auto h = historyArea (0);
        g.setColour (Colour (0xffff6060));
        g.setFont (Font (FontOptions (11.f)));
        g.drawText ("PAUSED", (int) h.getRight() - 70, (int) h.getY() + 3, 64, 14, Justification::centredRight);
    }

    // Hover: one horizontal line across history and strip — the shared axis —
    // and a readout of what is under the pointer.
    if (! hovering_)
        return;

    for (int li = 0; li < (int) lanes_.size(); ++li)
    {
        const auto h = historyArea (li);
        const auto s = stripArea (li);
        const bool inH = h.contains (hover_), inS = s.contains (hover_);
        if (! inH && ! inS)
            continue;

        const auto& lane = lanes_[(size_t) li];
        const float hz = yToHz (hover_.y, h);
        const int row = jlimit (0, kRows - 1, (int) ((h.getBottom() - hover_.y) / h.getHeight() * (float) kRows));

        g.setColour (Colours::white.withAlpha (0.4f));
        g.drawHorizontalLine ((int) hover_.y, h.getX(), h.getRight());
        g.drawHorizontalLine ((int) hover_.y, s.getX(), s.getRight());

        StringArray lines;
        lines.add (formatHz (hz) + "   " + noteName (hz));

        if (inH)
        {
            g.drawVerticalLine ((int) hover_.x, h.getY(), h.getBottom());
            const float cw = h.getWidth() / (float) kCols;
            const int back = (int) ((h.getRight() - hover_.x) / cw);
            if (back < filled && newest >= 0)
            {
                const int col = ((newest - back) % kCols + kCols) % kCols;
                lines.add (String (lane.db[(size_t) col * kRows + (size_t) row], 1) + " dB");
                lines.add (String (-(float) back * span / (float) kCols, 2) + " s");
            }
        }
        else
        {
            lines.add ("now  " + String (lane.inst[(size_t) row], 1) + " dB");
            lines.add ("avg  " + String (10.f * std::log10 (jmax (lane.avgPow[(size_t) row], 1.0e-20f)), 1) + " dB");
            lines.add ("peak " + String (lane.peak[(size_t) row], 1) + " dB");
        }

        g.setFont (Font (FontOptions (11.f)));
        int bw = 0;
        for (auto& l : lines) bw = jmax (bw, GlyphArrangement::getStringWidthInt (g.getCurrentFont(), l));
        bw += 12;
        const int bh = 14 * lines.size() + 6;
        const auto area = plotArea();
        int bx = (int) hover_.x + 12, by = (int) hover_.y + 12;
        if (bx + bw > (int) area.getRight())  bx = (int) hover_.x - 12 - bw;
        if (by + bh > (int) area.getBottom()) by = (int) hover_.y - 12 - bh;
        by = jmax ((int) area.getY(), by);

        g.setColour (Colours::black.withAlpha (0.78f));
        g.fillRoundedRectangle ((float) bx, (float) by, (float) bw, (float) bh, 3.f);
        g.setColour (Colours::white);
        for (int i = 0; i < lines.size(); ++i)
            g.drawText (lines[i], bx + 6, by + 3 + 14 * i, bw - 8, 14, Justification::centredLeft);
        break;
    }
}

void SpectrogramComponent::mouseMove (const MouseEvent& e)
{
    hover_ = e.position;
    hovering_ = true;
    repaint();
}

void SpectrogramComponent::mouseExit (const MouseEvent&)
{
    hovering_ = false;
    repaint();
}
