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

#ifndef SPECTROGRAMCOMPONENT_H_INCLUDED
#define SPECTROGRAMCOMPONENT_H_INCLUDED

#include "JuceHeader.h"
#include "ScopeCapture.h"
#include "Dsp/RealFFT.h"
#include <vector>

/** View settings for the spectrogram, kept in the processor like the scope's. */
struct SpectroSettings
{
    int  firstCh = 0, lastCh = 0;   // inclusive range, 0-based
    bool stacked = false;           // one lane per channel instead of their average
    int  spanIdx  = 1;              // index into the span steps
    int  topIdx   = 0;              // index into the top-of-scale steps
    int  rangeIdx = 2;              // index into the dB range steps
};

/** Scrolling spectrogram with a live spectrum alongside it.

    Taken from what made SpectraFoo's spectragram good: the history and the
    spectrum share one frequency axis and one dB scale, so a line in the
    history and a peak in the spectrum are visibly the same thing. Here time
    runs left to right (newest at the right edge), frequency runs up a log
    axis, and the spectrum strip at the right shows the instantaneous, the
    ~1 s average and a held peak against the same axis.

    Reads the scope's sample history rather than keeping its own, so the audio
    thread does nothing new for it. Columns are produced on a fixed hop in
    absolute sample time, not once per timer tick, so the time axis is exact
    whatever the timer's jitter.

    A channel range is either averaged (in power) into one lane, or stacked as
    one lane per channel. At most kMaxChannels are analysed — that bounds the
    GUI thread's FFT load. */
class SpectrogramComponent : public Component,
                             private ComboBox::Listener,
                             private Button::Listener
{
public:
    static constexpr int kMaxChannels = 16;

    SpectrogramComponent();
    ~SpectrogramComponent() override = default;

    void setCapture (ScopeCapture* capture) { capture_ = capture; }
    void setSettings (SpectroSettings* settings);
    void setNumChannels (int numChannels);

    /** Produce whatever columns are due and repaint. From the editor's timer. */
    void tick();

    void setPaused (bool shouldBePaused);
    bool isPaused() const { return paused_; }

    void paint (Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

    void mouseMove (const MouseEvent&) override;
    void mouseExit (const MouseEvent&) override;

    /** Frequency of a display row's centre (0 = bottom). Public for the tests. */
    float rowHz (int row) const;

    static constexpr int kCols = 480;     // time resolution of the history
    static constexpr int kRows = 384;     // frequency resolution of the history

private:
    void comboBoxChanged (ComboBox*) override;
    void buttonClicked (Button*) override;

    void syncControlsFromSettings();
    void clampSettings();
    int  numShown() const;
    int  numLanes() const;

    void prepareAnalysis (double sr);
    void resetHistory();
    void produceColumn (int64 endAbs, int64 written);
    void recolourAll();

    float topDb() const;
    float rangeDb() const;
    Colour colourFor (float db) const;

    Rectangle<float> plotArea() const;
    Rectangle<float> laneArea (int lane) const;          // history + strip
    Rectangle<float> historyArea (int lane) const;
    Rectangle<float> stripArea (int lane) const;
    float hzToY (float hz, Rectangle<float> r) const;
    float yToHz (float y, Rectangle<float> r) const;

    static String formatHz (float hz);
    static String noteName (float hz);

    ScopeCapture*    capture_  = nullptr;
    SpectroSettings* settings_ = nullptr;
    SpectroSettings  ownSettings_;
    int numCh_ = 1;

    // Analysis.
    double sr_ = 0.0;
    mcfx::RealFFT fft_;
    std::vector<float> window_;
    float ampNorm_ = 1.f;               // power -> full-scale-sine-is-1 amplitude^2
    std::vector<float> magSq_, accum_;
    AudioSampleBuffer frame_;

    // Per display row: the FFT bins it covers, or a fractional bin to
    // interpolate at when the row is narrower than a bin.
    struct RowBins { int lo = 0, hi = 0; float centre = 0.f; bool interp = false; };
    std::vector<RowBins> rowBins_;
    float fMin_ = 20.f, fMax_ = 20000.f;

    struct Lane
    {
        Image image;                    // kCols x kRows, column ring
        std::vector<float> db;          // kCols * kRows, for re-colouring and readout
        std::vector<float> inst, avgPow, peak, peakAge;   // kRows each, the strip
    };
    std::vector<Lane> lanes_;
    int64  colCount_  = 0;              // columns written since reset
    double nextColAbs_ = -1.0;          // absolute sample index of the next column's end

    std::vector<Colour> lut_;

    bool paused_ = false;
    Point<float> hover_;
    bool hovering_ = false;

    ComboBox cbFirst_, cbLast_, cbLayout_, cbSpan_, cbTop_, cbRange_;
    TextButton btnPause_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrogramComponent)
};

#endif // SPECTROGRAMCOMPONENT_H_INCLUDED
