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

#ifndef PLUGINEDITOR_H_INCLUDED
#define PLUGINEDITOR_H_INCLUDED

#include "JuceHeader.h"
#include "PluginProcessor.h"
#include "EqGraph.h"
#include "EqBandEditor.h"
#include "RoutingOverview.h"

class Mcfx_mimoeqAudioProcessorEditor;

class EqTabBar : public TabbedButtonBar
{
public:
    EqTabBar(Mcfx_mimoeqAudioProcessorEditor* owner)
        : TabbedButtonBar(TabsAtTop), owner_(owner) {}
    void currentTabChanged(int newIndex, const String&) override;
private:
    Mcfx_mimoeqAudioProcessorEditor* owner_;
};

class Mcfx_mimoeqAudioProcessorEditor : public AudioProcessorEditor,
                                          public ChangeListener,
                                          public ComboBox::Listener,
                                          public EqGraph::Listener,
                                          public EqBandEditor::Listener,
                                          public Button::Listener,
                                          public FileDragAndDropTarget,
                                          public RoutingOverviewComponent::Listener
{
    friend class EqTabBar;
public:
    Mcfx_mimoeqAudioProcessorEditor(Mcfx_mimoeqAudioProcessor* processor);
    ~Mcfx_mimoeqAudioProcessorEditor();

    void paint(Graphics& g) override;
    void resized() override;

    void changeListenerCallback(ChangeBroadcaster* source) override;
    void comboBoxChanged(ComboBox* cb) override;

    // EqGraph::Listener
    void eqBandDragged(int bandIndex, float newFreqHz, float newGainDB) override;
    void eqBandQChanged(int bandIndex, float newQ) override;
    void eqBandSelected(int bandIndex) override;
    void eqBandEnableToggled(int bandIndex) override;
    void eqBandDeleteRequested(int bandIndex) override;
    void eqBandDoubleClicked(float freqHz, float gainDB) override;
    void eqUndoRequested() override;
    void eqRedoRequested() override;

    // EqBandEditor::Listener
    void bandParameterChanged(int bandIndex) override;
    void bandEnableChanged(int bandIndex, bool enabled) override;
    void bandStructureChanged(int bandIndex) override;

    // Button::Listener
    void buttonClicked(Button* b) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const StringArray& files) override;
    void filesDropped(const StringArray& files, int x, int y) override;

    // RoutingOverviewComponent::Listener
    void routingPathSelected(int inCh, int outCh) override;

private:
    Mcfx_mimoeqAudioProcessor* getProcessor() const
    {
        return static_cast<Mcfx_mimoeqAudioProcessor*>(getAudioProcessor());
    }

    EqChain* getActiveChain();
    void refreshTabs();
    void selectBand(int index);
    void notifyChainChanged();       // full rebuild (structural changes: add/remove band, type change)
    void notifyParameterChanged();   // lightweight sync (freq/Q/gain/enable changes)
    void updatePathSelector();

    // Mode & path selection
    TextButton btnModeDiag_    { "Diagonal" };
    TextButton btnModeMIMO_    { "MIMO" };
    ComboBox   cbPathSelector_;
    TextButton btnAddPath_     { "Add Path" };
    TextButton btnRemovePath_  { "Remove Path" };
    TextButton btnRouting_     { "Routing" };
    PathKey    selectedPath_   { 1, 1 };

    void rebuildPathDropdown();
    void showAddPathPopup();
    void showRoutingOverview();

    EqGraph graph_;
    EqBandEditor bandEditor_;
    EqTabBar tabs_;

    TextButton btnAdd_ { "+" };
    TextButton btnRemove_ { "-" };
    TextButton btnUndo_ { "Undo" };
    TextButton btnRedo_ { "Redo" };
    TextButton btnLoad_ { "Load" };
    TextButton btnSave_ { "Save" };

    Label lblTitle_;

    int selectedBand_ = -1;
    bool diagonalMode_ = true; // true = editing diagonal chain
    bool refreshingTabs_ = false; // reentrance guard
    bool dragUndoPushed_ = false; // coalescing: only push undo once per drag gesture

    void performUndo();
    void performRedo();
    void updateUndoRedoButtons();

    struct TabLookAndFeel : public LookAndFeel_V4
    {
        Font getTabButtonFont(TabBarButton&, float height) override
        {
            return Font(jmin(height * 0.6f, 14.f), Font::plain);
        }

        void drawTabButton(TabBarButton& button, Graphics& g,
                           bool isMouseOver, bool isMouseDown) override
        {
            auto area = button.getActiveArea();
            auto tabColour = button.getTabBackgroundColour();
            bool isFront = button.isFrontTab();

            // Background: black for selected, band colour (darkened) for unselected
            g.setColour(isFront ? Colour(0xff111111) : tabColour.darker(0.5f));
            g.fillRoundedRectangle(area.toFloat(), 4.f);

            // Color stripe at top of tab
            auto stripe = area.toFloat().removeFromTop(3.f);
            g.setColour(isFront ? tabColour : tabColour.withAlpha(0.5f));
            g.fillRoundedRectangle(stripe, 2.f);

            // Text: band colour for selected (bold, bigger), white dimmed for unselected
            float fontSize = isFront ? jmin(area.getHeight() * 0.62f, 14.f)
                                     : jmin(area.getHeight() * 0.5f, 12.f);
            g.setFont(Font(fontSize, isFront ? Font::bold : Font::plain));
            g.setColour(isFront ? tabColour : Colours::white.withAlpha(0.6f));
            g.drawText(button.getButtonText(), area, Justification::centred, false);
        }
    };

    TabLookAndFeel tabLookAndFeel_;
    LookAndFeel_V4 lookAndFeel_;
    Label statusBar_;

    void mouseEnter(const MouseEvent& e) override;
    void mouseExit(const MouseEvent& e) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Mcfx_mimoeqAudioProcessorEditor)
};

#endif
