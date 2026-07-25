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
#include "CQTAnalyzer.h"

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

    /** Provide live dynamic gain offsets (dB) per band index for metering. Returns
        0 (or leave unset) when the band has no dynamic action to display. */
    void setLiveDynOffsetProvider(std::function<float(int)> fn) { liveDynOffsetProvider_ = std::move(fn); }

    /** Set spectrum analyzers to overlay on the graph. Pass nullptr to disable. */
    void setAnalyzers(SpectrumAnalyzer* input, SpectrumAnalyzer* output)
    {
        inputAnalyzer_ = input;
        outputAnalyzer_ = output;
    }
    void setAnalyzerEnabled(bool on) { analyzerOn_ = on; }
    void setAnalyzerAutoNormalize(bool on) { analyzerAutoNormalize_ = on; }
    void setAnalyzerOffset(float dbOffset) { analyzerOffset_ = dbOffset; }

    /** Rolling spectrogram view (X = frequency, Y = time, colour = level) drawn
        behind the EQ curve, instead of the spectrum overlay. Off by default. */
    void setSpectrogramMode(bool on);
    bool getSpectrogramMode() const { return spectrogramMode_; }
    /** Constant-Q analyzer used for the spectrogram (log-spaced bins). When set,
        the waterfall reads it instead of the linear-bin spectrum analyzer. */
    void setCQTAnalyzer(CQTAnalyzer* c) { cqt_ = c; }

    /** Which signal the spectrogram shows: false = pre-EQ (input), true = post-EQ. */
    void setSpectrogramSource(bool post) { spectroPost_ = post; }
    bool getSpectrogramSource() const { return spectroPost_; }

    static Colour getBandColour(int bandIndex);

    void paint(Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void mouseDown(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;
    void mouseDoubleClick(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseMove(const MouseEvent& e) override;
    void mouseExit(const MouseEvent& e) override;
    void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) override;
    bool keyPressed(const KeyPress& key) override;

private:
    int dbtoypos(float db_val) const;
    float ypostodb(int ypos) const;
    int hztoxpos(float hz_val) const;
    float xpostohz(int xpos) const;

    void calcPaths();
    void calcAnalyzerPaths();
    void writeSpectroRow();
    void drawSpectrogram(Graphics& g, Rectangle<int> plot);
    Colour spectroColour(float v01) const;
    void rebuildGridPaths();
    void drawGrid(Graphics& g);
    void drawBandHandles(Graphics& g);
    int findBandAtPosition(Point<int> pos) const;

    EqChain* chain_ = nullptr;
    Listener* listener_ = nullptr;
    std::function<float(int)> liveDynOffsetProvider_;
    int selectedBand_ = -1;
    int dragBand_ = -1;
    bool draggingYAxis_ = false;     // true when dragging the Y-axis (pan)
    float dragStartMinDb_ = 0.f;
    float dragStartMaxDb_ = 0.f;
    int dragStartY_ = 0;

    // Hover-cursor freq/mag readout in the bottom-right corner
    int hoverX_ = -1;

    // Graph refresh: fast while animating (analyzer / live dynamic band), slow when
    // idle. ~30 fps reads as smooth for a meter; idle ~10 fps is just a safety tick
    // (edits/mouse repaint directly). Bump kRefreshFastMs to 16 for 60 fps if wanted.
    static constexpr int kRefreshFastMs = 33;   // ~30 fps
    static constexpr int kRefreshIdleMs = 100;  // ~10 fps

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
    CQTAnalyzer* cqt_ = nullptr;   // spectrogram source (log-spaced bins)
    Path pathAnalyzerIn_;
    Path pathAnalyzerOut_;

    // Rolling spectrogram (fixed-resolution ring-buffer image, scaled to the plot
    // so it survives resizes). Default off; populated on the refresh timer.
    bool spectrogramMode_ = false;
    bool spectroPost_ = false;                 // false = input (pre), true = output (post)
    static constexpr int kSpecW = 1024;        // log-frequency columns
    static constexpr int kSpecH = 512;         // history rows
    static constexpr float kSpectroRangeDb = 72.f;  // colour scale spans 0 .. -72 dB
    Image spectro_;
    int specWrite_ = 0;
    std::vector<float> specSmoothDb_;          // per-column temporal smoothing

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqGraph)
};

#endif // EQGRAPH_H_INCLUDED
