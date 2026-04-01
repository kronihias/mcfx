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

#include "EqBandEditor.h"

EqBandEditor::EqBandEditor()
{
    // Band type combo
    addAndMakeVisible(lblType_);
    addAndMakeVisible(cbBandType_);
    cbBandType_.addItem("IIR", 1);
    cbBandType_.addItem("FIR", 2);
    cbBandType_.addItem("Gain", 3);
    cbBandType_.addItem("Delay", 4);
    cbBandType_.addListener(this);

    // IIR subtype combo
    addAndMakeVisible(lblSubType_);
    addAndMakeVisible(cbIIRSubType_);
    cbIIRSubType_.addItem("Low Pass", 1);
    cbIIRSubType_.addItem("High Pass", 2);
    cbIIRSubType_.addItem("Band Pass", 3);
    cbIIRSubType_.addItem("Notch", 4);
    cbIIRSubType_.addItem("All Pass", 5);
    cbIIRSubType_.addItem("Low Shelf", 6);
    cbIIRSubType_.addItem("High Shelf", 7);
    cbIIRSubType_.addItem("Peak", 8);
    cbIIRSubType_.addListener(this);

    // Frequency slider
    addAndMakeVisible(lblFreq_);
    addAndMakeVisible(sldFreq_);
    addAndMakeVisible(lblHz_);
    sldFreq_.setRange(20.0, 20000.0, 0.1);
    sldFreq_.setSkewFactorFromMidPoint(1000.0);
    sldFreq_.setNumDecimalPlacesToDisplay(1);
    sldFreq_.setTextBoxStyle(Slider::TextBoxLeft, false, 80, 20);
    sldFreq_.setSliderStyle(Slider::LinearHorizontal);
    sldFreq_.addListener(this);

    // Q slider
    addAndMakeVisible(lblQ_);
    addAndMakeVisible(sldQ_);
    sldQ_.setRange(0.1, 20.0, 0.001);
    sldQ_.setSkewFactorFromMidPoint(1.0);
    sldQ_.setNumDecimalPlacesToDisplay(3);
    sldQ_.setTextBoxStyle(Slider::TextBoxLeft, false, 70, 20);
    sldQ_.setSliderStyle(Slider::LinearHorizontal);
    sldQ_.addListener(this);

    // Gain slider
    addAndMakeVisible(lblGain_);
    addAndMakeVisible(sldGain_);
    addAndMakeVisible(lblDb_);
    sldGain_.setRange(-24.0, 24.0, 0.1);
    sldGain_.setNumDecimalPlacesToDisplay(1);
    sldGain_.setTextBoxStyle(Slider::TextBoxLeft, false, 70, 20);
    sldGain_.setSliderStyle(Slider::LinearHorizontal);
    sldGain_.setDoubleClickReturnValue(true, 0.0);
    sldGain_.addListener(this);

    // Delay slider
    addAndMakeVisible(lblDelay_);
    addAndMakeVisible(sldDelay_);
    addAndMakeVisible(lblSamples_);
    sldDelay_.setRange(0, 10000, 1);
    sldDelay_.setTextBoxStyle(Slider::TextBoxLeft, false, 60, 20);
    sldDelay_.setSliderStyle(Slider::LinearHorizontal);
    sldDelay_.addListener(this);

    // Enable toggle
    addAndMakeVisible(btnEnable_);
    btnEnable_.addListener(this);

    // FIR load button
    addAndMakeVisible(btnLoadFIR_);
    btnLoadFIR_.addListener(this);
    addAndMakeVisible(lblFIRInfo_);

    // Set label styles
    for (auto* lbl : { &lblType_, &lblSubType_, &lblFreq_, &lblQ_, &lblGain_, &lblDelay_, &lblHz_, &lblDb_, &lblSamples_, &lblFIRInfo_ })
    {
        lbl->setFont(Font(13.f, Font::plain));
        lbl->setColour(Label::textColourId, Colours::white);
    }
}

EqBandEditor::~EqBandEditor()
{
}

void EqBandEditor::setBand(EqBand* band, int bandIndex)
{
    band_ = band;
    bandIndex_ = bandIndex;
    updateFromBand();
}

void EqBandEditor::updateFromBand()
{
    if (band_ == nullptr)
        return;

    updating_ = true;

    auto type = band_->getType();
    switch (type)
    {
        case EqBandType::IIR:   cbBandType_.setSelectedId(1, dontSendNotification); break;
        case EqBandType::FIR:   cbBandType_.setSelectedId(2, dontSendNotification); break;
        case EqBandType::Gain:  cbBandType_.setSelectedId(3, dontSendNotification); break;
        case EqBandType::Delay: cbBandType_.setSelectedId(4, dontSendNotification); break;
    }

    if (type == EqBandType::IIR)
    {
        switch (band_->getIIRSubType())
        {
            case IIRSubType::LowPass:   cbIIRSubType_.setSelectedId(1, dontSendNotification); break;
            case IIRSubType::HighPass:   cbIIRSubType_.setSelectedId(2, dontSendNotification); break;
            case IIRSubType::BandPass:   cbIIRSubType_.setSelectedId(3, dontSendNotification); break;
            case IIRSubType::Notch:      cbIIRSubType_.setSelectedId(4, dontSendNotification); break;
            case IIRSubType::AllPass:    cbIIRSubType_.setSelectedId(5, dontSendNotification); break;
            case IIRSubType::LowShelf:   cbIIRSubType_.setSelectedId(6, dontSendNotification); break;
            case IIRSubType::HighShelf:  cbIIRSubType_.setSelectedId(7, dontSendNotification); break;
            case IIRSubType::Peak:       cbIIRSubType_.setSelectedId(8, dontSendNotification); break;
        }
        sldFreq_.setValue(band_->getFrequency(), dontSendNotification);
        sldQ_.setValue(band_->getQ(), dontSendNotification);
        sldGain_.setValue(band_->getGainDB(), dontSendNotification);
    }
    else if (type == EqBandType::Gain)
    {
        sldGain_.setValue(band_->getGainDB(), dontSendNotification);
    }
    else if (type == EqBandType::Delay)
    {
        sldDelay_.setValue(band_->getDelaySamples(), dontSendNotification);
    }
    else if (type == EqBandType::FIR)
    {
        String info;
        info << band_->getFIRCoefficients().size() << " taps";
        lblFIRInfo_.setText(info, dontSendNotification);
    }

    btnEnable_.setToggleState(band_->isEnabled(), dontSendNotification);

    showControlsForType(type);

    updating_ = false;
}

void EqBandEditor::showControlsForType(EqBandType type)
{
    bool isIIR = (type == EqBandType::IIR);
    bool isFIR = (type == EqBandType::FIR);
    bool isGain = (type == EqBandType::Gain);
    bool isDelay = (type == EqBandType::Delay);

    // Some IIR subtypes don't use gain (BandPass, Notch, AllPass)
    bool iirUsesGain = isIIR && band_ != nullptr
        && band_->getIIRSubType() != IIRSubType::BandPass
        && band_->getIIRSubType() != IIRSubType::Notch
        && band_->getIIRSubType() != IIRSubType::AllPass
        && band_->getIIRSubType() != IIRSubType::LowPass
        && band_->getIIRSubType() != IIRSubType::HighPass;

    lblSubType_.setVisible(isIIR);
    cbIIRSubType_.setVisible(isIIR);
    lblFreq_.setVisible(isIIR);
    sldFreq_.setVisible(isIIR);
    lblHz_.setVisible(isIIR);
    lblQ_.setVisible(isIIR);
    sldQ_.setVisible(isIIR);

    lblGain_.setVisible(iirUsesGain || isGain);
    sldGain_.setVisible(iirUsesGain || isGain);
    lblDb_.setVisible(iirUsesGain || isGain);

    lblDelay_.setVisible(isDelay);
    sldDelay_.setVisible(isDelay);
    lblSamples_.setVisible(isDelay);

    btnLoadFIR_.setVisible(isFIR);
    lblFIRInfo_.setVisible(isFIR);
}

void EqBandEditor::resized()
{
    int y = 4;
    int rowH = 24;
    int lblW = 45;
    int x = 4;
    int w = getWidth() - 8;

    // Enable + Type on first row
    btnEnable_.setBounds(x, y, 70, rowH);
    lblType_.setBounds(x + 75, y, lblW, rowH);
    cbBandType_.setBounds(x + 120, y, 90, rowH);
    y += rowH + 4;

    // IIR subtype
    lblSubType_.setBounds(x, y, lblW, rowH);
    cbIIRSubType_.setBounds(x + lblW + 4, y, 100, rowH);
    y += rowH + 4;

    // Frequency
    lblFreq_.setBounds(x, y, lblW, rowH);
    sldFreq_.setBounds(x + lblW + 4, y, w - lblW - 40, rowH);
    lblHz_.setBounds(getWidth() - 35, y, 30, rowH);
    y += rowH + 4;

    // Q
    lblQ_.setBounds(x, y, lblW, rowH);
    sldQ_.setBounds(x + lblW + 4, y, w - lblW - 10, rowH);
    y += rowH + 4;

    // Gain
    lblGain_.setBounds(x, y, lblW, rowH);
    sldGain_.setBounds(x + lblW + 4, y, w - lblW - 35, rowH);
    lblDb_.setBounds(getWidth() - 30, y, 25, rowH);
    y += rowH + 4;

    // Delay
    lblDelay_.setBounds(x, y, lblW, rowH);
    sldDelay_.setBounds(x + lblW + 4, y, w - lblW - 50, rowH);
    lblSamples_.setBounds(getWidth() - 45, y, 40, rowH);

    // FIR
    btnLoadFIR_.setBounds(x + lblW + 4, y, 100, rowH);
    lblFIRInfo_.setBounds(x + lblW + 110, y, 100, rowH);
}

void EqBandEditor::sliderValueChanged(Slider* s)
{
    if (updating_ || band_ == nullptr)
        return;

    if (s == &sldFreq_)
        band_->setFrequency((float)sldFreq_.getValue());
    else if (s == &sldQ_)
        band_->setQ((float)sldQ_.getValue());
    else if (s == &sldGain_)
        band_->setGainDB((float)sldGain_.getValue());
    else if (s == &sldDelay_)
        band_->setDelaySamples((int)sldDelay_.getValue());

    if (listener_ != nullptr)
        listener_->bandParameterChanged(bandIndex_);
}

void EqBandEditor::comboBoxChanged(ComboBox* cb)
{
    if (updating_ || band_ == nullptr)
        return;

    if (cb == &cbBandType_)
    {
        int sel = cbBandType_.getSelectedId();
        switch (sel)
        {
            case 1: band_->setType(EqBandType::IIR); break;
            case 2: band_->setType(EqBandType::FIR); break;
            case 3: band_->setType(EqBandType::Gain); break;
            case 4: band_->setType(EqBandType::Delay); break;
        }
        updateFromBand();
    }
    else if (cb == &cbIIRSubType_)
    {
        int sel = cbIIRSubType_.getSelectedId();
        switch (sel)
        {
            case 1: band_->setIIRSubType(IIRSubType::LowPass); break;
            case 2: band_->setIIRSubType(IIRSubType::HighPass); break;
            case 3: band_->setIIRSubType(IIRSubType::BandPass); break;
            case 4: band_->setIIRSubType(IIRSubType::Notch); break;
            case 5: band_->setIIRSubType(IIRSubType::AllPass); break;
            case 6: band_->setIIRSubType(IIRSubType::LowShelf); break;
            case 7: band_->setIIRSubType(IIRSubType::HighShelf); break;
            case 8: band_->setIIRSubType(IIRSubType::Peak); break;
        }
        showControlsForType(band_->getType());
    }

    if (listener_ != nullptr)
        listener_->bandParameterChanged(bandIndex_);
}

void EqBandEditor::buttonClicked(Button* b)
{
    if (updating_ || band_ == nullptr)
        return;

    if (b == &btnEnable_)
    {
        band_->setEnabled(btnEnable_.getToggleState());
        if (listener_ != nullptr)
            listener_->bandEnableChanged(bandIndex_, band_->isEnabled());
    }
    else if (b == &btnLoadFIR_)
    {
        FileChooser chooser("Load FIR coefficients", File(), "*.txt;*.csv;*.dat");
        if (chooser.browseForFileToOpen())
        {
            auto file = chooser.getResult();
            StringArray lines;
            file.readLines(lines);
            std::vector<float> coeffs;
            for (auto& line : lines)
            {
                line = line.trim();
                if (line.isNotEmpty() && !line.startsWith("#") && !line.startsWith("//"))
                {
                    // Handle comma-separated or space-separated values
                    StringArray tokens;
                    tokens.addTokens(line, ",; \t", "");
                    for (auto& tok : tokens)
                    {
                        tok = tok.trim();
                        if (tok.isNotEmpty())
                            coeffs.push_back(tok.getFloatValue());
                    }
                }
            }
            if (!coeffs.empty())
            {
                band_->setFIRCoefficients(coeffs);
                String info;
                info << coeffs.size() << " taps";
                lblFIRInfo_.setText(info, dontSendNotification);
                if (listener_ != nullptr)
                    listener_->bandParameterChanged(bandIndex_);
            }
        }
    }
}
