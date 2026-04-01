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

class EqBandEditor : public Component,
                     public Slider::Listener,
                     public ComboBox::Listener,
                     public Button::Listener
{
public:

    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void bandParameterChanged(int bandIndex) = 0;
        virtual void bandEnableChanged(int bandIndex, bool enabled) = 0;
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

    TextButton btnLoadFIR_ { "Load FIR..." };
    Label lblFIRInfo_ { {}, "" };

    bool updating_ = false; // prevent feedback loops

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqBandEditor)
};

#endif // EQBANDEDITOR_H_INCLUDED
