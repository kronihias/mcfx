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

#ifndef EQGRAPH_H_INCLUDED
#define EQGRAPH_H_INCLUDED

#include "JuceHeader.h"
#include "EqChain.h"
#include "SpectrumAnalyzer.h"

class EqGraph : public Component,
                public SettableTooltipClient,
                public Timer
{
public:

    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void eqBandDragged(int bandIndex, float newFreqHz, float newGainDB) = 0;
        virtual void eqBandQChanged(int bandIndex, float newQ) = 0;
        virtual void eqBandSelected(int bandIndex) = 0;
        virtual void eqBandEnableToggled(int bandIndex) = 0;
        virtual void eqBandDeleteRequested(int bandIndex) = 0;
        virtual void eqBandDoubleClicked(float freqHz, float gainDB) = 0;
        virtual void eqUndoRequested() = 0;
        virtual void eqRedoRequested() = 0;
    };

    EqGraph();
    ~EqGraph();

    void setChain(EqChain* chain) { chain_ = chain; }
    void setListener(Listener* l) { listener_ = l; }
    void setSelectedBand(int idx) { selectedBand_ = idx; repaint(); }

    /** Set spectrum analyzers to overlay on the graph. Pass nullptr to disable. */
    void setAnalyzers(SpectrumAnalyzer* input, SpectrumAnalyzer* output)
    {
        inputAnalyzer_ = input;
        outputAnalyzer_ = output;
    }
    void setAnalyzerEnabled(bool on) { analyzerOn_ = on; }
    void setAnalyzerAutoNormalize(bool on) { analyzerAutoNormalize_ = on; }
    void setAnalyzerOffset(float dbOffset) { analyzerOffset_ = dbOffset; }

    static Colour getBandColour(int bandIndex);

    void paint(Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void mouseDown(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;
    void mouseDoubleClick(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) override;
    bool keyPressed(const KeyPress& key) override;

private:
    int dbtoypos(float db_val) const;
    float ypostodb(int ypos) const;
    int hztoxpos(float hz_val) const;
    float xpostohz(int xpos) const;

    void calcPaths();
    void calcAnalyzerPaths();
    void rebuildGridPaths();
    void drawGrid(Graphics& g);
    void drawBandHandles(Graphics& g);
    int findBandAtPosition(Point<int> pos) const;

    EqChain* chain_ = nullptr;
    Listener* listener_ = nullptr;
    int selectedBand_ = -1;
    int dragBand_ = -1;
    bool draggingYAxis_ = false;     // true when dragging the Y-axis (pan)
    float dragStartMinDb_ = 0.f;
    float dragStartMaxDb_ = 0.f;
    int dragStartY_ = 0;

    float minf_ = 20.f;
    float maxf_ = 20000.f;
    float mindb_ = -24.f;
    float maxdb_ = 24.f;
    float gridDiv_ = 6.f;
    float xmargin_ = 35.f;
    float ymargin_ = 12.f;

    Path pathMag_;
    Path pathFill_;             // filled area under combined response
    std::vector<Path> bandPaths_;     // individual band magnitude curves
    std::vector<Path> bandFills_;     // filled areas for individual bands
    Path pathGrid_;
    Path pathGridW_;

    // Spectrum analyzer
    SpectrumAnalyzer* inputAnalyzer_ = nullptr;
    SpectrumAnalyzer* outputAnalyzer_ = nullptr;
    bool analyzerOn_ = false;
    bool analyzerAutoNormalize_ = true;
    float analyzerOffset_ = 0.f;
    Path pathAnalyzerIn_;
    Path pathAnalyzerOut_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqGraph)
};

#endif // EQGRAPH_H_INCLUDED
