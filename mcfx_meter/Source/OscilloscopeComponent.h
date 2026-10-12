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

#ifndef OSCILLOSCOPECOMPONENT_H_INCLUDED
#define OSCILLOSCOPECOMPONENT_H_INCLUDED

#include "JuceHeader.h"
#include "ScopeCapture.h"

/** View settings the scope keeps in the processor, so they survive closing the
    editor and are saved with the session. Not parameters, like the rest of the
    meter's view state. */
/** How much to enlarge text, margins and line widths for a view of this size:
    1 at the editor's usual size, growing with the window up to 2.5 — so a
    view popped out fullscreen, or onto a projector, is readable from a
    distance. Shared by the scope and the spectrogram. */
inline float displayScaleFor (int width, int height)
{
    return jlimit (1.f, 2.5f, jmin ((float) width / 1000.f, (float) height / 560.f));
}

struct ScopeSettings
{
    int  firstCh = 0, lastCh = 1;   // inclusive range, 0-based
    bool stacked = false;           // one lane per channel instead of overlaid
    int  timeIdx = 0;               // index into the time steps, 0 = auto
    int  ampIdx  = 0;               // index into the amplitude steps, 0 = auto
    bool trigger = true;            // rising zero crossing on the first channel
};

/** Waveform view of one channel or a range of them.

    Deliberately small: a channel range, overlaid or stacked, a time window and
    an amplitude range that are each either auto or one of a few fixed steps,
    a trigger switch and pause. Everything else is the mouse:
      - hover:            crosshair with time and the level of every trace
      - click:            set cursor A, then B (shows the time difference and
                          the matching frequency); double click clears them
      - wheel:            time zoom;  shift + wheel: amplitude zoom
      - drag (paused):    scroll back through the captured second */
class OscilloscopeComponent : public Component,
                              private ComboBox::Listener,
                              private Button::Listener
{
public:
    OscilloscopeComponent();
    ~OscilloscopeComponent() override;

    void setCapture (ScopeCapture* capture) { capture_ = capture; }
    void setSettings (ScopeSettings* settings);
    void setNumChannels (int numChannels);

    /** Pull fresh data and repaint. Driven by the editor's timer. */
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

private:
    void comboBoxChanged (ComboBox*) override;
    void buttonClicked (Button*) override;

    void syncControlsFromSettings();
    void clampChannelRange();

    int   numShown() const;
    int   windowSamples (double sr) const;
    float currentAmp() const;

    void updateLive (double sr);
    void updatePaused (double sr);
    void updateAutoTime (const float* ref, int len, double sr);
    void updateAutoAmp();

    Rectangle<float> plotArea() const;
    Rectangle<float> laneArea (int lane) const;
    int   laneAt (float y) const;

    double xToIdx (float x) const;
    float  idxToX (double idx) const;
    double idxToTime (double idx) const { return (idx - zeroIdx_) / jmax (1.0, sr_); }
    double timeToIdx (double t) const   { return zeroIdx_ + t * sr_; }
    float  valueAt (int ch, double idx) const;

    void drawTrace (Graphics&, int ch, Rectangle<float> lane, Colour c) const;
    static Colour channelColour (int ch);
    static String formatTime (double seconds);
    static String formatLevel (float v);

    float s_ = 1.f;                 // display scale, see displayScaleFor()

    ScopeCapture*  capture_  = nullptr;
    ScopeSettings* settings_ = nullptr;
    ScopeSettings  ownSettings_;
    int numCh_ = 1;

    // What is on screen: work_ holds the shown channels, viewStart_ is the
    // fractional work_ index at the left edge, viewLen_ the window in samples,
    // zeroIdx_ the work_ index the time axis calls 0 (the trigger point, or
    // the newest sample when free-running).
    AudioSampleBuffer work_;
    double viewStart_ = 0.0;
    int    viewLen_   = 1;
    double zeroIdx_   = 0.0;
    double sr_        = 48000.0;
    bool   triggered_ = false;

    // Absolute sample index (counted since the capture was prepared) of
    // work_[0]. Paused positions are kept in these absolute terms.
    int64  workBase_ = 0;

    bool   paused_       = false;
    bool   needsUpdate_  = true;     // paused: something changed the view
    double pauseEndAbs_  = 0.0;      // absolute index at the right edge
    double pauseZeroAbs_ = 0.0;      // absolute index of t = 0

    // Auto ranging state.
    float ampAuto_      = 1.f;
    int   ampShrinkTicks_ = 0;
    int   autoTimeStep_ = 5;          // index into the time steps
    int   autoTimeCandidate_ = 5;
    int   autoTimeTicks_ = 0;
    float shownPeak_ = 0.f;

    // Mouse.
    Point<float> hover_ { -1.f, -1.f };
    bool   hovering_ = false;
    bool   hasA_ = false, hasB_ = false;
    double cursorA_ = 0.0, cursorB_ = 0.0;    // seconds on the time axis
    bool   dragging_ = false;
    double dragStartEndAbs_ = 0.0;
    float  wheelAccum_ = 0.f;

    ComboBox cbFirst_, cbLast_, cbLayout_, cbTime_, cbAmp_;
    ToggleButton tglTrig_;
    TextButton   btnPause_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscilloscopeComponent)
};

#endif // OSCILLOSCOPECOMPONENT_H_INCLUDED
