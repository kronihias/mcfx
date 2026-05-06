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

#ifndef FIRDESIGNDIALOG_H_INCLUDED
#define FIRDESIGNDIALOG_H_INCLUDED

#include "JuceHeader.h"
#include "EqBand.h"
#include "EqBandEditor.h"            // for FIRPlot
#include "LinearPhaseFIRDesigner.h"

#include <functional>
#include <memory>

//==============================================================================
/** Modal dialog for designing a linear-phase FIR filter from biquad-style
    parameters (LP/HP/BP/Notch/Peak/Shelf, plus Butterworth LP/HP for steeper
    slopes). Provides live previews of the impulse response and realised
    magnitude curve, then on OK calls back with the designed coefficients. */
class FIRDesignDialog : public Component,
                         public Slider::Listener,
                         public ComboBox::Listener,
                         public Button::Listener
{
public:
    using ApplyCallback = std::function<void (const std::vector<float>& coeffs,
                                                double sampleRate,
                                                const var& designerState)>;

    FIRDesignDialog (double sampleRate,
                      const var& initialState,
                      ApplyCallback onApply);
    ~FIRDesignDialog() override;

    void paint (Graphics& g) override;
    void resized() override;
    void sliderValueChanged (Slider*) override;
    void comboBoxChanged (ComboBox*) override;
    void buttonClicked (Button*) override;

    /** Convenience: launch as a modal DialogWindow. Pass the band's stored
        designer state as initialState (or {} for defaults). */
    static void launch (double sampleRate,
                         const var& initialState,
                         ApplyCallback onApply);

    /** Snapshot the current parameters as a var that round-trips via JSON. */
    var captureState() const;

private:
    //==============================================================================
    /** Tiny magnitude-response viewer — draws a log-frequency curve in dB.
        Mouse-wheel zooms the dB axis around the cursor; vertical drag pans;
        double-click resets to default range. */
    class MagPreview : public Component
    {
    public:
        void setCoefficients (const std::vector<float>& coeffs, double sampleRate);
        void paint (Graphics&) override;
        void mouseWheelMove   (const MouseEvent&, const MouseWheelDetails&) override;
        void mouseDown        (const MouseEvent&) override;
        void mouseDrag        (const MouseEvent&) override;
        void mouseDoubleClick (const MouseEvent&) override;
    private:
        void updateGridStep();
        float yToDb (int y) const;

        std::vector<float> coeffs_;
        double sampleRate_ = 48000.0;
        float  dbMin_  = -72.f;
        float  dbMax_  =  24.f;
        float  dbStep_ =  12.f;
        // Pan-drag bookkeeping
        float  dragStartMinDb_ = 0.f;
        float  dragStartMaxDb_ = 0.f;
    };

    void rebuildBandFromUI();
    void recomputeFIR();
    void updateLatencyLabel();
    void showHideControls();
    void applyInitialState (const var& s);

    double sampleRate_;
    ApplyCallback onApply_;

    EqBand tempBand_;                   // magnitude template — same code path as the live IIR types
    std::vector<float> currentCoeffs_;

    // Filter parameters
    ComboBox cbType_;
    Slider sldFreq_;
    Slider sldQ_;
    Slider sldGain_;
    ComboBox cbOrder_;
    Label lblType_  { {}, "Type:" };
    Label lblFreq_  { {}, "Freq:" };
    Label lblQ_     { {}, "Q:" };
    Label lblGain_  { {}, "Gain:" };
    Label lblOrder_ { {}, "Order:" };

    // FIR parameters
    ComboBox cbLength_;
    ComboBox cbWindow_;
    Slider sldKaiserBeta_;
    Label lblLength_     { {}, "Length:" };
    Label lblWindow_     { {}, "Window:" };
    Label lblKaiserBeta_ { {}, "Beta:" };
    Label lblLatency_;

    // Preview
    FIRPlot impulsePlot_;
    MagPreview magPlot_;
    Label lblImpulse_  { {}, "Impulse response" };
    Label lblMag_      { {}, "Magnitude (realised)" };

    // Buttons
    TextButton btnApply_  { "Apply" };
    TextButton btnClose_  { "Close" };

    bool updating_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FIRDesignDialog)
};

#endif // FIRDESIGNDIALOG_H_INCLUDED
