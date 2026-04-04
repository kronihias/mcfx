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

#ifndef IR_INSPECTOR_COMPONENT_H_INCLUDED
#define IR_INSPECTOR_COMPONENT_H_INCLUDED

#include "JuceHeader.h"
#include "ConvolverData.h"

class Mcfx_convolverAudioProcessor;

//==============================================================================
// Displays time-domain and frequency-domain plots for a single IR
class IrPlotComponent : public Component
{
public:
    IrPlotComponent();

    void setIR(const AudioBuffer<float>* irData, double sampleRate, int inCh, int outCh);
    void clearIR();

    void paint(Graphics& g) override;
    void resized() override;

private:
    void computeFrequencyResponse();
    void drawTimeDomain(Graphics& g, Rectangle<int> area);
    void drawFrequencyDomain(Graphics& g, Rectangle<int> area);
    void drawGrid(Graphics& g, Rectangle<int> area, bool isFreqDomain);
    void drawDashedLine(Graphics& g, float x1, float y1, float x2, float y2);

    // helper to map frequency (Hz) to x position (log scale)
    float hzToX(float hz, float xLeft, float width, float minHz, float maxHz) const;
    // helper to map dB to y position
    float dbToY(float db, float yTop, float height, float minDb, float maxDb) const;

    const AudioBuffer<float>* currentIR = nullptr;
    double sampleRate_ = 44100.0;
    int inCh_ = -1;
    int outCh_ = -1;

    std::vector<float> magnitudeDb_; // frequency domain magnitude in dB
    float freqResolution_ = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IrPlotComponent)
};

//==============================================================================
// Matrix grid showing in/out IR combinations
class IrMatrixComponent : public Component,
                          public TooltipClient
{
public:
    IrMatrixComponent();

    void updateMatrix(ConvolverData& convData);
    void setSelectedCell(int in, int out);

    std::function<void(int inCh, int outCh)> onCellClicked;

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseMove(const MouseEvent& e) override;
    void mouseExit(const MouseEvent& e) override;

    String getTooltip() override
    {
        if (hoveredIn_ >= 0 && hoveredOut_ >= 0)
            return "In " + String(hoveredIn_ + 1) + ", Out " + String(hoveredOut_ + 1);
        return {};
    }

private:
    struct CellInfo {
        int irIndex = -1; // index into ConvolverData, or -1 if no IR
    };

    int numInputs_ = 0;
    int numOutputs_ = 0;
    int selectedIn_ = -1;
    int selectedOut_ = -1;
    int hoveredIn_ = -1;
    int hoveredOut_ = -1;

    std::vector<std::vector<CellInfo>> matrix_; // [out][in]

    static constexpr int cellSize = 18;
    static constexpr int headerSize = 28;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IrMatrixComponent)
};

//==============================================================================
// Main inspector component combining matrix + plots
class IrInspectorComponent : public Component,
                             public ChangeListener
{
public:
    IrInspectorComponent(Mcfx_convolverAudioProcessor& processor);
    ~IrInspectorComponent() override;

    void paint(Graphics& g) override;
    void resized() override;

    void changeListenerCallback(ChangeBroadcaster* source) override;

    void refresh();

private:
    Mcfx_convolverAudioProcessor& processor_;

    IrMatrixComponent matrixComponent_;
    Viewport matrixViewport_;
    IrPlotComponent plotComponent_;
    Label titleLabel_;
    Label infoLabel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IrInspectorComponent)
};

//==============================================================================
// Popup window wrapper
class IrInspectorWindow : public DocumentWindow
{
public:
    IrInspectorWindow(Mcfx_convolverAudioProcessor& processor);
    ~IrInspectorWindow() override;

    void closeButtonPressed() override;
    void moved() override;
    void resized() override;

private:
    Mcfx_convolverAudioProcessor& processor_;
    TooltipWindow tooltipWindow_ { this, 500 };
    void saveBounds();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IrInspectorWindow)
};

#endif // IR_INSPECTOR_COMPONENT_H_INCLUDED
