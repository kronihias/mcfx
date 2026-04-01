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
#include "EqGraph.h"

EqBandEditor::EqBandEditor()
{
    // Band type combo
    addAndMakeVisible(lblType_);
    addAndMakeVisible(cbBandType_);
    cbBandType_.addItem("IIR", 1);
    cbBandType_.addItem("FIR", 2);
    cbBandType_.addItem("Gain", 3);
    cbBandType_.addItem("Delay", 4);
    cbBandType_.setTooltip("Band type: IIR filter, FIR filter, gain, or delay");
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
    cbIIRSubType_.addItem("Butterworth LP", 9);
    cbIIRSubType_.addItem("Butterworth HP", 10);
    cbIIRSubType_.addItem("Crossover LP", 11);
    cbIIRSubType_.addItem("Crossover HP", 12);
    cbIIRSubType_.addItem("Crossover AP", 13);
    cbIIRSubType_.setTooltip("IIR filter type");
    cbIIRSubType_.addListener(this);

    // Order combo (populated dynamically for Butterworth vs Crossover)
    addAndMakeVisible(lblOrder_);
    addAndMakeVisible(cbOrder_);
    populateOrderCombo(false); // default: Butterworth items
    cbOrder_.addListener(this);

    // Frequency slider
    addAndMakeVisible(lblFreq_);
    addAndMakeVisible(sldFreq_);
    addAndMakeVisible(lblHz_);
    sldFreq_.setRange(20.0, 20000.0, 0.1);
    sldFreq_.setSkewFactorFromMidPoint(1000.0);
    sldFreq_.setNumDecimalPlacesToDisplay(1);
    sldFreq_.setTextBoxStyle(Slider::TextBoxLeft, false, 80, 20);
    sldFreq_.setSliderStyle(Slider::LinearHorizontal);
    sldFreq_.setTooltip("Filter cutoff/center frequency");
    sldFreq_.addListener(this);

    // Q slider
    addAndMakeVisible(lblQ_);
    addAndMakeVisible(sldQ_);
    sldQ_.setRange(0.1, 20.0, 0.001);
    sldQ_.setSkewFactorFromMidPoint(1.0);
    sldQ_.setNumDecimalPlacesToDisplay(3);
    sldQ_.setTextBoxStyle(Slider::TextBoxLeft, false, 70, 20);
    sldQ_.setSliderStyle(Slider::LinearHorizontal);
    sldQ_.setTooltip("Filter Q (bandwidth). Use mouse wheel on graph handles to adjust");
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
    sldGain_.setTooltip("Gain in dB. Double-click to reset to 0 dB");
    sldGain_.addListener(this);

    // Delay slider
    addAndMakeVisible(lblDelay_);
    addAndMakeVisible(sldDelay_);
    addAndMakeVisible(lblSamples_);
    sldDelay_.setRange(0, 10000, 1);
    sldDelay_.setTextBoxStyle(Slider::TextBoxLeft, false, 60, 20);
    sldDelay_.setSliderStyle(Slider::LinearHorizontal);
    sldDelay_.setTooltip("Delay in samples");
    sldDelay_.addListener(this);

    // Enable toggle
    addAndMakeVisible(btnEnable_);
    btnEnable_.setTooltip("Enable/disable this band (shortcut: E in graph)");
    btnEnable_.addListener(this);

    // FIR load button and plot
    addAndMakeVisible(btnLoadFIR_);
    btnLoadFIR_.setTooltip("Load FIR coefficients from a text file");
    btnLoadFIR_.addListener(this);
    addAndMakeVisible(lblFIRInfo_);
    addAndMakeVisible(firPlot_);

    // Set label styles
    for (auto* lbl : { &lblType_, &lblSubType_, &lblFreq_, &lblQ_, &lblOrder_, &lblGain_, &lblDelay_, &lblHz_, &lblDb_, &lblSamples_, &lblFIRInfo_ })
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
    repaint(); // redraw band color indicator
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
            case IIRSubType::Peak:          cbIIRSubType_.setSelectedId(8, dontSendNotification); break;
            case IIRSubType::ButterworthLP: cbIIRSubType_.setSelectedId(9, dontSendNotification); break;
            case IIRSubType::ButterworthHP: cbIIRSubType_.setSelectedId(10, dontSendNotification); break;
            case IIRSubType::CrossoverLP:   cbIIRSubType_.setSelectedId(11, dontSendNotification); break;
            case IIRSubType::CrossoverHP:   cbIIRSubType_.setSelectedId(12, dontSendNotification); break;
            case IIRSubType::CrossoverAP:   cbIIRSubType_.setSelectedId(13, dontSendNotification); break;
        }
        sldFreq_.setValue(band_->getFrequency(), dontSendNotification);
        sldQ_.setValue(band_->getQ(), dontSendNotification);
        sldGain_.setValue(band_->getGainDB(), dontSendNotification);

        bool isCrossover = band_->getIIRSubType() == IIRSubType::CrossoverLP
                        || band_->getIIRSubType() == IIRSubType::CrossoverHP
                        || band_->getIIRSubType() == IIRSubType::CrossoverAP;
        bool isBW = band_->getIIRSubType() == IIRSubType::ButterworthLP
                 || band_->getIIRSubType() == IIRSubType::ButterworthHP;
        if (isCrossover)
        {
            bool isAP = band_->getIIRSubType() == IIRSubType::CrossoverAP;
            populateOrderCombo(true, isAP);
            cbOrder_.setSelectedId(band_->getCrossoverOrder(), dontSendNotification);
        }
        else if (isBW)
        {
            populateOrderCombo(false);
            cbOrder_.setSelectedId(band_->getButterworthOrder(), dontSendNotification);
        }
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
        auto& coeffs = band_->getFIRCoefficients();
        String info;
        info << coeffs.size() << " taps";
        lblFIRInfo_.setText(info, dontSendNotification);
        firPlot_.setCoefficients(coeffs);
    }

    btnEnable_.setToggleState(band_->isEnabled(), dontSendNotification);

    showControlsForType(type);

    updating_ = false;
}

void EqBandEditor::populateOrderCombo(bool isCrossover, bool isAP)
{
    cbOrder_.clear(dontSendNotification);
    if (isCrossover)
    {
        if (!isAP)
            cbOrder_.addItem("12 dB/oct (LR2)", 2);
        cbOrder_.addItem("24 dB/oct (LR4)", 4);
        if (!isAP)
            cbOrder_.addItem("36 dB/oct (LR6)", 6);
        cbOrder_.addItem("48 dB/oct (LR8)", 8);
        cbOrder_.addItem("96 dB/oct (LR16)", 16);
        cbOrder_.setTooltip(isAP ? "Linkwitz-Riley allpass compensation order (LR4, LR8, LR16)."
                                 : "Linkwitz-Riley crossover order. LP+HP at same freq/order will sum flat.");
    }
    else
    {
        cbOrder_.addItem("6 dB/oct (1st)", 1);
        cbOrder_.addItem("12 dB/oct (2nd)", 2);
        cbOrder_.addItem("18 dB/oct (3rd)", 3);
        cbOrder_.addItem("24 dB/oct (4th)", 4);
        cbOrder_.addItem("30 dB/oct (5th)", 5);
        cbOrder_.addItem("36 dB/oct (6th)", 6);
        cbOrder_.addItem("42 dB/oct (7th)", 7);
        cbOrder_.addItem("48 dB/oct (8th)", 8);
        cbOrder_.setTooltip("Butterworth filter order (slope)");
    }
}

void EqBandEditor::showControlsForType(EqBandType type)
{
    bool isIIR = (type == EqBandType::IIR);
    bool isFIR = (type == EqBandType::FIR);
    bool isGain = (type == EqBandType::Gain);
    bool isDelay = (type == EqBandType::Delay);

    // Butterworth and Crossover types use order combo, hide Q and gain
    bool isButterworth = isIIR && band_ != nullptr
        && (band_->getIIRSubType() == IIRSubType::ButterworthLP
            || band_->getIIRSubType() == IIRSubType::ButterworthHP);

    bool isCrossover = isIIR && band_ != nullptr
        && (band_->getIIRSubType() == IIRSubType::CrossoverLP
            || band_->getIIRSubType() == IIRSubType::CrossoverHP
            || band_->getIIRSubType() == IIRSubType::CrossoverAP);

    bool hasOrder = isButterworth || isCrossover;

    bool iirUsesGain = isIIR && band_ != nullptr
        && band_->getIIRSubType() != IIRSubType::BandPass
        && band_->getIIRSubType() != IIRSubType::Notch
        && band_->getIIRSubType() != IIRSubType::AllPass
        && band_->getIIRSubType() != IIRSubType::LowPass
        && band_->getIIRSubType() != IIRSubType::HighPass
        && !hasOrder;

    lblSubType_.setVisible(isIIR);
    cbIIRSubType_.setVisible(isIIR);
    lblFreq_.setVisible(isIIR);
    sldFreq_.setVisible(isIIR);
    lblHz_.setVisible(isIIR);
    lblQ_.setVisible(isIIR && !hasOrder);
    sldQ_.setVisible(isIIR && !hasOrder);

    lblOrder_.setVisible(hasOrder);
    cbOrder_.setVisible(hasOrder);

    lblGain_.setVisible(iirUsesGain || isGain);
    sldGain_.setVisible(iirUsesGain || isGain);
    lblDb_.setVisible(iirUsesGain || isGain);

    lblDelay_.setVisible(isDelay);
    sldDelay_.setVisible(isDelay);
    lblSamples_.setVisible(isDelay);

    btnLoadFIR_.setVisible(isFIR);
    lblFIRInfo_.setVisible(isFIR);
    firPlot_.setVisible(isFIR);

    resized(); // re-layout based on new visibility
}

void EqBandEditor::paint(Graphics& g)
{
    if (bandIndex_ >= 0)
    {
        // Draw band color indicator on the right side
        float indicatorSize = 40.f;
        float margin = 10.f;
        float cx = getWidth() - margin - indicatorSize / 2.f;
        float cy = 4.f + indicatorSize / 2.f; // aligned with first row

        Colour bandCol = EqGraph::getBandColour(bandIndex_);
        g.setColour(bandCol);
        g.fillRoundedRectangle(cx - indicatorSize / 2.f, cy - indicatorSize / 2.f,
                               indicatorSize, indicatorSize, 8.f);

        // Band number
        g.setColour(bandCol.getPerceivedBrightness() > 0.5f ? Colour(0xff1a1a1a) : Colours::white);
        g.setFont(Font(indicatorSize * 0.5f, Font::bold));
        g.drawText(String(bandIndex_ + 1),
                   (int)(cx - indicatorSize / 2.f), (int)(cy - indicatorSize / 2.f),
                   (int)indicatorSize, (int)indicatorSize,
                   Justification::centred, false);
    }
}

void EqBandEditor::resized()
{
    int y = 4;
    int rowH = 24;
    int gap = 4;
    int lblW = 45;
    int x = 4;
    int w = getWidth() - 8;

    // Enable + Type on first row
    btnEnable_.setBounds(x, y, 70, rowH);
    lblType_.setBounds(x + 75, y, lblW, rowH);
    cbBandType_.setBounds(x + 120, y, 90, rowH);
    y += rowH + gap;

    // IIR subtype (only advances y if visible)
    lblSubType_.setBounds(x, y, lblW, rowH);
    cbIIRSubType_.setBounds(x + lblW + 4, y, 130, rowH);
    if (cbIIRSubType_.isVisible()) y += rowH + gap;

    // Butterworth order
    lblOrder_.setBounds(x, y, lblW, rowH);
    cbOrder_.setBounds(x + lblW + 4, y, 140, rowH);
    if (cbOrder_.isVisible()) y += rowH + gap;

    // Frequency
    lblFreq_.setBounds(x, y, lblW, rowH);
    sldFreq_.setBounds(x + lblW + 4, y, w - lblW - 40, rowH);
    lblHz_.setBounds(getWidth() - 35, y, 30, rowH);
    if (sldFreq_.isVisible()) y += rowH + gap;

    // Q
    lblQ_.setBounds(x, y, lblW, rowH);
    sldQ_.setBounds(x + lblW + 4, y, w - lblW - 10, rowH);
    if (sldQ_.isVisible()) y += rowH + gap;

    // Gain
    lblGain_.setBounds(x, y, lblW, rowH);
    sldGain_.setBounds(x + lblW + 4, y, w - lblW - 35, rowH);
    lblDb_.setBounds(getWidth() - 30, y, 25, rowH);
    if (sldGain_.isVisible()) y += rowH + gap;

    // Delay
    lblDelay_.setBounds(x, y, lblW, rowH);
    sldDelay_.setBounds(x + lblW + 4, y, w - lblW - 50, rowH);
    lblSamples_.setBounds(getWidth() - 45, y, 40, rowH);

    // FIR (shares same y as delay since they're mutually exclusive)
    btnLoadFIR_.setBounds(x + lblW + 4, y, 100, rowH);
    lblFIRInfo_.setBounds(x + lblW + 110, y, 100, rowH);
    if (btnLoadFIR_.isVisible()) y += rowH + gap;

    // FIR impulse response plot — fills remaining space
    if (firPlot_.isVisible())
    {
        int plotH = jmax(60, getHeight() - y - 4);
        firPlot_.setBounds(x, y, w, plotH);
    }
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

    bool isStructuralChange = false;

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
        isStructuralChange = true;
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
            case 9: band_->setIIRSubType(IIRSubType::ButterworthLP); break;
            case 10: band_->setIIRSubType(IIRSubType::ButterworthHP); break;
            case 11: band_->setIIRSubType(IIRSubType::CrossoverLP); break;
            case 12: band_->setIIRSubType(IIRSubType::CrossoverHP); break;
            case 13: band_->setIIRSubType(IIRSubType::CrossoverAP); break;
        }

        // Repopulate order combo when switching between Butterworth and Crossover
        auto st = band_->getIIRSubType();
        bool isCrossover = st == IIRSubType::CrossoverLP || st == IIRSubType::CrossoverHP
                        || st == IIRSubType::CrossoverAP;
        bool isBW = st == IIRSubType::ButterworthLP || st == IIRSubType::ButterworthHP;
        if (isCrossover)
        {
            bool isAP = st == IIRSubType::CrossoverAP;
            populateOrderCombo(true, isAP);
            int curOrder = band_->getCrossoverOrder();
            if (isAP && (curOrder == 2 || curOrder == 6))
                band_->setCrossoverOrder(4);
            cbOrder_.setSelectedId(band_->getCrossoverOrder(), dontSendNotification);
        }
        else if (isBW)
        {
            populateOrderCombo(false);
            cbOrder_.setSelectedId(band_->getButterworthOrder(), dontSendNotification);
        }

        showControlsForType(band_->getType());
        isStructuralChange = true;
    }
    else if (cb == &cbOrder_)
    {
        auto st = band_->getIIRSubType();
        if (st == IIRSubType::CrossoverLP || st == IIRSubType::CrossoverHP
            || st == IIRSubType::CrossoverAP)
            band_->setCrossoverOrder(cbOrder_.getSelectedId());
        else
            band_->setButterworthOrder(cbOrder_.getSelectedId());
        isStructuralChange = true; // order changes cascade section count
    }

    if (listener_ != nullptr)
    {
        if (isStructuralChange)
            listener_->bandStructureChanged(bandIndex_);
        else
            listener_->bandParameterChanged(bandIndex_);
    }
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
                firPlot_.setCoefficients(coeffs);
                if (listener_ != nullptr)
                    listener_->bandParameterChanged(bandIndex_);
            }
        }
    }
}
