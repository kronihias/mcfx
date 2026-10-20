/*
 ==============================================================================

 This file is part of the mcfx (Multichannel Effects) plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com

 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)

 Details of these licenses can be found at: www.gnu.org/licenses

 ambix is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 ==============================================================================
 */

//[Headers] You can add your own extra header files here...

//[/Headers]

#include "PluginEditor.h"
#include "PluginProcessor.h"

#define Q(x) #x
#define QUOTE(x) Q(x)

extern float iec_scale(float dB);

//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
Ambix_meterAudioProcessorEditor::Ambix_meterAudioProcessorEditor (Ambix_meterAudioProcessor* ownerFilter) :
AudioProcessorEditor (ownerFilter)
{
    // Everything below reads the state as it is now; a later restore is
    // picked up by syncToRestoredState().
    _appliedStateGen = ownerFilter->_state_generation.load (std::memory_order_acquire);

    LookAndFeel::setDefaultLookAndFeel(&MyLookAndFeel);

    tooltipWindow.setMillisecondsBeforeTipAppears (500); // tooltip delay

    addAndMakeVisible (label);
    label.setFont (Font (FontOptions (15.0000f, Font::plain)));
    label.setJustificationType (Justification::centredLeft);
    label.setEditable (false, false, false);
    label.setColour (Label::textColourId, Colours::aquamarine);
    label.setColour (TextEditor::textColourId, Colours::black);
    label.setColour (TextEditor::backgroundColourId, Colour (0x0));

    addAndMakeVisible (sld_hold);
    sld_hold.setTooltip ("peak hold time in [s]");
    sld_hold.setRange (0, 5, 0.1);
    sld_hold.setSliderStyle (Slider::Rotary);
    sld_hold.setTextBoxStyle (Slider::TextBoxLeft, false, 40, 18);
    sld_hold.setColour (Slider::rotarySliderFillColourId, Colours::white);
    sld_hold.addListener (this);
    sld_hold.setSkewFactor(0.7f);
    sld_hold.setDoubleClickReturnValue(true, 0.5f);

    addAndMakeVisible (sld_fall);
    sld_fall.setTooltip ("peak fall [dB/s]");
    sld_fall.setRange (0, 99, 1);
    sld_fall.setSliderStyle (Slider::Rotary);
    sld_fall.setTextBoxStyle (Slider::TextBoxLeft, false, 40, 18);
    sld_fall.setColour (Slider::rotarySliderFillColourId, Colours::white);
    sld_fall.addListener (this);
    sld_fall.setSkewFactor(0.6f);
    sld_fall.setDoubleClickReturnValue(true, 15.f);

    addAndMakeVisible (label2);
    label2.setText("hold [s]\n", dontSendNotification);
    label2.setFont (Font (FontOptions (15.0000f, Font::plain)));
    label2.setJustificationType (Justification::centredLeft);
    label2.setEditable (false, false, false);
    label2.setColour (Label::textColourId, Colours::white);
    label2.setColour (TextEditor::textColourId, Colours::black);
    label2.setColour (TextEditor::backgroundColourId, Colour (0x0));

    addAndMakeVisible (label3);
    label3.setText("fall [dB/s]\n", dontSendNotification);
    label3.setFont (Font (FontOptions (15.0000f, Font::plain)));
    label3.setJustificationType (Justification::centredLeft);
    label3.setEditable (false, false, false);
    label3.setColour (Label::textColourId, Colours::white);
    label3.setColour (TextEditor::textColourId, Colours::black);
    label3.setColour (TextEditor::backgroundColourId, Colour (0x0));

    // not make visible... not needed
    addAndMakeVisible (tgl_pkhold);
    tgl_pkhold.setButtonText ("peak hold");
    tgl_pkhold.setTooltip("additional stable peak hold indicator");
    tgl_pkhold.addListener (this);
    tgl_pkhold.setColour (ToggleButton::textColourId, Colours::white);
    tgl_pkhold.setColour (TextButton::buttonColourId, Colours::grey);

    addAndMakeVisible (sld_offset);
    sld_offset.setTooltip ("offset scale");
    sld_offset.setRange (-36, 18, 1);
    sld_offset.setSliderStyle (Slider::LinearVertical);
    sld_offset.setTextBoxStyle (Slider::NoTextBox, true, 40, 18);
    sld_offset.setColour (Slider::rotarySliderFillColourId, Colours::white);
    sld_offset.setColour (Slider::thumbColourId, Colours::grey);
    sld_offset.addListener (this);
    sld_offset.setDoubleClickReturnValue(true, 0.f);

    //[UserPreSize]
    //[/UserPreSize]

    // Build the per-channel meter strips, scales and set the editor size for
    // the host's currently-negotiated channel count. Extracted into a helper
    // so it can also be called from changeListenerCallback() when the host
    // re-negotiates the bus layout (track channel count change in Reaper).
    rebuildChannelStrips();

    // register as change listener (gui/dsp sync)
    ownerFilter->addChangeListener(this);
    ownerFilter->sendChangeMessage(); // get status from dsp

    //[Constructor] You can add your own custom stuff here..
    addAndMakeVisible (cb_view);
    cb_view.addItem ("bars",   1);
    cb_view.addItem ("circle", 2);
    cb_view.addItem ("waterfall", 3);
    cb_view.addItem ("dots", 4);
    cb_view.addItem ("scope", 5);
    cb_view.addItem ("spectrogram", 6);
    cb_view.addListener (this);
    cb_view.setTooltip ("How the channels are laid out. The ring keeps a square "
                        "footprint whatever the channel count, and an odd channel "
                        "shows up as a break in its symmetry.");
    cb_view.setSelectedId ((int) getProcessor()->_view_mode + 1, dontSendNotification);

    // addChildComponent, not addAndMakeVisible: this one belongs to the
    // waterfall only, and applyModeVisibility() has already run by now — via
    // rebuildChannelStrips() above, which happens before this block. Making it
    // visible here would override that decision and leave an empty selector
    // sitting in the strip in bar and circle mode.
    addChildComponent (cb_channel);
    cb_channel.addListener (this);
    cb_channel.setTooltip ("Keep one channel highlighted, and read its level. "
                           "Clicking it in the view selects it too; clicking the "
                           "background clears it. Double click resets peak hold.");

    addChildComponent (_circular);
    addChildComponent (_waterfall);
    addChildComponent (_dots);
    addChildComponent (_scope);
    _scope.setCapture (&getProcessor()->_scope_capture);
    _scope.setSettings (&getProcessor()->_scope_settings);
    addChildComponent (_spectro);
    _spectro.setCapture (&getProcessor()->_scope_capture);
    _spectro.setSettings (&getProcessor()->_spectro_settings);
    auto selectChannel = [this] (int ch)
    {
        getProcessor()->_selected_ch = ch;
        _circular.setSelectedChannel (ch);
        _waterfall.setSelectedChannel (ch);
        _dots.setSelectedChannel (ch);
        cb_channel.setSelectedId (ch + 2, dontSendNotification);
    };
    _circular.onChannelSelected  = selectChannel;
    _waterfall.onChannelSelected = selectChannel;
    _dots.onChannelSelected      = selectChannel;

    _dots.onResetAll = [this] { resetAllMeters(); };
    _dots.onChannelReset = [this] (int ch)
    {
        Ambix_meterAudioProcessor* p = getProcessor();
        if (isPositiveAndBelow (ch, p->_my_meter_dsp.size()))
            p->_my_meter_dsp.getUnchecked (ch)->reset();
    };
    _waterfall.setAnalyser (&getProcessor()->_band_analyser);

    _circular.onResetAll = [this] { resetAllMeters(); };
    _circular.onChannelReset = [this] (int ch)
    {
        Ambix_meterAudioProcessor* p = getProcessor();
        if (isPositiveAndBelow (ch, p->_my_meter_dsp.size()))
            p->_my_meter_dsp.getUnchecked (ch)->reset();
    };

    {
        // Four corner brackets: the usual "fullscreen" glyph, drawn rather than
        // taken from a font that may not have U+2922.
        Path corners;
        const float a = 0.f, b = 10.f, l = 3.5f;
        corners.startNewSubPath (a, a + l); corners.lineTo (a, a); corners.lineTo (a + l, a);
        corners.startNewSubPath (b - l, a); corners.lineTo (b, a); corners.lineTo (b, a + l);
        corners.startNewSubPath (b, b - l); corners.lineTo (b, b); corners.lineTo (b - l, b);
        corners.startNewSubPath (a + l, b); corners.lineTo (a, b); corners.lineTo (a, b - l);
        Path stroked;
        PathStrokeType (1.6f).createStrokedPath (stroked, corners);
        btn_detach.setShape (stroked, false, true, false);
        btn_detach.setBorderSize (BorderSize<int> (4));
        btn_detach.setTooltip ("Open this view in its own window, or fullscreen on any "
                               "display (for a second screen or a projector). "
                               "In that window: F toggles fullscreen, Esc leaves it.");
        btn_detach.onClick = [this] { showDetachMenu(); };
        addChildComponent (btn_detach);
    }

    // Everything the mode governs is a child by now, so settle visibility once
    // more rather than relying on the order of the two constructor blocks.
    applyModeVisibility();

    // Resizable, but the bar view pins min == max in applyModeSizing() so it
    // behaves exactly as it always has.
    setResizable (true, true);
    startTimer (40);
    //[/Constructor]
}

Ambix_meterAudioProcessorEditor::~Ambix_meterAudioProcessorEditor()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    // Take a popped-out view back before anything it points at goes away.
    reattachView();
    //[/Destructor_pre]
    Ambix_meterAudioProcessor* ourProcessor = getProcessor();

    // remove me as listener for changes
    ourProcessor->removeChangeListener(this);

    // Nothing shows the scope's capture once the editor is gone. (The
    // component's own destructor has not run yet, so unfreeze here too.)
    ourProcessor->_scope_capture.setActive (false);
    ourProcessor->_scope_capture.setFrozen (false);

    // Same for the waterfall's analyser, which used to keep copying every
    // block on the audio thread after the editor closed in that view.
    // applyModeVisibility() turns it back on when an editor reopens there.
    ourProcessor->_band_analyser.setActive (false);

    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void Ambix_meterAudioProcessorEditor::paint (Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    // Fill what the editor actually is, not what it was last sized to. The
    // resizable views let the user change the window without going through
    // applyModeSizing(), and filling a stale _width/_height left the gradient
    // as a visible rectangle over part of the background.
    //
    // Rendered once per size and blitted: the animated views are non-opaque
    // children covering most of the window, and every one of their 25 fps
    // repaints re-fills the editor behind them. Rasterizing a radial gradient
    // is per-pixel work — at waterfall window sizes it cost more than the view
    // being painted over it.
    if (_background.getWidth() != getWidth() || _background.getHeight() != getHeight())
    {
        _background = Image (Image::RGB, jmax (1, getWidth()), jmax (1, getHeight()), false);
        Graphics bg (_background);
        bg.fillAll (Colour (0xff1a1a1a));
        bg.setGradientFill (ColourGradient (Colour (0xff4e4e4e),
                                            (float) (proportionOfWidth (0.6400f)), (float) (proportionOfHeight (0.6933f)),
                                            Colours::black,
                                            (float) (proportionOfWidth (0.1143f)), (float) (proportionOfHeight (0.0800f)),
                                            true));
        bg.fillRect (getLocalBounds());
    }
    g.drawImageAt (_background, 0, 0);

    if (_detachedView != nullptr)
    {
        g.setColour (Colours::white.withAlpha (0.6f));
        g.setFont (Font (FontOptions (14.f)));
        g.drawFittedText ("This view is open in its own window.\n"
                          "Close that window (or press Esc in it) to bring it back here.",
                          getLocalBounds().reduced (20).withTrimmedTop (30), Justification::centred, 3);
    }

    /* Version text */
    g.setColour (Colours::white);
    g.setFont (Font (FontOptions (10.00f, Font::plain)));
    String version_string;
    version_string << "v" << QUOTE(VERSION);
    // Left of the resize corner (18 px, setResizable() puts it in every view),
    // which otherwise draws over the last characters.
    g.drawText (version_string,
                getWidth()-51-18, getHeight()-11, 50, 10,
                Justification::bottomRight, true);

    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void Ambix_meterAudioProcessorEditor::resized()
{
    // Keep these in step with the real size: a user drag in one of the
    // resizable views does not go through applyModeSizing(), and everything
    // below that positions against _width would otherwise use a stale value.
    _width  = getWidth();
    _height = getHeight();

    const auto mode = getProcessor()->_view_mode;
    const bool bars = mode == Ambix_meterAudioProcessor::ViewMode::Bars;
    const bool wf   = mode == Ambix_meterAudioProcessor::ViewMode::Waterfall;
    // Both carry their own toolbar and channel range.
    const bool scope = mode == Ambix_meterAudioProcessor::ViewMode::Scope
                    || mode == Ambix_meterAudioProcessor::ViewMode::Spectrogram;

    // Strip order: title, which view, which channel, then the level controls.
    // The two selectors belong together — both answer "what am I looking at" —
    // and the level controls are the ones that can go when space runs out.
    // Wide enough for "spectrogram", the longest view name.
    const int kViewComboX = 108;
    const int kViewComboW = 112;
    const int kChanComboX = kViewComboX + kViewComboW + 4;

    // Only the bars have no channel selection, so only they get that space
    // back. The level controls shift by whatever precedes them.
    const bool hasChanCombo = ! bars && ! scope;   // the scope has its own range
    const int  kCtlShift    = hasChanCombo ? 212 : 116;

    // Below the width its own controls need, the strip keeps the selectors and
    // drops the level controls — which is what lets the ring shrink to a
    // corner-of-the-screen glance. They mean nothing to the waterfall or the
    // dots in any case.
    const int  needed        = hasChanCombo ? 693 : kMinEditorWidth;
    const bool showLevelCtls = ! wf && ! scope && mode != Ambix_meterAudioProcessor::ViewMode::Dots
                                   && getWidth() >= needed;

    label.setBounds (0, 0, 104, 16);
    cb_view.setBounds (kViewComboX, 1, kViewComboW, 22);
    cb_channel.setBounds (kChanComboX, 1, 88, 22);

    // Goes too once the window is narrower than it is wide — at the ring's
    // 230 px minimum it would hang off the edge. Clicking a wedge still selects
    // a channel there, so nothing is lost but the drop-down.
    cb_channel.setVisible (hasChanCombo && getWidth() >= kChanComboX + 92);

    label2.setBounds (111 + kCtlShift, 3, 64, 16);
    sld_hold.setBounds (166 + kCtlShift, 0, 70, 24);
    label3.setBounds (239 + kCtlShift, 3, 77, 16);
    sld_fall.setBounds (307 + kCtlShift, 0, 70, 24);
    tgl_pkhold.setBounds (382 + kCtlShift, 0, 91, 24);

    label2.setVisible (showLevelCtls);
    label3.setVisible (showLevelCtls);
    sld_hold.setVisible (showLevelCtls);
    sld_fall.setVisible (showLevelCtls);
    tgl_pkhold.setVisible (showLevelCtls);

    // Bars keep the original fixed slider; the resizable views stretch it.
    sld_offset.setBounds (4, 23, 18, bars ? 167 : jmax (60, getHeight() - 31));
    // A level-meter offset means nothing to a waveform.
    sld_offset.setVisible (! scope);

    // Inset past the offset slider. These used to start at x=0 and, being added
    // after it, sat on top of it and swallowed every click — the slider was
    // unreachable in both of these views even though its value worked.
    const int viewX = 26;
    // A popped-out view is sized by its window, not by us.
    auto place = [this] (Component& c, Rectangle<int> r) { if (! isDetached (c)) c.setBounds (r); };
    place (_circular,  { viewX, 28, jmax (0, getWidth() - viewX), jmax (0, getHeight() - 28) });
    place (_waterfall, { viewX, 28, jmax (0, getWidth() - viewX), jmax (0, getHeight() - 28) });
    place (_dots,      { viewX, 30, jmax (0, getWidth() - viewX), jmax (0, getHeight() - 34) });
    // No offset slider to step around, so the scope takes the full width.
    place (_scope,     { 4, 28, jmax (0, getWidth() - 8), jmax (0, getHeight() - 40) });
    place (_spectro,   { 4, 28, jmax (0, getWidth() - 8), jmax (0, getHeight() - 40) });

    // Top-right corner of the strip, clear of every control left of it.
    btn_detach.setBounds (getWidth() - 26, 1, 22, 22);
    btn_detach.setVisible (activeView() != nullptr && getWidth() >= 300);

    const int row_y_offset = 215;

    const int numActive = _meters.size();
    for (int i = 0; i < numActive; i++)
    {
        const int row = (int)floor(i / 64);
        const int group = (i - row * 64) / GROUP_CHANNELS;

        const int x = 50 + (i - row * 64) * METER_WIDTH + group * METER_GROUP_SPACE;
        const int y = 30 + row * row_y_offset;

        _meters.getUnchecked(i)->setBounds(x, y, 8, 163);

        _labels.getUnchecked(i)->setBounds((int)(x-METER_WIDTH*0.75), y - 30 + 195, (int)(METER_WIDTH*2), 14);
    }

    const int rows = numActive > 0 ? (int)floor((numActive - 1.0) / 64.0) + 1 : 0;
    
    for (int i = 0; i < rows; i++) {
        _scales.getUnchecked(i * 2)->setBounds(20, 23 + i * row_y_offset, 50, 200); // left
        _scales.getUnchecked(i * 2 + 1)->setBounds(_width-65, 23 + i * row_y_offset, 50, 200); // right
    }
    

    //[UserResized] Add your own custom resize handling here..
    // Remember the size per view. The bar view is pinned min == max so this
    // just records the size it was given; the resizable views record the user's.
    Ambix_meterAudioProcessor* ourProcessor = getProcessor();
    const bool restorePending = ourProcessor->_state_generation.load (std::memory_order_acquire)
                                  != _appliedStateGen;
    switch (restorePending ? Ambix_meterAudioProcessor::ViewMode::Bars : ourProcessor->_view_mode)
    {
        case Ambix_meterAudioProcessor::ViewMode::Circle:
            ourProcessor->_size_circle_w = getWidth();
            ourProcessor->_size_circle_h = getHeight();
            break;
        case Ambix_meterAudioProcessor::ViewMode::Waterfall:
            ourProcessor->_size_wf_w = getWidth();
            ourProcessor->_size_wf_h = getHeight();
            break;
        case Ambix_meterAudioProcessor::ViewMode::Dots:
            ourProcessor->_size_dots_w = getWidth();
            ourProcessor->_size_dots_h = getHeight();
            break;
        case Ambix_meterAudioProcessor::ViewMode::Scope:
            ourProcessor->_size_scope_w = getWidth();
            ourProcessor->_size_scope_h = getHeight();
            break;
        case Ambix_meterAudioProcessor::ViewMode::Spectrogram:
            ourProcessor->_size_spec_w = getWidth();
            ourProcessor->_size_spec_h = getHeight();
            break;
        default: break;
    }
    //[/UserResized]
}

void Ambix_meterAudioProcessorEditor::sliderValueChanged (Slider* sliderThatWasMoved)
{
    Ambix_meterAudioProcessor* ourProcessor = getProcessor();

    if (sliderThatWasMoved == &sld_hold)
    {
        //[UserSliderCode_sld_hold] -- add your slider handling code here..
        ourProcessor->setParameterNotifyingHost(Ambix_meterAudioProcessor::HoldParam, (float)sld_hold.getValue()/5.f);

        //[/UserSliderCode_sld_hold]
    }
    else if (sliderThatWasMoved == &sld_fall)
    {
        //[UserSliderCode_sld_fall] -- add your slider handling code here..
        ourProcessor->setParameterNotifyingHost(Ambix_meterAudioProcessor::FallParam, (float)sld_fall.getValue()/99.f);

        //[/UserSliderCode_sld_fall]
    }
    else if (sliderThatWasMoved == &sld_offset)
    {
        ourProcessor->setParameterNotifyingHost(Ambix_meterAudioProcessor::OffsetParam, (float)(sld_offset.getValue()+36.f)/54.f);
    }

    //[UsersliderValueChanged_Post]
    //[/UsersliderValueChanged_Post]
}

void Ambix_meterAudioProcessorEditor::buttonClicked (Button* buttonThatWasClicked)
{
    Ambix_meterAudioProcessor* ourProcessor = getProcessor();

    if (buttonThatWasClicked == &tgl_pkhold)
    {
        ourProcessor->setParameterNotifyingHost(Ambix_meterAudioProcessor::PkHoldParam, (float)tgl_pkhold.getToggleState());

    }

    //[UserbuttonClicked_Post]
    //[/UserbuttonClicked_Post]
}

void Ambix_meterAudioProcessorEditor::timerCallback() // update meters
{
    Ambix_meterAudioProcessor* ourProcessor = getProcessor();

    // Before anything below goes by _view_mode: the change message for a
    // restore may still be queued.
    syncToRestoredState();


    // Feed only the view on screen — MeterComponent::setValue repaints
    // unconditionally, so walking 128 hidden strips every tick is pure waste.
    const bool bars = ourProcessor->_view_mode == Ambix_meterAudioProcessor::ViewMode::Bars;

    if (bars)
    {
        for (int i=0; i<_meters.size(); i++)
        {
            _meters.getUnchecked(i)->setValue(ourProcessor->_my_meter_dsp.getUnchecked(i)->getRMS(), ourProcessor->_my_meter_dsp.getUnchecked(i)->getPeak(), ourProcessor->_my_meter_dsp.getUnchecked(i)->getPeakHold());
        }
    }
    else if (ourProcessor->_view_mode == Ambix_meterAudioProcessor::ViewMode::Circle
             || ourProcessor->_view_mode == Ambix_meterAudioProcessor::ViewMode::Dots)
    {
        const bool ring = ourProcessor->_view_mode == Ambix_meterAudioProcessor::ViewMode::Circle;
        const int n = jmin (_cachedNumCh, ourProcessor->_my_meter_dsp.size());
        for (int i = 0; i < n; ++i)
        {
            MyMeterDsp* d = ourProcessor->_my_meter_dsp.getUnchecked (i);
            if (ring) _circular.setValue (i, d->getRMS(), d->getPeak(), d->getPeakHold());
            else      _dots.setValue     (i, d->getRMS(), d->getPeak(), d->getPeakHold());
        }
        // Repaint only when a level actually moved — the same guard the bar
        // view has always had. An idle plugin left open (the dots view's
        // stated use case) then paints nothing at all. setOffset() and the
        // hover/selection handlers issue their own repaints.
        if (ring) { _circular.setOffset ((int) ourProcessor->_offset); if (_circular.takeDirty()) _circular.repaint(); }
        else      { _dots.setOffset     ((int) ourProcessor->_offset); if (_dots.takeDirty())     _dots.repaint(); }
    }
    else if (ourProcessor->_view_mode == Ambix_meterAudioProcessor::ViewMode::Scope
             || ourProcessor->_view_mode == Ambix_meterAudioProcessor::ViewMode::Spectrogram)
    {
        // A sample-rate change does not come through applyModeVisibility(), so
        // the scope's ring follows it here. prepare() is a no-op otherwise.
        const double sr = ourProcessor->getSampleRate();
        if (sr > 0.0 && sr != ourProcessor->_scope_capture.getSampleRate())
            ourProcessor->_scope_capture.prepare (sr, jmax (1, _cachedNumCh));

        if (ourProcessor->_view_mode == Ambix_meterAudioProcessor::ViewMode::Scope)
            _scope.tick();
        else
            _spectro.tick();
    }
    else
    {
        // Bounded work per tick: a fixed slice of channels is transformed and
        // the cursor carries on next time, so the cost is the same at 8
        // channels as at 128.
        // Every channel, every tick, from one latched read position — the
        // channels have to describe the same instant to be comparable.
        ourProcessor->_band_analyser.compute();
        _waterfall.setOffset ((int) ourProcessor->_offset);

        // The analysis is cheap; the drawing is not. Measured at 128 channels:
        // 20 ms a paint against well under 1 ms for every transform. So the data
        // refreshes at the full timer rate and only the repaint is thinned,
        // which the temporal smoothing above makes look continuous anyway.
        const int n = jmax (1, _cachedNumCh);
        const int ticksPerPaint = n <= 32 ? 1 : (n <= 64 ? 2 : 4);
        if (++_wf_tick >= ticksPerPaint)
        {
            _wf_tick = 0;
            _waterfall.repaint();
        }
    }

}

void Ambix_meterAudioProcessorEditor::rebuildChannelStrips()
{
    Ambix_meterAudioProcessor* ourProcessor = getProcessor();

    // In the MC build NUM_CHANNELS == MCFX_MAX_CHANNELS (128) but the host
    // may have negotiated fewer channels — only draw what is actually active.
    const int numCh = jlimit (1, NUM_CHANNELS, ourProcessor->getTotalNumInputChannels());
    _cachedNumCh = numCh;

    String label_text = "mcfx_meter";
    label_text << numCh;
    label.setText (label_text, dontSendNotification);

    // Clear any existing per-channel components so the rebuild is idempotent
    // (called from both the constructor and changeListenerCallback).
    _meters.clear();
    _labels.clear();
    _scales.clear();

    // create meters and labels — one per active host channel
    for (int i = 0; i < numCh; i++)
    {
        if (MeterComponent* const METER = new MeterComponent())
        {
            _meters.add (METER);
            addChildComponent (_meters.getUnchecked (i));
            _meters.getUnchecked (i)->setVisible (true);
            // Re-seed current DSP state so the strip doesn't flash through 0
            // after a rebuild (numChannelsChanged → rebuild keeps audio flowing).
            _meters.getUnchecked (i)->offset ((int) ourProcessor->_offset);
            _meters.getUnchecked (i)->_peak_hold = ourProcessor->_pk_hold;

            if (Label* const LABEL = new Label ("new label", String (i + 1)))
            {
                _labels.add (LABEL);
                addChildComponent (_labels.getUnchecked (i));
                _labels.getUnchecked (i)->setVisible (true);
                const float font_size = (i < 99) ? 11.f : 9.f;
                _labels.getUnchecked (i)->setFont (Font (FontOptions (font_size, Font::plain)));
                _labels.getUnchecked (i)->setColour (Label::textColourId, Colours::white);
                _labels.getUnchecked (i)->setJustificationType (Justification::centred);
            }
        }
    }

    int rows = (int) floor ((numCh - 1.0) / 64.0) + 1;

    for (int i = 0; i < rows; i++)
    {
        auto* const scale_left = new MeterScaleComponent (163, false);
        _scales.add (scale_left);
        addAndMakeVisible (_scales.getUnchecked (i * 2));

        // TODO: For some reason the right scale is not visible...!
        auto* const scale_right = new MeterScaleComponent (163, true);
        _scales.add (scale_right);
        addAndMakeVisible (_scales.getUnchecked (i * 2 + 1));
    }

    applyModeSizing();
}

void Ambix_meterAudioProcessorEditor::applyModeVisibility()
{
    const bool bars = getProcessor()->_view_mode == Ambix_meterAudioProcessor::ViewMode::Bars;

    for (auto* m : _meters) m->setVisible (bars);
    for (auto* l : _labels) l->setVisible (bars);
    for (auto* s : _scales) s->setVisible (bars);

    const auto mode = getProcessor()->_view_mode;
    const bool wf   = mode == Ambix_meterAudioProcessor::ViewMode::Waterfall;

    _circular.setNumChannels (_cachedNumCh);
    _circular.setVisible (mode == Ambix_meterAudioProcessor::ViewMode::Circle);

    _waterfall.setNumChannels (_cachedNumCh);
    _waterfall.setVisible (wf);

    _dots.setNumChannels (_cachedNumCh);
    _dots.setVisible (mode == Ambix_meterAudioProcessor::ViewMode::Dots);

    const bool scope = mode == Ambix_meterAudioProcessor::ViewMode::Scope;
    _scope.setNumChannels (_cachedNumCh);
    _scope.setVisible (scope);

    const bool spectro = mode == Ambix_meterAudioProcessor::ViewMode::Spectrogram;
    _spectro.setNumChannels (_cachedNumCh);
    _spectro.setVisible (spectro);
    // cb_channel's visibility is resized()'s call: it depends on the width as
    // well as the mode.

    // Hold time, fall rate and the peak-hold toggle are laid out and shown or
    // hidden by resized(), which is the one place that knows the width as well
    // as the mode.
    if (! bars && ! scope && ! spectro)
        rebuildChannelSelector();

    // The analyser is the only expensive thing here, so it runs only while its
    // view is actually on screen. prepare() allocates, hence the GUI thread.
    Ambix_meterAudioProcessor* p = getProcessor();
    if (wf)
    {
        p->_band_analyser.prepare (p->getSampleRate() > 0.0 ? p->getSampleRate() : 48000.0,
                                   jmax (1, _cachedNumCh));
        // ~200 ms to settle, from the 40 ms timer.
        p->_band_analyser.setSmoothing (std::exp (-0.040f / 0.20f));
    }
    p->_band_analyser.setActive (wf);

    // Same deal for the scope's ring: allocated on first use, filled only
    // while the scope is on screen.
    if (scope || spectro)
        p->_scope_capture.prepare (p->getSampleRate() > 0.0 ? p->getSampleRate() : 48000.0,
                                   jmax (1, _cachedNumCh));
    p->_scope_capture.setActive (scope || spectro);
}

void Ambix_meterAudioProcessorEditor::rebuildChannelSelector()
{
    Ambix_meterAudioProcessor* p = getProcessor();
    const int n = jmax (1, _cachedNumCh);

    // Rebuilt only when the count changes: refilling on every mode switch would
    // close the popup out from under the user.
    if (cb_channel.getNumItems() != n + 1)
    {
        cb_channel.clear (dontSendNotification);
        cb_channel.addItem ("no highlight", 1);
        for (int ch = 0; ch < n; ++ch)
            cb_channel.addItem ("ch " + String (ch + 1), ch + 2);
    }

    if (p->_selected_ch >= n)
        p->_selected_ch = -1;

    _circular.setSelectedChannel (p->_selected_ch);
    _waterfall.setSelectedChannel (p->_selected_ch);
    _dots.setSelectedChannel (p->_selected_ch);
    cb_channel.setSelectedId (p->_selected_ch + 2, dontSendNotification);
}

void Ambix_meterAudioProcessorEditor::applyModeSizing()
{
    Ambix_meterAudioProcessor* ourProcessor = getProcessor();
    const int numCh = jmax (1, _cachedNumCh);

    // The bar view's size is dictated by how many strips it has to show.
    const int gr_of_eight = std::min (64, numCh) / GROUP_CHANNELS;
    const int barsW = jmax (kMinEditorWidth,
                            50 + METER_WIDTH * std::min (64, numCh) + 50
                              + gr_of_eight * METER_GROUP_SPACE);
    const int barsH = 220 * ((int) floor ((numCh - 1.0) / 64.0) + 1);

    switch (ourProcessor->_view_mode)
    {
        case Ambix_meterAudioProcessor::ViewMode::Bars:
        default:
            // Locked: min == max. MeterComponent paints at a hard-coded 8x163
            // and has an empty resized(), so the bar view must not be stretched.
            _width  = barsW;
            _height = barsH;
            setResizeLimits (barsW, barsH, barsW, barsH);
            setSize (barsW, barsH);
            break;

        // NB both resizable cases read the remembered size *before* touching the
        // limits. setResizeLimits() constrains the current bounds, which fires
        // resized(), which records getWidth()/getHeight() back into these very
        // members — so reading them afterwards yields the minimum size, not the
        // user's. That silently reset the window on every editor open.
        case Ambix_meterAudioProcessor::ViewMode::Circle:
        {
            const int w = ourProcessor->_size_circle_w;
            const int h = ourProcessor->_size_circle_h;
            // Far below the strip's natural width: resized() compacts the
            // strip to title + view selector, so the ring stays usable as a
            // small always-there window rather than a full-size panel.
            setResizeLimits (230, 220, 2000, 2000);
            _width  = w;
            _height = h;
            setSize (w, h);
            break;
        }

        case Ambix_meterAudioProcessor::ViewMode::Dots:
        {
            const int w = ourProcessor->_size_dots_w;
            const int h = ourProcessor->_size_dots_h;
            // The compact one: it only needs the strip's compacted width and
            // enough height for a row of dots.
            setResizeLimits (230, 90, 2000, 2000);
            _width  = w;
            _height = h;
            setSize (w, h);
            break;
        }

        case Ambix_meterAudioProcessor::ViewMode::Spectrogram:
        {
            const int w = ourProcessor->_size_spec_w;
            const int h = ourProcessor->_size_spec_h;
            setResizeLimits (720, 300, 3000, 2000);   // its toolbar is ~710 px
            _width  = w;
            _height = h;
            setSize (w, h);
            break;
        }

        case Ambix_meterAudioProcessor::ViewMode::Scope:
        {
            const int w = ourProcessor->_size_scope_w;
            const int h = ourProcessor->_size_scope_h;
            // The scope's own toolbar needs ~600 px.
            setResizeLimits (620, 300, 3000, 2000);
            _width  = w;
            _height = h;
            setSize (w, h);
            break;
        }

        case Ambix_meterAudioProcessor::ViewMode::Waterfall:
        {
            const int w = ourProcessor->_size_wf_w;
            const int h = ourProcessor->_size_wf_h;
            setResizeLimits (jmax (kMinEditorWidth, 700), 480, 2400, 1600);
            _width  = w;
            _height = h;
            setSize (w, h);
            break;
        }
    }

    applyModeVisibility();
    resized();
    repaint();
}

void Ambix_meterAudioProcessorEditor::syncToRestoredState()
{
    Ambix_meterAudioProcessor* p = getProcessor();
    const int gen = p->_state_generation.load (std::memory_order_acquire);
    if (gen == _appliedStateGen)
        return;

    // Until _appliedStateGen catches up, resized() leaves the remembered
    // sizes alone: the window still has the old view's size, and recording it
    // under the restored view would overwrite the size just restored.

    // The popped-out window shows the old view; bring it home first, as a
    // switch from the selector does.
    reattachView();

    cb_view.setSelectedId ((int) p->_view_mode + 1, dontSendNotification);

    // Both clamp against the live channel count and rebuild what depends on
    // the settings (the spectrogram's window, axis and history).
    _scope.setSettings (&p->_scope_settings);
    _spectro.setSettings (&p->_spectro_settings);

    // Size, visibility (and with it which analyser / capture runs) and the
    // channel highlight, for the restored view.
    applyModeSizing();

    _appliedStateGen = gen;
}

void Ambix_meterAudioProcessorEditor::changeListenerCallback (ChangeBroadcaster *source)
{
    Ambix_meterAudioProcessor* ourProcessor = getProcessor();

    syncToRestoredState();

    // Rebuild the per-channel strips when the host has re-negotiated the
    // layout (e.g. track channel count changed in Reaper).
    const int hostCh = jlimit (1, NUM_CHANNELS, ourProcessor->getTotalNumInputChannels());
    if (hostCh != _cachedNumCh)
        rebuildChannelStrips();

    _circular.setOffset ((int) ourProcessor->_offset);
    _circular.setPeakHoldVisible (ourProcessor->_pk_hold);
    _dots.setOffset ((int) ourProcessor->_offset);
    _dots.setPeakHoldVisible (ourProcessor->_pk_hold);

    sld_hold.setValue(ourProcessor->_hold,dontSendNotification);
    sld_fall.setValue(ourProcessor->_fall,dontSendNotification);
    tgl_pkhold.setToggleState(ourProcessor->_pk_hold, dontSendNotification);

    sld_offset.setValue(ourProcessor->_offset, dontSendNotification);

    for (int i = 0; i < _scales.size(); i++) {
        _scales.getUnchecked(i)->offset(sld_offset.getValue());
    }

    for (int i=0; i<_meters.size(); i++)
    {
        _meters.getUnchecked(i)->offset( (int)ourProcessor->_offset);
        _meters.getUnchecked(i)->_peak_hold = ourProcessor->_pk_hold;
    }

}

void Ambix_meterAudioProcessorEditor::comboBoxChanged (ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &cb_channel)
    {
        const int ch = cb_channel.getSelectedId() - 2;   // id 1 == no highlight
        getProcessor()->_selected_ch = ch;
        _circular.setSelectedChannel (ch);
        _waterfall.setSelectedChannel (ch);
        _dots.setSelectedChannel (ch);
        repaint();
        return;
    }

    if (comboBoxThatHasChanged != &cb_view)
        return;

    // The popped-out window shows one view; switching brings it home first.
    reattachView();

    Ambix_meterAudioProcessor* ourProcessor = getProcessor();
    const auto mode = (Ambix_meterAudioProcessor::ViewMode) jlimit (0, 5, cb_view.getSelectedId() - 1);
    if (mode == ourProcessor->_view_mode)
        return;

    ourProcessor->_view_mode = mode;
    applyModeSizing();          // applies visibility too
}

Component* Ambix_meterAudioProcessorEditor::activeView()
{
    switch (getProcessor()->_view_mode)
    {
        case Ambix_meterAudioProcessor::ViewMode::Circle:    return &_circular;
        case Ambix_meterAudioProcessor::ViewMode::Waterfall: return &_waterfall;
        case Ambix_meterAudioProcessor::ViewMode::Dots:      return &_dots;
        case Ambix_meterAudioProcessor::ViewMode::Scope:     return &_scope;
        case Ambix_meterAudioProcessor::ViewMode::Spectrogram: return &_spectro;
        default:                                             return nullptr;
    }
}

void Ambix_meterAudioProcessorEditor::showDetachMenu()
{
    if (_detached != nullptr)
    {
        reattachView();
        return;
    }

    PopupMenu m;
    m.addItem (1, "Open in window");
    m.addSeparator();

    const auto& displays = Desktop::getInstance().getDisplays().displays;
    for (int i = 0; i < displays.size(); ++i)
    {
        const auto r = displays.getReference (i).totalArea;
        String name = "Fullscreen on display " + String (i + 1) + "  ("
                    + String (r.getWidth()) + " x " + String (r.getHeight()) + ")";
        if (displays.getReference (i).isMain)
            name << "  main";
        m.addItem (100 + i, name);
    }

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&btn_detach),
                     [safe = Component::SafePointer<Ambix_meterAudioProcessorEditor> (this)] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r == 1) safe->detachView (-1, false);
        else        safe->detachView (r - 100, true);
    });
}

void Ambix_meterAudioProcessorEditor::detachView (int displayIndex, bool fullscreen)
{
    Component* view = activeView();
    if (view == nullptr || _detached != nullptr)
        return;

    const auto& displays = Desktop::getInstance().getDisplays().displays;
    const Displays::Display* display = isPositiveAndBelow (displayIndex, displays.size())
                                         ? &displays.getReference (displayIndex)
                                         : Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds());

    _detachedView = view;
    _detached = std::make_unique<DetachedViewWindow> ("mcfx_meter" + String (_cachedNumCh) + " - "
                                                        + cb_view.getText(), *view);
    _detached->onClose = [this] { reattachView(); };
    _detached->showOn (display, fullscreen);
    view->setVisible (true);

    resized();
    repaint();
}

void Ambix_meterAudioProcessorEditor::reattachView()
{
    if (_detached == nullptr)
        return;

    if (_detached->isFullScreen())
        _detached->setFullScreen (false);

    _detached->clearContentComponent();   // non-owned: only removes it
    _detached.reset();

    if (_detachedView != nullptr)
        addChildComponent (_detachedView);
    _detachedView = nullptr;

    applyModeVisibility();
    resized();
    repaint();
}

void Ambix_meterAudioProcessorEditor::resetAllMeters()
{
    Ambix_meterAudioProcessor* ourProcessor = getProcessor();

    // Walk the DSP array, not _meters: the bar strips only exist in bar mode,
    // and the ring needs every channel cleared too.
    for (int i = 0; i < ourProcessor->_my_meter_dsp.size(); ++i)
        ourProcessor->_my_meter_dsp.getUnchecked (i)->reset();

    for (int i = 0; i < _meters.size(); ++i)
        _meters.getUnchecked (i)->reset();

    _circular.repaint();
}

void Ambix_meterAudioProcessorEditor::mouseDown (const MouseEvent& e)
{
    //[UserCode_mouseDown] -- Add your code here...
    resetAllMeters();
    //[/UserCode_mouseDown]
}


//==============================================================================
#if 0
/*  -- Jucer information section --

    This is where the Jucer puts all of its metadata, so don't change anything in here!

BEGIN_JUCER_METADATA

<JUCER_COMPONENT documentType="Component" className="Ambix_meterAudioProcessorEditor"
                 componentName="" parentClasses="public AudioProcessorEditor"
                 constructorParams="Ambix_meterAudioProcessor* ownerFilter" variableInitialisers="AudioProcessorEditor (ownerFilter)"
                 snapPixels="8" snapActive="1" snapShown="1" overlayOpacity="0.330000013"
                 fixedSize="1" initialWidth="400" initialHeight="200">
  <BACKGROUND backgroundColour="ffffffff"/>
  <LABEL name="new label" id="b45e45d811b90270" memberName="label" virtualName=""
         explicitFocusOrder="0" pos="224 184 176 16" edTextCol="ff000000"
         edBkgCol="0" labelText="Ambix::meter" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
         fontsize="15" bold="0" italic="0" justification="34"/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif

//[EndFile] You can add extra defines here...
//[/EndFile]
