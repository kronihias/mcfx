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
#include "OscilloscopeComponent.h"   // displayScaleFor()

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
    updateWantedResolution();
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
        updateWantedResolution();
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
    if (! (s.zoomHi > s.zoomLo && s.zoomLo > 0.f))
        s.zoomLo = s.zoomHi = 0.f;
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
    return fMin_ * std::pow (fMax_ / fMin_, ((float) row + 0.5f) / (float) rows_);
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
    buildRowBins();
}

void SpectrogramComponent::buildRowBins()
{
    if (! fft_.isReady() || sr_ <= 0.0)
        return;

    // Which bins each analysis row covers. Rows narrower than a bin (the low
    // end, on a log axis) interpolate instead, so they do not show as blocky
    // steps of the same bin repeated.
    const double binHz = sr_ / (double) fft_.getSize();
    const int maxBin = fft_.getNumBins() - 1;
    rowBins_.assign ((size_t) rows_, RowBins{});
    for (int r = 0; r < rows_; ++r)
    {
        const double lo = fMin_ * std::pow ((double) fMax_ / fMin_, (double) r / rows_) / binHz;
        const double hi = fMin_ * std::pow ((double) fMax_ / fMin_, (double) (r + 1) / rows_) / binHz;
        auto& rb = rowBins_[(size_t) r];
        // Only rows well under a bin wide interpolate. A row about one bin
        // wide may contain no bin centre at all, and interpolating there
        // under-reads a tone by over a dB, so it takes its nearest bins' max.
        rb.interp = (hi - lo) < 0.5;
        rb.lo = jlimit (0, maxBin, (int) std::lround (lo));
        rb.hi = jlimit (rb.lo, maxBin, (int) std::lround (hi));
        rb.centre = (float) jlimit (0.0, (double) maxBin - 1.0, std::sqrt (lo * hi));
    }
}

void SpectrogramComponent::resetHistory()
{
    // Starting over anyway, so take exactly the resolution the size asks for.
    const bool rowsChanged = rows_ != wantRows_;
    cols_ = wantCols_;
    rows_ = wantRows_;
    if (rowsChanged || (int) rowBins_.size() != rows_)
        buildRowBins();

    lanes_.resize ((size_t) numLanes());
    for (auto& l : lanes_)
    {
        l.image = Image (Image::RGB, cols_, rows_, false);
        l.image.clear (l.image.getBounds(), lut_[0]);
        l.db.assign ((size_t) cols_ * rows_, kFloorDb);
        l.inst.assign ((size_t) rows_, kFloorDb);
        l.avgPow.assign ((size_t) rows_, 0.f);
        l.peak.assign ((size_t) rows_, kFloorDb);
        l.peakAge.assign ((size_t) rows_, 0.f);
    }
    colCount_ = 0;
    nextColAbs_ = -1.0;
    repaint();
}

void SpectrogramComponent::updateWantedResolution()
{
    const auto h = historyArea (0);

    // Transform budget: ~3000 8192-point FFTs a second, a few percent of a
    // core. Every shown channel is one transform per column in either layout,
    // so a short span over many channels gets fewer columns than its width.
    const int N = fft_.isReady() ? fft_.getSize() : 8192;
    const double budget = 3000.0 * 8192.0 / (double) N;
    const int byBudget = (int) (budget * kSpanSec[settings_->spanIdx] / (double) jmax (1, numShown()));
    const int cols = jlimit (120, 2048, jmin (roundToInt (h.getWidth()), byBudget));

    // Rows cover the whole range, so zoomed in they need more of them for the
    // shown part to stay as fine as the screen.
    const float shownFraction = std::log (zoomHi() / zoomLo()) / std::log (fMax_ / fMin_);
    const int rows = jlimit (96, 1024, roundToInt (h.getHeight() / jmax (0.05f, shownFraction)));

    // Ignore small changes: each one costs a resample of the whole history,
    // and a window being dragged larger produces a stream of them.
    if (std::abs (cols - cols_) > cols_ / 8) wantCols_ = cols;
    else                                    wantCols_ = cols_;
    if (std::abs (rows - rows_) > rows_ / 8) wantRows_ = rows;
    else                                    wantRows_ = rows_;
}

void SpectrogramComponent::resampleHistory (int newCols, int newRows)
{
    const int filled = (int) jmin ((int64) cols_, colCount_);
    const int newest = colCount_ > 0 ? (int) ((colCount_ - 1) % cols_) : 0;
    const int newFilled = filled > 0 ? jlimit (1, newCols, roundToInt ((double) filled * newCols / cols_)) : 0;

    // Source rows for each new row. Shrinking takes the max over the rows it
    // merges, so a narrow tone survives being drawn smaller.
    auto rowSpan = [&] (int nr, int& r0, int& r1)
    {
        r0 = jlimit (0, rows_ - 1, (int) ((double) nr * rows_ / newRows));
        r1 = jlimit (r0, rows_ - 1, (int) std::ceil ((double) (nr + 1) * rows_ / newRows) - 1);
    };

    for (auto& lane : lanes_)
    {
        std::vector<float> db ((size_t) newCols * newRows, kFloorDb);
        for (int a = 0; a < newFilled; ++a)                 // a = age in new columns
        {
            const int oldAge = (int) ((double) a * cols_ / newCols);
            if (oldAge >= filled) continue;
            const int oc = ((newest - oldAge) % cols_ + cols_) % cols_;
            const int nc = newFilled - 1 - a;
            for (int nr = 0; nr < newRows; ++nr)
            {
                int r0, r1; rowSpan (nr, r0, r1);
                float v = kFloorDb;
                for (int r = r0; r <= r1; ++r)
                    v = jmax (v, lane.db[(size_t) oc * rows_ + (size_t) r]);
                db[(size_t) nc * newRows + (size_t) nr] = v;
            }
        }
        lane.db.swap (db);

        auto resampleRows = [&] (std::vector<float>& v, bool takeMax)
        {
            std::vector<float> out ((size_t) newRows);
            for (int nr = 0; nr < newRows; ++nr)
            {
                int r0, r1; rowSpan (nr, r0, r1);
                float x = v[(size_t) r0];
                for (int r = r0 + 1; r <= r1; ++r)
                    x = takeMax ? jmax (x, v[(size_t) r]) : x;
                out[(size_t) nr] = x;
            }
            v.swap (out);
        };
        resampleRows (lane.inst, true);
        resampleRows (lane.avgPow, true);
        resampleRows (lane.peak, true);
        resampleRows (lane.peakAge, false);

        lane.image = Image (Image::RGB, newCols, newRows, false);
    }

    cols_ = newCols;
    rows_ = newRows;
    colCount_ = newFilled;
    buildRowBins();
    recolourAll();
}

void SpectrogramComponent::tick()
{
    if (capture_ == nullptr || ! capture_->isReady() || ! isShowing())
        return;

    const double sr = capture_->getSampleRate();
    if (sr != sr_ || ! fft_.isReady())
    {
        prepareAnalysis (sr);
        updateWantedResolution();
        resetHistory();
    }

    // A resize or zoom asked for a different resolution: resample what we
    // have rather than lose it. Done here, at most once a tick, rather than
    // in resized(), which a window drag calls far more often.
    if (wantCols_ != cols_ || wantRows_ != rows_)
        resampleHistory (wantCols_, wantRows_);

    if (paused_)
        return;

    const int    N       = fft_.getSize();
    const int64  written = capture_->getNumWritten();
    const double hop     = kSpanSec[settings_->spanIdx] * sr / (double) cols_;

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

    const float hopSec    = kSpanSec[settings_->spanIdx] / (float) cols_;
    const float avgAlpha  = std::exp (-hopSec / kAvgTauSec);
    const int   col       = (int) (colCount_ % cols_);

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

        Image::BitmapData bd (lane.image, col, 0, 1, rows_, Image::BitmapData::writeOnly);

        for (int r = 0; r < rows_; ++r)
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
            lane.db[(size_t) col * rows_ + (size_t) r] = db;
            bd.setPixelColour (0, rows_ - 1 - r, colourFor (db));

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
        for (int c = 0; c < cols_; ++c)
            for (int r = 0; r < rows_; ++r)
            {
                bd.setPixelColour (c, rows_ - 1 - r, colourFor (lane.db[(size_t) c * rows_ + (size_t) r]));
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
    {
        updateWantedResolution();
        resetHistory();             // the old picture means something else now
    }
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
    s_ = displayScaleFor (getWidth(), getHeight());

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

    updateWantedResolution();
}

Rectangle<float> SpectrogramComponent::plotArea() const
{
    return Rectangle<float> (kMarginL * s_, (float) kPlotTop,
                             jmax (10.f, (float) getWidth() - kMarginL * s_ - kMarginR),
                             jmax (10.f, (float) getHeight() - (float) kPlotTop - kMarginB * s_));
}

Rectangle<float> SpectrogramComponent::laneArea (int lane) const
{
    const auto p = plotArea();
    const int n = jmax (1, numLanes());
    const float h = (p.getHeight() - kLaneGap * (float) (n - 1)) / (float) n;
    return { p.getX(), p.getY() + (h + kLaneGap) * (float) lane, p.getWidth(), jmax (1.f, h) };
}

Rectangle<float> SpectrogramComponent::stripArea (int lane) const
{
    const auto l = laneArea (lane);
    const float w = jlimit (60.f * s_, 160.f * s_, l.getWidth() * 0.15f);
    return l.withLeft (l.getRight() - w);
}

Rectangle<float> SpectrogramComponent::historyArea (int lane) const
{
    const auto l = laneArea (lane);
    return l.withRight (stripArea (lane).getX() - 6.f * s_);
}

int SpectrogramComponent::laneAt (Point<float> p) const
{
    for (int i = 0; i < numLanes(); ++i)
        if (laneArea (i).contains (p))
            return i;
    return -1;
}

float SpectrogramComponent::zoomLo() const
{
    return settings_->zoomHi > 0.f ? jlimit (fMin_, fMax_, settings_->zoomLo) : fMin_;
}

float SpectrogramComponent::zoomHi() const
{
    return settings_->zoomHi > 0.f ? jlimit (zoomLo() * 1.01f, fMax_, settings_->zoomHi) : fMax_;
}

float SpectrogramComponent::rowPos (float hz) const
{
    return std::log (hz / fMin_) / std::log (fMax_ / fMin_) * (float) rows_;
}

void SpectrogramComponent::setZoom (float lo, float hi)
{
    const float full = std::log (fMax_ / fMin_);
    const float minSpan = std::log (2.f) * 0.5f;          // half an octave

    if (hi <= 0.f || lo <= 0.f)
    {
        settings_->zoomLo = settings_->zoomHi = 0.f;
    }
    else
    {
        float a = std::log (lo / fMin_), b = std::log (hi / fMin_);
        float span = jlimit (minSpan, full, b - a);
        const float mid = 0.5f * (a + b);
        a = jlimit (0.f, full - span, mid - 0.5f * span);
        b = a + span;

        if (span >= full * 0.999f)
            settings_->zoomLo = settings_->zoomHi = 0.f;    // all of it: say so
        else
        {
            settings_->zoomLo = fMin_ * std::exp (a);
            settings_->zoomHi = fMin_ * std::exp (b);
        }
    }

    updateWantedResolution();
    repaint();
}

float SpectrogramComponent::hzToY (float hz, Rectangle<float> r) const
{
    const float t = std::log (hz / zoomLo()) / std::log (zoomHi() / zoomLo());
    return r.getBottom() - t * r.getHeight();
}

float SpectrogramComponent::yToHz (float y, Rectangle<float> r) const
{
    const float t = (r.getBottom() - y) / r.getHeight();
    return zoomLo() * std::pow (zoomHi() / zoomLo(), t);
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
    const float span    = kSpanSec[settings_->spanIdx];
    const float floorDb = topDb() - rangeDb();
    const int   filled  = (int) jmin ((int64) cols_, colCount_);
    const int   newest  = colCount_ > 0 ? (int) ((colCount_ - 1) % cols_) : -1;
    const float zLo = zoomLo(), zHi = zoomHi();
    const bool  zoomed = settings_->zoomHi > 0.f;

    // Text and margins scale with the window, for projection.
    auto S = [this] (float v) { return roundToInt (v * s_); };

    // Candidate grid frequencies, fine enough to still label a zoomed-in
    // range; the ones that would crowd their neighbour are skipped.
    static const float gridHz[] = { 20.f, 30.f, 40.f, 50.f, 60.f, 80.f, 100.f, 150.f, 200.f, 300.f,
                                    400.f, 500.f, 600.f, 800.f, 1000.f, 1500.f, 2000.f, 3000.f, 4000.f,
                                    5000.f, 6000.f, 8000.f, 10000.f, 12000.f, 15000.f, 20000.f };

    // The image rows the zoom shows (image y grows downwards, rows upwards).
    const float imgTop = (float) rows_ - rowPos (zHi);
    const float imgBot = (float) rows_ - rowPos (zLo);

    g.setImageResamplingQuality (Graphics::mediumResamplingQuality);

    for (int li = 0; li < (int) lanes_.size(); ++li)
    {
        const auto& lane = lanes_[(size_t) li];
        const auto h = historyArea (li);
        const auto s = stripArea (li);

        // History: the column ring drawn oldest-to-newest, newest at the right
        // edge. Not yet full, it grows in from the right. Drawn through a
        // transform so the zoom can show any slice of the rows.
        g.setColour (lut_[0]);
        g.fillRect (h);

        if (newest >= 0)
        {
            const float cw = h.getWidth() / (float) cols_;
            const float sy = h.getHeight() / jmax (1.0e-3f, imgBot - imgTop);
            auto blit = [&] (int c0, int c1, float xRight)   // cols c0..c1 inclusive, ending at xRight
            {
                if (c1 < c0) return;
                const float w = cw * (float) (c1 - c0 + 1);
                Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (Rectangle<float> (xRight - w, h.getY(), w, h.getHeight())
                                        .getIntersection (h).getSmallestIntegerContainer());
                g.drawImageTransformed (lane.image,
                                        AffineTransform::scale (cw, sy)
                                            .translated (xRight - cw * (float) (c1 + 1), h.getY() - imgTop * sy));
            };
            blit (0, newest, h.getRight());
            if (filled == cols_)
                blit (newest + 1, cols_ - 1, h.getRight() - cw * (float) (newest + 1));
        }

        // Strip: the current spectrum against the same frequency axis.
        g.setColour (Colour (0xe0101010));
        g.fillRect (s);

        auto dbToX = [&] (float db) { return s.getX() + jlimit (0.f, 1.f, (db - floorDb) / rangeDb()) * s.getWidth(); };

        g.setColour (Colours::white.withAlpha (0.07f));
        for (float d = topDb(); d > floorDb; d -= 12.f)
            g.drawVerticalLine ((int) dbToX (d), s.getY(), s.getBottom());

        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (s.getSmallestIntegerContainer());

            const int r0 = jlimit (0, rows_ - 1, (int) std::floor (rowPos (zLo)) - 1);
            const int r1 = jlimit (0, rows_ - 1, (int) std::ceil (rowPos (zHi)) + 1);

            Path avg, inst, peak;
            for (int r = r0; r <= r1; ++r)
            {
                const float y = hzToY (rowHz (r), s);
                const float aDb = 10.f * std::log10 (jmax (lane.avgPow[(size_t) r], 1.0e-20f));
                const Point<float> pa (dbToX (aDb), y), pi (dbToX (lane.inst[(size_t) r]), y),
                                   pp (dbToX (lane.peak[(size_t) r]), y);
                if (r == r0)
                {
                    avg.startNewSubPath (s.getX(), y);
                    inst.startNewSubPath (pi);
                    peak.startNewSubPath (pp);
                }
                avg.lineTo (pa);
                if (r != r0) { inst.lineTo (pi); peak.lineTo (pp); }
                if (r == r1)
                    avg.lineTo (s.getX(), y);
            }
            avg.closeSubPath();

            g.setColour (Colour (0xff3d7bd9).withAlpha (0.45f));
            g.fillPath (avg);
            g.setColour (Colours::white.withAlpha (0.75f));
            g.strokePath (inst, PathStrokeType (jmax (1.f, s_)));
            g.setColour (kAccent.withAlpha (0.9f));
            g.strokePath (peak, PathStrokeType (jmax (1.f, s_)));
        }

        // Shared frequency grid, across history and strip alike.
        g.setFont (Font (FontOptions (10.f * s_)));
        float lastY = 1.0e9f;
        for (float f : gridHz)
        {
            if (f < zLo || f > zHi) continue;
            const float y = hzToY (f, h);
            if (lastY - y < 16.f * s_) continue;                // would crowd the one below
            lastY = y;

            g.setColour (Colours::white.withAlpha (0.12f));
            g.drawHorizontalLine ((int) y, h.getX(), h.getRight());
            g.drawHorizontalLine ((int) y, s.getX(), s.getRight());
            if (h.getHeight() >= 60.f * s_)
            {
                g.setColour (Colours::white.withAlpha (0.7f));
                // Kept inside the lane, so the top label of one stacked lane
                // cannot land on the bottom label of the lane above.
                const int ly = jlimit ((int) h.getY(), (int) h.getBottom() - S (12), (int) y - S (6));
                g.drawText (f >= 1000.f ? String (f / 1000.f) + "k" : String ((int) f),
                            0, ly, S (kMarginL) - 4, S (12), Justification::centredRight);
            }
        }

        // Lane label.
        String label = numShown() == 1 || settings_->stacked
                         ? "ch " + String (settings_->firstCh + li + 1)
                         : "ch " + String (settings_->firstCh + 1) + "-" + String (settings_->lastCh + 1) + " avg";
        g.setFont (Font (FontOptions (11.f * s_)));
        const int lw = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label) + S (10);
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillRect ((int) h.getX() + 3, (int) h.getY() + 3, lw, S (14));
        g.setColour (Colours::white.withAlpha (0.85f));
        g.drawText (label, (int) h.getX() + 3, (int) h.getY() + 3, lw, S (14), Justification::centred);

        g.setColour (Colours::white.withAlpha (0.25f));
        g.drawRect (h, 1.f);
        g.drawRect (s, 1.f);
    }

    // Bottom axes: time under the history, the strip's dB scale under it.
    if (! lanes_.empty())
    {
        const auto h = historyArea ((int) lanes_.size() - 1);
        const auto s = stripArea ((int) lanes_.size() - 1);
        g.setFont (Font (FontOptions (10.f * s_)));
        g.setColour (Colours::white.withAlpha (0.75f));
        const int y = (int) h.getBottom() + 2;
        for (int i = 0; i <= 4; ++i)
        {
            const float x = h.getX() + h.getWidth() * (float) i / 4.f;
            const float t = -span * (1.f - (float) i / 4.f);
            const String txt = i == 4 ? String ("now")
                                     : (std::abs (t) < 10.f ? String (t, 1) : String (roundToInt (t))) + " s";
            const auto j = i == 0 ? Justification::centredLeft : (i == 4 ? Justification::centredRight : Justification::centred);
            g.drawText (txt, i == 0 ? (int) x : (i == 4 ? (int) x - S (60) : (int) x - S (30)), y, S (60), S (12), j);
        }
        g.drawText (String ((int) floorDb), (int) s.getX(), y, S (40), S (12), Justification::centredLeft);
        g.drawText (String ((int) topDb()) + " dB", (int) s.getRight() - S (50), y, S (50), S (12), Justification::centredRight);
    }

    // Status, top right of the first history: pause, and the zoom with how to undo it.
    {
        const auto h = historyArea (0);
        String st;
        if (zoomed)
            st << formatHz (zLo) << " - " << formatHz (zHi) << "   (double click: all)";
        if (paused_)
            st << (st.isEmpty() ? "" : "   ") << "PAUSED";
        if (st.isNotEmpty())
        {
            g.setFont (Font (FontOptions (11.f * s_)));
            const int tw = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), st) + S (10);
            g.setColour (Colours::black.withAlpha (0.55f));
            g.fillRect ((int) h.getRight() - tw - 3, (int) h.getY() + 3, tw, S (14));
            g.setColour (paused_ ? Colour (0xffff6060) : Colours::white.withAlpha (0.85f));
            g.drawText (st, (int) h.getRight() - tw - 3, (int) h.getY() + 3, tw, S (14), Justification::centred);
        }
    }

    // Hover: one horizontal line across history and strip — the shared axis —
    // and a readout of what is under the pointer.
    if (! hovering_)
        return;

    const int li = laneAt (hover_);
    if (! isPositiveAndBelow (li, (int) lanes_.size()))
        return;

    const auto h = historyArea (li);
    const auto s = stripArea (li);
    const bool inH = h.contains (hover_), inS = s.contains (hover_);
    if (! inH && ! inS)
        return;

    const auto& lane = lanes_[(size_t) li];
    const float hz = yToHz (hover_.y, h);
    const int row = jlimit (0, rows_ - 1, (int) rowPos (hz));

    g.setColour (Colours::white.withAlpha (0.4f));
    g.drawHorizontalLine ((int) hover_.y, h.getX(), h.getRight());
    g.drawHorizontalLine ((int) hover_.y, s.getX(), s.getRight());

    StringArray lines;
    lines.add (formatHz (hz) + "   " + noteName (hz));

    if (inH)
    {
        g.drawVerticalLine ((int) hover_.x, h.getY(), h.getBottom());
        const float cw = h.getWidth() / (float) cols_;
        const int back = (int) ((h.getRight() - hover_.x) / cw);
        if (back < filled && newest >= 0)
        {
            const int col = ((newest - back) % cols_ + cols_) % cols_;
            lines.add (String (lane.db[(size_t) col * rows_ + (size_t) row], 1) + " dB");
            lines.add (String (-(float) back * span / (float) cols_, 2) + " s");
        }
    }
    else
    {
        lines.add ("now  " + String (lane.inst[(size_t) row], 1) + " dB");
        lines.add ("avg  " + String (10.f * std::log10 (jmax (lane.avgPow[(size_t) row], 1.0e-20f)), 1) + " dB");
        lines.add ("peak " + String (lane.peak[(size_t) row], 1) + " dB");
    }

    g.setFont (Font (FontOptions (11.f * s_)));
    int bw = 0;
    for (auto& l : lines) bw = jmax (bw, GlyphArrangement::getStringWidthInt (g.getCurrentFont(), l));
    bw += S (12);
    const int lh = S (14);
    const int bh = lh * lines.size() + S (6);
    const auto area = plotArea();
    int bx = (int) hover_.x + 12, by = (int) hover_.y + 12;
    if (bx + bw > (int) area.getRight())  bx = (int) hover_.x - 12 - bw;
    if (by + bh > (int) area.getBottom()) by = (int) hover_.y - 12 - bh;
    by = jmax ((int) area.getY(), by);

    g.setColour (Colours::black.withAlpha (0.78f));
    g.fillRoundedRectangle ((float) bx, (float) by, (float) bw, (float) bh, 3.f);
    g.setColour (Colours::white);
    for (int i = 0; i < lines.size(); ++i)
        g.drawText (lines[i], bx + S (6), by + S (3) + lh * i, bw - S (8), lh, Justification::centredLeft);
}

//==============================================================================
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

void SpectrogramComponent::mouseDown (const MouseEvent& e)
{
    dragLane_ = laneAt (e.position);
    dragZoomLo_ = zoomLo();
    dragZoomHi_ = zoomHi();
}

void SpectrogramComponent::mouseDrag (const MouseEvent& e)
{
    hover_ = e.position;
    if (dragLane_ < 0 || settings_->zoomHi <= 0.f)
        return;

    // Grab-and-move: dragging the picture down brings higher frequencies in.
    const auto h = historyArea (dragLane_);
    const float shift = (float) e.getDistanceFromDragStartY() / h.getHeight()
                      * std::log (dragZoomHi_ / dragZoomLo_);
    setZoom (dragZoomLo_ * std::exp (shift), dragZoomHi_ * std::exp (shift));
}

void SpectrogramComponent::mouseDoubleClick (const MouseEvent&)
{
    setZoom (0.f, 0.f);
}

void SpectrogramComponent::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const int li = laneAt (e.position);
    if (li < 0)
        return;

    // Zoom about the frequency under the pointer, so it stays where it is.
    const auto h = historyArea (li);
    const float d = w.deltaY != 0.f ? w.deltaY : w.deltaX;
    const float hz = yToHz (jlimit (h.getY(), h.getBottom(), e.position.y), h);
    const float a = std::log (zoomLo()), b = std::log (zoomHi()), x = std::log (hz);
    const float t = (x - a) / (b - a);
    const float span = (b - a) * std::exp (-d * 1.5f);    // wheel up = zoom in
    setZoom (std::exp (x - t * span), std::exp (x + (1.f - t) * span));
}
