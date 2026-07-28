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
    addChildComponent (cb_wf_channel);
    cb_wf_channel.addListener (this);
    cb_wf_channel.setTooltip ("Keep one channel highlighted in the waterfall. "
                              "Clicking a channel number on the depth axis selects it too.");

    addChildComponent (_circular);
    addChildComponent (_waterfall);
    _waterfall.setAnalyser (&getProcessor()->_band_analyser);
    _waterfall.onChannelSelected = [this] (int ch)
    {
        getProcessor()->_wf_selected_ch = ch;
        cb_wf_channel.setSelectedId (ch + 2, dontSendNotification);
    };
    _circular.onResetAll = [this] { resetAllMeters(); };
    _circular.onChannelReset = [this] (int ch)
    {
        Ambix_meterAudioProcessor* p = getProcessor();
        if (isPositiveAndBelow (ch, p->_my_meter_dsp.size()))
            p->_my_meter_dsp.getUnchecked (ch)->reset();
    };

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
    //[/Destructor_pre]
    Ambix_meterAudioProcessor* ourProcessor = getProcessor();

    // remove me as listener for changes
    ourProcessor->removeChangeListener(this);

    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void Ambix_meterAudioProcessorEditor::paint (Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    g.fillAll (Colour (0xff1a1a1a));

    g.setGradientFill (ColourGradient (Colour (0xff4e4e4e),
                                       (float) (proportionOfWidth (0.6400f)), (float) (proportionOfHeight (0.6933f)),
                                       Colours::black,
                                       (float) (proportionOfWidth (0.1143f)), (float) (proportionOfHeight (0.0800f)),
                                       true));
    // Fill what the editor actually is, not what it was last sized to. The
    // resizable views let the user change the window without going through
    // applyModeSizing(), and filling a stale _width/_height left the gradient
    // as a visible rectangle over part of the background.
    g.fillRect (getLocalBounds());

    /* Version text */
    g.setColour (Colours::white);
    g.setFont (Font (FontOptions (10.00f, Font::plain)));
    String version_string;
    version_string << "v" << QUOTE(VERSION);
    g.drawText (version_string,
                getWidth()-51, getHeight()-11, 50, 10,
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

    label.setBounds (0, 0, 104, 16);
    sld_hold.setBounds (166, 0, 70, 24);
    sld_fall.setBounds (307, 0, 70, 24);
    label2.setBounds (111, 3, 64, 16);
    label3.setBounds (239, 3, 77, 16);
    tgl_pkhold.setBounds (382, 0, 91, 24);

    sld_offset.setBounds (4, 23, 18, 167);
    cb_view.setBounds (481, 1, 92, 22);
    // Only ever shown in the waterfall view, whose minimum width is 700 —
    // wide enough for this, unlike the 581 the other views allow.
    cb_wf_channel.setBounds (577, 1, 88, 22);

    // The ring and the waterfall each own everything below the control strip.
    _circular.setBounds  (0, 28, getWidth(), jmax (0, getHeight() - 28));
    _waterfall.setBounds (0, 28, getWidth(), jmax (0, getHeight() - 28));

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
    switch (ourProcessor->_view_mode)
    {
        case Ambix_meterAudioProcessor::ViewMode::Circle:
            ourProcessor->_size_circle_w = getWidth();
            ourProcessor->_size_circle_h = getHeight();
            break;
        case Ambix_meterAudioProcessor::ViewMode::Waterfall:
            ourProcessor->_size_wf_w = getWidth();
            ourProcessor->_size_wf_h = getHeight();
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
    else if (ourProcessor->_view_mode == Ambix_meterAudioProcessor::ViewMode::Circle)
    {
        const int n = jmin (_cachedNumCh, ourProcessor->_my_meter_dsp.size());
        for (int i = 0; i < n; ++i)
        {
            MyMeterDsp* d = ourProcessor->_my_meter_dsp.getUnchecked (i);
            _circular.setValue (i, d->getRMS(), d->getPeak(), d->getPeakHold());
        }
        _circular.setOffset ((int) ourProcessor->_offset);
        _circular.repaint();
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
    const bool wf = mode == Ambix_meterAudioProcessor::ViewMode::Waterfall;

    _circular.setNumChannels (_cachedNumCh);
    _circular.setVisible (mode == Ambix_meterAudioProcessor::ViewMode::Circle);

    _waterfall.setNumChannels (_cachedNumCh);
    _waterfall.setVisible (wf);
    cb_wf_channel.setVisible (wf);

    // Peak hold time, fall rate and the peak-hold toggle drive MyMeterDsp, which
    // only the bar and ring meters read — the waterfall shows band levels and
    // has no peak to hold, it smooths in the analyser instead. Hidden rather
    // than left sitting there doing nothing. The offset slider stays: it shifts
    // the level scale in all three views.
    sld_hold.setVisible (! wf);
    sld_fall.setVisible (! wf);
    label2.setVisible (! wf);
    label3.setVisible (! wf);
    tgl_pkhold.setVisible (! wf);
    if (wf)
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
}

void Ambix_meterAudioProcessorEditor::rebuildChannelSelector()
{
    Ambix_meterAudioProcessor* p = getProcessor();
    const int n = jmax (1, _cachedNumCh);

    // Rebuilt only when the count changes: refilling on every mode switch would
    // close the popup out from under the user.
    if (cb_wf_channel.getNumItems() != n + 1)
    {
        cb_wf_channel.clear (dontSendNotification);
        cb_wf_channel.addItem ("no highlight", 1);
        for (int ch = 0; ch < n; ++ch)
            cb_wf_channel.addItem ("ch " + String (ch + 1), ch + 2);
    }

    if (p->_wf_selected_ch >= n)
        p->_wf_selected_ch = -1;

    _waterfall.setSelectedChannel (p->_wf_selected_ch);
    cb_wf_channel.setSelectedId (p->_wf_selected_ch + 2, dontSendNotification);
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
            setResizeLimits (kMinEditorWidth, 560, 2000, 2000);
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

void Ambix_meterAudioProcessorEditor::changeListenerCallback (ChangeBroadcaster *source)
{
    Ambix_meterAudioProcessor* ourProcessor = getProcessor();

    // Rebuild the per-channel strips when the host has re-negotiated the
    // layout (e.g. track channel count changed in Reaper).
    const int hostCh = jlimit (1, NUM_CHANNELS, ourProcessor->getTotalNumInputChannels());
    if (hostCh != _cachedNumCh)
        rebuildChannelStrips();

    _circular.setOffset ((int) ourProcessor->_offset);
    _circular.setPeakHoldVisible (ourProcessor->_pk_hold);

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
    if (comboBoxThatHasChanged == &cb_wf_channel)
    {
        const int ch = cb_wf_channel.getSelectedId() - 2;   // id 1 == no highlight
        getProcessor()->_wf_selected_ch = ch;
        _waterfall.setSelectedChannel (ch);
        _waterfall.repaint();
        return;
    }

    if (comboBoxThatHasChanged != &cb_view)
        return;

    Ambix_meterAudioProcessor* ourProcessor = getProcessor();
    const auto mode = (Ambix_meterAudioProcessor::ViewMode) jlimit (0, 2, cb_view.getSelectedId() - 1);
    if (mode == ourProcessor->_view_mode)
        return;

    ourProcessor->_view_mode = mode;
    applyModeSizing();          // applies visibility too
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
