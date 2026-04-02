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
/** Small component that draws a FIR impulse response (time-domain waveform). */
class FIRPlot : public Component
{
public:
    FIRPlot() { setOpaque(false); }

    void setCoefficients(const std::vector<float>& coeffs)
    {
        coeffs_ = coeffs;
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
            g.setColour(Colours::white.withAlpha(0.3f));
            g.setFont(Font(12.f));
            g.drawText("No FIR loaded", area, Justification::centred);
            return;
        }

        const float padT = 6.f, padB = 6.f, padR = 6.f;
        const float axisW = 38.f; // left margin for Y-axis labels
        auto plotArea = Rectangle<float>(area.getX() + axisW, area.getY() + padT,
                                          area.getWidth() - axisW - padR,
                                          area.getHeight() - padT - padB);
        float w = plotArea.getWidth();
        float h = plotArea.getHeight();
        float x0 = plotArea.getX();
        float y0 = plotArea.getY();

        // Find peak for scaling — round up to a "nice" value
        float rawPeak = 0.f;
        for (auto c : coeffs_)
            rawPeak = jmax(rawPeak, std::abs(c));
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

        g.setFont(Font(9.f));

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

        // --- Waveform ---
        int n = (int)coeffs_.size();
        float sampleW = w / (float)jmax(1, n - 1);

        // Fill area under curve
        Path fillPath;
        bool started = false;
        for (int i = 0; i < n; ++i)
        {
            float xp = x0 + (float)i * sampleW;
            float yp = midY - coeffs_[i] * scale;
            if (!started) { fillPath.startNewSubPath(xp, midY); started = true; }
            fillPath.lineTo(xp, yp);
        }
        fillPath.lineTo(x0 + (float)(n - 1) * sampleW, midY);
        fillPath.closeSubPath();
        g.setColour(Colours::white.withAlpha(0.08f));
        g.fillPath(fillPath);

        // Draw line
        Path linePath;
        for (int i = 0; i < n; ++i)
        {
            float xp = x0 + (float)i * sampleW;
            float yp = midY - coeffs_[i] * scale;
            if (i == 0) linePath.startNewSubPath(xp, yp);
            else linePath.lineTo(xp, yp);
        }
        g.setColour(Colours::white.withAlpha(0.7f));
        g.strokePath(linePath, PathStrokeType(1.2f));

        // Draw sample dots if not too many
        if (n <= 200)
        {
            float dotR = (n <= 64) ? 2.f : 1.2f;
            g.setColour(Colours::white.withAlpha(0.9f));
            for (int i = 0; i < n; ++i)
            {
                float xp = x0 + (float)i * sampleW;
                float yp = midY - coeffs_[i] * scale;
                g.fillEllipse(xp - dotR, yp - dotR, dotR * 2.f, dotR * 2.f);
            }
        }

        // Label: tap count
        g.setColour(Colours::white.withAlpha(0.35f));
        g.setFont(Font(10.f));
        g.drawText(String(n) + " taps",
                   (int)(x0), (int)(y0 + h - 14.f), (int)w - 4, 14,
                   Justification::bottomRight);
    }

private:
    std::vector<float> coeffs_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FIRPlot)
};

//==============================================================================
class EqBandEditor : public Component,
                     public Slider::Listener,
                     public ComboBox::Listener,
                     public Button::Listener,
                     public Label::Listener
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
    void showControlsForType(EqBandType type);
    void populateOrderCombo(bool isCrossover, bool isAP = false);

    EqBand* band_ = nullptr;
    int bandIndex_ = -1;
    Listener* listener_ = nullptr;

    ComboBox cbBandType_;       // IIR, FIR, Gain, Delay
    ComboBox cbIIRSubType_;     // low_pass, high_pass, etc.
    ComboBox cbOrder_;          // Butterworth order (1-8)

    Slider sldFreq_;
    Slider sldQ_;
    Slider sldGain_;
    Slider sldLinearGain_;
    ToggleButton btnGainLinear_ { "Linear" };
    ToggleButton btnInvertGain_ { "Invert" };
    Slider sldDelay_;

    Label lblType_    { {}, "Type:" };
    Label lblSubType_ { {}, "Filter:" };
    Label lblFreq_    { {}, "Freq:" };
    Label lblQ_       { {}, "Q:" };
    Label lblOrder_   { {}, "Order:" };
    Label lblGain_    { {}, "Gain:" };
    Label lblDelay_   { {}, "Delay:" };
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

    TextButton btnLoadFIR_ { "Load FIR..." };
    Label lblFIRInfo_ { {}, "" };
    FIRPlot firPlot_;

    bool updating_ = false; // prevent feedback loops

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqBandEditor)
};

#endif // EQBANDEDITOR_H_INCLUDED
