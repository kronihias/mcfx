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

#include "OscilloscopeComponent.h"

namespace
{
    // Index 0 of both is "auto". Whole window, not per division.
    constexpr float kTimeMs[] = { 0.f, 1.f, 2.f, 5.f, 10.f, 20.f, 50.f, 100.f, 200.f, 500.f, 1000.f };
    constexpr int   kNumTimes = (int) (sizeof (kTimeMs) / sizeof (kTimeMs[0]));

    // Half-height of the plot, linear. Labelled with their dBFS too.
    constexpr float kAmp[] = { 0.f, 1.f, 0.5f, 0.25f, 0.1f, 0.05f, 0.01f, 0.001f };
    constexpr int   kNumAmps = (int) (sizeof (kAmp) / sizeof (kAmp[0]));

    // Auto time picks from the short end only: past 200 ms nothing periodic is
    // left to lock on to and the trace is a level envelope anyway.
    constexpr int kAutoTimeMin = 1, kAutoTimeMax = 8, kAutoTimeDefault = 5;   // 1..200 ms, 20 ms

    constexpr int kToolbarH = 22;
    constexpr int kPlotTop  = 28;
    constexpr float kMarginL = 50.f, kMarginR = 6.f, kMarginB = 16.f;

    constexpr int kTimeDivs = 10;
    constexpr int kAmpDivs  = 8;

    const Colour kCursorA (0xffffd54a);
    const Colour kCursorB (0xffff8a3d);

    // Rising crossings of 0 with hysteresis: a sample must first dip below
    // -hyst to re-arm, so noise around a slow crossing fires once, not ten
    // times. Positions are interpolated to a fraction of a sample, which is
    // what keeps a triggered trace from jittering by a sample each frame.
    template <typename Fn>
    void forEachRisingCrossing (const float* x, int len, float hyst, Fn&& fn)
    {
        bool armed = false;
        for (int i = 1; i < len; ++i)
        {
            if (x[i] < -hyst)
                armed = true;
            else if (armed && x[i] >= 0.f && x[i - 1] < 0.f)
            {
                const float d = x[i] - x[i - 1];
                fn ((double) (i - 1) + (d > 0.f ? (double) (-x[i - 1] / d) : 0.0));
                armed = false;
            }
        }
    }

    float peakOf (const float* x, int len)
    {
        if (len <= 0) return 0.f;
        auto r = FloatVectorOperations::findMinAndMax (x, len);
        return jmax (std::abs (r.getStart()), std::abs (r.getEnd()));
    }
}

//==============================================================================
OscilloscopeComponent::OscilloscopeComponent()
{
    setOpaque (false);
    settings_ = &ownSettings_;

    auto setupCombo = [this] (ComboBox& cb, const String& tip)
    {
        addAndMakeVisible (cb);
        cb.setTooltip (tip);
        cb.addListener (this);
    };

    setupCombo (cbFirst_, "First channel shown. Also the trigger source.");
    setupCombo (cbLast_,  "Last channel shown. Set it to the first one to look at a single channel.");

    setupCombo (cbLayout_, "Draw the channels on top of each other, or each in its own lane.");
    cbLayout_.addItem ("overlay", 1);
    cbLayout_.addItem ("stacked", 2);

    setupCombo (cbTime_, "Width of the window. Auto shows a few periods of the first channel. "
                         "Mouse wheel over the plot steps through these.");
    for (int i = 0; i < kNumTimes; ++i)
        cbTime_.addItem (i == 0 ? String ("time auto")
                                : (kTimeMs[i] >= 1000.f ? String (kTimeMs[i] / 1000.f) + " s"
                                                        : String ((int) kTimeMs[i]) + " ms"), i + 1);

    setupCombo (cbAmp_, "Vertical range. Auto follows the loudest shown channel. "
                        "Shift + mouse wheel over the plot steps through these.");
    for (int i = 0; i < kNumAmps; ++i)
        cbAmp_.addItem (i == 0 ? String ("range auto")
                               : String::charToString ((juce_wchar) 0xb1) + String (kAmp[i]) + "  ("
                                   + String (roundToInt (Decibels::gainToDecibels (kAmp[i]))) + " dB)", i + 1);

    addAndMakeVisible (tglTrig_);
    tglTrig_.setButtonText ("trig");
    tglTrig_.setTooltip ("Hold periodic signals still: start the window at a rising zero "
                         "crossing of the first shown channel. Free-runs when there is none.");
    tglTrig_.setColour (ToggleButton::textColourId, Colours::white);
    tglTrig_.addListener (this);

    addAndMakeVisible (btnPause_);
    btnPause_.setButtonText ("pause");
    btnPause_.setClickingTogglesState (true);
    btnPause_.setColour (TextButton::buttonOnColourId, Colour (0xffb03030));
    btnPause_.setTooltip ("Freeze the display to inspect it. While paused, drag the plot to "
                          "scroll back through the last second.");
    btnPause_.addListener (this);

    syncControlsFromSettings();
}

OscilloscopeComponent::~OscilloscopeComponent()
{
    // Never leave the capture frozen behind a closed editor.
    if (capture_ != nullptr && paused_)
        capture_->setFrozen (false);
}

void OscilloscopeComponent::setSettings (ScopeSettings* settings)
{
    settings_ = settings != nullptr ? settings : &ownSettings_;
    clampChannelRange();
    syncControlsFromSettings();
}

void OscilloscopeComponent::setNumChannels (int numChannels)
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
    }
    clampChannelRange();
    syncControlsFromSettings();
    needsUpdate_ = true;
}

void OscilloscopeComponent::clampChannelRange()
{
    settings_->firstCh = jlimit (0, numCh_ - 1, settings_->firstCh);
    settings_->lastCh  = jlimit (settings_->firstCh, numCh_ - 1, settings_->lastCh);
    settings_->timeIdx = jlimit (0, kNumTimes - 1, settings_->timeIdx);
    settings_->ampIdx  = jlimit (0, kNumAmps - 1, settings_->ampIdx);
}

void OscilloscopeComponent::syncControlsFromSettings()
{
    cbFirst_.setSelectedId (settings_->firstCh + 1, dontSendNotification);
    cbLast_.setSelectedId (settings_->lastCh + 1, dontSendNotification);
    cbLayout_.setSelectedId (settings_->stacked ? 2 : 1, dontSendNotification);
    cbTime_.setSelectedId (settings_->timeIdx + 1, dontSendNotification);
    cbAmp_.setSelectedId (settings_->ampIdx + 1, dontSendNotification);
    tglTrig_.setToggleState (settings_->trigger, dontSendNotification);
}

int OscilloscopeComponent::numShown() const
{
    return settings_->lastCh - settings_->firstCh + 1;
}

int OscilloscopeComponent::windowSamples (double sr) const
{
    const int idx = settings_->timeIdx == 0 ? autoTimeStep_ : settings_->timeIdx;
    const int n = roundToInt (kTimeMs[idx] * 0.001 * sr);
    const int cap = capture_ != nullptr ? capture_->getCapacity() : 0;
    return jlimit (16, jmax (16, cap - 8), n);
}

float OscilloscopeComponent::currentAmp() const
{
    return settings_->ampIdx == 0 ? ampAuto_ : kAmp[settings_->ampIdx];
}

//==============================================================================
void OscilloscopeComponent::tick()
{
    if (capture_ == nullptr || ! capture_->isReady() || ! isShowing())
        return;

    if (paused_ && ! needsUpdate_)
        return;

    sr_ = capture_->getSampleRate();
    clampChannelRange();

    if (paused_) updatePaused (sr_);
    else         updateLive (sr_);

    // Peak of what is actually on screen, for auto range and nothing else.
    shownPeak_ = 0.f;
    const int i0 = jlimit (0, work_.getNumSamples(), (int) std::floor (viewStart_));
    const int i1 = jlimit (0, work_.getNumSamples(), (int) std::ceil (viewStart_ + viewLen_) + 1);
    for (int c = 0; c < work_.getNumChannels(); ++c)
        shownPeak_ = jmax (shownPeak_, peakOf (work_.getReadPointer (c) + i0, i1 - i0));

    updateAutoAmp();

    needsUpdate_ = false;
    repaint();
}

void OscilloscopeComponent::updateLive (double sr)
{
    const int cap = capture_->getCapacity();
    const int W   = windowSamples (sr);
    const int pre = settings_->trigger ? W / 10 : 0;

    // How far back to look for a trigger: a window's worth, but at least 50 ms
    // so a short window still finds a low note's crossing.
    const int search   = settings_->trigger ? jmax (W, (int) (0.05 * sr)) : 0;
    const int autoSpan = settings_->timeIdx == 0 ? (int) (0.2 * sr) : 0;
    const int L = jmin (cap, W + 2 + jmax (search, autoSpan));

    const int64 written = capture_->read (work_, settings_->firstCh, numShown(), L, 0);
    workBase_ = written - L;

    const float* ref = work_.getReadPointer (0);

    if (settings_->timeIdx == 0)
    {
        const int span = jmin (L, autoSpan);
        updateAutoTime (ref + (L - span), span, sr);
    }

    triggered_ = false;
    if (settings_->trigger)
    {
        const float hyst = jmax (1.0e-5f, 0.05f * peakOf (ref, L));
        double best = -1.0;
        forEachRisingCrossing (ref, L, hyst, [&] (double pos)
        {
            if (pos - pre >= 0.0 && pos - pre + W <= (double) (L - 1))
                best = pos;          // the latest one that still fits wins
        });

        if (best >= 0.0)
        {
            triggered_ = true;
            viewStart_ = best - pre;
            zeroIdx_   = best;
        }
    }

    if (! triggered_)
    {
        viewStart_ = (double) (L - 1 - W);
        zeroIdx_   = (double) (L - 1);
    }
    viewLen_ = W;
}

void OscilloscopeComponent::updatePaused (double sr)
{
    const int W = windowSamples (sr);
    const int64 written = capture_->getNumWritten();
    const int valid = capture_->getNumValid();

    // Keep the window inside what the frozen ring actually holds.
    const double lo = (double) (written - valid + W + 2);
    const double hi = (double) (written - 1);
    pauseEndAbs_ = lo <= hi ? jlimit (lo, hi, pauseEndAbs_) : hi;

    const int64 lastAbs = jmin (written - 1, (int64) std::floor (pauseEndAbs_) + 1);
    const int L = W + 4;
    const int endBack = (int) jmax ((int64) 0, written - 1 - lastAbs);

    capture_->read (work_, settings_->firstCh, numShown(), L, endBack);
    workBase_ = lastAbs - (L - 1);

    viewLen_   = W;
    viewStart_ = pauseEndAbs_ - W - (double) workBase_;
    zeroIdx_   = pauseZeroAbs_ - (double) workBase_;
}

void OscilloscopeComponent::updateAutoTime (const float* ref, int len, double sr)
{
    const float peak = peakOf (ref, len);
    if (peak < 1.0e-4f)
        return;                               // silence: keep whatever we had

    int count = 0;
    double first = 0.0, last = 0.0;
    forEachRisingCrossing (ref, len, 0.1f * peak, [&] (double pos)
    {
        if (count++ == 0) first = pos;
        last = pos;
    });

    int candidate = kAutoTimeDefault;
    if (count >= 3)
    {
        // About four periods on screen.
        const double periodMs = (last - first) / (double) (count - 1) / sr * 1000.0;
        candidate = kAutoTimeMax;
        for (int i = kAutoTimeMin; i <= kAutoTimeMax; ++i)
            if (kTimeMs[i] >= 4.0 * periodMs) { candidate = i; break; }
    }

    // Only follow a change that has lasted ~300 ms, so a note boundary or a
    // noisy estimate does not make the window flicker between two steps.
    if (candidate == autoTimeStep_)
        autoTimeTicks_ = 0;
    else if (candidate == autoTimeCandidate_)
    {
        if (++autoTimeTicks_ >= 8)
        {
            autoTimeStep_ = candidate;
            autoTimeTicks_ = 0;
        }
    }
    else
    {
        autoTimeCandidate_ = candidate;
        autoTimeTicks_ = 1;
    }
}

void OscilloscopeComponent::updateAutoAmp()
{
    if (shownPeak_ < 1.0e-6f)
        return;

    // Smallest step that leaves a little headroom over the peak. Finer than
    // 1-2-5: with that, a -6 dBFS sine landed in a +-1 box at half height.
    float target = 10.f;
    for (float decade = 1.0e-4f; decade < 10.f; decade *= 10.f)
    {
        bool found = false;
        for (float m : { 1.f, 1.5f, 2.f, 3.f, 5.f, 7.5f })
            if (decade * m >= shownPeak_ * 1.1f) { target = decade * m; found = true; break; }
        if (found) break;
    }

    // Grow at once so nothing clips; shrink only after a second of quiet.
    if (target >= ampAuto_)
    {
        ampAuto_ = target;
        ampShrinkTicks_ = 0;
    }
    else if (++ampShrinkTicks_ >= 25)
    {
        ampAuto_ = target;
        ampShrinkTicks_ = 0;
    }
}

//==============================================================================
void OscilloscopeComponent::setPaused (bool shouldBePaused)
{
    if (shouldBePaused == paused_)
        return;

    if (shouldBePaused && capture_ != nullptr && capture_->isReady())
    {
        // Freeze first, then take the frame we will keep, so it comes from the
        // ring as frozen. It is pinned in absolute samples, so a block that was
        // already on its way in when the freeze landed shifts nothing.
        capture_->setFrozen (true);
        sr_ = capture_->getSampleRate();
        updateLive (sr_);
        pauseEndAbs_  = (double) workBase_ + viewStart_ + viewLen_;
        pauseZeroAbs_ = (double) workBase_ + zeroIdx_;
        paused_ = true;
    }
    else
    {
        if (capture_ != nullptr)
            capture_->setFrozen (false);
        paused_ = false;
        dragging_ = false;
    }

    btnPause_.setToggleState (paused_, dontSendNotification);
    btnPause_.setButtonText (paused_ ? "run" : "pause");
    needsUpdate_ = true;
    tick();
    repaint();
}

void OscilloscopeComponent::visibilityChanged()
{
    // Leaving the view resumes it: a frozen capture with nothing showing it
    // would just look broken the next time the scope is opened.
    if (! isVisible())
        setPaused (false);
}

//==============================================================================
void OscilloscopeComponent::comboBoxChanged (ComboBox* cb)
{
    if (cb == &cbFirst_)
    {
        settings_->firstCh = cbFirst_.getSelectedId() - 1;
        if (settings_->lastCh < settings_->firstCh)
            settings_->lastCh = settings_->firstCh;
    }
    else if (cb == &cbLast_)
    {
        settings_->lastCh = cbLast_.getSelectedId() - 1;
        if (settings_->firstCh > settings_->lastCh)
            settings_->firstCh = settings_->lastCh;
    }
    else if (cb == &cbLayout_)
    {
        settings_->stacked = cbLayout_.getSelectedId() == 2;
    }
    else if (cb == &cbTime_)
    {
        const int oldW = windowSamples (sr_);
        settings_->timeIdx = jlimit (0, kNumTimes - 1, cbTime_.getSelectedId() - 1);
        if (settings_->timeIdx == 0)
            autoTimeTicks_ = 0;

        // Paused, zoom about the middle of what is on screen rather than the
        // right edge — that is where whatever you were looking at usually is.
        if (paused_)
            pauseEndAbs_ += 0.5 * (double) (windowSamples (sr_) - oldW);
    }
    else if (cb == &cbAmp_)
    {
        settings_->ampIdx = jlimit (0, kNumAmps - 1, cbAmp_.getSelectedId() - 1);
    }

    clampChannelRange();
    syncControlsFromSettings();
    needsUpdate_ = true;
    tick();
}

void OscilloscopeComponent::buttonClicked (Button* b)
{
    if (b == &tglTrig_)
    {
        settings_->trigger = tglTrig_.getToggleState();
        needsUpdate_ = true;
    }
    else if (b == &btnPause_)
    {
        setPaused (btnPause_.getToggleState());
    }
}

//==============================================================================
void OscilloscopeComponent::resized()
{
    int x = 0;
    auto place = [&] (Component& c, int w, int gap = 4)
    {
        c.setBounds (x, 0, w, kToolbarH);
        x += w + gap;
    };

    place (cbFirst_, 66);
    place (cbLast_, 66, 10);
    place (cbLayout_, 80, 10);
    place (cbTime_, 86);
    place (cbAmp_, 118, 8);
    place (tglTrig_, 52, 6);
    place (btnPause_, 58);
}

Rectangle<float> OscilloscopeComponent::plotArea() const
{
    return Rectangle<float> (kMarginL, (float) kPlotTop,
                             jmax (10.f, (float) getWidth() - kMarginL - kMarginR),
                             jmax (10.f, (float) getHeight() - (float) kPlotTop - kMarginB));
}

Rectangle<float> OscilloscopeComponent::laneArea (int lane) const
{
    const auto p = plotArea();
    if (! settings_->stacked)
        return p;

    const float h = p.getHeight() / (float) jmax (1, numShown());
    return { p.getX(), p.getY() + h * (float) lane, p.getWidth(), h };
}

int OscilloscopeComponent::laneAt (float y) const
{
    const auto p = plotArea();
    if (! settings_->stacked)
        return -1;
    const float h = p.getHeight() / (float) jmax (1, numShown());
    return jlimit (0, numShown() - 1, (int) ((y - p.getY()) / h));
}

double OscilloscopeComponent::xToIdx (float x) const
{
    const auto p = plotArea();
    return viewStart_ + (double) ((x - p.getX()) / p.getWidth()) * (double) viewLen_;
}

float OscilloscopeComponent::idxToX (double idx) const
{
    const auto p = plotArea();
    return p.getX() + (float) ((idx - viewStart_) / (double) viewLen_) * p.getWidth();
}

float OscilloscopeComponent::valueAt (int ch, double idx) const
{
    const int n = work_.getNumSamples();
    if (! isPositiveAndBelow (ch, work_.getNumChannels()) || n <= 0)
        return 0.f;

    const float* x = work_.getReadPointer (ch);
    const double c = jlimit (0.0, (double) (n - 1), idx);
    const int i = (int) c;
    const float f = (float) (c - i);
    return i + 1 < n ? x[i] + f * (x[i + 1] - x[i]) : x[i];
}

Colour OscilloscopeComponent::channelColour (int ch)
{
    // Golden-ratio hue steps: neighbours always differ, and channel 1 starts on
    // the aquamarine the rest of the plug-in uses.
    const float hue = std::fmod (0.45f + 0.618034f * (float) ch, 1.f);
    return Colour::fromHSV (hue, 0.6f, 1.f, 1.f);
}

String OscilloscopeComponent::formatTime (double s)
{
    const double ms = s * 1000.0;
    const double a = std::abs (ms);
    if (a >= 100.0) return String (ms, 1) + " ms";
    if (a >= 10.0)  return String (ms, 2) + " ms";
    return String (ms, 3) + " ms";
}

String OscilloscopeComponent::formatLevel (float v)
{
    String s = (v >= 0.f ? "+" : "") + String (v, 4);
    const float a = std::abs (v);
    s << "  (" << (a > 0.f ? String (Decibels::gainToDecibels (a, -200.f), 1) : String ("-inf")) << " dBFS)";
    return s;
}

//==============================================================================
void OscilloscopeComponent::drawTrace (Graphics& g, int ch, Rectangle<float> lane, Colour c) const
{
    const int n = work_.getNumSamples();
    if (n < 2 || viewLen_ <= 0)
        return;

    const float* x = work_.getReadPointer (ch);
    const float amp = jmax (1.0e-9f, currentAmp());
    const float cy = lane.getCentreY();
    const float sy = 0.5f * lane.getHeight() / amp;
    const float w  = lane.getWidth();
    const double spp = (double) viewLen_ / (double) w;   // samples per pixel

    g.setColour (c.withAlpha (0.9f));

    if (spp > 1.0)
    {
        // Dense: one vertical min..max bar per pixel column. Each column's range
        // overlaps the next by a sample so the trace stays connected.
        RectangleList<float> bars;
        bars.ensureStorageAllocated ((int) w + 1);

        for (int px = 0; px < (int) w; ++px)
        {
            const double a = viewStart_ + px * spp;
            const int i0 = jlimit (0, n - 1, (int) std::floor (a));
            const int i1 = jlimit (0, n - 1, (int) std::ceil (a + spp));
            auto r = FloatVectorOperations::findMinAndMax (x + i0, i1 - i0 + 1);
            const float yTop = cy - r.getEnd() * sy;
            const float yBot = cy - r.getStart() * sy;
            bars.addWithoutMerging ({ lane.getX() + (float) px, yTop, 1.f, jmax (1.f, yBot - yTop) });
        }
        g.fillRectList (bars);
    }
    else
    {
        // Sparse: straight lines between the samples, and the samples
        // themselves once they are far enough apart to tell apart.
        const int i0 = jlimit (0, n - 1, (int) std::floor (viewStart_) - 1);
        const int i1 = jlimit (0, n - 1, (int) std::ceil (viewStart_ + viewLen_) + 1);

        Path p;
        p.preallocateSpace ((i1 - i0 + 1) * 3);
        for (int i = i0; i <= i1; ++i)
        {
            const Point<float> pt (idxToX (i), cy - x[i] * sy);
            if (i == i0) p.startNewSubPath (pt);
            else         p.lineTo (pt);
        }
        g.strokePath (p, PathStrokeType (1.3f));

        if (spp < 1.0 / 6.0)
            for (int i = i0; i <= i1; ++i)
                g.fillEllipse (idxToX (i) - 2.f, cy - x[i] * sy - 2.f, 4.f, 4.f);
    }
}

void OscilloscopeComponent::paint (Graphics& g)
{
    const auto plot = plotArea();
    const int  shown = numShown();
    const bool stacked = settings_->stacked;
    const float amp = currentAmp();

    g.setColour (Colour (0xe0101010));
    g.fillRect (plot);

    // Grid.
    g.setColour (Colours::white.withAlpha (0.08f));
    for (int i = 1; i < kTimeDivs; ++i)
    {
        const float gx = plot.getX() + plot.getWidth() * (float) i / (float) kTimeDivs;
        g.drawVerticalLine ((int) gx, plot.getY(), plot.getBottom());
    }

    if (! stacked)
    {
        for (int i = 1; i < kAmpDivs; ++i)
        {
            const float gy = plot.getY() + plot.getHeight() * (float) i / (float) kAmpDivs;
            g.setColour (Colours::white.withAlpha (i == kAmpDivs / 2 ? 0.22f : 0.08f));
            g.drawHorizontalLine ((int) gy, plot.getX(), plot.getRight());
        }
    }
    else
    {
        for (int c = 0; c < shown; ++c)
        {
            const auto lane = laneArea (c);
            g.setColour (Colours::white.withAlpha (0.18f));
            g.drawHorizontalLine ((int) lane.getCentreY(), plot.getX(), plot.getRight());
            if (c > 0)
            {
                g.setColour (Colours::white.withAlpha (0.10f));
                g.drawHorizontalLine ((int) lane.getY(), plot.getX(), plot.getRight());
            }
        }
    }

    // Traces.
    for (int c = 0; c < jmin (shown, work_.getNumChannels()); ++c)
    {
        const auto lane = laneArea (c);
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (lane.toNearestInt());
        drawTrace (g, c, lane, channelColour (settings_->firstCh + c));
    }

    g.setColour (Colours::white.withAlpha (0.3f));
    g.drawRect (plot, 1.f);

    // Trigger point.
    if (triggered_ || paused_)
    {
        const float tx = idxToX (zeroIdx_);
        if (tx >= plot.getX() && tx <= plot.getRight())
        {
            Path tri;
            tri.addTriangle (tx - 5.f, plot.getY(), tx + 5.f, plot.getY(), tx, plot.getY() + 7.f);
            g.setColour (Colours::white.withAlpha (triggered_ ? 0.7f : 0.35f));
            g.fillPath (tri);
        }
    }

    // Amplitude labels.
    g.setFont (Font (FontOptions (10.f)));
    g.setColour (Colours::white.withAlpha (0.75f));
    if (! stacked)
    {
        for (int i = 0; i <= kAmpDivs; i += 2)
        {
            const float v = amp * (1.f - 2.f * (float) i / (float) kAmpDivs);
            const float gy = plot.getY() + plot.getHeight() * (float) i / (float) kAmpDivs;
            g.drawText ((v > 0.f ? "+" : "") + String (v, v != 0.f && std::abs (v) < 0.01f ? 4 : (std::abs (v) < 0.1f ? 3 : 2)),
                        0, (int) gy - 6, (int) kMarginL - 4, 12, Justification::centredRight);
        }
    }
    else
    {
        for (int c = 0; c < shown; ++c)
        {
            const auto lane = laneArea (c);
            if (lane.getHeight() < 9.f) break;
            g.setColour (channelColour (settings_->firstCh + c));
            g.drawText ("ch " + String (settings_->firstCh + c + 1), 0, (int) lane.getCentreY() - 6,
                        (int) kMarginL - 4, 12, Justification::centredRight);
        }
    }

    // Time labels, relative to the trigger point (or to "now" when free-running).
    g.setColour (Colours::white.withAlpha (0.75f));
    for (int i = 0; i <= kTimeDivs; i += 2)
    {
        const float gx = plot.getX() + plot.getWidth() * (float) i / (float) kTimeDivs;
        const Justification j = i == 0 ? Justification::centredLeft
                              : (i == kTimeDivs ? Justification::centredRight : Justification::centred);
        const int lx = i == 0 ? (int) gx : (i == kTimeDivs ? (int) gx - 80 : (int) gx - 40);
        g.drawText (formatTime (idxToTime (xToIdx (gx))), lx, (int) plot.getBottom() + 2, 80, 12, j);
    }

    // Status line, top left inside the plot.
    {
        const int idx = settings_->timeIdx == 0 ? autoTimeStep_ : settings_->timeIdx;
        String s;
        s << (kTimeMs[idx] >= 1000.f ? String (kTimeMs[idx] / 1000.f) + " s" : String ((int) kTimeMs[idx]) + " ms")
          << (settings_->timeIdx == 0 ? " (auto)" : "")
          << "   " << String::charToString ((juce_wchar) 0xb1) << String (amp, amp < 0.01f ? 4 : 3)
          << (settings_->ampIdx == 0 ? " (auto)" : "");
        if (settings_->trigger)
            s << "   " << (triggered_ ? "trig'd" : "free-run");

        g.setFont (Font (FontOptions (11.f)));
        g.setColour (Colours::white.withAlpha (0.6f));
        g.drawText (s, (int) plot.getX() + 6, (int) plot.getY() + 3, 360, 14, Justification::centredLeft);

        if (paused_)
        {
            g.setColour (Colour (0xffff6060));
            g.drawText ("PAUSED  -  drag to scroll", (int) plot.getX() + 6, (int) plot.getY() + 17, 240, 14,
                        Justification::centredLeft);
        }
    }

    // Overlay legend, top right.
    if (! stacked && shown > 1 && shown <= 16)
    {
        g.setFont (Font (FontOptions (11.f)));
        float lx = plot.getRight() - 6.f;
        for (int c = shown - 1; c >= 0; --c)
        {
            const String t = String (settings_->firstCh + c + 1);
            const float tw = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), t) + 8.f;
            lx -= tw;
            g.setColour (channelColour (settings_->firstCh + c));
            g.drawText (t, (int) lx, (int) plot.getY() + 3, (int) tw, 14, Justification::centred);
        }
    }

    // Cursors A / B.
    auto drawCursor = [&] (double t, Colour c, const String& name)
    {
        const float cx = idxToX (timeToIdx (t));
        if (cx < plot.getX() || cx > plot.getRight())
            return;
        g.setColour (c.withAlpha (0.85f));
        const float dashes[] = { 4.f, 3.f };
        g.drawDashedLine ({ cx, plot.getY(), cx, plot.getBottom() }, dashes, 2, 1.f);
        g.setFont (Font (FontOptions (11.f, Font::bold)));
        g.drawText (name, (int) cx + 3, (int) plot.getBottom() - 15, 14, 13, Justification::centredLeft);
    };
    if (hasA_) drawCursor (cursorA_, kCursorA, "A");
    if (hasB_) drawCursor (cursorB_, kCursorB, "B");

    if (hasA_)
    {
        String s;
        s << "A " << formatTime (cursorA_);
        if (hasB_)
        {
            const double dt = std::abs (cursorB_ - cursorA_);
            s << "    B " << formatTime (cursorB_) << "    " << String::charToString ((juce_wchar) 0x394) << "t " << formatTime (dt);
            if (dt > 0.0)
                s << "  =  " << String (1.0 / dt, 1) << " Hz";
        }
        else
        {
            s << "    (click again for B)";
        }

        g.setFont (Font (FontOptions (11.f)));
        const int tw = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), s) + 12;
        Rectangle<int> box ((int) plot.getRight() - tw - 4, (int) plot.getBottom() - 20, tw, 16);
        g.setColour (Colours::black.withAlpha (0.7f));
        g.fillRect (box);
        g.setColour (kCursorA);
        g.drawText (s, box, Justification::centred);
    }

    // Hover crosshair and readout.
    if (hovering_ && plot.contains (hover_))
    {
        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawVerticalLine ((int) hover_.x, plot.getY(), plot.getBottom());
        g.drawHorizontalLine ((int) hover_.y, plot.getX(), plot.getRight());

        const double idx = xToIdx (hover_.x);
        const int lane = laneAt (hover_.y);
        const auto laneR = laneArea (jmax (0, lane));
        const float yVal = (laneR.getCentreY() - hover_.y) / (0.5f * laneR.getHeight()) * amp;

        StringArray lines;
        lines.add ("t  " + formatTime (idxToTime (idx)));
        lines.add ("y  " + formatLevel (yVal));

        Array<int> chans;
        if (lane >= 0) chans.add (lane);
        else for (int c = 0; c < jmin (shown, 8); ++c) chans.add (c);

        for (int c : chans)
            lines.add ("ch " + String (settings_->firstCh + c + 1) + "  " + formatLevel (valueAt (c, idx)));
        if (lane < 0 && shown > 8)
            lines.add (String (shown - 8) + " more...");

        g.setFont (Font (FontOptions (11.f)));
        int bw = 0;
        for (auto& l : lines)
            bw = jmax (bw, GlyphArrangement::getStringWidthInt (g.getCurrentFont(), l));
        bw += 12;
        const int lh = 14;
        const int bh = lh * lines.size() + 6;

        int bx = (int) hover_.x + 12;
        int by = (int) hover_.y + 12;
        if (bx + bw > (int) plot.getRight()) bx = (int) hover_.x - 12 - bw;
        if (by + bh > (int) plot.getBottom()) by = (int) hover_.y - 12 - bh;
        bx = jmax ((int) plot.getX() + 2, bx);
        by = jmax ((int) plot.getY() + 2, by);

        g.setColour (Colours::black.withAlpha (0.75f));
        g.fillRoundedRectangle ((float) bx, (float) by, (float) bw, (float) bh, 3.f);

        for (int i = 0; i < lines.size(); ++i)
        {
            Colour c = Colours::white;
            if (i >= 2 && i - 2 < chans.size())
                c = channelColour (settings_->firstCh + chans[i - 2]);
            g.setColour (c);
            g.drawText (lines[i], bx + 6, by + 3 + i * lh, bw - 8, lh, Justification::centredLeft);
        }
    }
}

//==============================================================================
void OscilloscopeComponent::mouseMove (const MouseEvent& e)
{
    hover_ = e.position;
    hovering_ = true;
    repaint();
}

void OscilloscopeComponent::mouseExit (const MouseEvent&)
{
    hovering_ = false;
    repaint();
}

void OscilloscopeComponent::mouseDown (const MouseEvent&)
{
    dragging_ = false;
    dragStartEndAbs_ = pauseEndAbs_;
}

void OscilloscopeComponent::mouseDrag (const MouseEvent& e)
{
    hover_ = e.position;

    if (! paused_ || e.getDistanceFromDragStart() < 4)
        return;

    dragging_ = true;
    const double samplesPerPx = (double) viewLen_ / (double) plotArea().getWidth();
    pauseEndAbs_ = dragStartEndAbs_ - (double) e.getDistanceFromDragStartX() * samplesPerPx;
    needsUpdate_ = true;
    tick();
}

void OscilloscopeComponent::mouseUp (const MouseEvent& e)
{
    if (dragging_)
    {
        dragging_ = false;
        return;
    }

    if (e.getNumberOfClicks() > 1 || ! plotArea().contains (e.position))
        return;

    const double t = idxToTime (xToIdx (e.position.x));
    if (! hasA_ || hasB_)
    {
        hasA_ = true;
        hasB_ = false;
        cursorA_ = t;
    }
    else
    {
        hasB_ = true;
        cursorB_ = t;
    }
    repaint();
}

void OscilloscopeComponent::mouseDoubleClick (const MouseEvent&)
{
    hasA_ = hasB_ = false;
    repaint();
}

void OscilloscopeComponent::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    // Shift + wheel arrives as horizontal scroll on macOS.
    const float d = w.deltaY != 0.f ? w.deltaY : w.deltaX;
    wheelAccum_ += d;     // same sense as EqGraph: up = zoom in
    if (std::abs (wheelAccum_) < 0.12f)
        return;

    const int dir = wheelAccum_ > 0.f ? -1 : 1;     // up = zoom in
    wheelAccum_ = 0.f;

    if (e.mods.isShiftDown())
    {
        int cur = settings_->ampIdx;
        if (cur == 0)
        {
            // From auto, start at the fixed step nearest what auto shows now.
            cur = 1;
            for (int i = 1; i < kNumAmps; ++i)
                if (std::abs (std::log (kAmp[i] / ampAuto_)) < std::abs (std::log (kAmp[cur] / ampAuto_)))
                    cur = i;
        }
        cbAmp_.setSelectedId (jlimit (1, kNumAmps - 1, cur - dir) + 1);   // smaller range = zoom in
    }
    else
    {
        const int cur = settings_->timeIdx == 0 ? autoTimeStep_ : settings_->timeIdx;
        cbTime_.setSelectedId (jlimit (1, kNumTimes - 1, cur + dir) + 1);
    }
}
