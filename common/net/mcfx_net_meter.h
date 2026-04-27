/*
 ==============================================================================

 mcfx_net_meter.h — multichannel level meter for mcfx_send / mcfx_receive.

 Header-only port of sonolink/Source/MeterDsp.h + MultiChannelMeter.h
 (which are themselves a port of mcfx_meter's MyMeterDsp). Renamed to
 mcfx::net namespace; behaviour identical:

  - MeterDsp:  audio-thread peak/RMS/peak-hold tracker per channel.
               Time constants: 1 s peak hold, 15 dB/s peak fall,
               6 dB/s RMS fall.
  - MeterBank: vector of MeterDsp; measureBlock() called from processBlock.
  - MultiChannelMeter: paint widget. IEC-style nonlinear scale (mcfx_meter
                       convention), green/yellow/red gradient fill, peak
                       line, monotonic peak-hold marker. Click to reset
                       holds.

 GPL v2+ (matches mcfx).

 ==============================================================================
*/

#pragma once

#include "JuceHeader.h"

#include <atomic>
#include <cmath>
#include <vector>

namespace mcfx { namespace net {

class MeterDsp
{
public:
    MeterDsp() { recompute(); }

    MeterDsp (const MeterDsp& other) noexcept { copyFrom (other); }
    MeterDsp& operator= (const MeterDsp& other) noexcept { copyFrom (other); return *this; }
    MeterDsp (MeterDsp&& other) noexcept { copyFrom (other); }
    MeterDsp& operator= (MeterDsp&& other) noexcept { copyFrom (other); return *this; }

    void setAudioParams (double sampleRate, int blockSize) noexcept
    {
        sr = sampleRate;
        bs = blockSize;
        recompute();
    }

    void calc (const float* data, int numSamples) noexcept
    {
        if (data == nullptr || numSamples <= 0) return;

        float instPeak = 0.0f;
        float sumSq    = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const float a = std::fabs (data[i]);
            if (a > instPeak) instPeak = a;
            sumSq += data[i] * data[i];
        }
        const float instRms = std::sqrt (sumSq / static_cast<float> (numSamples));

        float p = peak.load (std::memory_order_relaxed);
        if (instPeak > p)
        {
            p = instPeak;
            holdSampleCount = 0;
        }
        else if (holdSampleCount >= holdSamples)
        {
            if (p > 1.0e-5f) p *= fallPeakFactor;
            else             p = 0.0f;
        }
        else
        {
            holdSampleCount += numSamples;
        }
        peak.store (p, std::memory_order_relaxed);

        float ph = peakHold.load (std::memory_order_relaxed);
        if (p > ph) peakHold.store (p, std::memory_order_relaxed);

        float r = rms.load (std::memory_order_relaxed);
        if (instRms > r) r = instRms;
        else if (r > 1.0e-5f) r *= fallRmsFactor;
        else r = 0.0f;
        rms.store (r, std::memory_order_relaxed);
    }

    void reset() noexcept
    {
        rms     .store (0.0f, std::memory_order_relaxed);
        peak    .store (0.0f, std::memory_order_relaxed);
        peakHold.store (0.0f, std::memory_order_relaxed);
        holdSampleCount = 0;
    }

    void clearPeakHold() noexcept
    {
        peakHold.store (peak.load (std::memory_order_relaxed),
                        std::memory_order_relaxed);
    }

    float getRMS()      const noexcept { return rms     .load (std::memory_order_relaxed); }
    float getPeak()     const noexcept { return peak    .load (std::memory_order_relaxed); }
    float getPeakHold() const noexcept { return peakHold.load (std::memory_order_relaxed); }

private:
    void copyFrom (const MeterDsp& o) noexcept
    {
        rms     .store (o.rms     .load (std::memory_order_relaxed), std::memory_order_relaxed);
        peak    .store (o.peak    .load (std::memory_order_relaxed), std::memory_order_relaxed);
        peakHold.store (o.peakHold.load (std::memory_order_relaxed), std::memory_order_relaxed);
        holdSampleCount = o.holdSampleCount;
        holdSamples     = o.holdSamples;
        fallPeakFactor  = o.fallPeakFactor;
        fallRmsFactor   = o.fallRmsFactor;
        sr              = o.sr;
        bs              = o.bs;
    }

    void recompute() noexcept
    {
        if (sr > 0.0 && bs > 0)
        {
            const float blockSec = static_cast<float> (bs) / static_cast<float> (sr);
            fallPeakFactor = std::pow (10.0f, -0.05f * 15.0f * blockSec);
            fallRmsFactor  = std::pow (10.0f, -0.05f *  6.0f * blockSec);
            holdSamples    = static_cast<int> (sr * 1.0); // 1 s hold
        }
        else
        {
            fallPeakFactor = 1.0f;
            fallRmsFactor  = 1.0f;
            holdSamples    = 48000;
        }
    }

    std::atomic<float> rms      { 0.0f };
    std::atomic<float> peak     { 0.0f };
    std::atomic<float> peakHold { 0.0f };

    int   holdSampleCount = 0;
    int   holdSamples     = 48000;
    float fallPeakFactor  = 1.0f;
    float fallRmsFactor   = 1.0f;

    double sr = 48000.0;
    int    bs = 128;
};

class MeterBank
{
public:
    void resize (int numChannels)
    {
        const int n = juce::jmax (0, numChannels);
        const auto prevSize = meters.size();
        meters.resize (static_cast<size_t> (n));
        for (size_t i = prevSize; i < meters.size(); ++i)
            meters[i].setAudioParams (sr_, bs_);
    }

    void setAudioParams (double sampleRate, int blockSize)
    {
        sr_ = sampleRate;
        bs_ = blockSize;
        for (auto& m : meters) m.setAudioParams (sampleRate, blockSize);
    }

    int  getNumChannels() const noexcept { return static_cast<int> (meters.size()); }

    void measureBlock (const juce::AudioBuffer<float>& buffer) noexcept
    {
        const int n   = buffer.getNumSamples();
        const int chs = juce::jmin (static_cast<int> (meters.size()), buffer.getNumChannels());
        for (int ch = 0; ch < chs; ++ch)
            meters[static_cast<size_t> (ch)].calc (buffer.getReadPointer (ch), n);
    }

    void clearAllPeakHolds() noexcept
    {
        for (auto& m : meters) m.clearPeakHold();
    }

    float getRMSLevel      (int ch) const noexcept { return inRange (ch) ? meters[(size_t) ch].getRMS()      : 0.0f; }
    float getPeakLevel     (int ch) const noexcept { return inRange (ch) ? meters[(size_t) ch].getPeak()     : 0.0f; }
    float getPeakHoldLevel (int ch) const noexcept { return inRange (ch) ? meters[(size_t) ch].getPeakHold() : 0.0f; }

private:
    bool inRange (int ch) const noexcept { return ch >= 0 && ch < static_cast<int> (meters.size()); }

    std::vector<MeterDsp> meters;
    double sr_ = 48000.0;
    int    bs_ = 128;
};

// --- View ---------------------------------------------------------------

class MultiChannelMeter : public juce::Component,
                          private juce::Timer
{
public:
    MultiChannelMeter()
    {
        setBufferedToImage (false);
        startTimerHz (30);
    }

    ~MultiChannelMeter() override { stopTimer(); }

    void setMeterBank (MeterBank* b) noexcept { bank = b; }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float scaleW = 28.0f;
        const auto barArea = bounds.withTrimmedLeft (scaleW).reduced (2.0f, 2.0f);

        g.fillAll (juce::Colour (0xff121316));

        drawScale (g, juce::Rectangle<float> (0.0f, barArea.getY(), scaleW, barArea.getHeight()));

        if (bank == nullptr) return;
        const int channels = juce::jmax (1, bank->getNumChannels());
        if (channels <= 0 || barArea.getWidth() < 4 || barArea.getHeight() < 4) return;

        const float gap   = juce::jmin (2.0f, barArea.getWidth() / (channels * 4.0f));
        const float barW  = (barArea.getWidth() - gap * (channels - 1)) / channels;
        const float top   = barArea.getY();
        const float h     = barArea.getHeight();

        for (int ch = 0; ch < channels; ++ch)
        {
            const float x = barArea.getX() + ch * (barW + gap);
            const auto barRect = juce::Rectangle<float> (x, top, barW, h);

            const float rmsLin  = bank->getRMSLevel     (ch);
            const float peakLin = bank->getPeakLevel    (ch);
            const float holdLin = bank->getPeakHoldLevel (ch);

            paintBar (g, barRect, rmsLin, peakLin, holdLin);
        }
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (bank != nullptr) bank->clearAllPeakHolds();
    }

private:
    void timerCallback() override
    {
        if (bank != nullptr) repaint();
    }

    static float linToDb (float lin) noexcept
    {
        if (lin <= 1.0e-7f) return -140.0f;
        return 20.0f * std::log10 (lin);
    }

    // mcfx_meter's iec_scale — non-linear dB→fraction. Top half is -20..0 dB.
    static float iecScale (float dB) noexcept
    {
        if (dB < -70.0f) return 0.0f;
        if (dB < -60.0f) return (dB + 70.0f) * 0.0025f;
        if (dB < -50.0f) return (dB + 60.0f) * 0.005f  + 0.025f;
        if (dB < -40.0f) return (dB + 50.0f) * 0.0075f + 0.075f;
        if (dB < -30.0f) return (dB + 40.0f) * 0.015f  + 0.15f;
        if (dB < -20.0f) return (dB + 30.0f) * 0.02f   + 0.3f;
        return juce::jmin (1.0f, (dB + 20.0f) * 0.025f + 0.5f);
    }

    static juce::Colour colourForDb (float dB) noexcept
    {
        if (dB >= 0.0f)  return juce::Colour (0xffe04040);
        if (dB >= -6.0f) return juce::Colour (0xffd9b438);
        return juce::Colour (0xff4ec96b);
    }

    void drawScale (juce::Graphics& g, juce::Rectangle<float> r) const
    {
        g.setColour (juce::Colour (0xff8a8a8a));
        g.setFont (juce::Font (juce::FontOptions (10.0f)));

        constexpr int marks[] = { 0, -3, -6, -10, -20, -30, -40, -60 };
        for (int dB : marks)
        {
            const float frac = iecScale (static_cast<float> (dB));
            const float y = r.getBottom() - frac * r.getHeight();
            g.drawText (juce::String (dB),
                        static_cast<int> (r.getX()), static_cast<int> (y - 6.0f),
                        static_cast<int> (r.getWidth() - 2.0f), 12,
                        juce::Justification::centredRight, false);
        }
    }

    void paintBar (juce::Graphics& g,
                   juce::Rectangle<float> bar,
                   float rmsLin, float peakLin, float holdLin) const
    {
        g.setColour (juce::Colour (0xff202024));
        g.fillRoundedRectangle (bar, 2.0f);

        const float rmsDb  = linToDb (rmsLin);
        const float peakDb = linToDb (peakLin);
        const float holdDb = linToDb (holdLin);

        const float rmsFrac = iecScale (rmsDb);
        if (rmsFrac > 0.0f)
        {
            const float fillH = rmsFrac * bar.getHeight();
            const auto rmsRect = juce::Rectangle<float> (bar.getX(),
                                                         bar.getBottom() - fillH,
                                                         bar.getWidth(), fillH);

            juce::ColourGradient grad (juce::Colour (0xff4ec96b),
                                       bar.getX(), bar.getBottom(),
                                       juce::Colour (0xffe04040),
                                       bar.getX(), bar.getY(),
                                       false);
            grad.addColour (0.7,  juce::Colour (0xffd9b438));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (rmsRect, 2.0f);
        }

        if (peakDb > -70.0f)
        {
            const float y = bar.getBottom() - iecScale (peakDb) * bar.getHeight();
            g.setColour (colourForDb (peakDb).brighter (0.2f));
            g.fillRect (juce::Rectangle<float> (bar.getX(), y - 1.0f, bar.getWidth(), 2.0f));
        }

        if (holdDb > -70.0f)
        {
            const float y = bar.getBottom() - iecScale (holdDb) * bar.getHeight();
            g.setColour (holdDb >= 0.0f ? juce::Colour (0xffff5050) : juce::Colours::white);
            g.fillRect (juce::Rectangle<float> (bar.getX(), y - 1.0f, bar.getWidth(), 1.5f));
        }
    }

    MeterBank* bank = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiChannelMeter)
};

}} // namespace mcfx::net
