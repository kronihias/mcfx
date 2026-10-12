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

    // Fine bass: constant-Q below kSplitLo, FFT above kSplitHi, crossfaded in
    // power between them. At kSplitLo the 8192-point FFT's bins are already
    // about a quarter tone wide; below it they quickly get coarser than that.
    constexpr float kSplitLo    = 350.f;
    constexpr float kSplitHi    = 550.f;
    constexpr float kBassTopHz  = 700.f;      // constant-Q bins stop here
    constexpr float kDecRateHz  = 3000.f;     // decimated rate, roughly
    constexpr int   kBassOrder  = 11;         // 2048 decimated samples: ~0.68 s
    // Quarter-tone sharp, placed at eighth tones — see
    // ConstantQ::Config::resolutionBinsPerOctave.
    constexpr int   kCqResolution = 24;
    constexpr int   kCqDensity    = 48;

    // Colour maps, quiet to loud, as anchors interpolated into a 256-entry
    // table. The first four are the perceptually uniform matplotlib maps —
    // monotonic in lightness, so level reads right in greyscale and on a
    // projector that washes colours out. The rest follow Sonic Visualiser's
    // set in spirit (its exact colours are not copied): green, sunset, ice, a
    // rainbow, plain grey either way round — black on white is the one for a
    // bright room or for printing — and a banded map whose steps make levels
    // easy to compare across the picture.
    struct ColourMap { const char* name; std::vector<std::array<uint8, 3>> anchors; int bands; };
    const std::vector<ColourMap>& colourMaps()
    {
        static const std::vector<ColourMap> maps {
            { "inferno", { {0,0,4}, {31,12,72}, {85,15,109}, {136,34,106}, {186,54,85},
                           {227,89,51}, {249,140,10}, {249,201,50}, {252,255,164} }, 0 },
            { "magma",   { {0,0,4}, {28,16,68}, {79,18,123}, {129,37,129}, {181,54,122},
                           {229,80,100}, {251,135,97}, {254,194,135}, {252,253,191} }, 0 },
            { "viridis", { {68,1,84}, {72,40,120}, {62,73,137}, {49,104,142}, {38,130,142},
                           {31,158,137}, {53,183,121}, {110,206,88}, {181,222,43}, {253,231,37} }, 0 },
            { "cividis", { {0,32,77}, {0,52,110}, {57,72,108}, {87,92,109}, {112,113,115},
                           {138,134,120}, {166,157,117}, {196,181,108}, {227,206,92}, {255,234,70} }, 0 },
            { "green",   { {0,0,0}, {0,48,0}, {0,120,30}, {40,190,20}, {160,230,0},
                           {255,255,90}, {255,255,255} }, 0 },
            { "sunset",  { {0,0,0}, {52,0,84}, {150,0,90}, {220,40,40}, {250,120,0},
                           {255,200,40}, {255,255,210} }, 0 },
            { "ice",     { {0,0,0}, {0,18,64}, {0,70,150}, {30,140,220}, {120,210,250},
                           {200,245,255}, {255,255,255} }, 0 },
            { "rainbow", { {48,18,59}, {70,107,227}, {40,187,236}, {50,241,151}, {164,252,60},
                           {237,208,58}, {251,128,34}, {205,51,10}, {122,4,3} }, 0 },
            { "white on black", { {0,0,0}, {255,255,255} }, 0 },
            { "black on white", { {255,255,255}, {0,0,0} }, 0 },
            { "banded",  { {48,18,59}, {70,107,227}, {40,187,236}, {50,241,151}, {164,252,60},
                           {237,208,58}, {251,128,34}, {205,51,10}, {122,4,3} }, 12 },
        };
        return maps;
    }

    // IEC 60268-18 meter law, as the bars use: 0 at -70 dB, 1 at 0 dB.
    float iecFraction (float dB)
    {
        if (dB < -70.f) return 0.f;
        if (dB < -60.f) return (dB + 70.f) * 0.0025f;
        if (dB < -50.f) return (dB + 60.f) * 0.005f + 0.025f;
        if (dB < -40.f) return (dB + 50.f) * 0.0075f + 0.075f;
        if (dB < -30.f) return (dB + 40.f) * 0.015f + 0.15f;
        if (dB < -20.f) return (dB + 30.f) * 0.02f + 0.3f;
        if (dB <   0.f) return (dB + 20.f) * 0.025f + 0.5f;
        return 1.f;
    }

    constexpr int kNumColourScales = 3, kNumNormalise = 4, kNumFreqScales = 3, kNumWindows = 4;

    const Colour kAccent (0xffff9a3d);
}

//==============================================================================
SpectrogramComponent::SpectrogramComponent()
{
    setOpaque (false);
    settings_ = &ownSettings_;

    buildLut();

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

    setupCombo (cbRes_, "fine bass: constant-Q below ~450 Hz, so close bass notes and their "
                        "harmonics come apart; shows the picture 0.34 s late.\n"
                        "fast: FFT only; blurrier bass, 85 ms late.\n"
                        "Either way a click is drawn as one vertical line.");
    cbRes_.addItem ("fast", 1);
    cbRes_.addItem ("fine bass", 2);

    addAndMakeVisible (btnPause_);
    btnPause_.setButtonText ("pause");
    btnPause_.setClickingTogglesState (true);
    btnPause_.setColour (TextButton::buttonOnColourId, Colour (0xffb03030));
    btnPause_.setTooltip ("Stop scrolling to read the picture. Hover for frequency, note, level and time.");
    btnPause_.addListener (this);

    {
        // A gear: eight teeth around a hole.
        Path gear;
        gear.addStar ({ 8.f, 8.f }, 8, 5.2f, 8.f);
        gear.addEllipse (5.2f, 5.2f, 5.6f, 5.6f);
        gear.setUsingNonZeroWinding (false);
        btnSettings_.setShape (gear, false, true, false);
        btnSettings_.setBorderSize (BorderSize<int> (3));
        btnSettings_.setTooltip ("Display settings: colours, colour scale, normalisation, "
                                 "frequency axis, note names, window size...");
        btnSettings_.onClick = [this] { showSettings(); };
        addAndMakeVisible (btnSettings_);
    }

    syncControlsFromSettings();
}

SpectrogramComponent::~SpectrogramComponent()
{
    // The settings callout lives on the desktop and edits our settings; it
    // must not outlive us.
    if (auto* c = callout_.getComponent())
        delete c;
}

StringArray SpectrogramComponent::colourMapNames()
{
    StringArray names;
    for (auto& m : colourMaps())
        names.add (m.name);
    return names;
}

void SpectrogramComponent::buildLut()
{
    const auto& map = colourMaps()[(size_t) jlimit (0, (int) colourMaps().size() - 1, settings_->colourMap)];
    const int nA = (int) map.anchors.size();
    lut_.resize (256);
    for (int i = 0; i < 256; ++i)
    {
        float t = (float) i / 255.f;
        if (map.bands > 0)        // stepped: hold each band's colour flat
            t = jmin (1.f, std::floor (t * (float) map.bands) / (float) (map.bands - 1));
        const float x = t * (float) (nA - 1);
        const int a = jmin (nA - 2, (int) x);
        const float f = x - (float) a;
        auto mix = [&] (int k) { return (uint8) roundToInt ((1.f - f) * map.anchors[(size_t) a][(size_t) k]
                                                             + f * map.anchors[(size_t) a + 1][(size_t) k]); };
        lut_[(size_t) i] = Colour (mix (0), mix (1), mix (2));
    }
}

void SpectrogramComponent::setSettings (SpectroSettings* settings)
{
    settings_ = settings != nullptr ? settings : &ownSettings_;
    clampSettings();
    syncControlsFromSettings();
    buildLut();
    if (sr_ > 0.0)
        prepareAnalysis (sr_);      // window size and frequency axis may differ
    applied_ = *settings_;
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
    s.colourMap   = jlimit (0, (int) colourMaps().size() - 1, s.colourMap);
    s.colourScale = jlimit (0, kNumColourScales - 1, s.colourScale);
    s.normalise   = jlimit (0, kNumNormalise - 1, s.normalise);
    s.freqScale   = jlimit (0, kNumFreqScales - 1, s.freqScale);
    s.windowIdx   = jlimit (0, kNumWindows - 1, s.windowIdx);
}

void SpectrogramComponent::syncControlsFromSettings()
{
    cbFirst_.setSelectedId (settings_->firstCh + 1, dontSendNotification);
    cbLast_.setSelectedId (settings_->lastCh + 1, dontSendNotification);
    cbLayout_.setSelectedId (settings_->stacked ? 2 : 1, dontSendNotification);
    cbSpan_.setSelectedId (settings_->spanIdx + 1, dontSendNotification);
    cbTop_.setSelectedId (settings_->topIdx + 1, dontSendNotification);
    cbRange_.setSelectedId (settings_->rangeIdx + 1, dontSendNotification);
    cbRes_.setSelectedId (settings_->fineBass ? 2 : 1, dontSendNotification);
    // Auto top sets the top itself.
    cbTop_.setEnabled (settings_->normalise != 3);
}

int SpectrogramComponent::numShown() const { return settings_->lastCh - settings_->firstCh + 1; }
int SpectrogramComponent::numLanes() const { return settings_->stacked ? numShown() : 1; }

float SpectrogramComponent::topDb() const
{
    return settings_->normalise == 3 ? colouredAutoTop_ : kTopDb[settings_->topIdx];
}

float SpectrogramComponent::rangeDb() const { return kRangeDb[settings_->rangeIdx]; }

Colour SpectrogramComponent::colourFor (float db, float offset) const
{
    const float top = topDb();
    const float x = db + offset;
    float t;
    switch (settings_->colourScale)
    {
        case 1:  t = iecFraction (x - top); break;                        // meter: fixed 70 dB law
        case 2:  t = Decibels::decibelsToGain (x - top, -400.f); break;   // linear amplitude
        default: t = (x - (top - rangeDb())) / rangeDb(); break;          // dB
    }
    return lut_[(size_t) jlimit (0, 255, roundToInt (t * 255.f))];
}

float SpectrogramComponent::colourOffset (int lane, int col) const
{
    // Column normalisation lifts each column so its loudest row reaches the
    // top: quiet passages and steady tones in a loud mix become visible. Half
    // (Sonic Visualiser's "hybrid", roughly) keeps half of the level
    // difference between columns. Silence is never lifted past 120 dB.
    const int mode = settings_->normalise;
    if (mode != 1 && mode != 2)
        return 0.f;
    const float m = jmax (-120.f, lanes_[(size_t) lane].colMax[(size_t) col]);
    const float lift = topDb() - m;
    return mode == 1 ? lift : 0.5f * lift;
}

void SpectrogramComponent::updateAutoTop()
{
    // Auto top (Sonic Visualiser's "view" normalisation): the loudest level in
    // the visible history becomes the top. Re-colouring everything is not
    // free, so only when it has moved by more than a dB.
    if (settings_->normalise != 3)
        return;
    float m = -120.f;
    const int filled = (int) jmin ((int64) cols_, colCount_);
    for (auto& lane : lanes_)
        for (int c = 0; c < filled; ++c)
            m = jmax (m, lane.colMax[(size_t) c]);
    autoTop_ = m;
    if (std::abs (autoTop_ - colouredAutoTop_) > 1.f)
    {
        colouredAutoTop_ = autoTop_;
        recolourAll();
    }
}

float SpectrogramComponent::rowHz (int row) const
{
    const float a = warp (fMin_), b = warp (fMax_);
    return unwarp (a + (b - a) * ((float) row + 0.5f) / (float) rows_);
}

//==============================================================================
void SpectrogramComponent::prepareAnalysis (double sr)
{
    sr_ = sr;

    // ~6 Hz bins whatever the rate: 8192 points at 48 kHz.
    // Same time span whatever the rate: 2048..16384 points at 48 kHz.
    const int order = jlimit (11, 16, 11 + settings_->windowIdx + roundToInt (std::log2 (sr / 48000.0)));
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
    prepareBass (sr);
    buildRowBins();
}

void SpectrogramComponent::prepareBass (double sr)
{
    // A power of two near sr / 3 kHz: 16 at 44.1/48 kHz, 32 at 88.2/96 kHz.
    decim_ = 1 << jlimit (0, 8, roundToInt (std::log2 (sr / (double) kDecRateHz)));
    const double srD = sr / (double) decim_;

    // Anti-alias lowpass, linear phase so its delay is one exact number to
    // take out. Only what folds onto the analysed band (< kBassTopHz) matters,
    // so the transition can run from kBassTopHz up to srD - kBassTopHz: wide,
    // hence short. Blackman-windowed sinc, ~75 dB down past the transition.
    const double transition = jmax (200.0, srD - 2.0 * kBassTopHz);
    const int L = jlimit (31, 1023, ((int) std::ceil (5.5 * sr / transition)) | 1);
    const double fc = 0.5 * srD / sr;                  // cycles per raw sample
    fir_.assign ((size_t) L, 0.f);
    double sum = 0.0;
    const int M = (L - 1) / 2;
    for (int n = 0; n < L; ++n)
    {
        const double x = (double) (n - M);
        const double sinc = x == 0.0 ? 2.0 * fc : std::sin (MathConstants<double>::twoPi * fc * x) / (MathConstants<double>::pi * x);
        const double t = MathConstants<double>::twoPi * n / (double) (L - 1);
        const double w = 0.42 - 0.5 * std::cos (t) + 0.08 * std::cos (2.0 * t);
        fir_[(size_t) n] = (float) (sinc * w);
        sum += sinc * w;
    }
    for (auto& h : fir_) h = (float) (h / sum);
    firDelay_ = M;

    mcfx::ConstantQ::Config cfg;
    cfg.sampleRate    = srD;
    cfg.fftOrder      = kBassOrder;
    cfg.fMin          = 20.0;
    cfg.fMax          = kBassTopHz;
    cfg.binsPerOctave = kCqDensity;
    cfg.resolutionBinsPerOctave = kCqResolution;
    cfg.alignment     = mcfx::ConstantQ::Alignment::Centred;
    cq_.prepare (cfg);

    cqMag_.assign ((size_t) jmax (1, cq_.getNumBins()), 0.f);
    bassPow_ = bassMag_ = cqMag_;
    resetBass();
}

void SpectrogramComponent::resetBass()
{
    decRing_.assign ((size_t) numShown(), std::vector<float> ((size_t) kDecRing, 0.f));
    nextDec_ = -1;
}

int SpectrogramComponent::leadSamples() const
{
    const int fftLead = fft_.getSize() / 2;
    if (! bassActive())
        return fftLead;
    // The bass frame's newest decimated sample, back in raw samples, plus the
    // decimator's own delay and a sample of slack for rounding to the grid.
    return jmax (fftLead, (cq_.getFrameSize() / 2) * decim_ + firDelay_ + 2 * decim_);
}

void SpectrogramComponent::updateDecimation (int64 written)
{
    if (! bassActive() || fir_.empty())
        return;

    const int L = (int) fir_.size();
    const int64 D = decim_;
    const int valid = capture_->getNumValid();

    // Start, or start over after falling behind: fill the decimated ring from
    // as much history as the capture still has, so the bass is complete at
    // once rather than building up over the first 0.7 s.
    if (nextDec_ < 0 || written - nextDec_ * D > (int64) valid - L)
    {
        const int64 startAbs = written - jmin ((int64) valid, (int64) kDecRing * D) + L;
        nextDec_ = (startAbs + D - 1) / D;
        for (auto& r : decRing_) std::fill (r.begin(), r.end(), 0.f);
    }

    const int64 lastDec = (written - 1) / D;
    if (lastDec < nextDec_)
        return;
    if (lastDec - nextDec_ + 1 > kDecRing)
        nextDec_ = lastDec - kDecRing + 1;

    const int count = (int) (lastDec - nextDec_ + 1);
    const int len   = (count - 1) * (int) D + L;
    const int shown = jmin (numShown(), (int) decRing_.size());

    // rawBuf_[0] is raw sample nextDec_*D - (L-1): exactly the history the
    // first output's filter needs.
    capture_->read (rawBuf_, settings_->firstCh, shown, len, (int) (written - 1 - lastDec * D));

    for (int c = 0; c < shown; ++c)
    {
        const float* x = rawBuf_.getReadPointer (c);
        auto& ring = decRing_[(size_t) c];
        for (int i = 0; i < count; ++i)
        {
            // Symmetric taps, so no reversal needed.
            const float* xi = x + (size_t) i * (size_t) D;
            float y = 0.f;
            for (int k = 0; k < L; ++k)
                y += fir_[(size_t) k] * xi[k];
            ring[(size_t) ((nextDec_ + i) & (kDecRing - 1))] = y;
        }
    }

    nextDec_ = lastDec + 1;
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
        // Row edges on the chosen axis, in FFT bins.
        const float w0 = warp (fMin_), w1 = warp (fMax_);
        const double lo = (double) unwarp (w0 + (w1 - w0) * (float) r / (float) rows_) / binHz;
        const double hi = (double) unwarp (w0 + (w1 - w0) * (float) (r + 1) / (float) rows_) / binHz;
        auto& rb = rowBins_[(size_t) r];
        // Only rows well under a bin wide interpolate. A row about one bin
        // wide may contain no bin centre at all, and interpolating there
        // under-reads a tone by over a dB, so it takes its nearest bins' max.
        rb.interp = (hi - lo) < 0.5;
        rb.lo = jlimit (0, maxBin, (int) std::lround (lo));
        rb.hi = jlimit (rb.lo, maxBin, (int) std::lround (hi));
        rb.centre = (float) jlimit (0.0, (double) maxBin - 1.0, std::sqrt (lo * hi));
    }

    // Where each row sits among the constant-Q bins, and how much of it the
    // bass analysis supplies: all below kSplitLo, none above kSplitHi.
    rowCqPos_.assign ((size_t) rows_, 0.f);
    rowBassW_.assign ((size_t) rows_, 0.f);
    for (int r = 0; r < rows_; ++r)
    {
        const float f = rowHz (r);
        rowCqPos_[(size_t) r] = (float) kCqDensity * std::log2 (f / 20.f);
        rowBassW_[(size_t) r] = f <= kSplitLo ? 1.f
                              : f >= kSplitHi ? 0.f
                              : 1.f - std::log (f / kSplitLo) / std::log (kSplitHi / kSplitLo);
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
        l.colMax.assign ((size_t) cols_, kFloorDb);
    }
    colCount_ = 0;
    nextColAbs_ = -1.0;
    resetBass();
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
    // The bass transform adds roughly a third of an FFT per channel.
    const double perChannel = settings_->fineBass ? 1.3 : 1.0;
    const int byBudget = (int) (budget * kSpanSec[settings_->spanIdx] / ((double) jmax (1, numShown()) * perChannel));
    const int cols = jlimit (120, 2048, jmin (roundToInt (h.getWidth()), byBudget));

    // Rows cover the whole range, so zoomed in they need more of them for the
    // shown part to stay as fine as the screen.
    const float shownFraction = (warp (zoomHi()) - warp (zoomLo())) / (warp (fMax_) - warp (fMin_));
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

        lane.colMax.assign ((size_t) newCols, kFloorDb);
        for (int c = 0; c < newCols; ++c)
            for (int r = 0; r < newRows; ++r)
                lane.colMax[(size_t) c] = jmax (lane.colMax[(size_t) c], lane.db[(size_t) c * newRows + (size_t) r]);
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

    updateDecimation (written);

    // Columns are placed by their centre instant, which lags the newest
    // sample by the lead. Start there, and start over there if we fell so far
    // behind that the frames we owe have already left the ring.
    const int lead = leadSamples();
    if (nextColAbs_ < 0.0 || (double) written - nextColAbs_ > (double) (capture_->getCapacity() - N))
        nextColAbs_ = (double) (written - lead);

    int produced = 0;
    while (nextColAbs_ + lead <= (double) written && produced < 64)
    {
        produceColumn ((int64) nextColAbs_, written);
        nextColAbs_ += hop;
        ++produced;
    }

    if (produced > 0)
    {
        updateAutoTop();
        repaint();
    }
}

void SpectrogramComponent::produceColumn (int64 centreAbs, int64 written)
{
    const int N = fft_.getSize();
    const int shown = numShown();

    // FFT frame centred on the column's instant.
    capture_->read (frame_, settings_->firstCh, shown, N, (int) jmax ((int64) 0, written - (centreAbs + N / 2)));

    // Bass frame centred on the same instant: the decimated sample whose
    // (delayed) raw time is closest to it sits in the middle of the frame,
    // where every constant-Q window is centred.
    const int Nd = cq_.getFrameSize();
    int64 bassStart = 0;
    bool haveBass = false;
    if (bassActive())
    {
        const int64 jc = (int64) std::llround ((double) (centreAbs + firDelay_) / (double) decim_);
        bassStart = jc - Nd / 2;
        haveBass = bassStart + Nd - 1 < nextDec_ && bassStart >= nextDec_ - kDecRing;
    }

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

        // Bass for this lane: constant-Q per channel, averaged in power.
        if (haveBass)
        {
            std::fill (bassPow_.begin(), bassPow_.end(), 0.f);
            for (int c = c0; c < c1 && c < (int) decRing_.size(); ++c)
            {
                const auto& ring = decRing_[(size_t) c];
                float* t = cq_.getTimeBuffer();
                for (int i = 0; i < Nd; ++i)
                    t[i] = ring[(size_t) ((bassStart + i) & (kDecRing - 1))];
                cq_.transform (cqMag_.data());
                for (size_t k = 0; k < cqMag_.size(); ++k)
                    bassPow_[k] += cqMag_[k] * cqMag_[k];
            }
            const float inv = 1.f / (float) jmax (1, c1 - c0);
            for (size_t k = 0; k < bassPow_.size(); ++k)
                bassPow_[k] *= inv;
        }
        const int lastCq = (int) bassPow_.size() - 1;

        float colMax = kFloorDb;

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

            if (haveBass && rowBassW_[(size_t) r] > 0.f)
            {
                // Interpolated in power between constant-Q bins.
                const float pos = jlimit (0.f, (float) lastCq, rowCqPos_[(size_t) r]);
                const int   k   = jmin ((int) pos, jmax (0, lastCq - 1));
                const float fr  = pos - (float) k;
                const float pb  = bassPow_[(size_t) k] + fr * (bassPow_[(size_t) jmin (k + 1, lastCq)] - bassPow_[(size_t) k]);
                const float w   = rowBassW_[(size_t) r];
                p = w * pb + (1.f - w) * p;
            }

            const float db = 10.f * std::log10 (jmax (p, 1.0e-20f));
            lane.db[(size_t) col * rows_ + (size_t) r] = db;
            colMax = jmax (colMax, db);

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

        // Coloured once the column's maximum is known, for normalising.
        lane.colMax[(size_t) col] = colMax;
        const float offset = colourOffset (li, col);
        Image::BitmapData bd (lane.image, col, 0, 1, rows_, Image::BitmapData::writeOnly);
        const float* dbCol = lane.db.data() + (size_t) col * rows_;
        for (int r = 0; r < rows_; ++r)
            bd.setPixelColour (0, rows_ - 1 - r, colourFor (dbCol[r], offset));
    }

    ++colCount_;
}

void SpectrogramComponent::recolourAll()
{
    for (int li = 0; li < (int) lanes_.size(); ++li)
    {
        auto& lane = lanes_[(size_t) li];
        Image::BitmapData bd (lane.image, Image::BitmapData::writeOnly);
        for (int c = 0; c < cols_; ++c)
        {
            const float offset = colourOffset (li, c);
            for (int r = 0; r < rows_; ++r)
                bd.setPixelColour (c, rows_ - 1 - r, colourFor (lane.db[(size_t) c * rows_ + (size_t) r], offset));
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
    else if (cb == &cbRes_)    settings_->fineBass = cbRes_.getSelectedId() == 2;

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
        || s.stacked != before.stacked || s.spanIdx != before.spanIdx
        || s.fineBass != before.fineBass)
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
namespace
{
    /** The display options, in a callout from the gear button. Edits the
        settings in place and reports each change. */
    class SpectroSettingsPanel : public Component
    {
    public:
        SpectroSettingsPanel (SpectroSettings& s, std::function<void()> onChange)
            : settings (s), changed (std::move (onChange))
        {
            addRow ("colours",        SpectrogramComponent::colourMapNames(), s.colourMap,
                    "Colour map. The first four read correctly in greyscale; black on white "
                    "is for a bright room or for printing; banded makes equal levels easy to spot.",
                    [] (SpectroSettings& x, int v) { x.colourMap = v; });
            addRow ("colour scale",   { "dB", "meter (IEC)", "linear" }, s.colourScale,
                    "How level maps to colour. dB: evenly over the range. meter: the bars' "
                    "IEC law, fixed 70 dB, more room near the top. linear: amplitude, so only "
                    "the strongest components show.",
                    [] (SpectroSettings& x, int v) { x.colourScale = v; });
            addRow ("normalise",      { "off", "each column", "half", "auto top" }, s.normalise,
                    "each column: every moment's loudest component reaches the top, so quiet "
                    "passages and steady tones in a loud mix show. half: keeps half of the "
                    "level difference between moments. auto top: the loudest level in view "
                    "sets the top.",
                    [] (SpectroSettings& x, int v) { x.normalise = v; });
            addRow ("frequency axis", { "log", "mel", "linear" }, s.freqScale,
                    "log: equal space per octave (musical). mel: roughly how pitch is heard; "
                    "more room for the mids. linear: equal space per Hz, so harmonics are "
                    "evenly spaced.",
                    [] (SpectroSettings& x, int v) { x.freqScale = v; });
            addRow ("axis labels",    { "Hz", "notes + keyboard" }, s.noteLabels ? 1 : 0,
                    "Label the frequency axis in Hz, or with note names and a keyboard.",
                    [] (SpectroSettings& x, int v) { x.noteLabels = v == 1; });
            addRow ("treble window",  { "2048 (sharpest in time)", "4096", "8192", "16384 (finest in pitch)" },
                    s.windowIdx,
                    "FFT length at 48 kHz (scaled with the rate). Shorter: transients sharper, "
                    "pitch coarser, less latency. Longer: the reverse. The fine-bass analysis "
                    "below ~450 Hz is not affected.",
                    [] (SpectroSettings& x, int v) { x.windowIdx = v; });

            addToggle ("smooth picture", s.smooth, "Interpolate between analysis cells, or show them as pixels.",
                       [] (SpectroSettings& x, bool v) { x.smooth = v; });
            addToggle ("spectrum strip", s.showStrip, "The live spectrum beside the history.",
                       [] (SpectroSettings& x, bool v) { x.showStrip = v; });
            addToggle ("harmonic cursor", s.harmonics,
                       "While hovering, mark 2x to 20x the frequency under the pointer: "
                       "tells a fundamental and its harmonics from unrelated tones.",
                       [] (SpectroSettings& x, bool v) { x.harmonics = v; });

            setSize (330, 8 + (int) combos.size() * 28 + (int) toggles.size() * 24 + 6);
        }

        void resized() override
        {
            int y = 8;
            for (size_t i = 0; i < combos.size(); ++i, y += 28)
            {
                labels[i]->setBounds (8, y, 104, 22);
                combos[i]->setBounds (114, y, getWidth() - 122, 22);
            }
            for (auto* t : toggles)
            {
                t->setBounds (110, y, getWidth() - 118, 22);
                y += 24;
            }
        }

    private:
        void addRow (const String& name, const StringArray& items, int selected, const String& tip,
                     std::function<void (SpectroSettings&, int)> set)
        {
            auto* l = labels.add (new Label ({}, name));
            l->setColour (Label::textColourId, Colours::white);
            l->setJustificationType (Justification::centredRight);
            addAndMakeVisible (l);

            auto* cb = combos.add (new ComboBox());
            cb->addItemList (items, 1);
            cb->setSelectedItemIndex (selected, dontSendNotification);
            cb->setTooltip (tip);
            cb->onChange = [this, cb, set] { set (settings, cb->getSelectedItemIndex()); changed(); };
            addAndMakeVisible (cb);
        }

        void addToggle (const String& name, bool on, const String& tip,
                        std::function<void (SpectroSettings&, bool)> set)
        {
            auto* t = toggles.add (new ToggleButton (name));
            t->setToggleState (on, dontSendNotification);
            t->setTooltip (tip);
            t->setColour (ToggleButton::textColourId, Colours::white);
            t->onClick = [this, t, set] { set (settings, t->getToggleState()); changed(); };
            addAndMakeVisible (t);
        }

        SpectroSettings& settings;
        std::function<void()> changed;
        OwnedArray<Label> labels;
        OwnedArray<ComboBox> combos;
        OwnedArray<ToggleButton> toggles;
    };
}

void SpectrogramComponent::showSettings()
{
    if (callout_ != nullptr)
        return;

    auto panel = std::make_unique<SpectroSettingsPanel> (*settings_,
        [safe = Component::SafePointer<SpectrogramComponent> (this)]
        {
            if (safe != nullptr)
                safe->settingsChanged();
        });
    callout_ = &CallOutBox::launchAsynchronously (std::move (panel), btnSettings_.getScreenBounds(), nullptr);
}

void SpectrogramComponent::settingsChanged()
{
    clampSettings();
    const auto& s = *settings_;
    const auto  a = applied_;

    if (s.colourMap != a.colourMap)
        buildLut();

    if (s.windowIdx != a.windowIdx || s.freqScale != a.freqScale)
    {
        // The analysis rows mean something else now: rebuild and start over.
        if (sr_ > 0.0)
            prepareAnalysis (sr_);
        updateWantedResolution();
        resetHistory();
    }
    else
    {
        if (s.showStrip != a.showStrip)
            updateWantedResolution();          // the history got wider or narrower

        if (s.normalise == 3 && a.normalise != 3)
            colouredAutoTop_ = autoTop_ = jmax (-120.f, autoTop_);

        if (s.colourMap != a.colourMap || s.colourScale != a.colourScale || s.normalise != a.normalise)
        {
            if (s.normalise == 3)
            {
                colouredAutoTop_ = -1000.f;    // force updateAutoTop() to recolour
                updateAutoTop();
            }
            recolourAll();
        }
    }

    applied_ = s;
    syncControlsFromSettings();
    repaint();
}

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
    place (cbRange_, 112);
    place (cbRes_, 92, 8);
    place (btnPause_, 58, 6);
    place (btnSettings_, 22);

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
    if (! settings_->showStrip)
        return l.withLeft (l.getRight());       // none: the history takes the width
    const float w = jlimit (60.f * s_, 160.f * s_, l.getWidth() * 0.15f);
    return l.withLeft (l.getRight() - w);
}

Rectangle<float> SpectrogramComponent::historyArea (int lane) const
{
    const auto l = laneArea (lane);
    return settings_->showStrip ? l.withRight (stripArea (lane).getX() - 6.f * s_) : l;
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

// The frequency axis. Everything that places a frequency — analysis rows,
// zoom, drawing, hover — goes through warp(), so log, mel and linear axes are
// one code path. warp() is monotonic; only differences of it matter.
float SpectrogramComponent::warp (float hz) const
{
    switch (settings_->freqScale)
    {
        case 1:  return 2595.f * std::log10 (1.f + hz / 700.f);    // mel
        case 2:  return hz;                                         // linear
        default: return std::log (jmax (1.0e-3f, hz));              // log
    }
}

float SpectrogramComponent::unwarp (float w) const
{
    switch (settings_->freqScale)
    {
        case 1:  return 700.f * (std::pow (10.f, w / 2595.f) - 1.f);
        case 2:  return w;
        default: return std::exp (w);
    }
}

float SpectrogramComponent::rowPos (float hz) const
{
    return (warp (hz) - warp (fMin_)) / (warp (fMax_) - warp (fMin_)) * (float) rows_;
}

void SpectrogramComponent::setZoom (float lo, float hi)
{
    const float w0 = warp (fMin_);
    const float full = warp (fMax_) - w0;
    // Half an octave on the log axis; a hundredth of the range on the others.
    const float minSpan = settings_->freqScale == 0 ? std::log (2.f) * 0.5f : full * 0.01f;

    if (hi <= 0.f || lo <= 0.f)
    {
        settings_->zoomLo = settings_->zoomHi = 0.f;
    }
    else
    {
        float a = warp (lo) - w0, b = warp (hi) - w0;
        float span = jlimit (minSpan, full, b - a);
        const float mid = 0.5f * (a + b);
        a = jlimit (0.f, full - span, mid - 0.5f * span);
        b = a + span;

        if (span >= full * 0.999f)
            settings_->zoomLo = settings_->zoomHi = 0.f;    // all of it: say so
        else
        {
            settings_->zoomLo = unwarp (w0 + a);
            settings_->zoomHi = unwarp (w0 + b);
        }
    }

    updateWantedResolution();
    repaint();
}

float SpectrogramComponent::hzToY (float hz, Rectangle<float> r) const
{
    const float a = warp (zoomLo()), b = warp (zoomHi());
    const float t = (warp (hz) - a) / (b - a);
    return r.getBottom() - t * r.getHeight();
}

float SpectrogramComponent::yToHz (float y, Rectangle<float> r) const
{
    const float a = warp (zoomLo()), b = warp (zoomHi());
    const float t = (r.getBottom() - y) / r.getHeight();
    return unwarp (a + t * (b - a));
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
    const float leadSec = sr_ > 0.0 ? (float) ((double) leadSamples() / sr_) : 0.f;
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

    g.setImageResamplingQuality (settings_->smooth ? Graphics::mediumResamplingQuality
                                                   : Graphics::lowResamplingQuality);

    // Notes, for the note labels and the keyboard.
    auto midiOf  = [] (float hz) { return 69.f + 12.f * std::log2 (jmax (1.0e-3f, hz) / 440.f); };
    auto hzOfMidi = [] (float m) { return 440.f * std::pow (2.f, (m - 69.f) / 12.f); };
    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    auto isBlackKey = [] (int n) { const int k = ((n % 12) + 12) % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; };

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
        const bool hasStrip = s.getWidth() >= 1.f;
        if (hasStrip)
        {
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

        // Colour legend along the strip's bottom edge: the strip's level axis
        // is the colour scale's, so this is exactly how a level is coloured.
        {
            const float lh = 5.f * s_;
            for (int px = 0; px < (int) s.getWidth(); ++px)
            {
                const float db = floorDb + rangeDb() * ((float) px + 0.5f) / s.getWidth();
                g.setColour (colourFor (db));
                g.fillRect (s.getX() + (float) px, s.getBottom() - lh, 1.f, lh);
            }
        }
        }

        // Shared frequency grid, across history and strip alike: Hz, or notes
        // with a keyboard down the axis (as Sonic Visualiser draws it).
        struct Mark { float hz; String text; bool major; };
        std::vector<Mark> marks;
        const bool notes = settings_->noteLabels;
        const float kbW = notes && h.getHeight() >= 60.f * s_ ? 10.f * s_ : 0.f;

        if (notes)
        {
            const int n0 = (int) std::ceil (midiOf (zLo)), n1 = (int) std::floor (midiOf (zHi));
            for (int n = n0; n <= n1; ++n)
            {
                const float f = hzOfMidi ((float) n);
                const float sp = std::abs (hzToY (f, h) - hzToY (hzOfMidi ((float) n + 1.f), h));
                const bool isC = ((n % 12) + 12) % 12 == 0;
                // Every C; every note once there is room for each.
                if (isC || sp >= 13.f * s_)
                    marks.push_back ({ f, String (noteNames[((n % 12) + 12) % 12]) + String (n / 12 - 1), isC });
            }
        }
        else if (settings_->freqScale == 2)
        {
            // Linear axis: an even step from 1-2-5, as many as fit.
            const float want = jmax (2.f, h.getHeight() / (32.f * s_));
            const float raw = (zHi - zLo) / want;
            const float p10 = std::pow (10.f, std::floor (std::log10 (raw)));
            const float m = raw / p10;
            const float step = p10 * (m <= 1.f ? 1.f : m <= 2.f ? 2.f : m <= 5.f ? 5.f : 10.f);
            for (float f = std::ceil (zLo / step) * step; f <= zHi; f += step)
                marks.push_back ({ f, f >= 1000.f ? String (f / 1000.f) + "k" : String (roundToInt (f)), true });
        }
        else
        {
            for (float f : gridHz)
                if (f >= zLo && f <= zHi)
                    marks.push_back ({ f, f >= 1000.f ? String (f / 1000.f) + "k" : String ((int) f), true });
        }

        if (kbW > 0.f)
        {
            // The keyboard, in the margin against the history's left edge.
            const float x0 = h.getX() - kbW - 1.f;
            const int n0 = (int) std::floor (midiOf (zLo)), n1 = (int) std::ceil (midiOf (zHi));
            for (int n = n0; n <= n1; ++n)
            {
                const float yTop = jmax (h.getY(), hzToY (hzOfMidi ((float) n + 0.5f), h));
                const float yBot = jmin (h.getBottom(), hzToY (hzOfMidi ((float) n - 0.5f), h));
                if (yBot <= yTop) continue;
                const bool isC = ((n % 12) + 12) % 12 == 0;
                g.setColour (isBlackKey (n) ? Colour (0xff1c1c1c) : isC ? Colour (0xff9c9c9c) : Colour (0xffd4d4d4));
                g.fillRect (x0, yTop, kbW, yBot - yTop);
                if (! isBlackKey (n) && yBot - yTop > 3.f)
                {
                    g.setColour (Colours::black.withAlpha (0.35f));
                    g.drawHorizontalLine ((int) yBot, x0, x0 + kbW);
                }
            }
        }

        g.setFont (Font (FontOptions (10.f * s_)));
        float lastY = 1.0e9f;
        for (auto& mk : marks)
        {
            const float y = hzToY (mk.hz, h);
            if (lastY - y < (notes ? 12.f : 16.f) * s_) continue;   // would crowd the one below
            lastY = y;

            g.setColour (Colours::white.withAlpha (mk.major ? 0.14f : 0.07f));
            g.drawHorizontalLine ((int) y, h.getX(), h.getRight());
            if (hasStrip)
                g.drawHorizontalLine ((int) y, s.getX(), s.getRight());
            if (h.getHeight() >= 60.f * s_)
            {
                g.setColour (Colours::white.withAlpha (mk.major ? 0.8f : 0.55f));
                // Kept inside the lane, so the top label of one stacked lane
                // cannot land on the bottom label of the lane above.
                const int ly = jlimit ((int) h.getY(), (int) h.getBottom() - S (12), (int) y - S (6));
                g.drawText (mk.text, 0, ly, S (kMarginL) - 4 - roundToInt (kbW + (kbW > 0.f ? 2.f : 0.f)),
                            S (12), Justification::centredRight);
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
        if (hasStrip)
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
            // Relative to the real "now": the newest column is lead old.
            const float t = -leadSec - span * (1.f - (float) i / 4.f);
            const String txt = (std::abs (t) < 1.f ? String (t, 2)
                                 : std::abs (t) < 10.f ? String (t, 1) : String (roundToInt (t))) + " s";
            const auto j = i == 0 ? Justification::centredLeft : (i == 4 ? Justification::centredRight : Justification::centred);
            g.drawText (txt, i == 0 ? (int) x : (i == 4 ? (int) x - S (60) : (int) x - S (30)), y, S (60), S (12), j);
        }
        if (s.getWidth() >= 1.f)
        {
            g.drawText (String ((int) floorDb), (int) s.getX(), y, S (40), S (12), Justification::centredLeft);
            g.drawText (String (roundToInt (topDb())) + " dB", (int) s.getRight() - S (50), y, S (50), S (12),
                        Justification::centredRight);
        }
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

    if (inH || inS)
    {
        // Harmonic cursor: 2x..20x the pointer's frequency, longer ticks on the
        // even and the fourfold ones (as Sonic Visualiser marks them). Lines a
        // fundamental's partials up at a glance, in the history and against
        // the strip's spectrum alike.
        if (settings_->harmonics)
        {
            g.setFont (Font (FontOptions (9.f * s_)));
            float lastLabelY = 1.0e9f;
            for (int k = 2; k <= 20; ++k)
            {
                const float fk = hz * (float) k;
                if (fk > zoomHi()) break;
                const float y = hzToY (fk, h);
                const float len = (k % 4 == 0 ? 22.f : k % 2 == 0 ? 15.f : 9.f) * s_;
                g.setColour (Colours::white.withAlpha (0.85f));
                const float cx = inH ? hover_.x : h.getRight() - 14.f * s_;
                g.drawLine (cx - 0.5f * len, y, cx + 0.5f * len, y, jmax (1.f, s_));
                if (s.getWidth() >= 1.f)
                    g.drawLine (s.getX(), y, s.getX() + 0.4f * len, y, jmax (1.f, s_));
                if (lastLabelY - y >= 11.f * s_)
                {
                    g.drawText (String (k), roundToInt (cx + 0.5f * len) + 2, (int) y - S (5), S (20), S (10),
                                Justification::centredLeft);
                    lastLabelY = y;
                }
            }
        }
    }

    if (inH)
    {
        g.drawVerticalLine ((int) hover_.x, h.getY(), h.getBottom());
        const float cw = h.getWidth() / (float) cols_;
        const int back = (int) ((h.getRight() - hover_.x) / cw);
        if (back < filled && newest >= 0)
        {
            const int col = ((newest - back) % cols_ + cols_) % cols_;
            lines.add (String (lane.db[(size_t) col * rows_ + (size_t) row], 1) + " dB");
            lines.add (String (-leadSec - (float) back * span / (float) cols_, 2) + " s");
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
    const float a = warp (dragZoomLo_), b = warp (dragZoomHi_);
    const float shift = (float) e.getDistanceFromDragStartY() / h.getHeight() * (b - a);
    setZoom (unwarp (a + shift), unwarp (b + shift));
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
    const float a = warp (zoomLo()), b = warp (zoomHi()), x = warp (hz);
    const float t = (x - a) / (b - a);
    const float span = (b - a) * std::exp (-d * 1.5f);    // wheel up = zoom in
    setZoom (unwarp (x - t * span), unwarp (x + (1.f - t) * span));
}
