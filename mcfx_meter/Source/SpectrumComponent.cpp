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

#include "SpectrumComponent.h"
#include "OscilloscopeComponent.h"   // displayScaleFor(), channel colours
#include "ViewStyle.h"

namespace
{
    // Time constant of the exponential average; 0 = none.
    constexpr float kAvgSec[]    = { 0.f, 0.125f, 1.f, 5.f };
    constexpr float kSmoothOct[] = { 0.f, 1.f / 24.f, 1.f / 12.f, 1.f / 6.f, 1.f / 3.f };
    constexpr float kTopDb[]     = { 0.f, -10.f, -20.f, -30.f, -40.f };
    constexpr float kRangeDb[]   = { 60.f, 90.f, 120.f, 150.f };
    template <typename T, size_t N> constexpr int countOf (const T (&)[N]) { return (int) N; }

    constexpr int   kToolbarH = 22;
    constexpr int   kPlotTop  = 28;
    constexpr float kMarginL  = 46.f, kMarginR = 6.f, kMarginB = 16.f;

    constexpr float kPeakHoldSec = 2.f;     // falling peaks wait this long...
    constexpr float kPeakFallDbS = 20.f;    // ...then fall at this rate
    constexpr int   kNumWindows = 4, kNumPeakModes = 3;

    // Limits of the free level view, and how far a trackpad delta pans.
    constexpr float kMinRangeDb = 12.f, kMaxRangeDb = 200.f, kMaxTopDb = 24.f;
    constexpr float kWheelPx = 256.f;

    const Colour kAverageColour = ViewStyle::accent;

    // Third-octave band centres, base-2 (1 kHz * 2^(k/3)), ISO-labelled.
    constexpr float kBandLabels[] = { 20.f, 25.f, 31.5f, 40.f, 50.f, 63.f, 80.f, 100.f, 125.f, 160.f,
                                      200.f, 250.f, 315.f, 400.f, 500.f, 630.f, 800.f, 1000.f, 1250.f,
                                      1600.f, 2000.f, 2500.f, 3150.f, 4000.f, 5000.f, 6300.f, 8000.f,
                                      10000.f, 12500.f, 16000.f, 20000.f };
    constexpr int kBandK0 = -17;            // kBandLabels[0] is 1 kHz * 2^(-17/3)

    float powToDb (float p) { return 10.f * std::log10 (jmax (p, 1.0e-20f)); }

    /** The settings behind the gear button. Edits in place, reports each change. */
    class SpectrumSettingsPanel : public ViewStyle::SettingsPanel<SpectrumSettings>
    {
    public:
        SpectrumSettingsPanel (SpectrumSettings& s, std::function<void()> onChange)
            : SettingsPanel (s, std::move (onChange))
        {
            addRow ("FFT window", { "4096 (fastest)", "8192", "16384", "32768 (finest)" }, s.windowIdx,
                    "FFT length at 48 kHz (scaled with the rate). Longer resolves the bass "
                    "better (8192: 5.9 Hz bins, 32768: 1.5 Hz) but reacts more slowly.",
                    [] (SpectrumSettings& x, int v) { x.windowIdx = v; });
            addRow ("peak hold", { "off", "falling", "held (click clears)" }, s.peakMode,
                    "falling: each frequency's peak waits 2 s, then falls at 20 dB/s. "
                    "held: stays until you click the plot.",
                    [] (SpectrumSettings& x, int v) { x.peakMode = v; });
            addRow ("display", { "curve", "third-octave bands" }, s.bands ? 1 : 0,
                    "curve: the spectrum, smoothed as chosen in the toolbar. bands: power in "
                    "each third octave, as a hardware RTA shows it; a tone reads its own level, "
                    "noise its band power. Bands below ~100 Hz need a long window.",
                    [] (SpectrumSettings& x, int v) { x.bands = v == 1; });
            addToggle ("fill under curve", s.fill, "Shade under the curve when only one is shown.",
                       [] (SpectrumSettings& x, bool v) { x.fill = v; });

            fitToContent();
        }
    };
}

//==============================================================================
SpectrumComponent::SpectrumComponent()
{
    setOpaque (false);
    settings_ = &ownSettings_;

    auto setupCombo = [this] (ComboBox& cb, const String& tip)
    {
        addAndMakeVisible (cb);
        cb.setTooltip (tip);
        cb.addListener (this);
    };

    setupCombo (cbFirst_, "First channel analysed.");
    setupCombo (cbLast_,  "Last channel analysed (at most " + String (kMaxChannels) + " at once). "
                          "Set it to the first one for a single channel.");

    setupCombo (cbLayout_, "One curve per channel, or the channels' power average as one curve.");
    cbLayout_.addItem ("overlay", 1);
    cbLayout_.addItem ("average", 2);

    setupCombo (cbAvg_, "Time averaging (exponential, in power). Longer is steadier for noise "
                        "and music, shorter follows changes.");
    cbAvg_.addItem ("no average", 1);
    cbAvg_.addItem ("fast 125 ms", 2);
    cbAvg_.addItem ("slow 1 s", 3);
    cbAvg_.addItem ("long 5 s", 4);

    setupCombo (cbSmooth_, "Fractional-octave smoothing: power averaged over this band around "
                           "each frequency. Shows the tonal balance; a single tone reads lower.");
    cbSmooth_.addItem ("smooth off", 1);
    cbSmooth_.addItem ("1/24 oct", 2);
    cbSmooth_.addItem ("1/12 oct", 3);
    cbSmooth_.addItem ("1/6 oct", 4);
    cbSmooth_.addItem ("1/3 oct", 5);

    setupCombo (cbTop_, "Level at the top of the plot (dBFS; a full-scale sine is 0).");
    for (int i = 0; i < countOf (kTopDb); ++i)
        cbTop_.addItem ("top " + String ((int) kTopDb[i]) + " dB", i + 1);

    setupCombo (cbRange_, "Levels shown below the top.");
    for (int i = 0; i < countOf (kRangeDb); ++i)
        cbRange_.addItem ("range " + String ((int) kRangeDb[i]) + " dB", i + 1);

    addAndMakeVisible (btnPause_);
    btnPause_.setButtonText ("pause");
    btnPause_.setClickingTogglesState (true);
    btnPause_.setColour (TextButton::buttonOnColourId, ViewStyle::pauseButtonOn);
    btnPause_.setTooltip ("Freeze the curves to read them. Hover for frequency, note and level.");
    btnPause_.addListener (this);

    ViewStyle::makeGearButton (btnSettings_, "FFT window, peak hold, curve or third-octave bands...");
    btnSettings_.onClick = [this] { showSettings(); };
    addAndMakeVisible (btnSettings_);

    syncControlsFromSettings();
}

SpectrumComponent::~SpectrumComponent()
{
    if (auto* c = callout_.getComponent())
        delete c;
}

void SpectrumComponent::setSettings (SpectrumSettings* settings)
{
    settings_ = settings != nullptr ? settings : &ownSettings_;
    clampSettings();
    syncControlsFromSettings();
    if (sr_ > 0.0)
        prepareAnalysis (sr_);
    else
        resetCurves();
    applied_ = *settings_;
}

void SpectrumComponent::setNumChannels (int numChannels)
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
        resetCurves();
    }
}

void SpectrumComponent::clampSettings()
{
    auto& s = *settings_;
    s.firstCh   = jlimit (0, numCh_ - 1, s.firstCh);
    s.lastCh    = jlimit (s.firstCh, jmin (numCh_ - 1, s.firstCh + kMaxChannels - 1), s.lastCh);
    s.avgIdx    = jlimit (0, countOf (kAvgSec) - 1, s.avgIdx);
    s.smoothIdx = jlimit (0, countOf (kSmoothOct) - 1, s.smoothIdx);
    s.topIdx    = jlimit (0, countOf (kTopDb) - 1, s.topIdx);
    s.rangeIdx  = jlimit (0, countOf (kRangeDb) - 1, s.rangeIdx);
    s.windowIdx = jlimit (0, kNumWindows - 1, s.windowIdx);
    s.peakMode  = jlimit (0, kNumPeakModes - 1, s.peakMode);
    if (! (s.zoomHi > s.zoomLo && s.zoomLo > 0.f))
        s.zoomLo = s.zoomHi = 0.f;
    if (s.dbRange > 0.f)
    {
        s.dbRange = jlimit (kMinRangeDb, kMaxRangeDb, s.dbRange);
        s.dbTop   = jlimit (-kMaxRangeDb + s.dbRange, kMaxTopDb, s.dbTop);
    }
    else
        s.dbTop = s.dbRange = 0.f;
}

void SpectrumComponent::syncControlsFromSettings()
{
    cbFirst_.setSelectedId (settings_->firstCh + 1, dontSendNotification);
    cbLast_.setSelectedId (settings_->lastCh + 1, dontSendNotification);
    cbLayout_.setSelectedId (settings_->average ? 2 : 1, dontSendNotification);
    cbAvg_.setSelectedId (settings_->avgIdx + 1, dontSendNotification);
    cbSmooth_.setSelectedId (settings_->smoothIdx + 1, dontSendNotification);
    if (settings_->dbRange > 0.f)
    {
        // Off the steps: say where it is instead.
        cbTop_.setText ("top " + String (roundToInt (settings_->dbTop)) + " dB", dontSendNotification);
        cbRange_.setText ("range " + String (roundToInt (settings_->dbRange)) + " dB", dontSendNotification);
    }
    else
    {
        cbTop_.setSelectedId (settings_->topIdx + 1, dontSendNotification);
        cbRange_.setSelectedId (settings_->rangeIdx + 1, dontSendNotification);
    }
    // The bands have their own fixed width.
    cbSmooth_.setEnabled (! settings_->bands);
}

int SpectrumComponent::numShown() const  { return settings_->lastCh - settings_->firstCh + 1; }
int SpectrumComponent::numCurves() const { return settings_->average ? 1 : numShown(); }

float SpectrumComponent::topDb() const
{
    return settings_->dbRange > 0.f ? settings_->dbTop : kTopDb[settings_->topIdx];
}

float SpectrumComponent::rangeDb() const
{
    return settings_->dbRange > 0.f ? settings_->dbRange : kRangeDb[settings_->rangeIdx];
}

//==============================================================================
void SpectrumComponent::prepareAnalysis (double sr)
{
    sr_ = sr;

    // Same time span whatever the rate: 4096..32768 points at 48 kHz, no longer
    // than the capture holds.
    int order = jlimit (11, 17, 12 + settings_->windowIdx + roundToInt (std::log2 (sr / 48000.0)));
    if (capture_ != nullptr && capture_->getCapacity() > 0)
        order = jmin (order, (int) std::floor (std::log2 ((double) capture_->getCapacity())));
    fft_.prepare (order);
    const int N = fft_.getSize();

    window_.resize ((size_t) N);
    double sum = 0.0, sumSq = 0.0;
    for (int i = 0; i < N; ++i)
    {
        const double t = MathConstants<double>::twoPi * i / (double) (N - 1);
        const double w = 0.35875 - 0.48829 * std::cos (t)
                       + 0.14128 * std::cos (2.0 * t) - 0.01168 * std::cos (3.0 * t);
        window_[(size_t) i] = (float) w;
        sum += w;
        sumSq += w * w;
    }

    // Unnormalised DFT: a sine of amplitude A peaks at A * N * wg / 2 in its
    // bin, so this turns bin power into A^2 and a full-scale sine reads 0 dB.
    const double wg = sum / (double) N;
    ampNorm_ = (float) (4.0 / ((double) N * wg * (double) N * wg));
    // How many bins a tone's power is spread over (2.0 for Blackman-Harris).
    enbw_ = (float) ((double) N * sumSq / (sum * sum));

    magSq_.assign ((size_t) fft_.getNumBins(), 0.f);
    resetCurves();
}

void SpectrumComponent::resetCurves()
{
    const int bins = fft_.isReady() ? fft_.getNumBins() : 1;
    curves_.resize ((size_t) numCurves());
    for (auto& c : curves_)
    {
        c.avg.assign ((size_t) bins, 0.f);
        c.peak.assign ((size_t) bins, 0.f);
        c.peakAge.assign ((size_t) bins, 0.f);
        c.cum.assign ((size_t) bins + 1, 0.0);
        c.cumPeak.assign ((size_t) bins + 1, 0.0);
    }
    fresh_ = true;
    lastWritten_ = -1;
    repaint();
}

void SpectrumComponent::clearPeaks()
{
    for (auto& c : curves_)
    {
        c.peak = c.avg;
        std::fill (c.peakAge.begin(), c.peakAge.end(), 0.f);
        c.cumPeak = c.cum;
    }
    repaint();
}

void SpectrumComponent::tick()
{
    if (capture_ == nullptr || ! capture_->isReady() || ! isShowing())
        return;

    const double sr = capture_->getSampleRate();
    if (sr != sr_ || ! fft_.isReady())
        prepareAnalysis (sr);

    if (paused_)
        return;

    // Nothing new (transport stopped): keep what is shown.
    const int64 written = capture_->getNumWritten();
    if (written == lastWritten_ || written <= 0)
        return;

    const int N = fft_.getSize();
    const int bins = fft_.getNumBins();
    const int shown = numShown();
    const int64 newSamples = lastWritten_ < 0 ? (int64) N : written - lastWritten_;
    lastWritten_ = written;

    capture_->readEndingAt (frame_, settings_->firstCh, shown, N, written - 1);

    const float dt    = (float) ((double) newSamples / sr);
    const float tau   = kAvgSec[settings_->avgIdx];
    const float alpha = fresh_ || tau <= 0.f ? 0.f : std::exp (-dt / tau);
    const float fall  = std::pow (10.f, -kPeakFallDbS * dt / 10.f);     // per tick, in power
    const int   peakMode = settings_->peakMode;

    for (int ci = 0; ci < (int) curves_.size(); ++ci)
    {
        auto& c = curves_[(size_t) ci];
        const int c0 = settings_->average ? 0 : ci;
        const int c1 = settings_->average ? shown : ci + 1;
        const float scale = ampNorm_ / (float) jmax (1, c1 - c0);

        // Instantaneous power, averaged over the channels of this curve.
        accum_.assign ((size_t) bins, 0.f);
        for (int ch = c0; ch < c1 && ch < frame_.getNumChannels(); ++ch)
        {
            FloatVectorOperations::multiply (fft_.getTimeBuffer(), frame_.getReadPointer (ch), window_.data(), N);
            fft_.magnitudesSquared (magSq_.data());
            FloatVectorOperations::add (accum_.data(), magSq_.data(), bins);
        }

        for (int k = 0; k < bins; ++k)
        {
            float& a = c.avg[(size_t) k];
            a = alpha * a + (1.f - alpha) * accum_[(size_t) k] * scale;

            float& pk  = c.peak[(size_t) k];
            float& age = c.peakAge[(size_t) k];
            if (fresh_ || peakMode == 0 || a >= pk)
            {
                pk = a;
                age = 0.f;
            }
            else if (peakMode == 1 && (age += dt) > kPeakHoldSec)
            {
                pk = jmax (a, pk * fall);
            }

            c.cum[(size_t) k + 1]     = c.cum[(size_t) k] + (double) a;
            c.cumPeak[(size_t) k + 1] = c.cumPeak[(size_t) k] + (double) pk;
        }
    }

    fresh_ = false;
    repaint();
}

//==============================================================================
float SpectrumComponent::powerAt (int ci, float hzLo, float hzHi, bool peak) const
{
    if (! isPositiveAndBelow (ci, (int) curves_.size()) || sr_ <= 0.0 || ! fft_.isReady())
        return 0.f;

    const auto& c   = curves_[(size_t) ci];
    const auto& v   = peak ? c.peak : c.avg;
    const auto& cum = peak ? c.cumPeak : c.cum;
    const int bins  = (int) v.size();
    const double binHz = sr_ / (double) fft_.getSize();

    const float fc  = std::sqrt (hzLo * hzHi);
    const float oct = kSmoothOct[settings_->smoothIdx];
    if (oct > 0.f)
    {
        hzLo = jmin (hzLo, fc * std::exp2 (-0.5f * oct));
        hzHi = jmax (hzHi, fc * std::exp2 (0.5f * oct));
    }

    const double a = hzLo / binHz, b = hzHi / binHz;

    // Narrower than a bin (the low end of the plot): interpolate, so the curve
    // stays smooth instead of turning into a staircase.
    if (! (b - a >= 1.0))
    {
        const double x = jlimit (0.0, (double) bins - 1.001, (double) fc / binHz);
        const int i = (int) x;
        const float f = (float) (x - i);
        return v[(size_t) i] + f * (v[(size_t) i + 1] - v[(size_t) i]);
    }

    if (oct <= 0.f)
    {
        // Unsmoothed: the loudest bin in the pixel, so narrow peaks high up the
        // log axis cannot fall between pixels.
        const int b0 = jlimit (0, bins - 1, (int) std::floor (a + 0.5));
        const int b1 = jlimit (b0, bins - 1, (int) std::floor (b + 0.5));
        float m = 0.f;
        for (int k = b0; k <= b1; ++k)
            m = jmax (m, v[(size_t) k]);
        return m;
    }

    // Smoothed: mean power over the band, bin k covering [k - 0.5, k + 0.5).
    auto integral = [&] (double x)
    {
        const double u = jlimit (0.0, (double) bins, x + 0.5);
        const int i = jmin ((int) u, bins - 1);
        return cum[(size_t) i] + (u - (double) i) * (double) v[(size_t) i];
    };
    return (float) ((integral (b) - integral (a)) / (b - a));
}

float SpectrumComponent::bandPower (int ci, float hzLo, float hzHi, bool peak) const
{
    if (! isPositiveAndBelow (ci, (int) curves_.size()) || sr_ <= 0.0 || ! fft_.isReady())
        return 0.f;

    const auto& c   = curves_[(size_t) ci];
    const auto& v   = peak ? c.peak : c.avg;
    const auto& cum = peak ? c.cumPeak : c.cum;
    const int bins  = (int) v.size();
    const double binHz = sr_ / (double) fft_.getSize();
    const double a = hzLo / binHz, b = hzHi / binHz;

    if (! (b - a >= 1.0))
    {
        // Under a bin wide: the interpolated density times the band's width.
        const double x = jlimit (0.0, (double) bins - 1.001, std::sqrt (a * b));
        const int i = (int) x;
        const float f = (float) (x - i);
        return (v[(size_t) i] + f * (v[(size_t) i + 1] - v[(size_t) i])) * (float) (b - a) / enbw_;
    }

    auto integral = [&] (double x)
    {
        const double u = jlimit (0.0, (double) bins, x + 0.5);
        const int i = jmin ((int) u, bins - 1);
        return cum[(size_t) i] + (u - (double) i) * (double) v[(size_t) i];
    };
    return (float) ((integral (b) - integral (a)) / (double) enbw_);
}

//==============================================================================
void SpectrumComponent::setPaused (bool shouldBePaused)
{
    if (paused_ == shouldBePaused)
        return;

    paused_ = shouldBePaused;
    if (! paused_)
        lastWritten_ = -1;      // carry on from now, not across the pause

    btnPause_.setToggleState (paused_, dontSendNotification);
    btnPause_.setButtonText (paused_ ? "run" : "pause");
    repaint();
}

void SpectrumComponent::visibilityChanged()
{
    if (! isVisible())
        setPaused (false);
    fresh_ = true;              // whatever was averaged is stale by now
    lastWritten_ = -1;
}

void SpectrumComponent::comboBoxChanged (ComboBox* cb)
{
    const auto before = *settings_;

    if      (cb == &cbFirst_)  { settings_->firstCh = cbFirst_.getSelectedId() - 1;
                                 settings_->lastCh = jmax (settings_->lastCh, settings_->firstCh); }
    else if (cb == &cbLast_)   { settings_->lastCh = cbLast_.getSelectedId() - 1;
                                 settings_->firstCh = jmin (settings_->firstCh, settings_->lastCh); }
    else if (cb == &cbLayout_) settings_->average   = cbLayout_.getSelectedId() == 2;
    else if (cb == &cbAvg_)    settings_->avgIdx    = cbAvg_.getSelectedId() - 1;
    else if (cb == &cbSmooth_) settings_->smoothIdx = cbSmooth_.getSelectedId() - 1;
    else if (cb == &cbTop_ || cb == &cbRange_)
    {
        // A step picked off the free view keeps the other end where it was.
        const float top = cb == &cbTop_ ? kTopDb[cbTop_.getSelectedId() - 1] : topDb();
        const float range = cb == &cbRange_ ? kRangeDb[cbRange_.getSelectedId() - 1] : rangeDb();
        setLevelView (top, range);
    }

    // Keep the range within the analysis budget by moving the other end.
    if (settings_->lastCh - settings_->firstCh + 1 > kMaxChannels)
    {
        if (cb == &cbFirst_) settings_->lastCh  = settings_->firstCh + kMaxChannels - 1;
        else                 settings_->firstCh = settings_->lastCh - kMaxChannels + 1;
    }

    clampSettings();
    syncControlsFromSettings();

    const auto& s = *settings_;
    if (s.firstCh != before.firstCh || s.lastCh != before.lastCh || s.average != before.average)
        resetCurves();          // the curves mean something else now
    applied_ = s;
    repaint();
}

void SpectrumComponent::buttonClicked (Button* b)
{
    if (b == &btnPause_)
        setPaused (btnPause_.getToggleState());
}

void SpectrumComponent::showSettings()
{
    if (callout_ != nullptr)
        return;

    auto panel = std::make_unique<SpectrumSettingsPanel> (*settings_,
        [safe = Component::SafePointer<SpectrumComponent> (this)]
        {
            if (safe != nullptr)
                safe->settingsChanged();
        });
    callout_ = &ViewStyle::launchCallout (std::move (panel), btnSettings_);
}

void SpectrumComponent::settingsChanged()
{
    clampSettings();
    const auto& s = *settings_;

    if (s.windowIdx != applied_.windowIdx && sr_ > 0.0)
        prepareAnalysis (sr_);
    else if (s.peakMode != applied_.peakMode)
        clearPeaks();

    applied_ = s;
    syncControlsFromSettings();
    repaint();
}

void SpectrumComponent::resized()
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
    place (cbAvg_, 96);
    place (cbSmooth_, 104, 10);
    place (cbTop_, 92);
    place (cbRange_, 112, 8);
    place (btnPause_, 58, 6);
    place (btnSettings_, 22);
}

//==============================================================================
Rectangle<float> SpectrumComponent::plotArea() const
{
    return Rectangle<float> (kMarginL * s_, (float) kPlotTop,
                             jmax (10.f, (float) getWidth() - kMarginL * s_ - kMarginR),
                             jmax (10.f, (float) getHeight() - (float) kPlotTop - kMarginB * s_));
}

float SpectrumComponent::zoomLo() const
{
    const float fMin = 20.f;
    const float fMax = (float) jmin (20000.0, 0.45 * (sr_ > 0.0 ? sr_ : 48000.0));
    return settings_->zoomHi > 0.f ? jlimit (fMin, fMax, settings_->zoomLo) : fMin;
}

float SpectrumComponent::zoomHi() const
{
    const float fMax = (float) jmin (20000.0, 0.45 * (sr_ > 0.0 ? sr_ : 48000.0));
    return settings_->zoomHi > 0.f ? jlimit (zoomLo() * 1.01f, fMax, settings_->zoomHi) : fMax;
}

void SpectrumComponent::setZoom (float lo, float hi)
{
    const float fMin = 20.f;
    const float fMax = (float) jmin (20000.0, 0.45 * (sr_ > 0.0 ? sr_ : 48000.0));
    const float full = std::log (fMax / fMin);

    if (hi <= 0.f || lo <= 0.f)
    {
        settings_->zoomLo = settings_->zoomHi = 0.f;
    }
    else
    {
        float a = std::log (lo / fMin), b = std::log (hi / fMin);
        const float span = jlimit (std::log (2.f) * 0.5f, full, b - a);   // half an octave at least
        const float mid = 0.5f * (a + b);
        a = jlimit (0.f, full - span, mid - 0.5f * span);
        b = a + span;

        if (span >= full * 0.999f)
            settings_->zoomLo = settings_->zoomHi = 0.f;
        else
        {
            settings_->zoomLo = fMin * std::exp (a);
            settings_->zoomHi = fMin * std::exp (b);
        }
    }
    repaint();
}

void SpectrumComponent::zoomFreqAbout (float x, float factor)
{
    // The frequency under `x` stays where it is.
    const float f = std::log (xToHz (x));
    const float a = std::log (zoomLo()), b = std::log (zoomHi());
    const float t = (f - a) / (b - a);
    const float span = (b - a) * factor;
    setZoom (std::exp (f - t * span), std::exp (f + (1.f - t) * span));
}

void SpectrumComponent::panFreq (float dx)
{
    const float a = std::log (zoomLo()), b = std::log (zoomHi());
    const float shift = -dx / plotArea().getWidth() * (b - a);
    if (settings_->zoomHi > 0.f)
        setZoom (std::exp (a + shift), std::exp (b + shift));
}

void SpectrumComponent::setLevelView (float top, float range)
{
    auto& s = *settings_;
    range = jlimit (kMinRangeDb, kMaxRangeDb, range);
    top   = jlimit (-kMaxRangeDb + range, kMaxTopDb, top);

    // Landing on the steps (or picking them) shows them as steps again.
    auto stepOf = [] (const float* steps, int n, float v)
    {
        for (int i = 0; i < n; ++i)
            if (std::abs (steps[i] - v) < 0.05f)
                return i;
        return -1;
    };
    const int ti = stepOf (kTopDb, countOf (kTopDb), top);
    const int ri = stepOf (kRangeDb, countOf (kRangeDb), range);
    if (ti >= 0 && ri >= 0)
    {
        s.topIdx = ti;
        s.rangeIdx = ri;
        s.dbTop = s.dbRange = 0.f;
    }
    else
    {
        s.dbTop = top;
        s.dbRange = range;
    }
    syncControlsFromSettings();
    repaint();
}

void SpectrumComponent::zoomLevelAbout (float y, float factor)
{
    // The level under `y` stays where it is.
    const auto p = plotArea();
    const float t = jlimit (0.f, 1.f, (y - p.getY()) / p.getHeight());
    const float range = jlimit (kMinRangeDb, kMaxRangeDb, rangeDb() * factor);
    const float db = topDb() - t * rangeDb();
    setLevelView (db + t * range, range);
}

SpectrumComponent::Region SpectrumComponent::regionAt (Point<float> pt) const
{
    const auto p = plotArea();
    if (p.contains (pt))
        return Region::plot;
    if (pt.x < p.getX() && pt.y >= p.getY() - 6.f && pt.y <= p.getBottom() + 6.f)
        return Region::levelAxis;
    if (pt.y > p.getBottom() && pt.x >= p.getX() && pt.x <= p.getRight())
        return Region::freqAxis;
    return Region::none;
}

float SpectrumComponent::hzToX (float hz) const
{
    const auto p = plotArea();
    const float a = std::log (zoomLo()), b = std::log (zoomHi());
    return p.getX() + (std::log (jmax (1.0e-3f, hz)) - a) / (b - a) * p.getWidth();
}

float SpectrumComponent::xToHz (float x) const
{
    const auto p = plotArea();
    const float a = std::log (zoomLo()), b = std::log (zoomHi());
    return std::exp (a + (x - p.getX()) / p.getWidth() * (b - a));
}

float SpectrumComponent::dbToY (float db) const
{
    const auto p = plotArea();
    return p.getY() + (topDb() - db) / rangeDb() * p.getHeight();
}

Colour SpectrumComponent::curveColour (int c) const
{
    return settings_->average ? kAverageColour
                              : OscilloscopeComponent::channelColour (settings_->firstCh + c);
}

String SpectrumComponent::curveName (int c) const
{
    if (! settings_->average || numShown() == 1)
        return "ch " + String (settings_->firstCh + c + 1);
    return "ch " + String (settings_->firstCh + 1) + "-" + String (settings_->lastCh + 1) + " avg";
}

String SpectrumComponent::formatHz (float hz)
{
    return hz >= 1000.f ? String (hz / 1000.f, hz >= 10000.f ? 1 : 2) + " kHz"
                        : (hz >= 100.f ? String (roundToInt (hz)) : String (hz, 1)) + " Hz";
}

String SpectrumComponent::noteName (float hz)
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
void SpectrumComponent::paint (Graphics& g)
{
    const auto plot = plotArea();
    const float zLo = zoomLo(), zHi = zoomHi();
    const float floorDb = topDb() - rangeDb();
    const int   nCurves = (int) curves_.size();
    const bool  bands = settings_->bands;
    const bool  showPeak = settings_->peakMode != 0;
    auto S = [this] (float v) { return roundToInt (v * s_); };

    g.setColour (ViewStyle::plotBg);
    g.fillRect (plot);

    // Frequency grid: decades first, then the 2s and 5s, as many as fit.
    g.setFont (Font (FontOptions (10.f * s_)));
    for (float f : ViewStyle::freqTicks (zLo, zHi, [this] (float hz) { return hzToX (hz); }, 34.f * s_))
    {
        const float x = hzToX (f);
        g.setColour (Colours::white.withAlpha (ViewStyle::isDecade (f) ? ViewStyle::gridMajor : ViewStyle::gridMinor));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (Colours::white.withAlpha (ViewStyle::axisText));
        // Kept inside the view, so the last label is not cut off at the edge.
        const int lx = jlimit (0, getWidth() - S (40), (int) x - S (20));
        g.drawText (ViewStyle::freqLabel (f), lx, (int) plot.getBottom() + 2, S (40), S (12), Justification::centred);
    }

    // Level grid: a step that keeps the lines at least ~22 px apart.
    {
        float step = 6.f;
        for (float st : { 1.f, 2.f, 3.f, 6.f, 10.f, 12.f, 20.f, 30.f, 50.f })
        {
            step = st;
            if (plot.getHeight() * st / rangeDb() >= 22.f * s_) break;
        }
        // On whole steps, so a dragged view keeps round labels.
        const float firstDb = std::floor (topDb() / step + 0.001f) * step;
        for (float d = firstDb; d >= floorDb - 0.01f; d -= step)
        {
            const float y = dbToY (d);
            g.setColour (Colours::white.withAlpha (d == 0.f ? ViewStyle::gridMajor : ViewStyle::gridMinor));
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            g.setColour (Colours::white.withAlpha (ViewStyle::axisText));
            g.drawText (String (roundToInt (d)) + (d == firstDb ? " dB" : ""), 0,
                        jlimit ((int) plot.getY(), (int) plot.getBottom() - S (12), (int) y - S (6)),
                        S (kMarginL) - 4, S (12), Justification::centredRight);
        }
    }

    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (plot.getSmallestIntegerContainer());

        if (bands)
        {
            // Third-octave bars, side by side within each band when overlaid.
            for (int bi = 0; bi < countOf (kBandLabels); ++bi)
            {
                const float fc = 1000.f * std::exp2 ((float) (kBandK0 + bi) / 3.f);
                const float lo = fc * std::exp2 (-1.f / 6.f), hi = fc * std::exp2 (1.f / 6.f);
                if (hi < zLo || lo > zHi || hi > 0.5f * (float) (sr_ > 0.0 ? sr_ : 48000.0)) continue;
                const float x0 = hzToX (lo) + 1.f, x1 = hzToX (hi) - 1.f;
                const float w = (x1 - x0) / (float) jmax (1, nCurves);
                for (int c = 0; c < nCurves; ++c)
                {
                    const float y = dbToY (powToDb (bandPower (c, lo, hi, false)));
                    const Colour col = curveColour (c);
                    g.setColour (col.withAlpha (0.75f));
                    g.fillRect (x0 + w * (float) c, y, jmax (1.f, w - (nCurves > 1 ? 1.f : 0.f)), plot.getBottom() - y);
                    if (showPeak)
                    {
                        const float yp = dbToY (powToDb (bandPower (c, lo, hi, true)));
                        g.setColour (col.brighter (0.4f));
                        g.fillRect (x0 + w * (float) c, yp - 1.f, jmax (1.f, w - (nCurves > 1 ? 1.f : 0.f)), jmax (1.5f, s_ * 1.5f));
                    }
                }
            }
        }
        else
        {
            // Peak marker for a single curve: where it is loudest, and how loud.
            float bestDb = -1.0e9f, bestX = 0.f, bestHz = 0.f;

            for (int c = 0; c < nCurves; ++c)
            {
                Path curve, peakPath;
                bool started = false;
                for (int px = (int) plot.getX(); px <= (int) plot.getRight(); ++px)
                {
                    const float h0 = xToHz ((float) px - 0.5f), h1 = xToHz ((float) px + 0.5f);
                    const float db = powToDb (powerAt (c, h0, h1, false));
                    const float y = jmin (plot.getBottom() + 2.f, dbToY (db));
                    const float yp = jmin (plot.getBottom() + 2.f, dbToY (powToDb (powerAt (c, h0, h1, true))));
                    if (! started)
                    {
                        curve.startNewSubPath ((float) px, y);
                        peakPath.startNewSubPath ((float) px, yp);
                        started = true;
                    }
                    else
                    {
                        curve.lineTo ((float) px, y);
                        peakPath.lineTo ((float) px, yp);
                    }
                    if (nCurves == 1 && db > bestDb) { bestDb = db; bestX = (float) px; bestHz = std::sqrt (h0 * h1); }
                }

                const Colour col = curveColour (c);
                if (nCurves == 1 && settings_->fill && started)
                {
                    Path fill (curve);
                    fill.lineTo (plot.getRight(), plot.getBottom() + 2.f);
                    fill.lineTo (plot.getX(), plot.getBottom() + 2.f);
                    fill.closeSubPath();
                    g.setColour (col.withAlpha (0.18f));
                    g.fillPath (fill);
                }
                if (showPeak)
                {
                    g.setColour (col.withAlpha (0.45f));
                    g.strokePath (peakPath, PathStrokeType (jmax (1.f, 0.8f * s_)));
                }
                g.setColour (col.withAlpha (0.95f));
                g.strokePath (curve, PathStrokeType (jmax (1.f, 1.3f * s_)));
            }

            if (nCurves == 1 && bestDb > floorDb && ! hovering_)
            {
                const float y = dbToY (bestDb);
                Path tri;
                tri.addTriangle (bestX - 4.f * s_, y - 9.f * s_, bestX + 4.f * s_, y - 9.f * s_, bestX, y - 3.f * s_);
                g.setColour (Colours::white.withAlpha (0.85f));
                g.fillPath (tri);
                g.setFont (Font (FontOptions (10.f * s_)));
                const String t = formatHz (bestHz) + "  " + String (bestDb, 1) + " dB";
                const int tw = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), t) + S (4);
                const int tx = jlimit ((int) plot.getX() + 2, (int) plot.getRight() - tw - 2, (int) bestX - tw / 2);
                g.drawText (t, tx, jmax ((int) plot.getY() + S (18), (int) (y - 22.f * s_)), tw, S (12), Justification::centred);
            }
        }
    }

    g.setColour (Colours::white.withAlpha (ViewStyle::frame));
    g.drawRect (plot, 1.f);

    // Status, top left: what the analysis is (the toolbar says the rest).
    {
        String st;
        if (fft_.isReady() && sr_ > 0.0)
            st << fft_.getSize() << " pt, " << String (sr_ / (double) fft_.getSize(), 1) << " Hz bins";
        if (bands)
            st << ", 1/3 oct bands";
        g.setFont (Font (FontOptions (11.f * s_)));
        g.setColour (Colours::white.withAlpha (ViewStyle::statusText));
        g.drawText (st, (int) plot.getX() + 6, (int) plot.getY() + 3, S (420), S (14), Justification::centredLeft);

        // Under the legend: the zoom with how to undo it, and PAUSED.
        int row = 1;
        String zoom;
        if (settings_->zoomHi > 0.f)
            zoom << formatHz (zLo) << " - " << formatHz (zHi) << "   ";
        if (settings_->zoomHi > 0.f || settings_->dbRange > 0.f)
            ViewStyle::drawStatusTag (g, zoom + ViewStyle::resetHint(), Colours::white.withAlpha (0.85f), plot, row++, s_);
        if (paused_)
            ViewStyle::drawStatusTag (g, "PAUSED", ViewStyle::pausedText, plot, row, s_);
    }

    // Legend, top right: which colour is which channel.
    if (nCurves > 1 && nCurves <= kMaxChannels)
    {
        g.setFont (Font (FontOptions (11.f * s_)));
        float lx = plot.getRight() - 6.f;
        for (int c = nCurves - 1; c >= 0; --c)
        {
            const String t = String (settings_->firstCh + c + 1);
            const float tw = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), t) + 8.f;
            lx -= tw;
            g.setColour (curveColour (c));
            g.drawText (t, (int) lx, (int) plot.getY() + 3, (int) tw, S (14), Justification::centred);
        }
    }
    else if (nCurves == 1)
    {
        g.setFont (Font (FontOptions (11.f * s_)));
        g.setColour (curveColour (0));
        g.drawText (curveName (0), (int) plot.getRight() - S (160) - 6, (int) plot.getY() + 3, S (160), S (14),
                    Justification::centredRight);
    }

    // Hover: frequency (or band), note, and every curve's level there.
    if (! hovering_ || ! plot.contains (hover_))
        return;

    const float hz = xToHz (hover_.x);
    StringArray lines;
    Array<Colour> colours;
    float lo = xToHz (hover_.x - 0.5f), hi = xToHz (hover_.x + 0.5f);

    if (bands)
    {
        const int bi = jlimit (0, countOf (kBandLabels) - 1, roundToInt (3.f * std::log2 (hz / 1000.f)) - kBandK0);
        const float fc = 1000.f * std::exp2 ((float) (kBandK0 + bi) / 3.f);
        lo = fc * std::exp2 (-1.f / 6.f);
        hi = fc * std::exp2 (1.f / 6.f);
        g.setColour (Colours::white.withAlpha (0.10f));
        g.fillRect (Rectangle<float> (hzToX (lo), plot.getY(), hzToX (hi) - hzToX (lo), plot.getHeight()));
        const float label = kBandLabels[bi];
        lines.add ((label >= 1000.f ? String (label / 1000.f) + " kHz" : String (label) + " Hz") + " band");
    }
    else
    {
        g.setColour (Colours::white.withAlpha (ViewStyle::crosshair));
        g.drawVerticalLine ((int) hover_.x, plot.getY(), plot.getBottom());
        lines.add (formatHz (hz) + "   " + noteName (hz));
    }
    colours.add (Colours::white);

    g.setColour (Colours::white.withAlpha (ViewStyle::crosshair * 0.6f));
    g.drawHorizontalLine ((int) hover_.y, plot.getX(), plot.getRight());
    lines.add ("y  " + String (topDb() - (hover_.y - plot.getY()) / plot.getHeight() * rangeDb(), 1) + " dB");
    colours.add (Colours::white.withAlpha (0.7f));

    const int rows = jmin (nCurves, 8);
    for (int c = 0; c < rows; ++c)
    {
        const float p  = bands ? bandPower (c, lo, hi, false) : powerAt (c, lo, hi, false);
        String l = curveName (c) + "  " + String (powToDb (p), 1) + " dB";
        if (showPeak)
        {
            const float pp = bands ? bandPower (c, lo, hi, true) : powerAt (c, lo, hi, true);
            l << "   (peak " << String (powToDb (pp), 1) << ")";
        }
        lines.add (l);
        colours.add (curveColour (c));
    }
    if (nCurves > rows)
    {
        lines.add (String (nCurves - rows) + " more...");
        colours.add (Colours::white.withAlpha (0.6f));
    }

    ViewStyle::drawReadoutBox (g, lines, colours, hover_, plot, s_);
}

//==============================================================================
void SpectrumComponent::mouseMove (const MouseEvent& e)
{
    hover_ = e.position;
    hovering_ = true;
    switch (regionAt (e.position))
    {
        case Region::levelAxis: setMouseCursor (MouseCursor::UpDownResizeCursor); break;
        case Region::freqAxis:  setMouseCursor (MouseCursor::LeftRightResizeCursor); break;
        default:                setMouseCursor (MouseCursor::NormalCursor); break;
    }
    repaint();
}

void SpectrumComponent::mouseExit (const MouseEvent&)
{
    hovering_ = false;
    repaint();
}

void SpectrumComponent::mouseDown (const MouseEvent& e)
{
    dragged_ = false;
    dragRegion_ = regionAt (e.position);
    dragZoomLo_ = zoomLo();
    dragZoomHi_ = zoomHi();
    dragTop_ = topDb();
    dragRange_ = rangeDb();
}

void SpectrumComponent::mouseDrag (const MouseEvent& e)
{
    hover_ = e.position;
    if (dragRegion_ == Region::none || (! dragged_ && e.getDistanceFromDragStart() < 4))
        return;
    if (! dragged_ && dragRegion_ == Region::plot)
        setMouseCursor (MouseCursor::DraggingHandCursor);
    dragged_ = true;

    const auto p = plotArea();
    const auto d = e.getOffsetFromDragStart().toFloat();
    const auto start = e.mouseDownPosition;

    // Everything from the values at mouse down, so the drag is exact both ways.
    const float a = std::log (dragZoomLo_), b = std::log (dragZoomHi_);
    auto freqPanned = [&] (float dx)
    {
        // Grab-and-move: dragging right brings lower frequencies in.
        const float shift = -dx / p.getWidth() * (b - a);
        setZoom (std::exp (a + shift), std::exp (b + shift));
    };
    auto levelPanned = [&] (float dy)
    {
        // Dragging down brings higher levels in.
        setLevelView (dragTop_ + dy / p.getHeight() * dragRange_, dragRange_);
    };

    switch (dragRegion_)
    {
        case Region::plot:
            freqPanned (d.x);       // a no-op while everything is shown
            levelPanned (d.y);
            break;

        case Region::levelAxis:
        {
            // Along: pan. Across, to the right: zoom in about the grabbed level.
            const float factor = std::exp (-d.x / (120.f * s_));
            const float t = jlimit (0.f, 1.f, (start.y - p.getY()) / p.getHeight());
            const float grabbed = dragTop_ - t * dragRange_ + d.y / p.getHeight() * dragRange_;
            const float range = jlimit (kMinRangeDb, kMaxRangeDb, dragRange_ * factor);
            setLevelView (grabbed + t * range, range);
            break;
        }

        case Region::freqAxis:
        {
            // Along: pan. Across, upwards: zoom in about the grabbed frequency.
            const float factor = std::exp (d.y / (120.f * s_));
            const float t = jlimit (0.f, 1.f, (start.x - p.getX()) / p.getWidth());
            const float grabbed = a + t * (b - a) - d.x / p.getWidth() * (b - a);
            const float span = (b - a) * factor;
            setZoom (std::exp (grabbed - t * span), std::exp (grabbed + (1.f - t) * span));
            break;
        }

        case Region::none:
            break;
    }
}

void SpectrumComponent::mouseUp (const MouseEvent& e)
{
    if (! dragged_ && e.getNumberOfClicks() == 1 && plotArea().contains (e.position))
        clearPeaks();
    if (dragged_)
        mouseMove (e);          // the cursor for wherever it ended
    dragged_ = false;
    dragRegion_ = Region::none;
}

void SpectrumComponent::mouseDoubleClick (const MouseEvent& e)
{
    const auto r = regionAt (e.position);
    if (r == Region::plot || r == Region::freqAxis)
        setZoom (0.f, 0.f);
    if (r == Region::plot || r == Region::levelAxis)
        setLevelView (kTopDb[settings_->topIdx], kRangeDb[settings_->rangeIdx]);
}

void SpectrumComponent::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const auto r = regionAt (e.position);
    if (r == Region::none)
        return;

    // A trackpad scrolls the view, both ways, as if it were a page; pinch zooms.
    // In JUCE's sense: positive deltas move the content right and down.
    if (w.isSmooth && r == Region::plot && ! e.mods.isAltDown() && ! e.mods.isCommandDown())
    {
        panFreq (w.deltaX * kWheelPx * s_);
        setLevelView (topDb() + w.deltaY * kWheelPx * s_ / plotArea().getHeight() * rangeDb(), rangeDb());
        return;
    }

    // A mouse wheel (or a trackpad on an axis) zooms about the pointer: up = in.
    const float d = w.deltaY != 0.f ? w.deltaY : w.deltaX;
    const float factor = std::exp (-d * 1.5f);
    if (r == Region::levelAxis || (r == Region::plot && e.mods.isAltDown()))
        zoomLevelAbout (e.position.y, factor);
    else
        zoomFreqAbout (e.position.x, factor);
}

void SpectrumComponent::mouseMagnify (const MouseEvent& e, float scaleFactor)
{
    const auto r = regionAt (e.position);
    if (r == Region::none || scaleFactor <= 0.f)
        return;

    // Fingers apart (scale > 1) zooms in.
    if (r == Region::levelAxis || (r == Region::plot && e.mods.isAltDown()))
        zoomLevelAbout (e.position.y, 1.f / scaleFactor);
    else
        zoomFreqAbout (e.position.x, 1.f / scaleFactor);
}
