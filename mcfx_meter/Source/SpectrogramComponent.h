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
#include "Dsp/ConstantQ.h"
#include <vector>

/** View settings for the spectrogram, kept in the processor like the scope's. */
struct SpectroSettings
{
    int  firstCh = 0, lastCh = 0;   // inclusive range, 0-based
    bool stacked = false;           // one lane per channel instead of their average
    int  spanIdx  = 1;              // index into the span steps
    int  topIdx   = 0;              // index into the top-of-scale steps
    int  rangeIdx = 2;              // index into the dB range steps
    float zoomLo = 0.f, zoomHi = 0.f;   // shown frequency range in Hz; 0 = all of it
    bool fineBass = true;           // constant-Q bass (sharper, 0.34 s later) vs FFT only

    // Display options, set from the settings panel.
    int  colourMap   = 0;           // index into the colour maps
    int  colourScale = 0;           // 0 dB, 1 meter (IEC), 2 linear amplitude
    int  normalise   = 0;           // 0 off, 1 each column, 2 auto top
    int  freqScale   = 0;           // 0 log, 1 mel, 2 linear
    bool noteLabels  = false;       // label the frequency axis with notes, not Hz
    int  windowIdx   = 2;           // treble FFT: 2048 / 4096 / 8192 / 16384 at 48 kHz
    bool smooth      = true;        // interpolate the picture, or show its pixels
    bool showStrip   = true;        // the live spectrum beside the history
    bool harmonics   = true;        // hover marks 2x..20x the frequency under the pointer
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

    The history's resolution follows the size it is shown at — a projector
    gets a picture as fine as its pixels, a corner window a cheap one — within
    a budget of transforms per second, so a short span over many channels
    cannot swamp the GUI thread. Resizing resamples the history rather than
    clearing it.

    The mouse wheel zooms the frequency axis about the pointer and dragging
    pans it; double click shows everything again. Zoom only changes what is
    drawn: the analysis always covers the whole range, so zooming never loses
    history.

    Bass: an FFT long enough to resolve 50 Hz from 55 Hz would smear the
    treble over a third of a second, so with "fine bass" the rows below
    ~450 Hz come from a constant-Q transform instead — 24 bins per octave,
    window lengthening into the bass — run on a copy of each channel
    decimated to ~3 kHz, which makes it about as cheap as the FFT. Above
    ~450 Hz the FFT is already finer than a quarter tone and stays in charge.

    Time alignment: a column describes one instant, its centre. The FFT frame
    is centred on it, and every constant-Q window too (ConstantQ::Centred,
    with the decimator's delay taken out), so a click is one vertical line
    from 20 Hz to 20 kHz rather than bending later into the bass — the fix
    from SpaceLevelMeter. The price is that the newest column is half the
    longest window old: 0.34 s with fine bass, 85 ms without. The time axis
    says so rather than calling the right edge "now".

    A channel range is either averaged (in power) into one lane, or stacked as
    one lane per channel. At most kMaxChannels are analysed at once. */
class SpectrogramComponent : public Component,
                             private ComboBox::Listener,
                             private Button::Listener
{
public:
    static constexpr int kMaxChannels = 16;

    SpectrogramComponent();
    ~SpectrogramComponent() override;

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
    void mouseDown (const MouseEvent&) override;
    void mouseDrag (const MouseEvent&) override;
    void mouseDoubleClick (const MouseEvent&) override;
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails&) override;

    /** Frequency of an analysis row's centre (0 = lowest). */
    float rowHz (int row) const;
    int   getNumCols() const { return cols_; }
    int   getNumRows() const { return rows_; }

private:
    void comboBoxChanged (ComboBox*) override;
    void buttonClicked (Button*) override;

    void syncControlsFromSettings();
    void clampSettings();
    int  numShown() const;
    int  numLanes() const;

    void prepareAnalysis (double sr);
    void buildRowBins();
    void resetHistory();
    void updateWantedResolution();
    void resampleHistory (int newCols, int newRows);
    void produceColumn (int64 centreAbs, int64 written);

    // Fine bass: decimator + constant-Q, see the class comment.
    void prepareBass (double sr);
    void resetBass();
    void updateDecimation (int64 written);
    bool bassActive() const { return settings_->fineBass && cq_.isReady(); }
    /** Samples between a column's centre and the newest sample it needs. */
    int  leadSamples() const;
    void recolourAll();

    float topDb() const;            // the effective top: auto top when normalising to it
    float rangeDb() const;
    /** Colour for a level, after adding a column's normalisation offset. */
    Colour colourFor (float db, float offset = 0.f) const;
    float  colourOffset (int lane, int col) const;
    void   buildLut();
    void   showSettings();
    /** Apply a change made in the settings panel. */
    void   settingsChanged();
    void   updateAutoTop();
public:
    static StringArray colourMapNames();
private:

    // Shown frequency range (the zoom), and the analysis row a frequency
    // falls in — fractional, 0 at the bottom of the lowest row.
    float zoomLo() const;
    float zoomHi() const;
    float rowPos (float hz) const;
    float warp (float hz) const;       // frequency -> axis units (log / mel / linear)
    float unwarp (float w) const;
    void  setZoom (float lo, float hi);

    Rectangle<float> plotArea() const;
    Rectangle<float> laneArea (int lane) const;          // history + strip
    Rectangle<float> historyArea (int lane) const;
    Rectangle<float> stripArea (int lane) const;
    float hzToY (float hz, Rectangle<float> r) const;
    float yToHz (float y, Rectangle<float> r) const;
    int   laneAt (Point<float> p) const;

    static String formatHz (float hz);
    static String noteName (float hz);

    ScopeCapture*    capture_  = nullptr;
    SpectroSettings* settings_ = nullptr;
    SpectroSettings  ownSettings_;
    int numCh_ = 1;
    float s_ = 1.f;                     // display scale, see displayScaleFor()

    // Analysis.
    double sr_ = 0.0;
    mcfx::RealFFT fft_;
    std::vector<float> window_;
    float ampNorm_ = 1.f;               // power -> full-scale-sine-is-1 amplitude^2
    std::vector<float> magSq_, accum_;
    AudioSampleBuffer frame_;

    // Per analysis row: the FFT bins it covers, or a fractional bin to
    // interpolate at when the row is narrower than a bin.
    struct RowBins { int lo = 0, hi = 0; float centre = 0.f; bool interp = false; };
    std::vector<RowBins> rowBins_;
    float fMin_ = 20.f, fMax_ = 20000.f;

    // Fine bass.
    static constexpr int kDecRing = 4096;            // decimated samples kept per channel
    mcfx::ConstantQ cq_;
    int   decim_ = 16;                                // decimation factor
    int   firDelay_ = 0;                              // decimator delay, raw samples
    std::vector<float> fir_;                          // linear-phase anti-alias lowpass
    std::vector<std::vector<float>> decRing_;         // per shown channel, kDecRing
    int64 nextDec_ = -1;                              // next decimated index to compute
    AudioSampleBuffer rawBuf_;
    std::vector<float> cqMag_, bassPow_, bassMag_;
    std::vector<float> rowCqPos_, rowBassW_;          // per row: constant-Q bin position, bass weight

    // History resolution: what it is, and what the current size asks for.
    int cols_ = 480, rows_ = 384;
    int wantCols_ = 480, wantRows_ = 384;

    struct Lane
    {
        Image image;                    // cols_ x rows_, column ring
        std::vector<float> db;          // cols_ * rows_, for re-colouring and readout
        std::vector<float> inst, avgPow, peak, peakAge;   // rows_ each, the strip
        std::vector<float> colMax;      // cols_, each column's loudest row, for normalising
    };
    std::vector<Lane> lanes_;
    int64  colCount_  = 0;              // columns written since reset
    double nextColAbs_ = -1.0;          // absolute sample index of the next column's end

    std::vector<Colour> lut_;
    float autoTop_ = 0.f;               // loudest level in the history, for "auto top"
    float colouredAutoTop_ = 0.f;       // the auto top the picture was last coloured with
    SpectroSettings applied_;           // what the analysis/history were built for

    bool paused_ = false;
    Point<float> hover_;
    bool hovering_ = false;
    float dragZoomLo_ = 0.f, dragZoomHi_ = 0.f;
    int   dragLane_ = -1;

    ComboBox cbFirst_, cbLast_, cbLayout_, cbSpan_, cbTop_, cbRange_, cbRes_;
    TextButton btnPause_;
    Component::SafePointer<CallOutBox> callout_;
    ShapeButton btnSettings_ { "settings", Colours::white.withAlpha (0.7f),
                               Colours::white, Colours::aquamarine };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrogramComponent)
};

#endif // SPECTROGRAMCOMPONENT_H_INCLUDED
