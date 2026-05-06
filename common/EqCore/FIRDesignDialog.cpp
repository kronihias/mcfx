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

#include "FIRDesignDialog.h"

#include <cmath>

//==============================================================================
// MagPreview — small log-frequency magnitude curve
//==============================================================================
void FIRDesignDialog::MagPreview::setCoefficients (const std::vector<float>& c, double sr)
{
    coeffs_ = c;
    sampleRate_ = sr;
    repaint();
}

void FIRDesignDialog::MagPreview::paint (Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    // Background — radial gradient matching EqGraph in the main editor
    g.setGradientFill (ColourGradient (Colour (0xff232338), area.getCentreX(), area.getCentreY(),
                                         Colour (0xff21222a), 2.5f, area.getCentreY(), true));
    g.fillRoundedRectangle (area, 4.f);

    if (coeffs_.empty()) return;

    // Layout: leave room for dB labels on the left and freq labels at the bottom.
    const float padL = 38.f, padR = 6.f, padT = 6.f, padB = 14.f;
    auto plot = Rectangle<float> (area.getX() + padL, area.getY() + padT,
                                   area.getWidth() - padL - padR,
                                   area.getHeight() - padT - padB);

    const double fLo  = 20.0;
    const double fHi  = jmin (20000.0, sampleRate_ * 0.5);
    const double dbMax = (double) dbMax_;
    const double dbMin = (double) dbMin_;
    const float  dbStep = dbStep_;   // grid spacing in dB (adapts on zoom)

    auto freqToX = [&] (double f) {
        double t = (std::log10 (f) - std::log10 (fLo)) / (std::log10 (fHi) - std::log10 (fLo));
        return plot.getX() + (float) t * plot.getWidth();
    };
    auto dbToY = [&] (double db) {
        double t = (db - dbMin) / (dbMax - dbMin);
        return plot.getBottom() - (float) t * plot.getHeight();
    };

    // ----- Grid -----
    Path gridDim, gridBright;

    // dB grid: every dbStep, with 0 dB on the bright path
    float firstDb = std::ceil (dbMin / dbStep) * dbStep;
    for (float db = firstDb; db <= dbMax; db += dbStep)
    {
        float y = dbToY (db);
        if (std::abs (db) < 0.01f) { gridBright.startNewSubPath (plot.getX(), y); gridBright.lineTo (plot.getRight(), y); }
        else                        { gridDim   .startNewSubPath (plot.getX(), y); gridDim   .lineTo (plot.getRight(), y); }
    }

    // Frequency grid: every step within each decade (20, 30, ..., 100, 200, ..., 1000, ...)
    // Major decades and half-decades go on the bright path with labels.
    auto isMajor = [] (double f) {
        return f == 50 || f == 100 || f == 500 || f == 1000 || f == 5000 || f == 10000;
    };
    for (double f = fLo; f <= fHi; f += std::pow (10.0, std::floor (std::log10 (f))))
    {
        float x = freqToX (f);
        if (isMajor (f)) { gridBright.startNewSubPath (x, plot.getY()); gridBright.lineTo (x, plot.getBottom()); }
        else              { gridDim   .startNewSubPath (x, plot.getY()); gridDim   .lineTo (x, plot.getBottom()); }
    }

    g.setColour (Colour (0x60ffffff));
    g.strokePath (gridDim,    PathStrokeType (0.25f));
    g.setColour (Colour (0xffffffff));
    g.strokePath (gridBright, PathStrokeType (0.25f));

    // ----- Labels -----
    g.setFont (Font (FontOptions ("Arial Rounded MT", 12.f, Font::plain)));
    g.setColour (Colour (0x60ffffff));

    // dB labels — at every major step, anchored to the grid line
    for (float db = firstDb; db <= dbMax; db += dbStep)
    {
        int y = (int) dbToY (db);
        String s; s << (int) db << "dB";
        g.drawText (s, 0, y - 6, (int) padL - 2, 12, Justification::centredRight, false);
    }

    // Frequency labels — at the major grid lines
    for (double f = fLo; f <= fHi; f += std::pow (10.0, std::floor (std::log10 (f))))
    {
        if (! isMajor (f)) continue;
        int x = (int) freqToX (f);
        g.drawText (String ((int) f), x - 22, (int) area.getBottom() - 12, 45, 12,
                    Justification::centred, false);
    }

    // ----- Magnitude curve — analytical via DFT of the FIR -----
    const int N = (int) coeffs_.size();
    const int npts = jmax (256, (int) plot.getWidth() * 2);

    Path path;
    bool started = false;
    for (int i = 0; i < npts; ++i)
    {
        double t = (double) i / (double) (npts - 1);
        double f = fLo * std::pow (fHi / fLo, t);
        double w = 2.0 * MathConstants<double>::pi * f / sampleRate_;
        std::complex<double> H (0.0, 0.0);
        for (int n = 0; n < N; ++n)
            H += (double) coeffs_[(size_t) n] * std::polar (1.0, -w * (double) n);
        double mag = std::abs (H);
        double db = (mag > 1e-10) ? 20.0 * std::log10 (mag) : -200.0;
        db = jlimit (dbMin, dbMax, db);
        float x = freqToX (f);
        float y = dbToY (db);
        if (! started) { path.startNewSubPath (x, y); started = true; }
        else            path.lineTo (x, y);
    }

    g.setColour (Colour (0xddffffff));
    g.strokePath (path, PathStrokeType (2.f));
}

float FIRDesignDialog::MagPreview::yToDb (int y) const
{
    const float padT = 6.f, padB = 14.f;
    float plotH = (float) getHeight() - padT - padB;
    if (plotH <= 0.f) return dbMin_;
    float frac = ((float) y - padT) / plotH;       // 0 = top, 1 = bottom
    frac = jlimit (0.f, 1.f, frac);
    return dbMax_ - frac * (dbMax_ - dbMin_);
}

void FIRDesignDialog::MagPreview::updateGridStep()
{
    float dyn = dbMax_ - dbMin_;
    if      (dyn <= 12.f)  dbStep_ = 1.f;
    else if (dyn <= 24.f)  dbStep_ = 3.f;
    else if (dyn <= 60.f)  dbStep_ = 6.f;
    else if (dyn <= 120.f) dbStep_ = 12.f;
    else                    dbStep_ = 24.f;
}

void FIRDesignDialog::MagPreview::mouseWheelMove (const MouseEvent& e,
                                                    const MouseWheelDetails& wheel)
{
    float dyn = dbMax_ - dbMin_;
    float zoom = jlimit (0.8f, 1.25f, 1.f - wheel.deltaY * 0.15f);
    float newDyn = jlimit (6.f, 200.f, dyn * zoom);

    // Zoom around the dB value at the mouse Y position
    float mouseDb = yToDb (e.getPosition().getY());
    float ratio   = (mouseDb - dbMin_) / dyn;

    dbMin_ = mouseDb -        ratio  * newDyn;
    dbMax_ = mouseDb + (1.f - ratio) * newDyn;

    updateGridStep();
    repaint();
}

void FIRDesignDialog::MagPreview::mouseDown (const MouseEvent&)
{
    dragStartMinDb_ = dbMin_;
    dragStartMaxDb_ = dbMax_;
}

void FIRDesignDialog::MagPreview::mouseDrag (const MouseEvent& e)
{
    const float padT = 6.f, padB = 14.f;
    float plotH = (float) getHeight() - padT - padB;
    if (plotH <= 0.f) return;
    float dyn = dragStartMaxDb_ - dragStartMinDb_;
    float deltaDb = (float) e.getDistanceFromDragStartY() * dyn / plotH;
    dbMin_ = dragStartMinDb_ + deltaDb;
    dbMax_ = dragStartMaxDb_ + deltaDb;
    repaint();
}

void FIRDesignDialog::MagPreview::mouseDoubleClick (const MouseEvent&)
{
    dbMin_  = -72.f;
    dbMax_  =  24.f;
    dbStep_ =  12.f;
    repaint();
}

//==============================================================================
// FIRDesignDialog
//==============================================================================
FIRDesignDialog::FIRDesignDialog (double sampleRate,
                                    const var& initialState,
                                    ApplyCallback onApply)
    : sampleRate_ (sampleRate),
      onApply_ (std::move (onApply))
{
    tempBand_.setType (EqBandType::IIR);
    tempBand_.setIIRSubType (IIRSubType::Peak);
    tempBand_.prepare (sampleRate_, 512);

    addAndMakeVisible (lblType_);
    addAndMakeVisible (cbType_);
    cbType_.addItem ("Low Pass",        1);
    cbType_.addItem ("High Pass",       2);
    cbType_.addItem ("Band Pass",       3);
    cbType_.addItem ("Notch",           4);
    cbType_.addItem ("Low Shelf",       5);
    cbType_.addItem ("High Shelf",      6);
    cbType_.addItem ("Peak",            7);
    cbType_.addSeparator();
    // Variable-order linear-phase LP/HP. Used as a complementary pair (same
    // freq / order / length / window) the LP and HP sum to flat magnitude.
    cbType_.addItem ("Crossover LP",   10);
    cbType_.addItem ("Crossover HP",   11);
    cbType_.setSelectedId (7, dontSendNotification);  // Peak by default
    cbType_.setTooltip ("Filter type. Crossover LP + Crossover HP at identical "
                        "settings (freq/order/length/window) sum to flat.");
    cbType_.addListener (this);

    addAndMakeVisible (lblFreq_);
    addAndMakeVisible (sldFreq_);
    sldFreq_.setRange (20.0, jmin (20000.0, sampleRate_ * 0.49), 0.1);
    sldFreq_.setSkewFactorFromMidPoint (1000.0);
    sldFreq_.setNumDecimalPlacesToDisplay (1);
    sldFreq_.setTextBoxStyle (Slider::TextBoxLeft, false, 80, 20);
    sldFreq_.setSliderStyle (Slider::LinearHorizontal);
    sldFreq_.setValue (1000.0, dontSendNotification);
    sldFreq_.addListener (this);

    addAndMakeVisible (lblQ_);
    addAndMakeVisible (sldQ_);
    sldQ_.setRange (0.1, 20.0, 0.001);
    sldQ_.setSkewFactorFromMidPoint (1.0);
    sldQ_.setNumDecimalPlacesToDisplay (3);
    sldQ_.setTextBoxStyle (Slider::TextBoxLeft, false, 70, 20);
    sldQ_.setSliderStyle (Slider::LinearHorizontal);
    sldQ_.setValue (0.707, dontSendNotification);
    sldQ_.addListener (this);

    addAndMakeVisible (lblGain_);
    addAndMakeVisible (sldGain_);
    sldGain_.setRange (-24.0, 24.0, 0.1);
    sldGain_.setNumDecimalPlacesToDisplay (1);
    sldGain_.setTextBoxStyle (Slider::TextBoxLeft, false, 70, 20);
    sldGain_.setSliderStyle (Slider::LinearHorizontal);
    sldGain_.setDoubleClickReturnValue (true, 0.0);
    sldGain_.setTextValueSuffix (" dB");
    sldGain_.setValue (6.0, dontSendNotification);
    sldGain_.addListener (this);

    addAndMakeVisible (lblOrder_);
    addAndMakeVisible (cbOrder_);
    for (int n = 1; n <= 8; ++n)
        cbOrder_.addItem (String (n * 6) + " dB/oct", n);
    cbOrder_.setSelectedId (4, dontSendNotification);
    cbOrder_.addListener (this);

    addAndMakeVisible (lblLength_);
    addAndMakeVisible (cbLength_);
    // Odd lengths (Type-I linear-phase FIRs). Each is one less than a power of two
    // so the internal IFFT stays at a clean 2^k size — no efficiency penalty.
    // Short lengths (15..127) are useful for low-latency crossovers / shaping;
    // longer lengths give sharper transitions at the cost of more group delay.
    for (int N : { 15, 31, 63, 127, 255, 511, 1023, 2047, 4095, 8191, 16383 })
        cbLength_.addItem (String (N) + " taps", N);
    cbLength_.setSelectedId (2047, dontSendNotification);
    cbLength_.addListener (this);

    addAndMakeVisible (lblWindow_);
    addAndMakeVisible (cbWindow_);
    cbWindow_.addItem ("Hann",            1);
    cbWindow_.addItem ("Hamming",         2);
    cbWindow_.addItem ("Blackman",        3);
    cbWindow_.addItem ("Blackman-Harris", 4);
    cbWindow_.addItem ("Kaiser",          5);
    cbWindow_.setSelectedId (5, dontSendNotification);  // Kaiser default
    cbWindow_.addListener (this);

    addAndMakeVisible (lblKaiserBeta_);
    addAndMakeVisible (sldKaiserBeta_);
    sldKaiserBeta_.setRange (0.0, 16.0, 0.1);
    sldKaiserBeta_.setNumDecimalPlacesToDisplay (1);
    sldKaiserBeta_.setTextBoxStyle (Slider::TextBoxLeft, false, 60, 20);
    sldKaiserBeta_.setSliderStyle (Slider::LinearHorizontal);
    sldKaiserBeta_.setDoubleClickReturnValue (true, 8.0);
    sldKaiserBeta_.setValue (8.0, dontSendNotification);
    sldKaiserBeta_.addListener (this);

    addAndMakeVisible (lblLatency_);
    lblLatency_.setFont (Font (FontOptions (12.f)));
    lblLatency_.setColour (Label::textColourId, Colours::white.withAlpha (0.7f));

    addAndMakeVisible (lblMag_);
    addAndMakeVisible (magPlot_);
    addAndMakeVisible (lblImpulse_);
    addAndMakeVisible (impulsePlot_);

    addAndMakeVisible (btnApply_);
    btnApply_.setTooltip ("Push the current design into the band — leaves this dialog open so you "
                          "can iterate and listen to it in context.");
    btnApply_.addListener (this);
    addAndMakeVisible (btnClose_);
    btnClose_.addListener (this);

    for (auto* lbl : { &lblType_, &lblFreq_, &lblQ_, &lblGain_, &lblOrder_,
                        &lblLength_, &lblWindow_, &lblKaiserBeta_,
                        &lblMag_, &lblImpulse_ })
    {
        lbl->setFont (Font (FontOptions (13.f)));
        lbl->setColour (Label::textColourId, Colours::white);
    }

    setSize (560, 580);

    if (initialState.isObject())
        applyInitialState (initialState);

    showHideControls();
    rebuildBandFromUI();
    recomputeFIR();
}

FIRDesignDialog::~FIRDesignDialog()
{
    // Explicitly unregister listeners — defensive; member destructors should also
    // unhook them, but doing it here protects against any teardown-order races.
    cbType_.removeListener (this);
    cbOrder_.removeListener (this);
    cbLength_.removeListener (this);
    cbWindow_.removeListener (this);
    sldFreq_.removeListener (this);
    sldQ_.removeListener (this);
    sldGain_.removeListener (this);
    sldKaiserBeta_.removeListener (this);
    btnApply_.removeListener (this);
    btnClose_.removeListener (this);
}

void FIRDesignDialog::paint (Graphics& g)
{
    g.fillAll (Colour (0xff202020));
}

void FIRDesignDialog::resized()
{
    int rowH = 24, gap = 4, lblW = 70, x = 12;
    int y = 12;
    int w = getWidth() - 24;

    auto row = [&] (Component& lbl, Component& ctrl, int ctrlW = 0) {
        lbl.setBounds (x, y, lblW, rowH);
        if (ctrlW <= 0) ctrlW = w - lblW - 4;
        ctrl.setBounds (x + lblW + 4, y, ctrlW, rowH);
        y += rowH + gap;
    };

    row (lblType_,  cbType_,  180);
    row (lblFreq_,  sldFreq_);
    row (lblQ_,     sldQ_);
    row (lblGain_,  sldGain_);
    if (cbOrder_.isVisible()) row (lblOrder_, cbOrder_, 180);

    y += 6;
    row (lblLength_, cbLength_, 140);
    row (lblWindow_, cbWindow_, 180);
    if (sldKaiserBeta_.isVisible()) row (lblKaiserBeta_, sldKaiserBeta_);

    lblLatency_.setBounds (x, y, w, rowH);
    y += rowH + gap;

    y += 6;

    // Bottom buttons — anchor to bottom-right.
    int bw = 80;
    int btnY = getHeight() - 12 - rowH;
    btnClose_.setBounds (getWidth() - 12 - bw, btnY, bw, rowH);
    btnApply_.setBounds (btnClose_.getX() - bw - 6, btnY, bw, rowH);

    // Two graphs (each with a header label) split the remaining vertical space 50/50.
    int areaTop    = y;
    int areaBottom = btnY - gap;
    int areaH      = jmax (40, areaBottom - areaTop);
    int halfH      = areaH / 2 - gap / 2;
    int labelH     = 18;
    int graphH     = jmax (24, halfH - labelH);

    int yMag = areaTop;
    lblMag_.setBounds (x, yMag, w, labelH);
    magPlot_.setBounds (x, yMag + labelH, w, graphH);

    int yImp = areaTop + halfH + gap;
    lblImpulse_.setBounds (x, yImp, w, labelH);
    impulsePlot_.setBounds (x, yImp + labelH, w, areaBottom - (yImp + labelH));
}

void FIRDesignDialog::sliderValueChanged (Slider*)
{
    if (updating_) return;
    rebuildBandFromUI();
    recomputeFIR();
}

void FIRDesignDialog::comboBoxChanged (ComboBox* cb)
{
    if (updating_) return;
    if (cb == &cbType_ || cb == &cbWindow_)
        showHideControls();
    rebuildBandFromUI();
    recomputeFIR();
}

void FIRDesignDialog::buttonClicked (Button* b)
{
    if (b == &btnApply_)
    {
        // Push the current design into the band but leave the dialog open so the user
        // can iterate and listen to the result in the context of the full filter chain.
        if (onApply_ && ! currentCoeffs_.empty())
            onApply_ (currentCoeffs_, sampleRate_, captureState());
    }
    else if (b == &btnClose_)
    {
        if (auto* dw = findParentComponentOfClass<DialogWindow>())
            dw->exitModalState (0);
    }
}

void FIRDesignDialog::showHideControls()
{
    auto type = cbType_.getSelectedId();
    bool isShelfPeak = (type == 5 || type == 6 || type == 7);   // shelves + peak
    bool isCrossover = (type == 10 || type == 11);
    bool needsQ      = (type >= 1 && type <= 7);
    // (Shelves use Q-like 'shape' parameter; keep visible — JUCE makeLowShelf takes Q.)

    sldGain_.setVisible (isShelfPeak);
    lblGain_.setVisible (isShelfPeak);
    sldQ_.setVisible (needsQ);
    lblQ_.setVisible (needsQ);
    cbOrder_.setVisible (isCrossover);
    lblOrder_.setVisible (isCrossover);

    bool isKaiser = (cbWindow_.getSelectedId() == 5);
    sldKaiserBeta_.setVisible (isKaiser);
    lblKaiserBeta_.setVisible (isKaiser);

    resized();
}

void FIRDesignDialog::rebuildBandFromUI()
{
    updating_ = true;

    IIRSubType st = IIRSubType::Peak;
    switch (cbType_.getSelectedId())
    {
        case 1:  st = IIRSubType::LowPass;   break;
        case 2:  st = IIRSubType::HighPass;  break;
        case 3:  st = IIRSubType::BandPass;  break;
        case 4:  st = IIRSubType::Notch;     break;
        case 5:  st = IIRSubType::LowShelf;  break;
        case 6:  st = IIRSubType::HighShelf; break;
        case 7:  st = IIRSubType::Peak;      break;
        // Crossover LP/HP both source their target curve from a Butterworth LP;
        // the HP variant is derived in recomputeFIR() as (δ − LP) so the pair
        // sums to a unit delay (= flat magnitude, linear phase).
        case 10:
        case 11: st = IIRSubType::ButterworthLP; break;
    }
    tempBand_.setIIRSubType (st);
    tempBand_.setFrequency ((float) sldFreq_.getValue());
    tempBand_.setQ ((float) sldQ_.getValue());
    tempBand_.setGainDB ((float) sldGain_.getValue());
    if (st == IIRSubType::ButterworthLP)
        tempBand_.setButterworthOrder (cbOrder_.getSelectedId());

    updating_ = false;
}

void FIRDesignDialog::recomputeFIR()
{
    int N = cbLength_.getSelectedId();
    if (N < 8) N = 2048;

    EqDesign::FIRWindow win = EqDesign::FIRWindow::Kaiser;
    switch (cbWindow_.getSelectedId())
    {
        case 1: win = EqDesign::FIRWindow::Hann;            break;
        case 2: win = EqDesign::FIRWindow::Hamming;         break;
        case 3: win = EqDesign::FIRWindow::Blackman;        break;
        case 4: win = EqDesign::FIRWindow::BlackmanHarris;  break;
        case 5: win = EqDesign::FIRWindow::Kaiser;          break;
    }

    auto magAt = [this] (double f) -> double {
        return (double) std::abs (tempBand_.getFrequencyResponse (f, /*alwaysCompute*/ true));
    };

    currentCoeffs_ = EqDesign::designLinearPhaseFIR (magAt, N, sampleRate_,
                                                       win, sldKaiserBeta_.getValue());

    // Crossover HP: derive as (δ − LP). Designing the LP and HP this way at the
    // same frequency / order / length / window guarantees |LP + HP| = 1 exactly
    // (within floating-point precision) — the perfect linear-phase complement.
    if (cbType_.getSelectedId() == 11 && ! currentCoeffs_.empty())
    {
        const int M = (int) currentCoeffs_.size();
        const int centre = (M - 1) / 2;
        for (int i = 0; i < M; ++i)
            currentCoeffs_[(size_t) i] = -currentCoeffs_[(size_t) i];
        currentCoeffs_[(size_t) centre] += 1.0f;
    }

    impulsePlot_.setCoefficients (currentCoeffs_);
    magPlot_.setCoefficients (currentCoeffs_, sampleRate_);
    updateLatencyLabel();
}

void FIRDesignDialog::updateLatencyLabel()
{
    int N = (int) currentCoeffs_.size();
    int latency = (N - 1) / 2;
    double ms = (double) latency * 1000.0 / sampleRate_;
    lblLatency_.setText ("Latency: " + String (latency) + " samples ("
                         + String (ms, 1) + " ms @ "
                         + String (sampleRate_ / 1000.0, 1) + " kHz)",
                         dontSendNotification);
}

void FIRDesignDialog::launch (double sampleRate,
                                const var& initialState,
                                ApplyCallback onApply)
{
    auto* dialog = new FIRDesignDialog (sampleRate, initialState, std::move (onApply));

    DialogWindow::LaunchOptions o;
    o.dialogTitle = "Design Linear-Phase FIR";
    o.content.setOwned (dialog);
    o.dialogBackgroundColour = Colour (0xff202020);
    o.escapeKeyTriggersCloseButton = true;
    // JUCE-rendered title bar (not native): keeps the dialog as a JUCE child of the
    // host's plugin window. A native NSWindow on macOS interacts poorly with
    // REAPER's plugin-host modal management and has caused crashes.
    o.useNativeTitleBar = false;
    o.resizable = true;
    if (auto* dw = o.launchAsync())
        dw->setResizeLimits (480, 480, 1600, 1600);
}

//==============================================================================
namespace
{
    String windowToString (int comboId)
    {
        switch (comboId)
        {
            case 1: return "hann";
            case 2: return "hamming";
            case 3: return "blackman";
            case 4: return "blackman_harris";
            case 5: return "kaiser";
        }
        return "kaiser";
    }
    int stringToWindowId (const String& s)
    {
        if (s == "hann")            return 1;
        if (s == "hamming")         return 2;
        if (s == "blackman")        return 3;
        if (s == "blackman_harris") return 4;
        if (s == "kaiser")          return 5;
        return 5;
    }

    String designTypeToString (int comboId)
    {
        switch (comboId)
        {
            case 1:  return "low_pass";
            case 2:  return "high_pass";
            case 3:  return "band_pass";
            case 4:  return "notch";
            case 5:  return "low_shelf";
            case 6:  return "high_shelf";
            case 7:  return "peak";
            case 10: return "crossover_lp";
            case 11: return "crossover_hp";
        }
        return "peak";
    }
    int stringToDesignTypeId (const String& s)
    {
        if (s == "low_pass")       return 1;
        if (s == "high_pass")      return 2;
        if (s == "band_pass")      return 3;
        if (s == "notch")          return 4;
        if (s == "low_shelf")      return 5;
        if (s == "high_shelf")     return 6;
        if (s == "peak")           return 7;
        if (s == "crossover_lp")   return 10;
        if (s == "crossover_hp")   return 11;
        return 7;
    }
}

var FIRDesignDialog::captureState() const
{
    auto* obj = new DynamicObject();
    obj->setProperty ("type",         designTypeToString (cbType_.getSelectedId()));
    obj->setProperty ("f_Hz",         sldFreq_.getValue());
    obj->setProperty ("Q",            sldQ_.getValue());
    obj->setProperty ("gain_db",      sldGain_.getValue());
    obj->setProperty ("order",        cbOrder_.getSelectedId());
    obj->setProperty ("fir_length",   cbLength_.getSelectedId());
    obj->setProperty ("window",       windowToString (cbWindow_.getSelectedId()));
    obj->setProperty ("kaiser_beta",  sldKaiserBeta_.getValue());
    return var (obj);
}

void FIRDesignDialog::applyInitialState (const var& s)
{
    updating_ = true;

    if (s.hasProperty ("type"))
        cbType_.setSelectedId (stringToDesignTypeId (s["type"].toString()), dontSendNotification);
    if (s.hasProperty ("f_Hz"))
        sldFreq_.setValue ((double) s["f_Hz"], dontSendNotification);
    if (s.hasProperty ("Q"))
        sldQ_.setValue ((double) s["Q"], dontSendNotification);
    if (s.hasProperty ("gain_db"))
        sldGain_.setValue ((double) s["gain_db"], dontSendNotification);
    if (s.hasProperty ("order"))
        cbOrder_.setSelectedId ((int) s["order"], dontSendNotification);
    if (s.hasProperty ("fir_length"))
        cbLength_.setSelectedId ((int) s["fir_length"], dontSendNotification);
    if (s.hasProperty ("window"))
        cbWindow_.setSelectedId (stringToWindowId (s["window"].toString()), dontSendNotification);
    if (s.hasProperty ("kaiser_beta"))
        sldKaiserBeta_.setValue ((double) s["kaiser_beta"], dontSendNotification);

    updating_ = false;
}
