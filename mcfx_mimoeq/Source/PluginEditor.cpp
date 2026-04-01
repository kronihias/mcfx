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

#include "PluginEditor.h"

#define Q(x) #x
#define QUOTE(x) Q(x)

void EqTabBar::currentTabChanged(int newIndex, const String&)
{
    if (owner_ != nullptr && newIndex >= 0 && !owner_->refreshingTabs_)
        owner_->selectBand(newIndex);
}

Mcfx_mimoeqAudioProcessorEditor::Mcfx_mimoeqAudioProcessorEditor(Mcfx_mimoeqAudioProcessor* processor)
    : AudioProcessorEditor(processor),
      tabs_(this)
{
    setLookAndFeel(&lookAndFeel_);

    addAndMakeVisible(statusBar_);
    statusBar_.setFont(Font(11.f, Font::plain));
    statusBar_.setColour(Label::textColourId, Colours::white.withAlpha(0.8f));
    statusBar_.setColour(Label::backgroundColourId, Colour(0xff111111));
    statusBar_.setJustificationType(Justification::centredLeft);
    statusBar_.setBorderSize(BorderSize<int>(2, 6, 2, 6));
    statusBar_.setMinimumHorizontalScale(1.0f);

    addAndMakeVisible(lblTitle_);
    lblTitle_.setText("mcfx_mimoeq", dontSendNotification);
    lblTitle_.setFont(Font(15.f, Font::plain));
    lblTitle_.setColour(Label::textColourId, Colours::aquamarine);

    // Path selector
    addAndMakeVisible(btnDiagonal_);
    btnDiagonal_.setToggleState(true, dontSendNotification);
    btnDiagonal_.addListener(this);
    btnDiagonal_.setTooltip("Diagonal: apply the same EQ chain to all channels independently");

    addAndMakeVisible(lblInput_);
    addAndMakeVisible(lblOutput_);
    addAndMakeVisible(lblArrow_);
    lblInput_.setColour(Label::textColourId, Colours::white);
    lblOutput_.setColour(Label::textColourId, Colours::white);
    lblArrow_.setColour(Label::textColourId, Colours::white);
    lblArrow_.setFont(Font(16.f, Font::plain));

    addAndMakeVisible(cbInputCh_);
    addAndMakeVisible(cbOutputCh_);
    cbInputCh_.addListener(this);
    cbOutputCh_.addListener(this);
    cbInputCh_.setTooltip("Input channel for this EQ path");
    cbOutputCh_.setTooltip("Output channel for this EQ path");

    int numCh = jmax(1, processor->getNumChannels_());
    for (int i = 1; i <= numCh; ++i)
    {
        cbInputCh_.addItem(String(i), i);
        cbOutputCh_.addItem(String(i), i);
    }
    cbInputCh_.setSelectedId(1, dontSendNotification);
    cbOutputCh_.setSelectedId(1, dontSendNotification);

    updatePathSelector();

    addAndMakeVisible(graph_);
    graph_.setListener(this);
    graph_.setTooltip("Drag handles to adjust frequency/gain. Mouse wheel to adjust Q.\nDouble-click to add a band. Double-click a handle to toggle enable. Press E to toggle enable.\nDrop a .json file to load configuration.");

    addAndMakeVisible(bandEditor_);
    bandEditor_.setListener(this);

    addAndMakeVisible(tabs_);
    tabs_.setLookAndFeel(&tabLookAndFeel_);

    addAndMakeVisible(btnAdd_);
    btnAdd_.addListener(this);
    btnAdd_.setTooltip("Add a new EQ band");
    addAndMakeVisible(btnRemove_);
    btnRemove_.addListener(this);
    btnRemove_.setTooltip("Remove the selected EQ band");
    addAndMakeVisible(btnLoad_);
    btnLoad_.addListener(this);
    btnLoad_.setTooltip("Load EQ configuration from JSON file");
    addAndMakeVisible(btnSave_);
    btnSave_.addListener(this);
    btnSave_.setTooltip("Save EQ configuration to JSON file");

    processor->addChangeListener(this);

    // Point graph at the active chain
    auto* chain = getActiveChain();
    graph_.setChain(chain);
    refreshTabs();
    if (chain != nullptr && chain->getNumBands() > 0)
        selectBand(0);

    // Listen for mouse enter/exit on all child components for status bar
    addMouseListener(this, true);

    setResizable(true, true);
    setResizeLimits(450, 400, 1200, 800);
    setSize(650, 500);
}

Mcfx_mimoeqAudioProcessorEditor::~Mcfx_mimoeqAudioProcessorEditor()
{
    getProcessor()->removeChangeListener(this);
    tabs_.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void Mcfx_mimoeqAudioProcessorEditor::paint(Graphics& g)
{
    g.fillAll(Colour(0xff1a1a1a));
    g.setGradientFill(ColourGradient(Colour(0xff4e4e4e),
                                      proportionOfWidth(0.6f), proportionOfHeight(0.7f),
                                      Colours::black,
                                      proportionOfWidth(0.1f), proportionOfHeight(0.1f),
                                      true));
    g.fillRect(getLocalBounds());

    // Version drawn just above the status bar
    int statusH = 42;
    g.setColour(Colours::white.withAlpha(0.35f));
    g.setFont(Font(10.f, Font::plain));
    String version;
    version << "v" << QUOTE(VERSION);
    g.drawText(version, getWidth() - 55, getHeight() - statusH - 13, 50, 12, Justification::centredRight, true);
}

void Mcfx_mimoeqAudioProcessorEditor::resized()
{
    int w = getWidth();
    int h = getHeight();

    lblTitle_.setBounds(4, 2, 150, 20);
    btnLoad_.setBounds(w - 120, 2, 55, 20);
    btnSave_.setBounds(w - 60, 2, 55, 20);

    // Path selector row
    int pathY = 26;
    btnDiagonal_.setBounds(4, pathY, 85, 22);
    lblInput_.setBounds(100, pathY, 25, 22);
    cbInputCh_.setBounds(125, pathY, 55, 22);
    lblArrow_.setBounds(185, pathY, 20, 22);
    lblOutput_.setBounds(210, pathY, 30, 22);
    cbOutputCh_.setBounds(240, pathY, 55, 22);

    // Graph
    int graphTop = pathY + 28;
    int graphHeight = (int)(h * 0.45f);
    graph_.setBounds(4, graphTop, w - 8, graphHeight);

    // Tab bar
    int tabY = graphTop + graphHeight + 4;
    int tabW = w - 80;
    tabs_.setBounds(4, tabY, tabW, 28);
    btnAdd_.setBounds(tabW + 8, tabY, 30, 28);
    btnRemove_.setBounds(tabW + 42, tabY, 30, 28);

    // Status bar at bottom
    int statusH = 42;
    statusBar_.setBounds(0, h - statusH, w, statusH);

    // Band editor
    int editorY = tabY + 32;
    bandEditor_.setBounds(4, editorY, w - 8, h - editorY - statusH);
}

void Mcfx_mimoeqAudioProcessorEditor::updatePathSelector()
{
    bool isDiag = btnDiagonal_.getToggleState();
    cbInputCh_.setEnabled(!isDiag);
    cbOutputCh_.setEnabled(!isDiag);
    lblInput_.setEnabled(!isDiag);
    lblOutput_.setEnabled(!isDiag);
    lblArrow_.setEnabled(!isDiag);
    diagonalMode_ = isDiag;
}

EqChain* Mcfx_mimoeqAudioProcessorEditor::getActiveChain()
{
    if (diagonalMode_)
        return &getProcessor()->getDiagonalChain();

    int inCh = cbInputCh_.getSelectedId();
    int outCh = cbOutputCh_.getSelectedId();
    return getProcessor()->getChainForPath(inCh, outCh);
}

void Mcfx_mimoeqAudioProcessorEditor::changeListenerCallback(ChangeBroadcaster*)
{
    auto* chain = getActiveChain();
    graph_.setChain(chain);
    refreshTabs();
    if (selectedBand_ >= 0 && chain != nullptr && selectedBand_ < chain->getNumBands())
        bandEditor_.updateFromBand();
    graph_.repaint();
}

void Mcfx_mimoeqAudioProcessorEditor::comboBoxChanged(ComboBox*)
{
    auto* chain = getActiveChain();
    graph_.setChain(chain);
    refreshTabs();
    if (chain != nullptr && chain->getNumBands() > 0)
        selectBand(0);
    else
        selectBand(-1);
}

void Mcfx_mimoeqAudioProcessorEditor::refreshTabs()
{
    if (refreshingTabs_)
        return;
    refreshingTabs_ = true;

    tabs_.clearTabs();
    auto* chain = getActiveChain();
    if (chain == nullptr)
    {
        refreshingTabs_ = false;
        return;
    }

    for (int i = 0; i < chain->getNumBands(); ++i)
    {
        String name = "Band " + String(i + 1);
        Colour tabColour = EqGraph::getBandColour(i);
        tabs_.addTab(name, tabColour, i);
    }

    if (selectedBand_ >= 0 && selectedBand_ < chain->getNumBands())
        tabs_.setCurrentTabIndex(selectedBand_, false);

    refreshingTabs_ = false;
}

void Mcfx_mimoeqAudioProcessorEditor::selectBand(int index)
{
    auto* chain = getActiveChain();
    if (chain != nullptr && index >= 0 && index < chain->getNumBands())
    {
        selectedBand_ = index;
        bandEditor_.setBand(chain->getBand(index), index);
        bandEditor_.setVisible(true);
        graph_.setSelectedBand(index);
        refreshTabs();  // update tab colours to highlight selected
    }
    else
    {
        selectedBand_ = -1;
        bandEditor_.setBand(nullptr, -1);
        bandEditor_.setVisible(false);
        graph_.setSelectedBand(-1);
        refreshTabs();
    }
}

void Mcfx_mimoeqAudioProcessorEditor::notifyChainChanged()
{
    auto* proc = getProcessor();
    proc->requestRebuild();  // deferred to audio thread to avoid race condition
    proc->sendChangeMessage();
}

void Mcfx_mimoeqAudioProcessorEditor::eqBandDragged(int bandIndex, float newFreqHz, float newGainDB)
{
    auto* chain = getActiveChain();
    if (chain == nullptr)
        return;

    auto* band = chain->getBand(bandIndex);
    if (band == nullptr)
        return;

    if (band->getType() == EqBandType::IIR)
    {
        band->setFrequency(newFreqHz);
        band->setGainDB(newGainDB);
    }
    else if (band->getType() == EqBandType::Gain)
    {
        band->setGainDB(newGainDB);
    }

    bandEditor_.updateFromBand();
    notifyChainChanged();
}

void Mcfx_mimoeqAudioProcessorEditor::eqBandQChanged(int bandIndex, float newQ)
{
    auto* chain = getActiveChain();
    if (chain == nullptr)
        return;

    auto* band = chain->getBand(bandIndex);
    if (band != nullptr)
    {
        band->setQ(newQ);
        bandEditor_.updateFromBand();
        notifyChainChanged();
    }
}

void Mcfx_mimoeqAudioProcessorEditor::eqBandSelected(int bandIndex)
{
    selectBand(bandIndex);
}

void Mcfx_mimoeqAudioProcessorEditor::eqBandEnableToggled(int bandIndex)
{
    auto* chain = getActiveChain();
    if (chain == nullptr)
        return;

    auto* band = chain->getBand(bandIndex);
    if (band != nullptr)
    {
        band->setEnabled(!band->isEnabled());
        bandEditor_.updateFromBand();
        notifyChainChanged();
    }
}

void Mcfx_mimoeqAudioProcessorEditor::eqBandDoubleClicked(float freqHz, float gainDB)
{
    EqChain* chain = nullptr;

    if (diagonalMode_)
    {
        chain = &getProcessor()->getDiagonalChain();
    }
    else
    {
        int inCh = cbInputCh_.getSelectedId();
        int outCh = cbOutputCh_.getSelectedId();
        chain = getProcessor()->getOrCreateChainForPath(inCh, outCh);
        graph_.setChain(chain);
    }

    auto* band = chain->addBand();
    band->setType(EqBandType::IIR);
    band->setIIRSubType(IIRSubType::Peak);
    band->setFrequency(freqHz);
    band->setGainDB(gainDB);
    band->setQ(1.5f);

    if (diagonalMode_)
        band->setDiagonal(true);
    else
    {
        band->setDiagonal(false);
        band->setInputChannel(cbInputCh_.getSelectedId());
        band->setOutputChannel(cbOutputCh_.getSelectedId());
    }
    band->prepare(getProcessor()->getSampleRate_(), 512);

    refreshTabs();
    selectBand(chain->getNumBands() - 1);
    notifyChainChanged();
}

void Mcfx_mimoeqAudioProcessorEditor::bandParameterChanged(int)
{
    notifyChainChanged();
}

void Mcfx_mimoeqAudioProcessorEditor::bandEnableChanged(int, bool)
{
    notifyChainChanged();
}

void Mcfx_mimoeqAudioProcessorEditor::buttonClicked(Button* b)
{
    if (b == &btnDiagonal_)
    {
        updatePathSelector();
        auto* chain = getActiveChain();
        graph_.setChain(chain);
        refreshTabs();
        if (chain != nullptr && chain->getNumBands() > 0)
            selectBand(0);
        else
            selectBand(-1);
        return;
    }

    if (b == &btnAdd_)
    {
        EqChain* chain = nullptr;

        if (diagonalMode_)
        {
            chain = &getProcessor()->getDiagonalChain();
        }
        else
        {
            int inCh = cbInputCh_.getSelectedId();
            int outCh = cbOutputCh_.getSelectedId();
            chain = getProcessor()->getOrCreateChainForPath(inCh, outCh);
            graph_.setChain(chain);
        }

        auto* band = chain->addBand();
        if (diagonalMode_)
            band->setDiagonal(true);
        else
        {
            band->setDiagonal(false);
            band->setInputChannel(cbInputCh_.getSelectedId());
            band->setOutputChannel(cbOutputCh_.getSelectedId());
        }
        band->prepare(getProcessor()->getSampleRate_(), 512);

        refreshTabs();
        selectBand(chain->getNumBands() - 1);
        notifyChainChanged();
    }
    else if (b == &btnRemove_)
    {
        auto* chain = getActiveChain();
        if (chain != nullptr && selectedBand_ >= 0)
        {
            chain->removeBand(selectedBand_);
            int newSel = jmin(selectedBand_, chain->getNumBands() - 1);
            refreshTabs();
            selectBand(newSel);
            notifyChainChanged();
        }
    }
    else if (b == &btnLoad_)
    {
        FileChooser chooser("Load EQ Config", File(), "*.json");
        if (chooser.browseForFileToOpen())
        {
            getProcessor()->loadConfigFromFile(chooser.getResult());
            auto* chain = getActiveChain();
            graph_.setChain(chain);
            refreshTabs();
            if (chain != nullptr && chain->getNumBands() > 0)
                selectBand(0);
            else
                selectBand(-1);
        }
    }
    else if (b == &btnSave_)
    {
        FileChooser chooser("Save EQ Config", File(), "*.json");
        if (chooser.browseForFileToSave(true))
        {
            getProcessor()->saveConfigToFile(chooser.getResult());
        }
    }
}

bool Mcfx_mimoeqAudioProcessorEditor::isInterestedInFileDrag(const StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase(".json"))
            return true;
    return false;
}

void Mcfx_mimoeqAudioProcessorEditor::filesDropped(const StringArray& files, int, int)
{
    for (auto& f : files)
    {
        if (f.endsWithIgnoreCase(".json"))
        {
            getProcessor()->loadConfigFromFile(File(f));
            auto* chain = getActiveChain();
            graph_.setChain(chain);
            refreshTabs();
            if (chain != nullptr && chain->getNumBands() > 0)
                selectBand(0);
            else
                selectBand(-1);
            break;
        }
    }
}

void Mcfx_mimoeqAudioProcessorEditor::mouseEnter(const MouseEvent& e)
{
    if (auto* ttc = dynamic_cast<TooltipClient*>(e.eventComponent))
    {
        auto tip = ttc->getTooltip();
        if (tip.isNotEmpty())
        {
            statusBar_.setText(tip, dontSendNotification);
            return;
        }
    }
    statusBar_.setText("", dontSendNotification);
}

void Mcfx_mimoeqAudioProcessorEditor::mouseExit(const MouseEvent&)
{
    statusBar_.setText("", dontSendNotification);
}
