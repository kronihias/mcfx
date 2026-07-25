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
    statusBar_.setFont(Font(FontOptions(11.f, Font::plain)));
    statusBar_.setColour(Label::textColourId, Colours::white.withAlpha(0.8f));
    statusBar_.setColour(Label::backgroundColourId, Colour(0xff111111));
    statusBar_.setJustificationType(Justification::centredLeft);
    statusBar_.setBorderSize(BorderSize<int>(2, 6, 2, 6));
    statusBar_.setMinimumHorizontalScale(1.0f);

    addAndMakeVisible(lblTitle_);
    lblTitle_.setText("mcfx_mimoeq", dontSendNotification);
    lblTitle_.setFont(Font(FontOptions(15.f, Font::plain)));
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
    btnModeDiag_.setTooltip("Diagonal: apply the same EQ chain to all selected channels (input-to-output) independently");
    btnModeMIMO_.setTooltip("MIMO: configure individual EQ paths between input/output channels");

    // Make the active mode unmistakable at a glance: each mode gets its own
    // bright background colour (aquamarine for Diagonal — matches the title;
    // warm orange for MIMO), with dark text on the active one. The inactive
    // button recedes to a dim grey with low-contrast text. Without this the
    // two buttons look identical under JUCE's default LookAndFeel and the
    // user has to read them to know which mode they're in.
    const Colour kDiagColour { 0xff5fdbb4 };  // aquamarine, matches lblTitle_
    const Colour kMimoColour { 0xffe88a3a };  // warm orange, distinct
    const Colour kInactiveBg { 0xff2a2a2a };  // recedes into toolbar
    const Colour kInactiveTx { Colours::white.withAlpha(0.55f) };

    btnModeDiag_.setColour(TextButton::buttonOnColourId,  kDiagColour);
    btnModeDiag_.setColour(TextButton::buttonColourId,    kInactiveBg);
    btnModeDiag_.setColour(TextButton::textColourOnId,    Colours::black);
    btnModeDiag_.setColour(TextButton::textColourOffId,   kInactiveTx);

    btnModeMIMO_.setColour(TextButton::buttonOnColourId,  kMimoColour);
    btnModeMIMO_.setColour(TextButton::buttonColourId,    kInactiveBg);
    btnModeMIMO_.setColour(TextButton::textColourOnId,    Colours::black);
    btnModeMIMO_.setColour(TextButton::textColourOffId,   kInactiveTx);

    // Diagonal channel selector
    addAndMakeVisible(btnDiagChans_);
    btnDiagChans_.addListener(this);
    btnDiagChans_.setTooltip("Select which channels the diagonal EQ applies to");
    updateDiagChannelButton();

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

    // Display row — every analyzer setting, no popup.
    addAndMakeVisible(lblDisplay_);
    lblDisplay_.setColour(Label::textColourId, Colours::white.withAlpha(0.55f));
    lblDisplay_.setFont(Font(FontOptions(13.f, Font::plain)));

    // "off" is just the first view, so the analyzer needs no separate toggle.
    addAndMakeVisible(cbAnalyzerView_);
    cbAnalyzerView_.addItem("analyzer off", 1);
    cbAnalyzerView_.addItem("spectrum",     2);
    cbAnalyzerView_.addItem("spectrogram",  3);
    cbAnalyzerView_.addListener(this);
    cbAnalyzerView_.setTooltip("Analyzer overlay: off, spectrum curve, or a rolling "
                               "spectrogram (time vs frequency, colour = level)");

    addAndMakeVisible(cbAnalyzerSource_);
    cbAnalyzerSource_.addItem("pre-EQ", 1);
    cbAnalyzerSource_.addItem("post-EQ", 2);
    cbAnalyzerSource_.addListener(this);
    cbAnalyzerSource_.setTooltip("Which signal the spectrogram displays");

    addAndMakeVisible(lblChannel_);
    lblChannel_.setColour(Label::textColourId, Colours::white.withAlpha(0.55f));
    lblChannel_.setFont(Font(FontOptions(13.f, Font::plain)));
    addAndMakeVisible(cbAnalyzerChannel_);
    {
        int numCh = jmax(1, processor->getNumChannels_());
        cbAnalyzerChannel_.addItem("all", 1);
        for (int ch = 1; ch <= numCh; ++ch)
            cbAnalyzerChannel_.addItem(String(ch), ch + 1);
        cbAnalyzerChannel_.setSelectedId(processor->editorAnalyzerChannel + 1,
                                         dontSendNotification);
        cbAnalyzerChannel_.setEditableText(true);   // type a channel number directly
    }
    cbAnalyzerChannel_.addListener(this);
    cbAnalyzerChannel_.setTooltip("Which channel the analyzer measures "
                                  "(set by the selected path in MIMO mode)");

    addAndMakeVisible(btnAutoNorm_);
    btnAutoNorm_.setToggleState(processor->analyzerAutoNormalize, dontSendNotification);
    btnAutoNorm_.addListener(this);
    btnAutoNorm_.setTooltip("Track the program level automatically instead of using a fixed offset");

    addAndMakeVisible(lblOffset_);
    lblOffset_.setColour(Label::textColourId, Colours::white.withAlpha(0.55f));
    lblOffset_.setFont(Font(FontOptions(13.f, Font::plain)));
    addAndMakeVisible(sldOffset_);
    sldOffset_.setRange(-80.0, 80.0, 0.5);
    sldOffset_.setValue(processor->analyzerOffset, dontSendNotification);
    sldOffset_.setSliderStyle(Slider::LinearHorizontal);
    sldOffset_.setTextBoxStyle(Slider::TextBoxRight, false, 46, 20);
    sldOffset_.setDoubleClickReturnValue(true, 0.0);
    sldOffset_.addListener(this);
    sldOffset_.setTooltip("dB offset for the analyzer display");

    // Phase toggle — shows a separate phase-response graph below the magnitude.
    addAndMakeVisible(btnPhase_);
    btnPhase_.setClickingTogglesState(true);
    btnPhase_.addListener(this);
    btnPhase_.setTooltip("Show phase-response graph below the magnitude graph.");

    addAndMakeVisible(graph_);
    graph_.setListener(this);
    graph_.setAnalyzers(&processor->getInputAnalyzer(), &processor->getOutputAnalyzer());
    graph_.setLiveDynOffsetProvider([this](int band) -> float {
        // Live dynamic metering is published for the diagonal chain only.
        return diagonalMode_ ? getProcessor()->getDiagDynMeter(band) : 0.f;
    });
    graph_.setTooltip("Drag handles to adjust frequency/gain. Mouse wheel to adjust Q.\nDouble-click to add a band. Double-click a handle to toggle enable. Press E to toggle enable.\nDrop a .json file to load configuration.");

    addChildComponent(phaseGraph_);   // hidden by default; updatePhaseGraphVisibility() flips it on

    addAndMakeVisible(bandEditorViewport_);
    bandEditorViewport_.setViewedComponent(&bandEditor_, false);
    bandEditorViewport_.setScrollBarsShown(true, false);
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
    addAndMakeVisible(btnPresets_);
    btnPresets_.addListener(this);
    btnPresets_.setTooltip("Save / load named EQ presets (stored under the user's app-data folder), or import / export a JSON file anywhere on disk.");

    processor->addChangeListener(this);

    // Restore editor view state from processor
    diagonalMode_ = processor->editorDiagonalMode;

    // If the user only ever populates MIMO paths, the saved editorDiagonalMode
    // can still be `true` (the factory default) and we'd land them on the empty
    // Diagonal page. Override to MIMO when diagonal is empty AND at least one
    // MIMO path is configured; the empty-both case (truly fresh instance) keeps
    // the Diagonal default. Re-evaluated at every editor open from live state
    // rather than persisted, so adding a single band to diagonal flips us back
    // next reopen.
    if (diagonalMode_
        && processor->getDiagonalChain().getNumBands() == 0
        && ! processor->getPathKeys().empty())
    {
        diagonalMode_ = false;
    }

    selectedPath_ = processor->editorSelectedPath;
    btnModeDiag_.setToggleState(diagonalMode_, dontSendNotification);
    btnModeMIMO_.setToggleState(!diagonalMode_, dontSendNotification);
    updatePathSelector();
    if (!diagonalMode_)
        rebuildPathDropdown();

    // Point graph at the active chain
    auto* chain = getActiveChain();
    graph_.setChain(chain); phaseGraph_.setChain(chain);
    refreshTabs();
    int restoredBand = processor->editorSelectedBand;
    if (chain != nullptr && restoredBand >= 0 && restoredBand < chain->getNumBands())
        selectBand(restoredBand);
    else if (chain != nullptr && chain->getNumBands() > 0)
        selectBand(0);

    // Restore analyzer state (the display row is synced by updateAnalyzerState)
    processor->getInputAnalyzer().setAnalyzerChannel(processor->editorAnalyzerChannel);
    processor->getOutputAnalyzer().setAnalyzerChannel(processor->editorAnalyzerChannel);
    updateAnalyzerState();

    // Restore phase-graph state
    btnPhase_.setToggleState(processor->editorPhaseOn, dontSendNotification);
    phaseGraph_.setChain(chain);
    updatePhaseGraphVisibility();

    // Listen for mouse enter/exit on all child components for status bar
    addMouseListener(this, true);

    setResizable(true, true);
    // A touch taller than before to carry the display row without eating the graph.
    // The minimum width is what that row needs with a usable offset slider.
    setResizeLimits(680, 508, 1400, 1200);
    setSize(800, 628);
}

Mcfx_mimoeqAudioProcessorEditor::~Mcfx_mimoeqAudioProcessorEditor()
{
    // Save editor view state to processor for next GUI open
    auto* proc = getProcessor();
    proc->editorDiagonalMode = diagonalMode_;
    proc->editorSelectedPath = selectedPath_;
    proc->editorSelectedBand = selectedBand_;
    proc->editorAnalyzerChannel = proc->getInputAnalyzer().getAnalyzerChannel();
    proc->editorPhaseOn = btnPhase_.getToggleState();

    // Dismiss any still-open routing overview CallOutBox. It holds a raw
    // Listener* pointing back at this editor, so if it outlives us a
    // subsequent mouse click would crash inside routingPathSelected().
    // setVisible(false) synchronously removes the desktop peer so no more
    // OS mouse events can reach it; dismiss() schedules the
    // ModalComponentManager to delete the wrapping callback (the CallOutBox
    // is not directly heap-owned by us — launchAsynchronously wraps it in a
    // CallOutBoxCallback that owns itself via the ModalComponentManager).
    if (routingCallOut_ != nullptr)
    {
        routingCallOut_->setVisible(false);
        routingCallOut_->dismiss();
    }
    if (diagChannelCallOut_ != nullptr)
    {
        diagChannelCallOut_->setVisible(false);
        diagChannelCallOut_->dismiss();
    }
    proc->removeChangeListener(this);
    tabs_.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void Mcfx_mimoeqAudioProcessorEditor::paint(Graphics& g)
{
    g.fillAll(Colour(0xff1a1a1a));
    g.setGradientFill(ColourGradient(Colour(0xff4e4e4e),
                                      (float)proportionOfWidth(0.6f), (float)proportionOfHeight(0.7f),
                                      Colours::black,
                                      (float)proportionOfWidth(0.1f), (float)proportionOfHeight(0.1f),
                                      true));
    g.fillRect(getLocalBounds());

    // Subtle vertical separator between Diagonal and MIMO sections
    {
        int sepX = btnDiagChans_.getRight() + 8;
        int sepY = btnModeDiag_.getY() + 2;
        int sepH = btnModeDiag_.getHeight() - 4;
        g.setColour(Colours::white.withAlpha(0.15f));
        g.drawVerticalLine(sepX, (float)sepY, (float)(sepY + sepH));
    }

    // Version drawn just above the status bar
    int statusH = 42;
    g.setColour(Colours::white.withAlpha(0.35f));
    g.setFont(Font(FontOptions(10.f, Font::plain)));
    String version;
    version << "v" << QUOTE(VERSION);
    g.drawText(version, getWidth() - 55, getHeight() - statusH - 13, 50, 12, Justification::centredRight, true);
}

void Mcfx_mimoeqAudioProcessorEditor::resized()
{
    int w = getWidth();
    int h = getHeight();

    lblTitle_.setBounds(4, 2, 150, 20);
    btnUndo_   .setBounds(w - 210, 2, 40, 20);
    btnRedo_   .setBounds(w - 168, 2, 40, 20);
    // One Presets button replaces the old Load + Save pair, taking their
    // combined footprint (55 + 2 gap + 55 = 112 px).
    btnPresets_.setBounds(w - 115, 2, 112, 20);

    // Mode & path selector row
    int pathY = 26;
    int mx = 4; // running x position
    btnModeDiag_.setBounds(mx, pathY, 75, 22);       mx += 79;
    btnDiagChans_.setBounds(mx, pathY, 65, 22);      mx += 69;

    mx += 16; // visual gap between diagonal section and MIMO section

    btnModeMIMO_.setBounds(mx, pathY, 55, 22);       mx += 59;
    cbPathSelector_.setBounds(mx, pathY, 150, 22);   mx += 154;
    btnAddPath_.setBounds(mx, pathY, 70, 22);        mx += 74;
    btnRemovePath_.setBounds(mx, pathY, 85, 22);     mx += 89;
    btnRouting_.setBounds(mx, pathY, 65, 22);        mx += 69;

    // Display row — everything that changes what the graph *shows*, kept separate
    // from the row above, which selects what is being edited.
    int dispY = pathY + 26;
    int dx = 4;
    lblDisplay_.setBounds(dx, dispY, 55, 22);        dx += 57;
    btnPhase_  .setBounds(dx, dispY, 65, 22);        dx += 69;

    dx += 10;   // gap: everything from here on belongs to the analyzer
    cbAnalyzerView_  .setBounds(dx, dispY, 118, 22); dx += 122;

    // The analyzer's settings are hidden, not just greyed, when they don't apply
    // (analyzer off, or pre/post source with no spectrogram to apply it to), so
    // the row closes up instead of showing dead controls.
    auto place = [&] (Component& c, int cw, int gap)
    {
        if (! c.isVisible())
            return;
        c.setBounds(dx, dispY, cw, 22);
        dx += cw + gap;
    };
    place(cbAnalyzerSource_,  88, 8);
    place(lblChannel_,        24, 2);
    place(cbAnalyzerChannel_, 64, 6);
    place(btnAutoNorm_,       92, 4);
    place(lblOffset_,         44, 2);
    // The offset slider takes whatever is left, so the row fits any window width.
    if (sldOffset_.isVisible())
        sldOffset_.setBounds(dx, dispY, jmax(60, w - 4 - dx), 22);

    // Reserve fixed space for the bottom sections so the graph area absorbs
    // any extra height (and shrinks first when the window gets smaller).
    constexpr int statusH    = 42;
    constexpr int tabBarH    = 32;     // 28-tall tab buttons + 4 gap
    constexpr int editorH    = 200;    // fixed band-parameter area

    int graphTop = dispY + 28;
    int reservedBelow = editorH + tabBarH + statusH;
    int graphHeight = jmax (80, h - graphTop - reservedBelow);

    // Graph(s) — magnitude on top; if phase enabled, an equal-sized phase strip
    // sits directly below it.
    if (phaseGraph_.isVisible())
    {
        int half = graphHeight / 2;
        graph_     .setBounds(4, graphTop,            w - 8, half);
        phaseGraph_.setBounds(4, graphTop + half + 2, w - 8, graphHeight - half - 2);
    }
    else
    {
        graph_.setBounds(4, graphTop, w - 8, graphHeight);
    }

    // Tab bar
    int tabY = graphTop + graphHeight + 4;
    int tabW = w - 80;
    tabs_.setBounds(4, tabY, tabW, 28);
    btnAdd_.setBounds(tabW + 8, tabY, 30, 28);
    btnRemove_.setBounds(tabW + 42, tabY, 30, 28);

    // Status bar at bottom
    statusBar_.setBounds(0, h - statusH, w, statusH);

    // Band editor — fixed viewport height; the band editor self-sizes its content
    // height and scrolls inside the viewport when it overflows (e.g. dynamics on).
    int editorY = tabY + tabBarH;
    bandEditorViewport_.setBounds(4, editorY, w - 8, editorH);
    int innerW = jmax(50, (w - 8) - bandEditorViewport_.getScrollBarThickness());
    bandEditor_.setSize(innerW, jmax(editorH, bandEditor_.getContentHeight()));
}

void Mcfx_mimoeqAudioProcessorEditor::updatePathSelector()
{
    diagonalMode_ = btnModeDiag_.getToggleState();
    bool mimo = !diagonalMode_;
    btnDiagChans_.setVisible(diagonalMode_);
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
    graph_.setChain(chain); phaseGraph_.setChain(chain);
    refreshTabs();
    if (selectedBand_ >= 0 && chain != nullptr && selectedBand_ < chain->getNumBands())
        bandEditor_.updateFromBand();
    graph_.repaint();
    updateUndoRedoButtons();
    updateDiagChannelButton();
}

void Mcfx_mimoeqAudioProcessorEditor::comboBoxChanged(ComboBox* cb)
{
    // Display-row controls only change what is drawn — no chain/tab rebuild needed.
    if (cb == &cbAnalyzerView_)
    {
        // 1 = off, 2 = spectrum, 3 = spectrogram.
        const int sel = cbAnalyzerView_.getSelectedId();
        getProcessor()->editorAnalyzerOn     = (sel != 1);
        getProcessor()->editorSpectrogramOn  = (sel == 3);
        updateAnalyzerState();
        return;
    }
    if (cb == &cbAnalyzerSource_)
    {
        bool post = cbAnalyzerSource_.getSelectedId() == 2;
        getProcessor()->editorSpectroPost = post;
        graph_.setSpectrogramSource(post);
        return;
    }
    if (cb == &cbAnalyzerChannel_)
    {
        int id = cbAnalyzerChannel_.getSelectedId();
        if (id == 0)
        {
            // Editable box: the user typed a value. Accept "all" or a channel
            // number, clamp it, and snap the box back to the matching item.
            const int numCh = jmax(1, getProcessor()->getNumChannels_());
            const String txt = cbAnalyzerChannel_.getText().trim();
            id = txt.equalsIgnoreCase("all") ? 1
                                             : jlimit(1, numCh + 1, txt.getIntValue() + 1);
            cbAnalyzerChannel_.setSelectedId(id, dontSendNotification);
        }
        getProcessor()->editorAnalyzerChannel = id - 1;   // 0 = all, 1..N = channel
        updateAnalyzerState();
        return;
    }

    if (cb == &cbPathSelector_)
    {
        int sel = cbPathSelector_.getSelectedId() - 1; // 0-based index
        auto keys = getProcessor()->getPathKeys();
        if (sel >= 0 && sel < (int)keys.size())
            selectedPath_ = keys[sel];
    }

    auto* chain = getActiveChain();
    graph_.setChain(chain); phaseGraph_.setChain(chain);
    refreshTabs();
    if (chain != nullptr && chain->getNumBands() > 0)
        selectBand(0);
    else
        selectBand(-1);
    updateAnalyzerState();
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
        bandEditor_.setDynamicDiagonalContext(diagonalMode_);
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
    if (diagonalMode_)
        proc->syncModelToAPVTS();  // keep host parameters in sync
    proc->sendChangeMessage();
}

void Mcfx_mimoeqAudioProcessorEditor::notifyParameterChanged()
{
    auto* proc = getProcessor();
    proc->requestParameterSync();  // lightweight: sync params to channel copies
    if (diagonalMode_)
        proc->syncModelToAPVTS();  // keep host parameters in sync
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
        graph_.setChain(chain); phaseGraph_.setChain(chain);
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
        graph_.setChain(chain); phaseGraph_.setChain(chain);
        refreshTabs();
        if (chain != nullptr && chain->getNumBands() > 0)
            selectBand(0);
        else
            selectBand(-1);
        updateAnalyzerState();
        return;
    }

    if (b == &btnDiagChans_)
    {
        showDiagChannelPopup();
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
            graph_.setChain(chain); phaseGraph_.setChain(chain);
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

    if (b == &btnAutoNorm_)
    {
        const bool autoOn = btnAutoNorm_.getToggleState();
        getProcessor()->analyzerAutoNormalize = autoOn;
        graph_.setAnalyzerAutoNormalize(autoOn);
        sldOffset_.setEnabled(!autoOn);   // a fixed offset only applies without it
        return;
    }

    if (b == &btnPhase_)
    {
        updatePhaseGraphVisibility();
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
            graph_.setChain(chain); phaseGraph_.setChain(chain);
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
    else if (b == &btnPresets_)
    {
        showPresetsMenu();
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
            loadPresetFile(File(f));
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
            graph_.setChain(chain); phaseGraph_.setChain(chain);
            refreshTabs();
            selectBand(-1); // new path has no bands yet
        });
}

void Mcfx_mimoeqAudioProcessorEditor::showRoutingOverview()
{
    auto keys = getProcessor()->getPathKeys();
    int numCh = jmax(1, getProcessor()->getNumChannels_());
    bool hasDiag = getProcessor()->getDiagonalChain().getNumBands() > 0;
    auto& diagMask = getProcessor()->getDiagChannelMask();

    // Build band count map for each path
    std::map<PathKey, int> bandCounts;
    for (auto& key : keys)
    {
        auto* chain = getProcessor()->getChainForPath(key.first, key.second);
        if (chain != nullptr)
            bandCounts[key] = chain->getNumBands();
    }

    auto* wrapper = new ScrollableRoutingOverview(keys, bandCounts, numCh, hasDiag, diagMask, this);
    wrapper->setShowMatrix(getProcessor()->editorRoutingMatrixView);
    wrapper->onViewChanged = [this](bool isMatrix) {
        getProcessor()->editorRoutingMatrixView = isMatrix;
    };
    wrapper->onSizeChanged = [this](int w, int h) {
        getProcessor()->editorRoutingPopupW = w;
        getProcessor()->editorRoutingPopupH = h;
    };
    if (!diagonalMode_)
    {
        wrapper->getOverview()->setSelectedPath(selectedPath_);
        wrapper->getMatrix()->setSelectedPath(selectedPath_);
    }

    // Restore previously saved popup size
    if (getProcessor()->editorRoutingPopupW > 0 && getProcessor()->editorRoutingPopupH > 0)
        wrapper->setSize(getProcessor()->editorRoutingPopupW, getProcessor()->editorRoutingPopupH);

    auto& box = CallOutBox::launchAsynchronously(std::unique_ptr<Component>(wrapper),
                                                  btnRouting_.getScreenBounds(),
                                                  nullptr);
    routingCallOut_ = &box;
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
    graph_.setChain(chain); phaseGraph_.setChain(chain);
    refreshTabs();
    if (chain != nullptr && chain->getNumBands() > 0)
        selectBand(0);
    else
        selectBand(-1);
}

void Mcfx_mimoeqAudioProcessorEditor::routingPathCreated(int inCh, int outCh)
{
    // Create the new path
    getProcessor()->pushUndoState();
    updateUndoRedoButtons();

    getProcessor()->getOrCreateChainForPath(inCh, outCh);

    // Switch to MIMO mode if in diagonal
    if (diagonalMode_)
    {
        btnModeMIMO_.setToggleState(true, dontSendNotification);
        updatePathSelector();
    }

    selectedPath_ = { inCh, outCh };
    rebuildPathDropdown();
    notifyChainChanged();

    auto* chain = getActiveChain();
    graph_.setChain(chain); phaseGraph_.setChain(chain);
    refreshTabs();
    selectBand(-1);

    // Update the routing overview in-place so the new wire appears immediately
    // The RoutingOverviewComponent is the source of this callback, so find it
    // by walking Component parents isn't needed — use dynamic_cast on sender.
    // Instead, rebuild paths and push to any visible overview.
    auto keys = getProcessor()->getPathKeys();
    std::map<PathKey, int> bandCounts;
    for (auto& key : keys)
    {
        auto* c = getProcessor()->getChainForPath(key.first, key.second);
        if (c != nullptr)
            bandCounts[key] = c->getNumBands();
    }
    // Find the RoutingOverviewComponent inside any visible CallOutBox
    for (int i = 0; i < Desktop::getInstance().getNumComponents(); ++i)
    {
        auto* comp = Desktop::getInstance().getComponent(i);
        if (auto* callout = dynamic_cast<CallOutBox*>(comp))
        {
            for (int c = 0; c < callout->getNumChildComponents(); ++c)
            {
                if (auto* wrapper = dynamic_cast<ScrollableRoutingOverview*>(callout->getChildComponent(c)))
                {
                    wrapper->updatePaths(keys, bandCounts);
                    break;
                }
            }
        }
    }
}

void Mcfx_mimoeqAudioProcessorEditor::routingPathRemoved(int inCh, int outCh)
{
    if (getProcessor()->getChainForPath(inCh, outCh) == nullptr)
        return;

    getProcessor()->pushUndoState();
    updateUndoRedoButtons();
    getProcessor()->removePathChain(inCh, outCh);

    auto keys = getProcessor()->getPathKeys();
    if (!keys.empty())
        selectedPath_ = keys[0];

    rebuildPathDropdown();
    notifyChainChanged();

    auto* chain = getActiveChain();
    graph_.setChain(chain); phaseGraph_.setChain(chain);
    refreshTabs();
    if (chain != nullptr && chain->getNumBands() > 0)
        selectBand(0);
    else
        selectBand(-1);

    // Update the routing overview in-place
    std::map<PathKey, int> bandCounts;
    for (auto& key : keys)
    {
        auto* c = getProcessor()->getChainForPath(key.first, key.second);
        if (c != nullptr)
            bandCounts[key] = c->getNumBands();
    }
    for (int i = 0; i < Desktop::getInstance().getNumComponents(); ++i)
    {
        auto* comp = Desktop::getInstance().getComponent(i);
        if (auto* callout = dynamic_cast<CallOutBox*>(comp))
        {
            for (int c = 0; c < callout->getNumChildComponents(); ++c)
            {
                if (auto* wrapper = dynamic_cast<ScrollableRoutingOverview*>(callout->getChildComponent(c)))
                {
                    wrapper->updatePaths(keys, bandCounts);
                    break;
                }
            }
        }
    }
}

void Mcfx_mimoeqAudioProcessorEditor::routingDiagonalRequested()
{
    // Dismiss the routing popup
    if (routingCallOut_ != nullptr)
    {
        routingCallOut_->setVisible(false);
        routingCallOut_->dismiss();
    }

    // Switch to diagonal mode
    btnModeDiag_.setToggleState(true, dontSendNotification);
    btnModeMIMO_.setToggleState(false, dontSendNotification);
    updatePathSelector();
    auto* chain = getActiveChain();
    graph_.setChain(chain); phaseGraph_.setChain(chain);
    refreshTabs();
    if (chain != nullptr && chain->getNumBands() > 0)
        selectBand(0);
    else
        selectBand(-1);
    updateAnalyzerState();
}

void Mcfx_mimoeqAudioProcessorEditor::updateDiagChannelButton()
{
    auto& mask = getProcessor()->getDiagChannelMask();
    int numCh = jmax(1, getProcessor()->getNumChannels_());

    if (mask.empty())
    {
        btnDiagChans_.setButtonText("Ch: All");
        return;
    }

    // Count valid channels (ignore sentinel 0)
    int validCount = 0;
    for (int ch : mask)
        if (ch >= 1 && ch <= numCh) validCount++;

    if (validCount == 0)
        btnDiagChans_.setButtonText("Ch: None");
    else if (validCount == numCh)
        btnDiagChans_.setButtonText("Ch: All");
    else if (validCount <= 3)
    {
        String txt = "Ch:";
        for (int ch : mask)
            if (ch >= 1) txt << " " << ch;
        btnDiagChans_.setButtonText(txt);
    }
    else
        btnDiagChans_.setButtonText("Ch: " + String(validCount) + "/" + String(numCh));
}

void Mcfx_mimoeqAudioProcessorEditor::showDiagChannelPopup()
{
    int numCh = jmax(1, getProcessor()->getNumChannels_());
    auto& mask = getProcessor()->getDiagChannelMask();

    auto* selector = new ChannelSelectorComponent(numCh, mask, this);
    auto& box = CallOutBox::launchAsynchronously(std::unique_ptr<Component>(selector),
                                                  btnDiagChans_.getScreenBounds(),
                                                  nullptr);
    diagChannelCallOut_ = &box;
}

void Mcfx_mimoeqAudioProcessorEditor::diagChannelMaskChanged(const std::set<int>& mask)
{
    auto& current = getProcessor()->getDiagChannelMask();
    if (mask != current)
    {
        getProcessor()->pushUndoState();
        updateUndoRedoButtons();
        getProcessor()->setDiagChannelMask(mask);
        updateDiagChannelButton();
        notifyChainChanged();
    }
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
        graph_.setChain(chain); phaseGraph_.setChain(chain);
        refreshTabs();
        if (chain != nullptr && chain->getNumBands() > 0)
            selectBand(jlimit(0, chain->getNumBands() - 1, selectedBand_));
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
        graph_.setChain(chain); phaseGraph_.setChain(chain);
        refreshTabs();
        if (chain != nullptr && chain->getNumBands() > 0)
            selectBand(jlimit(0, chain->getNumBands() - 1, selectedBand_));
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

void Mcfx_mimoeqAudioProcessorEditor::sliderValueChanged(Slider* s)
{
    if (s == &sldOffset_)
    {
        getProcessor()->analyzerOffset = (float) sldOffset_.getValue();
        graph_.setAnalyzerOffset((float) sldOffset_.getValue());
    }
}

void Mcfx_mimoeqAudioProcessorEditor::updateAnalyzerState()
{
    bool on = getProcessor()->editorAnalyzerOn;
    getProcessor()->setAnalyzerEnabled(on);
    graph_.setAnalyzerEnabled(on);
    graph_.setAnalyzerAutoNormalize(getProcessor()->analyzerAutoNormalize);
    graph_.setAnalyzerOffset(getProcessor()->analyzerOffset);
    graph_.setSpectrogramMode(getProcessor()->editorSpectrogramOn);
    graph_.setSpectrogramSource(getProcessor()->editorSpectroPost);

    // Keep the display row showing the state it controls, and grey out the parts
    // that do nothing right now: everything analyzer-related when it is off, the
    // pre/post source unless the spectrogram is showing, the offset under auto.
    const bool spectro = getProcessor()->editorSpectrogramOn;
    cbAnalyzerView_.setSelectedId(!on ? 1 : (spectro ? 3 : 2), dontSendNotification);
    cbAnalyzerSource_.setSelectedId(getProcessor()->editorSpectroPost ? 2 : 1,
                                    dontSendNotification);
    cbAnalyzerChannel_.setSelectedId(getProcessor()->editorAnalyzerChannel + 1,
                                     dontSendNotification);
    btnAutoNorm_.setToggleState(getProcessor()->analyzerAutoNormalize, dontSendNotification);
    sldOffset_.setValue(getProcessor()->analyzerOffset, dontSendNotification);

    // Hide what does not apply: the whole set while the analyzer is off, and the
    // pre/post source unless there is a spectrogram for it to act on.
    cbAnalyzerSource_.setVisible(on && spectro);
    lblChannel_.setVisible(on);
    cbAnalyzerChannel_.setVisible(on);
    btnAutoNorm_.setVisible(on);
    lblOffset_.setVisible(on);
    sldOffset_.setVisible(on);

    // In MIMO mode the channel follows the selected path, so it is not editable.
    cbAnalyzerChannel_.setEnabled(diagonalMode_);
    sldOffset_.setEnabled(!getProcessor()->analyzerAutoNormalize);

    resized();   // the row closes up around whatever is hidden

    // In MIMO mode, lock analyzers to the selected path's channels
    if (!diagonalMode_)
    {
        getProcessor()->getInputAnalyzer().setAnalyzerChannel(selectedPath_.first);
        getProcessor()->getOutputAnalyzer().setAnalyzerChannel(selectedPath_.second);
    }
    else
    {
        // In diagonal mode, both analyzers use the same user-selected channel
        int ch = getProcessor()->editorAnalyzerChannel;
        getProcessor()->getInputAnalyzer().setAnalyzerChannel(ch);
        getProcessor()->getOutputAnalyzer().setAnalyzerChannel(ch);
    }

    if (!on)
    {
        getProcessor()->getInputAnalyzer().reset();
        getProcessor()->getOutputAnalyzer().reset();
    }
}

void Mcfx_mimoeqAudioProcessorEditor::updatePhaseGraphVisibility()
{
    bool on = btnPhase_.getToggleState();
    phaseGraph_.setVisible (on);
    if (on)
        phaseGraph_.setChain (getActiveChain());
    resized();   // re-layout to allocate / reclaim space below the magnitude graph
}

//==============================================================================
// Presets menu
//
// One toolbar button replaces the old Load + Save pair. Named presets live as
// JSON files under userApplicationDataDirectory/mcfx_mimoeq/presets and are
// listed in the menu directly so the user can load one in a single click.
// All the prompts are async (juce::AlertWindow::enterModalState), keeping the
// audio thread / host UI responsive.

void Mcfx_mimoeqAudioProcessorEditor::savePresetFile(const File& file)
{
    if (getProcessor()->saveConfigToFile(file))
        statusBar_.setText("Saved " + file.getFileName(), dontSendNotification);
    else
        statusBar_.setText("Save failed", dontSendNotification);
}

void Mcfx_mimoeqAudioProcessorEditor::loadPresetFile(const File& file)
{
    // Mirror the pre-load ritual that the old btnLoad_ handler and the
    // drag-and-drop handler both did: push an undo snapshot, swap chains,
    // refresh the tabs, and reselect band 0 (or -1 when the chain is empty).
    dragUndoPushed_ = false;
    getProcessor()->pushUndoState();
    updateUndoRedoButtons();

    if (! getProcessor()->loadConfigFromFile(file))
    {
        statusBar_.setText("Load failed", dontSendNotification);
        return;
    }

    auto* chain = getActiveChain();
    graph_.setChain(chain); phaseGraph_.setChain(chain);
    refreshTabs();
    selectBand((chain != nullptr && chain->getNumBands() > 0) ? 0 : -1);
    statusBar_.setText("Loaded " + file.getFileName(), dontSendNotification);
}

namespace
{
    // Start the file pickers in the user's preset folder when it exists, so
    // the chooser drops them right where named presets live; otherwise fall
    // back to the home directory (matches the previous default of File()).
    File pickStartDir(const PresetManager& pm)
    {
        auto dir = pm.getPresetDir();
        if (dir.isDirectory()) return dir;
        return File::getSpecialLocation(File::userHomeDirectory);
    }

    // After an AlertWindow with a text-editor field enters its modal state,
    // focus that text editor and select its contents so the user can start
    // typing immediately. Has to be posted via callAsync because
    // grabKeyboardFocus() only takes effect once the AlertWindow is on
    // screen, and the modal-state machinery may still be settling when
    // enterModalState() returns. SafePointer protects against the user
    // dismissing the popup before the async callback runs.
    void focusAlertTextEditor(AlertWindow* aw, const String& editorName)
    {
        Component::SafePointer<AlertWindow> safe(aw);
        MessageManager::callAsync([safe, editorName]
        {
            if (auto* w = safe.getComponent())
                if (auto* ed = w->getTextEditor(editorName))
                {
                    ed->grabKeyboardFocus();
                    ed->selectAll();
                }
        });
    }
}

void Mcfx_mimoeqAudioProcessorEditor::showPresetsMenu()
{
    PopupMenu m;

    m.addItem("Load from file...", [this]
    {
        chooser_ = std::make_unique<FileChooser>(
                       "Load EQ Config",
                       pickStartDir(presets_),
                       "*.json");

        auto flags = FileBrowserComponent::openMode
                   | FileBrowserComponent::canSelectFiles;

        chooser_->launchAsync(flags, [this](const FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == File()) return;
            loadPresetFile(file);
        });
    });

    m.addItem("Save to file...", [this]
    {
        chooser_ = std::make_unique<FileChooser>(
                       "Save EQ Config",
                       pickStartDir(presets_).getChildFile("mcfx_mimoeq.json"),
                       "*.json");

        auto flags = FileBrowserComponent::saveMode
                   | FileBrowserComponent::canSelectFiles
                   | FileBrowserComponent::warnAboutOverwriting;

        chooser_->launchAsync(flags, [this](const FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == File()) return;
            savePresetFile(file);
        });
    });

    m.addSeparator();

    m.addItem("Save as named preset...", [this] { promptSaveAsNamedPreset(); });

    m.addSeparator();

    const auto entries = presets_.listPresets();
    if (entries.empty())
    {
        m.addItem("(no presets yet)", false, false, [] {});
    }
    else
    {
        for (const auto& e : entries)
        {
            auto file = e.file;
            m.addItem(e.name, [this, file] { loadPresetFile(file); });
        }
    }

    m.addSeparator();

    PopupMenu renameMenu, deleteMenu;
    for (const auto& e : entries)
    {
        auto file = e.file;
        renameMenu.addItem(e.name, [this, file] { promptRenamePreset(file); });
        deleteMenu.addItem(e.name, [this, file] { confirmDeletePreset(file); });
    }
    m.addSubMenu("Rename preset", renameMenu, ! entries.empty());
    m.addSubMenu("Delete preset", deleteMenu, ! entries.empty());

    m.addSeparator();

    m.addItem("Reveal preset folder", [this]
    {
        presets_.ensurePresetDirExists();
        presets_.getPresetDir().revealToUser();
    });

    m.showMenuAsync(PopupMenu::Options().withTargetComponent(&btnPresets_));
}

void Mcfx_mimoeqAudioProcessorEditor::promptSaveAsNamedPreset()
{
    alertWindow_ = std::make_unique<AlertWindow>(
                       "Save preset",
                       "Enter a name for this preset:",
                       MessageBoxIconType::QuestionIcon);

    alertWindow_->addTextEditor("name", "", {}, false);
    alertWindow_->addButton("Save",   1, KeyPress(KeyPress::returnKey));
    alertWindow_->addButton("Cancel", 0, KeyPress(KeyPress::escapeKey));

    alertWindow_->enterModalState(true,
        ModalCallbackFunction::create([this](int result)
        {
            if (result == 0 || alertWindow_ == nullptr) { alertWindow_.reset(); return; }

            const auto raw   = alertWindow_->getTextEditorContents("name");
            const auto clean = PresetManager::sanitise(raw);
            alertWindow_.reset();

            if (clean.isEmpty())
            {
                statusBar_.setText("Preset name is empty", dontSendNotification);
                return;
            }

            if (! presets_.ensurePresetDirExists())
            {
                statusBar_.setText("Could not create preset folder", dontSendNotification);
                return;
            }

            const auto file = presets_.fileForName(clean);

            if (file.existsAsFile())
            {
                alertWindow_ = std::make_unique<AlertWindow>(
                                   "Overwrite preset?",
                                   "A preset named \"" + clean + "\" already exists.\n"
                                   "Overwrite it?",
                                   MessageBoxIconType::WarningIcon);
                alertWindow_->addButton("Overwrite", 1, KeyPress(KeyPress::returnKey));
                alertWindow_->addButton("Cancel",    0, KeyPress(KeyPress::escapeKey));
                alertWindow_->enterModalState(true,
                    ModalCallbackFunction::create([this, file](int r)
                    {
                        alertWindow_.reset();
                        if (r != 1) return;
                        savePresetFile(file);
                    }), false);
                return;
            }

            savePresetFile(file);
        }), false);

    focusAlertTextEditor(alertWindow_.get(), "name");
}

void Mcfx_mimoeqAudioProcessorEditor::promptRenamePreset(const File& file)
{
    const auto oldName = file.getFileNameWithoutExtension();

    alertWindow_ = std::make_unique<AlertWindow>(
                       "Rename preset",
                       "Enter a new name for \"" + oldName + "\":",
                       MessageBoxIconType::QuestionIcon);

    alertWindow_->addTextEditor("name", oldName, {}, false);
    alertWindow_->addButton("Rename", 1, KeyPress(KeyPress::returnKey));
    alertWindow_->addButton("Cancel", 0, KeyPress(KeyPress::escapeKey));

    alertWindow_->enterModalState(true,
        ModalCallbackFunction::create([this, file, oldName](int result)
        {
            if (result == 0 || alertWindow_ == nullptr) { alertWindow_.reset(); return; }

            const auto raw   = alertWindow_->getTextEditorContents("name");
            const auto clean = PresetManager::sanitise(raw);
            alertWindow_.reset();

            if (clean.isEmpty() || clean == oldName) return;

            const auto target = presets_.fileForName(clean);
            if (target.existsAsFile())
            {
                statusBar_.setText("A preset named \"" + clean + "\" already exists",
                                   dontSendNotification);
                return;
            }

            if (file.moveFileTo(target))
                statusBar_.setText("Renamed to " + target.getFileName(),
                                   dontSendNotification);
            else
                statusBar_.setText("Rename failed", dontSendNotification);
        }), false);

    focusAlertTextEditor(alertWindow_.get(), "name");
}

void Mcfx_mimoeqAudioProcessorEditor::confirmDeletePreset(const File& file)
{
    alertWindow_ = std::make_unique<AlertWindow>(
                       "Delete preset?",
                       "Delete preset \"" + file.getFileNameWithoutExtension() + "\"?\n"
                       "This cannot be undone.",
                       MessageBoxIconType::WarningIcon);
    alertWindow_->addButton("Delete", 1, KeyPress(KeyPress::returnKey));
    alertWindow_->addButton("Cancel", 0, KeyPress(KeyPress::escapeKey));

    alertWindow_->enterModalState(true,
        ModalCallbackFunction::create([this, file](int result)
        {
            alertWindow_.reset();
            if (result != 1) return;

            if (file.deleteFile())
                statusBar_.setText("Deleted " + file.getFileName(), dontSendNotification);
            else
                statusBar_.setText("Delete failed", dontSendNotification);
        }), false);
}
