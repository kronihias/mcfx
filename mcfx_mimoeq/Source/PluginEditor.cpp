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

    // Mode selector (radio group)
    addAndMakeVisible(btnModeDiag_);
    addAndMakeVisible(btnModeMIMO_);
    btnModeDiag_.setRadioGroupId(1001);
    btnModeMIMO_.setRadioGroupId(1001);
    btnModeDiag_.setClickingTogglesState(true);
    btnModeMIMO_.setClickingTogglesState(true);
    btnModeDiag_.setToggleState(true, dontSendNotification);
    btnModeDiag_.addListener(this);
    btnModeMIMO_.addListener(this);
    btnModeDiag_.setTooltip("Diagonal: apply the same EQ chain to all channels independently");
    btnModeMIMO_.setTooltip("MIMO: configure individual EQ paths between input/output channels");

    // MIMO path controls
    addAndMakeVisible(cbPathSelector_);
    cbPathSelector_.addListener(this);
    cbPathSelector_.setTooltip("Select an EQ path to edit");

    addAndMakeVisible(btnAddPath_);
    btnAddPath_.addListener(this);
    btnAddPath_.setTooltip("Add a new EQ path between input and output channels");

    addAndMakeVisible(btnRemovePath_);
    btnRemovePath_.addListener(this);
    btnRemovePath_.setTooltip("Remove the currently selected path");

    addAndMakeVisible(btnRouting_);
    btnRouting_.addListener(this);
    btnRouting_.setTooltip("Show routing overview with all input/output connections");

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
    btnAdd_.setTooltip("Add a new EQ band (or double-click graph to add Peak at position)");
    addAndMakeVisible(btnRemove_);
    btnRemove_.addListener(this);
    btnRemove_.setTooltip("Remove the selected EQ band (shortcut: D or Delete in graph)");
    addAndMakeVisible(btnUndo_);
    btnUndo_.addListener(this);
    btnUndo_.setTooltip("Undo last change (Cmd+Z)");
    btnUndo_.setEnabled(false);
    addAndMakeVisible(btnRedo_);
    btnRedo_.addListener(this);
    btnRedo_.setTooltip("Redo (Cmd+Shift+Z)");
    btnRedo_.setEnabled(false);
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
    btnUndo_.setBounds(w - 210, 2, 40, 20);
    btnRedo_.setBounds(w - 168, 2, 40, 20);
    btnLoad_.setBounds(w - 115, 2, 55, 20);
    btnSave_.setBounds(w - 58, 2, 55, 20);

    // Mode & path selector row
    int pathY = 26;
    btnModeDiag_.setBounds(4, pathY, 75, 22);
    btnModeMIMO_.setBounds(83, pathY, 55, 22);
    cbPathSelector_.setBounds(145, pathY, 150, 22);
    btnAddPath_.setBounds(300, pathY, 70, 22);
    btnRemovePath_.setBounds(374, pathY, 85, 22);
    btnRouting_.setBounds(464, pathY, 65, 22);

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
    diagonalMode_ = btnModeDiag_.getToggleState();
    bool mimo = !diagonalMode_;
    cbPathSelector_.setVisible(mimo);
    btnAddPath_.setVisible(mimo);
    btnRemovePath_.setVisible(mimo);
    btnRouting_.setVisible(mimo);
    if (mimo)
        rebuildPathDropdown();
}

void Mcfx_mimoeqAudioProcessorEditor::rebuildPathDropdown()
{
    cbPathSelector_.clear(dontSendNotification);

    auto keys = getProcessor()->getPathKeys();
    int selIdx = -1;
    for (int i = 0; i < (int)keys.size(); ++i)
    {
        String label = "In " + String(keys[i].first) + " "
                      + String(CharPointer_UTF8("\xe2\x86\x92")) + " Out "
                      + String(keys[i].second);
        cbPathSelector_.addItem(label, i + 1);
        if (keys[i] == selectedPath_)
            selIdx = i;
    }

    if (selIdx >= 0)
        cbPathSelector_.setSelectedId(selIdx + 1, dontSendNotification);
    else if (!keys.empty())
    {
        selectedPath_ = keys[0];
        cbPathSelector_.setSelectedId(1, dontSendNotification);
    }

    btnRemovePath_.setEnabled(!keys.empty());
}

EqChain* Mcfx_mimoeqAudioProcessorEditor::getActiveChain()
{
    if (diagonalMode_)
        return &getProcessor()->getDiagonalChain();

    return getProcessor()->getChainForPath(selectedPath_.first, selectedPath_.second);
}

void Mcfx_mimoeqAudioProcessorEditor::changeListenerCallback(ChangeBroadcaster*)
{
    if (!diagonalMode_)
    {
        rebuildPathDropdown();
        // If the selected path was removed (e.g. by undo), fall back
        if (getProcessor()->getChainForPath(selectedPath_.first, selectedPath_.second) == nullptr)
        {
            auto keys = getProcessor()->getPathKeys();
            if (!keys.empty())
                selectedPath_ = keys[0];
        }
    }
    auto* chain = getActiveChain();
    graph_.setChain(chain);
    refreshTabs();
    if (selectedBand_ >= 0 && chain != nullptr && selectedBand_ < chain->getNumBands())
        bandEditor_.updateFromBand();
    graph_.repaint();
    updateUndoRedoButtons();
}

void Mcfx_mimoeqAudioProcessorEditor::comboBoxChanged(ComboBox* cb)
{
    if (cb == &cbPathSelector_)
    {
        int sel = cbPathSelector_.getSelectedId() - 1; // 0-based index
        auto keys = getProcessor()->getPathKeys();
        if (sel >= 0 && sel < (int)keys.size())
            selectedPath_ = keys[sel];
    }

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
    proc->requestRebuild();  // full rebuild: deferred to audio thread
    proc->sendChangeMessage();
}

void Mcfx_mimoeqAudioProcessorEditor::notifyParameterChanged()
{
    auto* proc = getProcessor();
    proc->requestParameterSync();  // lightweight: sync params to channel copies
    proc->sendChangeMessage();
}

void Mcfx_mimoeqAudioProcessorEditor::eqBandDragged(int bandIndex, float newFreqHz, float newGainDB)
{
    auto* chain = getActiveChain();
    if (chain == nullptr)
        return;

    if (!dragUndoPushed_)
    {
        getProcessor()->pushUndoState();
        dragUndoPushed_ = true;
        updateUndoRedoButtons();
    }

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
    notifyParameterChanged();
}

void Mcfx_mimoeqAudioProcessorEditor::eqBandQChanged(int bandIndex, float newQ)
{
    auto* chain = getActiveChain();
    if (chain == nullptr)
        return;

    if (!dragUndoPushed_)
    {
        getProcessor()->pushUndoState();
        dragUndoPushed_ = true;
        updateUndoRedoButtons();
    }

    auto* band = chain->getBand(bandIndex);
    if (band != nullptr)
    {
        band->setQ(newQ);
        bandEditor_.updateFromBand();
        notifyParameterChanged();
    }
}

void Mcfx_mimoeqAudioProcessorEditor::eqBandSelected(int bandIndex)
{
    dragUndoPushed_ = false;
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
        getProcessor()->pushUndoState();
        updateUndoRedoButtons();
        band->setEnabled(!band->isEnabled());
        bandEditor_.updateFromBand();
        notifyParameterChanged();
    }
}

void Mcfx_mimoeqAudioProcessorEditor::eqBandDeleteRequested(int bandIndex)
{
    auto* chain = getActiveChain();
    if (chain != nullptr && bandIndex >= 0 && bandIndex < chain->getNumBands())
    {
        dragUndoPushed_ = false;
        getProcessor()->pushUndoState();
        updateUndoRedoButtons();
        chain->removeBand(bandIndex);
        int newSel = jmin(bandIndex, chain->getNumBands() - 1);
        refreshTabs();
        selectBand(newSel);
        notifyChainChanged();
    }
}

void Mcfx_mimoeqAudioProcessorEditor::eqBandDoubleClicked(float freqHz, float gainDB)
{
    dragUndoPushed_ = false;
    getProcessor()->pushUndoState();
    updateUndoRedoButtons();

    EqChain* chain = nullptr;

    if (diagonalMode_)
    {
        chain = &getProcessor()->getDiagonalChain();
    }
    else
    {
        chain = getProcessor()->getOrCreateChainForPath(selectedPath_.first, selectedPath_.second);
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
        band->setInputChannel(selectedPath_.first);
        band->setOutputChannel(selectedPath_.second);
    }
    band->prepare(getProcessor()->getSampleRate_(), 512);

    refreshTabs();
    selectBand(chain->getNumBands() - 1);
    notifyChainChanged();
}

void Mcfx_mimoeqAudioProcessorEditor::bandParameterChanged(int)
{
    if (!dragUndoPushed_)
    {
        getProcessor()->pushUndoState();
        dragUndoPushed_ = true;
        updateUndoRedoButtons();
    }
    notifyParameterChanged();
}

void Mcfx_mimoeqAudioProcessorEditor::bandEnableChanged(int, bool)
{
    getProcessor()->pushUndoState();
    updateUndoRedoButtons();
    notifyParameterChanged();
}

void Mcfx_mimoeqAudioProcessorEditor::bandStructureChanged(int)
{
    dragUndoPushed_ = false;
    getProcessor()->pushUndoState();
    updateUndoRedoButtons();
    notifyChainChanged();
}

void Mcfx_mimoeqAudioProcessorEditor::buttonClicked(Button* b)
{
    if (b == &btnModeDiag_ || b == &btnModeMIMO_)
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

    if (b == &btnAddPath_)
    {
        showAddPathPopup();
        return;
    }

    if (b == &btnRemovePath_)
    {
        if (getProcessor()->getChainForPath(selectedPath_.first, selectedPath_.second) != nullptr)
        {
            dragUndoPushed_ = false;
            getProcessor()->pushUndoState();
            updateUndoRedoButtons();
            getProcessor()->removePathChain(selectedPath_.first, selectedPath_.second);

            auto keys = getProcessor()->getPathKeys();
            if (!keys.empty())
                selectedPath_ = keys[0];

            rebuildPathDropdown();
            notifyChainChanged();

            auto* chain = getActiveChain();
            graph_.setChain(chain);
            refreshTabs();
            if (chain != nullptr && chain->getNumBands() > 0)
                selectBand(0);
            else
                selectBand(-1);
        }
        return;
    }

    if (b == &btnRouting_)
    {
        showRoutingOverview();
        return;
    }

    if (b == &btnUndo_)
    {
        performUndo();
        return;
    }

    if (b == &btnRedo_)
    {
        performRedo();
        return;
    }

    if (b == &btnAdd_)
    {
        dragUndoPushed_ = false;
        getProcessor()->pushUndoState();
        updateUndoRedoButtons();

        EqChain* chain = nullptr;

        if (diagonalMode_)
        {
            chain = &getProcessor()->getDiagonalChain();
        }
        else
        {
            chain = getProcessor()->getOrCreateChainForPath(selectedPath_.first, selectedPath_.second);
            graph_.setChain(chain);
        }

        auto* band = chain->addBand();
        if (diagonalMode_)
            band->setDiagonal(true);
        else
        {
            band->setDiagonal(false);
            band->setInputChannel(selectedPath_.first);
            band->setOutputChannel(selectedPath_.second);
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
            dragUndoPushed_ = false;
            getProcessor()->pushUndoState();
            updateUndoRedoButtons();
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
            dragUndoPushed_ = false;
            getProcessor()->pushUndoState();
            updateUndoRedoButtons();
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
            dragUndoPushed_ = false;
            getProcessor()->pushUndoState();
            updateUndoRedoButtons();
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

void Mcfx_mimoeqAudioProcessorEditor::showAddPathPopup()
{
    int numCh = jmax(1, getProcessor()->getNumChannels_());
    auto existingKeys = getProcessor()->getPathKeys();
    std::set<PathKey> existing(existingKeys.begin(), existingKeys.end());

    PopupMenu menu;
    for (int inCh = 1; inCh <= numCh; ++inCh)
    {
        PopupMenu subMenu;
        bool hasItems = false;
        for (int outCh = 1; outCh <= numCh; ++outCh)
        {
            if (existing.count({inCh, outCh}) == 0)
            {
                int itemId = (inCh - 1) * numCh + (outCh - 1) + 1;
                subMenu.addItem(itemId, "Out " + String(outCh));
                hasItems = true;
            }
        }
        if (hasItems)
            menu.addSubMenu("In " + String(inCh), subMenu);
    }

    menu.showMenuAsync(PopupMenu::Options().withTargetComponent(&btnAddPath_),
        [this, numCh](int result)
        {
            if (result <= 0) return;
            int idx = result - 1;
            int inCh  = idx / numCh + 1;
            int outCh = idx % numCh + 1;

            dragUndoPushed_ = false;
            getProcessor()->pushUndoState();
            updateUndoRedoButtons();

            getProcessor()->getOrCreateChainForPath(inCh, outCh);
            selectedPath_ = { inCh, outCh };
            rebuildPathDropdown();
            notifyChainChanged();

            auto* chain = getActiveChain();
            graph_.setChain(chain);
            refreshTabs();
            selectBand(-1); // new path has no bands yet
        });
}

void Mcfx_mimoeqAudioProcessorEditor::showRoutingOverview()
{
    auto keys = getProcessor()->getPathKeys();
    int numCh = jmax(1, getProcessor()->getNumChannels_());
    bool hasDiag = getProcessor()->getDiagonalChain().getNumBands() > 0;

    auto* overview = new RoutingOverviewComponent(keys, numCh, hasDiag, this);
    auto& box = CallOutBox::launchAsynchronously(std::unique_ptr<Component>(overview),
                                                  btnRouting_.getScreenBounds(),
                                                  nullptr);
    (void)box;
}

void Mcfx_mimoeqAudioProcessorEditor::routingPathSelected(int inCh, int outCh)
{
    // Switch to MIMO mode if in diagonal
    if (diagonalMode_)
    {
        btnModeMIMO_.setToggleState(true, dontSendNotification);
        updatePathSelector();
    }

    selectedPath_ = { inCh, outCh };
    rebuildPathDropdown();

    auto* chain = getActiveChain();
    graph_.setChain(chain);
    refreshTabs();
    if (chain != nullptr && chain->getNumBands() > 0)
        selectBand(0);
    else
        selectBand(-1);
}

void Mcfx_mimoeqAudioProcessorEditor::eqUndoRequested()
{
    performUndo();
}

void Mcfx_mimoeqAudioProcessorEditor::eqRedoRequested()
{
    performRedo();
}

void Mcfx_mimoeqAudioProcessorEditor::performUndo()
{
    dragUndoPushed_ = false;
    if (getProcessor()->undo())
    {
        if (!diagonalMode_)
        {
            rebuildPathDropdown();
            if (getProcessor()->getChainForPath(selectedPath_.first, selectedPath_.second) == nullptr)
            {
                auto keys = getProcessor()->getPathKeys();
                if (!keys.empty())
                    selectedPath_ = keys[0];
            }
        }
        auto* chain = getActiveChain();
        graph_.setChain(chain);
        refreshTabs();
        if (chain != nullptr && chain->getNumBands() > 0)
            selectBand(jmin(selectedBand_, chain->getNumBands() - 1));
        else
            selectBand(-1);
        updateUndoRedoButtons();
    }
}

void Mcfx_mimoeqAudioProcessorEditor::performRedo()
{
    dragUndoPushed_ = false;
    if (getProcessor()->redo())
    {
        if (!diagonalMode_)
        {
            rebuildPathDropdown();
            if (getProcessor()->getChainForPath(selectedPath_.first, selectedPath_.second) == nullptr)
            {
                auto keys = getProcessor()->getPathKeys();
                if (!keys.empty())
                    selectedPath_ = keys[0];
            }
        }
        auto* chain = getActiveChain();
        graph_.setChain(chain);
        refreshTabs();
        if (chain != nullptr && chain->getNumBands() > 0)
            selectBand(jmin(selectedBand_, chain->getNumBands() - 1));
        else
            selectBand(-1);
        updateUndoRedoButtons();
    }
}

void Mcfx_mimoeqAudioProcessorEditor::updateUndoRedoButtons()
{
    btnUndo_.setEnabled(getProcessor()->canUndo());
    btnRedo_.setEnabled(getProcessor()->canRedo());
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
