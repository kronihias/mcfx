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

#ifndef SPECTRUMCOMPONENT_H_INCLUDED
#define SPECTRUMCOMPONENT_H_INCLUDED

#include "JuceHeader.h"
#include "ScopeCapture.h"
#include "Dsp/RealFFT.h"
#include <vector>

/** View settings for the spectrum, kept in the processor like the scope's. */
struct SpectrumSettings
{
    int  firstCh = 0, lastCh = 0;   // inclusive range, 0-based
    bool average = false;           // one curve, the channels' power average, instead of one each
    int  avgIdx    = 2;             // index into the time averaging steps
    int  smoothIdx = 0;             // index into the fractional-octave smoothing steps
    int  topIdx    = 0;             // index into the top-of-scale steps
    int  rangeIdx  = 1;             // index into the dB range steps
    float zoomLo = 0.f, zoomHi = 0.f;   // shown frequency range in Hz; 0 = all of it
    float dbTop = 0.f, dbRange = 0.f;   // level view dragged or zoomed off the steps; range 0 = the steps

    // Set from the settings panel.
    int  windowIdx = 1;             // FFT: 4096 / 8192 / 16384 / 32768 at 48 kHz
    int  peakMode  = 1;             // 0 off, 1 falling, 2 held until clicked
    bool bands     = false;         // third-octave bands instead of a curve
    bool fill      = true;          // shade under a single curve
};

/** Real-time spectrum of one channel or a range of them, the way mimoeq's and
    mcfx_filter's analysers draw it but on its own: log frequency across, dB
    up, one curve per channel in the scope's channel colours, or their power
    average as one.

    Levels are dBFS with a full-scale sine reading 0 dB, as in the
    spectrogram. Fractional-octave smoothing averages power over the band
    around each frequency, so it shows noise and dense music as a steady
    curve; a tone then reads lower by however much the band dilutes it. The
    third-octave bands sum power over each band and correct for the window's
    noise bandwidth, so a tone reads its level in its band and noise reads its
    band power, as a hardware RTA does.

    Reads the scope's sample history like the spectrogram, so the audio thread
    does nothing new for it. Every timer tick transforms the newest window of
    each shown channel; time averaging is exponential in power over the audio
    that actually arrived, so it means the same whatever the timer does.

    Mouse: hover reads frequency, note and every curve's level; a click
    clears the held peaks.
    - In the plot: dragging pans both axes; the mouse wheel and pinch zoom the
      frequency axis about the pointer (with alt, the level axis); trackpad
      scrolling pans both axes.
    - On an axis: dragging along it pans it, dragging across it zooms it about
      where it was grabbed; the wheel and pinch zoom it about the pointer.
    - Double click resets that axis, or both in the plot. */
class SpectrumComponent : public Component,
                          private ComboBox::Listener,
                          private Button::Listener
{
public:
    static constexpr int kMaxChannels = 16;

    SpectrumComponent();
    ~SpectrumComponent() override;

    void setCapture (ScopeCapture* capture) { capture_ = capture; }
    void setSettings (SpectrumSettings* settings);
    void setNumChannels (int numChannels);

    /** Transform the newest audio and repaint. From the editor's timer. */
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
    void mouseUp (const MouseEvent&) override;
    void mouseDoubleClick (const MouseEvent&) override;
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails&) override;
    void mouseMagnify (const MouseEvent&, float scaleFactor) override;

private:
    void comboBoxChanged (ComboBox*) override;
    void buttonClicked (Button*) override;

    void syncControlsFromSettings();
    void clampSettings();
    void showSettings();
    void settingsChanged();

    int  numShown() const;
    int  numCurves() const;

    void prepareAnalysis (double sr);
    void resetCurves();
    void clearPeaks();

    /** Power of curve `c` around a frequency: the averaged spectrum, smoothed
        over the band the settings ask for (or a pixel's span, whichever is
        wider). `peak` reads the held peaks instead. */
    float powerAt (int c, float hzLo, float hzHi, bool peak) const;
    /** Power summed over a band, corrected for the window's noise bandwidth. */
    float bandPower (int c, float hzLo, float hzHi, bool peak) const;

    float topDb() const;
    float rangeDb() const;
    float zoomLo() const;
    float zoomHi() const;
    void  setZoom (float lo, float hi);
    /** Zoom the frequency axis by `factor` (< 1 is in) about the pixel `x`. */
    void  zoomFreqAbout (float x, float factor);
    /** Shift the frequency axis so its content moves `dx` pixels right. */
    void  panFreq (float dx);
    void  setLevelView (float top, float range);
    /** Zoom the level axis by `factor` (< 1 is in) about the pixel `y`. */
    void  zoomLevelAbout (float y, float factor);

    enum class Region { none, plot, levelAxis, freqAxis };
    Region regionAt (Point<float>) const;

    Rectangle<float> plotArea() const;
    float hzToX (float hz) const;
    float xToHz (float x) const;
    float dbToY (float db) const;

    Colour curveColour (int c) const;
    String curveName (int c) const;

    static String formatHz (float hz);
    static String noteName (float hz);

    ScopeCapture*     capture_  = nullptr;
    SpectrumSettings* settings_ = nullptr;
    SpectrumSettings  ownSettings_;
    SpectrumSettings  applied_;
    int numCh_ = 1;
    float s_ = 1.f;                     // display scale, see displayScaleFor()

    // Analysis.
    double sr_ = 0.0;
    mcfx::RealFFT fft_;
    std::vector<float> window_;
    float ampNorm_ = 1.f;               // bin power -> full-scale-sine-is-1 amplitude^2
    float enbw_ = 2.f;                  // the window's noise bandwidth, in bins
    std::vector<float> magSq_, accum_;
    AudioSampleBuffer frame_;
    int64 lastWritten_ = -1;

    struct Curve
    {
        std::vector<float> avg;         // averaged power per bin
        std::vector<float> peak;        // held peak power per bin
        std::vector<float> peakAge;     // seconds since each peak was set
        std::vector<double> cum, cumPeak;   // running sums of avg / peak, for band averages
    };
    std::vector<Curve> curves_;
    bool fresh_ = true;                 // next frame starts the average over

    bool paused_ = false;
    Point<float> hover_;
    bool hovering_ = false;
    bool dragged_ = false;
    Region dragRegion_ = Region::none;
    float dragZoomLo_ = 0.f, dragZoomHi_ = 0.f;
    float dragTop_ = 0.f, dragRange_ = 0.f;

    ComboBox cbFirst_, cbLast_, cbLayout_, cbAvg_, cbSmooth_, cbTop_, cbRange_;
    TextButton btnPause_;
    Component::SafePointer<CallOutBox> callout_;
    ShapeButton btnSettings_ { "settings", Colours::white.withAlpha (0.7f),
                               Colours::white, Colours::aquamarine };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumComponent)
};

#endif // SPECTRUMCOMPONENT_H_INCLUDED
