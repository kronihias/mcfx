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

#ifndef EQBANDEDITOR_H_INCLUDED
#define EQBANDEDITOR_H_INCLUDED

#include "JuceHeader.h"
#include "EqBand.h"

//==============================================================================
/** Small component that draws a FIR impulse response (time-domain waveform).
    Supports drag-and-drop of .txt/.csv/.dat/.wav/.aif/.aiff files. */
class FIRPlot : public Component, public FileDragAndDropTarget
{
public:
    FIRPlot() { setOpaque(false); }

    /** Called when a compatible file is dropped onto the plot. */
    std::function<void(const File&)> onFileDropped;

    bool isInterestedInFileDrag(const StringArray& files) override
    {
        for (auto& f : files)
        {
            auto ext = File(f).getFileExtension().toLowerCase();
            if (ext == ".txt" || ext == ".csv" || ext == ".dat"
                || ext == ".wav" || ext == ".aif" || ext == ".aiff")
                return true;
        }
        return false;
    }

    void filesDropped(const StringArray& files, int, int) override
    {
        dragOver_ = false;
        repaint();
        if (onFileDropped == nullptr) return;
        for (auto& f : files)
        {
            auto ext = File(f).getFileExtension().toLowerCase();
            if (ext == ".txt" || ext == ".csv" || ext == ".dat"
                || ext == ".wav" || ext == ".aif" || ext == ".aiff")
            {
                onFileDropped(File(f));
                return;
            }
        }
    }

    void fileDragEnter(const StringArray&, int, int) override { dragOver_ = true;  repaint(); }
    void fileDragExit(const StringArray&)             override { dragOver_ = false; repaint(); }

    void setCoefficients(const std::vector<float>& coeffs)
    {
        coeffs_ = coeffs;
        // Reset view to the full FIR. Mouse-wheel zoom / horizontal drag / double-click
        // (in mouse handlers below) let the user inspect a sub-range without resampling.
        viewStart_ = 0.f;
        viewEnd_   = jmax (1.f, (float) ((int) coeffs_.size() - 1));
        repaint();
    }

    void paint(Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();

        // Background
        g.setColour(Colour(0xff1a1a1a));
        g.fillRoundedRectangle(area, 4.f);

        // Border
        g.setColour(Colour(0xff333333));
        g.drawRoundedRectangle(area.reduced(0.5f), 4.f, 1.f);

        if (coeffs_.empty())
        {
            if (!dragOver_)
            {
                g.setColour(Colours::white.withAlpha(0.3f));
                g.setFont(Font(FontOptions(12.f)));
                g.drawText("No FIR loaded", area, Justification::centred);
            }
            // fall through to drag overlay below
        }
        else
        {

        const float padT = 6.f, padB = 16.f, padR = 6.f;   // padB leaves room for x-axis labels
        const float axisW = 38.f; // left margin for Y-axis labels
        auto plotArea = Rectangle<float>(area.getX() + axisW, area.getY() + padT,
                                          area.getWidth() - axisW - padR,
                                          area.getHeight() - padT - padB);
        float w = plotArea.getWidth();
        float h = plotArea.getHeight();
        float x0 = plotArea.getX();
        float y0 = plotArea.getY();

        // ---- View range ----
        const int n = (int) coeffs_.size();
        float viewStart = jlimit (0.f, (float)(n - 1), viewStart_);
        float viewEnd   = jlimit (viewStart + 1.f, (float)(n - 1),
                                   viewEnd_ > viewStart ? viewEnd_ : (float)(n - 1));
        const float visible = viewEnd - viewStart;
        auto sampleToX = [&] (float s) {
            return x0 + (s - viewStart) / visible * w;
        };

        // Find peak for scaling — over the visible range only, so zoom-to-tail
        // shows useful detail of the small ringing samples instead of being
        // dwarfed by the (off-screen) main impulse.
        int iStart = jmax (0, (int) std::floor (viewStart));
        int iEnd   = jmin (n - 1, (int) std::ceil  (viewEnd));
        float rawPeak = 0.f;
        for (int i = iStart; i <= iEnd; ++i)
            rawPeak = jmax (rawPeak, std::abs (coeffs_[(size_t) i]));
        if (rawPeak < 1e-10f) rawPeak = 1.f;

        // Choose a nice axis range: find smallest nice number >= rawPeak
        float niceRange = rawPeak;
        {
            float mag = std::pow(10.f, std::floor(std::log10(rawPeak)));
            float norm = rawPeak / mag;
            if (norm <= 1.f)       niceRange = 1.f * mag;
            else if (norm <= 1.5f) niceRange = 1.5f * mag;
            else if (norm <= 2.f)  niceRange = 2.f * mag;
            else if (norm <= 2.5f) niceRange = 2.5f * mag;
            else if (norm <= 5.f)  niceRange = 5.f * mag;
            else                   niceRange = 10.f * mag;
        }

        float midY = y0 + h * 0.5f;
        float scale = (h * 0.45f) / niceRange;

        // --- Y-axis grid lines and labels ---
        // Choose tick step: divide niceRange into 2-5 steps
        float tickStep = niceRange;
        if (niceRange / 0.5f <= 5.f)        tickStep = 0.5f;
        else if (niceRange / 0.1f <= 5.f)   tickStep = 0.1f;
        else if (niceRange / 0.05f <= 5.f)  tickStep = 0.05f;
        else if (niceRange / 0.01f <= 5.f)  tickStep = 0.01f;
        else if (niceRange / 0.005f <= 5.f) tickStep = 0.005f;
        else                                 tickStep = niceRange / 4.f;

        // Ensure we get 2-5 ticks (including positive and negative)
        {
            float nTicks = niceRange / tickStep;
            if (nTicks > 5.f) tickStep = niceRange / 4.f;
            else if (nTicks < 1.5f) tickStep = niceRange / 4.f;
        }

        g.setFont(Font(FontOptions(9.f)));

        // Draw ticks from 0 up and down
        for (float v = 0.f; v <= niceRange * 1.01f; v += tickStep)
        {
            for (int sign = -1; sign <= 1; sign += 2)
            {
                float val = v * sign;
                float yp = midY - val * scale;

                if (yp < y0 - 1.f || yp > y0 + h + 1.f)
                    continue;

                // Grid line
                g.setColour(Colour(v < 0.001f ? 0xff555555 : 0xff333333));
                g.drawHorizontalLine((int)yp, x0, x0 + w);

                // Tick mark
                g.setColour(Colour(0xff555555));
                g.drawHorizontalLine((int)yp, x0 - 3.f, x0);

                // Label
                g.setColour(Colours::white.withAlpha(0.5f));
                String label;
                if (tickStep >= 0.5f)
                    label = String(val, (tickStep == (int)tickStep) ? 0 : 1);
                else if (tickStep >= 0.01f)
                    label = String(val, 2);
                else
                    label = String(val, 3);

                g.drawText(label,
                           (int)(area.getX() + 2.f), (int)(yp - 6.f),
                           (int)(axisW - 6.f), 12,
                           Justification::centredRight, false);

                if (v < 0.001f) break; // zero only once
            }
        }

        // --- X-axis grid + sample-number labels ---
        // Pick a "nice" sample step based on the visible range — aim for ~5–8 ticks.
        {
            double rawStep = (double) visible / 6.0;
            double mag10 = std::pow (10.0, std::floor (std::log10 (jmax (1.0, rawStep))));
            double norm  = rawStep / mag10;
            double nice  = norm < 1.5 ? 1.0 : norm < 3.5 ? 2.0 : norm < 7.5 ? 5.0 : 10.0;
            int step = jmax (1, (int) std::round (nice * mag10));
            int firstTick = ((int) std::ceil (viewStart / step)) * step;

            g.setColour (Colour (0xff2a2a2a));
            for (int s = firstTick; s <= (int) viewEnd; s += step)
            {
                float xp = sampleToX ((float) s);
                g.drawVerticalLine ((int) xp, y0, y0 + h);
            }

            g.setFont (Font (FontOptions (9.f)));
            g.setColour (Colours::white.withAlpha (0.45f));
            for (int s = firstTick; s <= (int) viewEnd; s += step)
            {
                float xp = sampleToX ((float) s);
                g.drawText (String (s), (int) xp - 24, (int) (y0 + h + 1), 48, 12,
                            Justification::centred, false);
            }
        }

        // --- Waveform ---
        // Decimate when there are more visible samples than 2× the pixel width.
        int visibleCount = iEnd - iStart + 1;
        bool decimated = (float) visibleCount > w * 2.f;

        Path fillPath, linePath;
        bool started = false;

        if (! decimated)
        {
            for (int i = iStart; i <= iEnd; ++i)
            {
                float xp = sampleToX ((float) i);
                float yp = midY - coeffs_[(size_t) i] * scale;
                if (! started) { fillPath.startNewSubPath (xp, midY); started = true; }
                fillPath.lineTo (xp, yp);
                if (i == iStart) linePath.startNewSubPath (xp, yp);
                else             linePath.lineTo (xp, yp);
            }
            fillPath.lineTo (sampleToX ((float) iEnd), midY);
        }
        else
        {
            // Stick plot: per-pixel column, draw min..max range from this column's samples.
            int numCols = (int) w;
            for (int col = 0; col < numCols; ++col)
            {
                float colXf = (float) col;
                float sStart = viewStart + (colXf       / w) * visible;
                float sEnd   = viewStart + ((colXf + 1) / w) * visible;
                int i0 = jmax (0,     (int) std::floor (sStart));
                int i1 = jmin (n - 1, (int) std::ceil  (sEnd));
                if (i1 < i0) continue;
                float vmin = coeffs_[(size_t) i0], vmax = vmin;
                for (int i = i0 + 1; i <= i1; ++i)
                {
                    vmin = jmin (vmin, coeffs_[(size_t) i]);
                    vmax = jmax (vmax, coeffs_[(size_t) i]);
                }
                float xp = x0 + colXf;
                float ypMin = midY - vmin * scale;
                float ypMax = midY - vmax * scale;
                if (vmax > 0.f) { fillPath.startNewSubPath (xp, midY); fillPath.lineTo (xp, ypMax); }
                if (vmin < 0.f) { fillPath.startNewSubPath (xp, midY); fillPath.lineTo (xp, ypMin); }
                linePath.startNewSubPath (xp, ypMin);
                linePath.lineTo (xp, ypMax);
            }
        }

        if (! decimated)
            fillPath.closeSubPath();
        g.setColour (Colours::white.withAlpha (0.08f));
        g.fillPath (fillPath);

        g.setColour (Colours::white.withAlpha (decimated ? 0.55f : 0.7f));
        g.strokePath (linePath, PathStrokeType (decimated ? 1.f : 1.2f));

        // Draw sample dots when zoomed enough that they don't cluster
        if (visibleCount <= 200 && ! decimated)
        {
            float dotR = (visibleCount <= 64) ? 2.f : 1.2f;
            g.setColour (Colours::white.withAlpha (0.9f));
            for (int i = iStart; i <= iEnd; ++i)
            {
                float xp = sampleToX ((float) i);
                float yp = midY - coeffs_[(size_t) i] * scale;
                g.fillEllipse (xp - dotR, yp - dotR, dotR * 2.f, dotR * 2.f);
            }
        }

        // Label: tap count
        g.setColour (Colours::white.withAlpha (0.35f));
        g.setFont (Font (FontOptions (10.f)));
        g.drawText (String (n) + " taps",
                    (int) (x0), (int) (y0 + h - 14.f), (int) w - 4, 14,
                    Justification::bottomRight);
        } // end of else (non-empty coefficients)

        // Drag-over highlight
        if (dragOver_)
        {
            g.setColour(Colours::white.withAlpha(0.15f));
            g.fillRoundedRectangle(area, 4.f);
            g.setColour(Colours::white.withAlpha(0.6f));
            g.drawRoundedRectangle(area.reduced(1.f), 4.f, 2.f);
            g.setFont(Font(FontOptions(13.f)));
            g.drawText("Drop FIR file here", area, Justification::centred);
        }
    }

    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel) override
    {
        const int n = (int) coeffs_.size();
        if (n < 4) return;
        const float axisW = 38.f, padR = 6.f;
        float plotX0 = (float) axisW;
        float plotW  = (float) getWidth() - axisW - padR;
        if (plotW <= 1.f) return;

        float vs = jlimit (0.f, (float)(n - 1), viewStart_);
        float ve = jlimit (vs + 1.f, (float)(n - 1),
                            viewEnd_ > vs ? viewEnd_ : (float)(n - 1));
        float visible = ve - vs;

        float zoom = jlimit (0.6f, 1.5f, 1.f - wheel.deltaY * 0.25f);
        float newVisible = jlimit (3.f, (float)(n - 1), visible * zoom);

        // Zoom around the sample under the cursor
        float frac = jlimit (0.f, 1.f, ((float) e.getPosition().getX() - plotX0) / plotW);
        float cursorSample = vs + frac * visible;

        viewStart_ = cursorSample - frac * newVisible;
        viewEnd_   = cursorSample + (1.f - frac) * newVisible;

        clampView (n);
        repaint();
    }

    void mouseDown (const MouseEvent&) override
    {
        dragStartViewStart_ = viewStart_;
        dragStartViewEnd_   = viewEnd_;
    }

    void mouseDrag (const MouseEvent& e) override
    {
        const int n = (int) coeffs_.size();
        if (n < 4) return;
        const float axisW = 38.f, padR = 6.f;
        float plotW = (float) getWidth() - axisW - padR;
        if (plotW <= 1.f) return;

        float visible = dragStartViewEnd_ - dragStartViewStart_;
        if (visible <= 0.f) return;
        float dxSamples = -(float) e.getDistanceFromDragStartX() * visible / plotW;
        viewStart_ = dragStartViewStart_ + dxSamples;
        viewEnd_   = dragStartViewEnd_   + dxSamples;
        clampView (n);
        repaint();
    }

    void mouseDoubleClick (const MouseEvent&) override
    {
        viewStart_ = 0.f;
        viewEnd_   = jmax (1.f, (float) ((int) coeffs_.size() - 1));
        repaint();
    }

private:
    void clampView (int n)
    {
        float maxEnd = (float) (n - 1);
        float visible = viewEnd_ - viewStart_;
        if (visible < 3.f) visible = 3.f;
        if (visible > maxEnd) visible = maxEnd;
        if (viewStart_ < 0.f)        { viewStart_ = 0.f;          viewEnd_ = visible; }
        if (viewEnd_   > maxEnd)     { viewEnd_   = maxEnd;       viewStart_ = maxEnd - visible; }
        if (viewStart_ < 0.f) viewStart_ = 0.f;
    }

    std::vector<float> coeffs_;
    bool dragOver_ = false;

    // X-axis view range (in samples). Mouse-wheel zooms, horizontal drag pans,
    // double-click resets. setCoefficients() also resets to the full FIR.
    float viewStart_ = 0.f;
    float viewEnd_   = 0.f;
    float dragStartViewStart_ = 0.f;
    float dragStartViewEnd_   = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FIRPlot)
};

//==============================================================================
class EqBandEditor : public Component,
                     public Slider::Listener,
                     public ComboBox::Listener,
                     public Button::Listener,
                     public Label::Listener,
                     private Timer
{
public:

    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void bandParameterChanged(int bandIndex) = 0;
        virtual void bandEnableChanged(int bandIndex, bool enabled) = 0;
        /** Called when band type, subtype, or order changes (structural — requires rebuild). */
        virtual void bandStructureChanged(int bandIndex) = 0;
    };

    EqBandEditor();
    ~EqBandEditor();

    void setBand(EqBand* band, int bandIndex);
    void setListener(Listener* l) { listener_ = l; }
    void updateFromBand();

    void paint(Graphics& g) override;
    void resized() override;
    void sliderValueChanged(Slider* s) override;
    void comboBoxChanged(ComboBox* cb) override;
    void buttonClicked(Button* b) override;
    void labelTextChanged(Label* l) override;

private:
    enum class OrderComboKind { Butterworth, Crossover, AnalogPrototype };
    void showControlsForType(EqBandType type);
    void populateOrderCombo(OrderComboKind kind, bool isAP = false);
    void updateDelayDisplay();

    EqBand* band_ = nullptr;
    int bandIndex_ = -1;
    Listener* listener_ = nullptr;

    /** ComboBox variant that groups the higher-order analog-prototype filter types
        (Chebyshev I/II, Elliptic, Bessel) into a "High-Order" submenu so the everyday
        biquad list isn't drowned by them. */
    class IIRSubTypeCombo : public ComboBox
    {
    public:
        void showPopup() override;
    };

    ComboBox cbBandType_;       // IIR, FIR, Gain, Delay
    IIRSubTypeCombo cbIIRSubType_; // low_pass, high_pass, etc., with submenu for analog prototypes
    ComboBox cbOrder_;          // Butterworth order (1-8)

    Slider sldFreq_;
    Slider sldQ_;
    Slider sldRipplePass_;     // Chebyshev I passband ripple (dB)
    Slider sldRippleStop_;     // Chebyshev II stopband attenuation (dB)
    Slider sldGain_;
    Slider sldLinearGain_;
    ToggleButton btnGainLinear_ { "Linear" };
    ToggleButton btnInvertGain_ { "Invert" };
    Slider sldDelay_;
    ToggleButton btnDelayMs_ { "ms" };  // toggle: off = samples, on = ms
    bool delayInMs_ = true;

    ComboBox cbSampleRate_;     // Original sample rate selector

    Label lblType_    { {}, "Type:" };
    Label lblSubType_ { {}, "Filter:" };
    Label lblFreq_    { {}, "Freq:" };
    Label lblQ_       { {}, "Q:" };
    Label lblOrder_   { {}, "Order:" };
    Label lblRipplePass_ { {}, "Ripple:" };
    Label lblRippleStop_ { {}, "Atten:" };
    Label lblGain_    { {}, "Gain:" };
    Label lblDelay_   { {}, "Delay:" };
    Label lblSampleRate_ { {}, "Rate:" };
    Label lblHz_      { {}, "Hz" };
    Label lblDb_      { {}, "dB" };
    Label lblSamples_ { {}, "smpls" };

    ToggleButton btnEnable_ { "Enable" };

    // Biquad coefficient editors
    Label lblBiquad_  { {}, "Coefficients:" };
    Label lblB0_      { {}, "b0:" };
    Label lblB1_      { {}, "b1:" };
    Label lblB2_      { {}, "b2:" };
    Label lblA0_      { {}, "a0:" };
    Label lblA1_      { {}, "a1:" };
    Label lblA2_      { {}, "a2:" };
    Label edB0_, edB1_, edB2_, edA0_, edA1_, edA2_;
    Label lblStability_ { {}, "" };

    void setupCoeffLabel(Label& ed, const String& tooltip);
    void applyBiquadCoeffsFromUI();
    void updateStabilityIndicator();
    void copyBiquadCoeffsToClipboard();
    void pasteBiquadCoeffsFromClipboard();

    TextButton btnCopyCoeffs_  { "Copy" };
    TextButton btnPasteCoeffs_ { "Paste" };

    TextButton btnLoadFIR_   { "Load FIR..." };
    TextButton btnDesignFIR_ { "Design..." };
    Label lblFIRInfo_ { {}, "" };
    FIRPlot firPlot_;
    void loadFIRFile(const File& file);
    void loadFIRAudioFile(const File& file, int channel);
    void timerCallback() override;

    // Deferred channel-selection popup for multi-channel audio drop
    File pendingAudioFile_;
    int pendingAudioChannels_ = 0;

    bool updating_ = false; // prevent feedback loops

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqBandEditor)
};

#endif // EQBANDEDITOR_H_INCLUDED
