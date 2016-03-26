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


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
LowhighpassAudioProcessorEditor::LowhighpassAudioProcessorEditor (LowhighpassAudioProcessor* ownerFilter)
    : AudioProcessorEditor (ownerFilter),
    changed_(true)
{
    //[Constructor_pre] You can add your own custom stuff here..
    //[/Constructor_pre]
    tooltipWindow.setMillisecondsBeforeTipAppears (700); // tooltip delay
    
    addAndMakeVisible (lbl_gd = new Label ("new label",
                                           TRANS("mcfx_filter")));
    lbl_gd->setFont (Font (15.00f, Font::plain));
    lbl_gd->setJustificationType (Justification::centredLeft);
    lbl_gd->setEditable (false, false, false);
    lbl_gd->setColour (Label::textColourId, Colours::aquamarine);
    lbl_gd->setColour (TextEditor::textColourId, Colours::black);
    lbl_gd->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    addAndMakeVisible (btn_lc_on = new ImageButton ("new button"));
    btn_lc_on->setTooltip (TRANS("low cut off"));
    btn_lc_on->addListener (this);

    btn_lc_on->setImages (false, true, true,
                          ImageCache::getFromMemory (lc_off_png, lc_off_pngSize), 1.000f, Colour (0x00000000),
                          ImageCache::getFromMemory (lc_over_png, lc_over_pngSize), 1.000f, Colour (0x00000000),
                          ImageCache::getFromMemory (lc_on_png, lc_on_pngSize), 1.000f, Colour (0x00000000));
    
    btn_lc_on->setClickingTogglesState(true);
    
    
    addAndMakeVisible (sld_lc_f = new Slider ("new slider"));
    sld_lc_f->setTooltip (TRANS("low cutoff frequency"));
    sld_lc_f->setRange (24, 21618, 1);
    sld_lc_f->setSliderStyle (Slider::Rotary);
    sld_lc_f->setTextBoxStyle (Slider::TextBoxRight, false, 55, 18);
    sld_lc_f->setColour (Slider::thumbColourId, Colour (0xff5a5a90));
    sld_lc_f->setColour (Slider::trackColourId, Colours::coral);
    sld_lc_f->setColour (Slider::rotarySliderFillColourId, Colours::coral);
    sld_lc_f->setColour (Slider::rotarySliderOutlineColourId, Colours::coral);
    sld_lc_f->addListener (this);
    sld_lc_f->setSkewFactor (0.55);
    sld_lc_f->setDoubleClickReturnValue(true, 48.f);
    
    addAndMakeVisible (btn_lc_order = new ImageButton ("new button"));
    btn_lc_order->setTooltip (TRANS("2nd order low cut"));
    btn_lc_order->addListener (this);

    btn_lc_order->setImages (false, true, true,
                             ImageCache::getFromMemory (_2nd_png, _2nd_pngSize), 1.000f, Colour (0x00000000),
                             Image(), 1.000f, Colour (0x00000000),
                             ImageCache::getFromMemory (_4th_png, _4th_pngSize), 1.000f, Colour (0x00000000));
    
    btn_lc_order->setClickingTogglesState(true);
    
    
    addAndMakeVisible (btn_hc_on = new ImageButton ("new button"));
    btn_hc_on->setTooltip (TRANS("high cut off"));
    btn_hc_on->addListener (this);

    btn_hc_on->setImages (false, true, true,
                          ImageCache::getFromMemory (hc_off_png, hc_off_pngSize), 1.000f, Colour (0x00000000),
                          ImageCache::getFromMemory (hc_over_png, hc_over_pngSize), 1.000f, Colour (0x00000000),
                          ImageCache::getFromMemory (hc_on_png, hc_on_pngSize), 1.000f, Colour (0x00000000));
    
    btn_hc_on->setClickingTogglesState(true);
    
    
    addAndMakeVisible (sld_hc_f = new Slider ("new slider"));
    sld_hc_f->setTooltip (TRANS("high cutoff frequency"));
    sld_hc_f->setRange (24, 21618, 1);
    sld_hc_f->setSliderStyle (Slider::Rotary);
    sld_hc_f->setTextBoxStyle (Slider::TextBoxLeft, false, 55, 18);
    sld_hc_f->setColour (Slider::thumbColourId, Colour (0xff5a5a90));
    sld_hc_f->setColour (Slider::trackColourId, Colours::coral);
    sld_hc_f->setColour (Slider::rotarySliderFillColourId, Colours::coral);
    sld_hc_f->setColour (Slider::rotarySliderOutlineColourId, Colours::coral);
    sld_hc_f->addListener (this);
    sld_hc_f->setSkewFactor (0.55);
    sld_hc_f->setDoubleClickReturnValue(true, 10960.f);
    
    addAndMakeVisible (btn_hc_order = new ImageButton ("new button"));
    btn_hc_order->setTooltip (TRANS("2nd order high cut"));
    btn_hc_order->addListener (this);

    btn_hc_order->setImages (false, true, true,
                             ImageCache::getFromMemory (_2nd_png, _2nd_pngSize), 1.000f, Colour (0x00000000),
                             Image(), 1.000f, Colour (0x00000000),
                             ImageCache::getFromMemory (_4th_png, _4th_pngSize), 1.000f, Colour (0x00000000));
    
    btn_hc_order->setClickingTogglesState(true);
    
    
    addAndMakeVisible (sld_hs_f = new Slider ("new slider"));
    sld_hs_f->setTooltip (TRANS("high shelf frequency"));
    sld_hs_f->setRange (24, 21618, 1);
    sld_hs_f->setSliderStyle (Slider::Rotary);
    sld_hs_f->setTextBoxStyle (Slider::TextBoxRight, false, 55, 18);
    sld_hs_f->setColour (Slider::thumbColourId, Colour (0xff5a5a90));
    sld_hs_f->setColour (Slider::trackColourId, Colours::chocolate);
    sld_hs_f->setColour (Slider::rotarySliderFillColourId, Colours::coral);
    sld_hs_f->setColour (Slider::rotarySliderOutlineColourId, Colours::coral);
    sld_hs_f->addListener (this);
    sld_hs_f->setSkewFactor (0.55);
    sld_hs_f->setDoubleClickReturnValue(true, 2817.f);

    addAndMakeVisible (sld_hs_g = new Slider ("new slider"));
    sld_hs_g->setTooltip (TRANS("high shelf gain"));
    sld_hs_g->setRange (-18, 18, 0.1);
    sld_hs_g->setSliderStyle (Slider::Rotary);
    sld_hs_g->setTextBoxStyle (Slider::TextBoxRight, false, 45, 18);
    sld_hs_g->setColour (Slider::thumbColourId, Colour (0xff5a5a90));
    sld_hs_g->setColour (Slider::trackColourId, Colours::yellow);
    sld_hs_g->setColour (Slider::rotarySliderFillColourId, Colours::yellow);
    sld_hs_g->setColour (Slider::rotarySliderOutlineColourId, Colours::yellow);
    sld_hs_g->addListener (this);
    sld_hs_g->setDoubleClickReturnValue(true, 0.f);

    addAndMakeVisible (sld_hs_q = new Slider ("new slider"));
    sld_hs_q->setTooltip (TRANS("high shelf q"));
    sld_hs_q->setRange (0.2, 20, 0.1);
    sld_hs_q->setSliderStyle (Slider::Rotary);
    sld_hs_q->setTextBoxStyle (Slider::TextBoxRight, false, 40, 18);
    sld_hs_q->setColour (Slider::thumbColourId, Colour (0xff5a5a90));
    sld_hs_q->setColour (Slider::trackColourId, Colours::aqua);
    sld_hs_q->setColour (Slider::rotarySliderFillColourId, Colours::aqua);
    sld_hs_q->setColour (Slider::rotarySliderOutlineColourId, Colours::aqua);
    sld_hs_q->addListener (this);
    sld_hs_q->setSkewFactor (0.5);
    sld_hs_q->setDoubleClickReturnValue(true, 0.7f);

    addAndMakeVisible (sld_p1_f = new Slider ("new slider"));
    sld_p1_f->setTooltip (TRANS("peak1 frequency"));
    sld_p1_f->setRange (24, 21618, 1);
    sld_p1_f->setSliderStyle (Slider::Rotary);
    sld_p1_f->setTextBoxStyle (Slider::TextBoxRight, false, 55, 18);
    sld_p1_f->setColour (Slider::thumbColourId, Colour (0xff5a5a90));
    sld_p1_f->setColour (Slider::trackColourId, Colours::chocolate);
    sld_p1_f->setColour (Slider::rotarySliderFillColourId, Colours::coral);
    sld_p1_f->setColour (Slider::rotarySliderOutlineColourId, Colours::coral);
    sld_p1_f->addListener (this);
    sld_p1_f->setSkewFactor (0.55);
    sld_p1_f->setDoubleClickReturnValue(true, 186.f);

    addAndMakeVisible (sld_p1_g = new Slider ("new slider"));
    sld_p1_g->setTooltip (TRANS("peak1 gain"));
    sld_p1_g->setRange (-18, 18, 0.1);
    sld_p1_g->setSliderStyle (Slider::Rotary);
    sld_p1_g->setTextBoxStyle (Slider::TextBoxRight, false, 45, 18);
    sld_p1_g->setColour (Slider::thumbColourId, Colour (0xff5a5a90));
    sld_p1_g->setColour (Slider::trackColourId, Colours::yellow);
    sld_p1_g->setColour (Slider::rotarySliderFillColourId, Colours::yellow);
    sld_p1_g->setColour (Slider::rotarySliderOutlineColourId, Colours::yellow);
    sld_p1_g->addListener (this);
    sld_p1_g->setDoubleClickReturnValue(true, 0.f);

    addAndMakeVisible (sld_p1_q = new Slider ("new slider"));
    sld_p1_q->setTooltip (TRANS("peak1 q"));
    sld_p1_q->setRange (0.2, 20, 0.1);
    sld_p1_q->setSliderStyle (Slider::Rotary);
    sld_p1_q->setTextBoxStyle (Slider::TextBoxRight, false, 40, 18);
    sld_p1_q->setColour (Slider::thumbColourId, Colour (0xff5a5a90));
    sld_p1_q->setColour (Slider::trackColourId, Colours::aqua);
    sld_p1_q->setColour (Slider::rotarySliderFillColourId, Colours::aqua);
    sld_p1_q->setColour (Slider::rotarySliderOutlineColourId, Colours::aqua);
    sld_p1_q->addListener (this);
    sld_p1_q->setSkewFactor (0.5);
    sld_p1_q->setDoubleClickReturnValue(true, 0.7f);

    addAndMakeVisible (sld_p2_f = new Slider ("new slider"));
    sld_p2_f->setTooltip (TRANS("peak2 frequency"));
    sld_p2_f->setRange (24, 21618, 1);
    sld_p2_f->setSliderStyle (Slider::Rotary);
    sld_p2_f->setTextBoxStyle (Slider::TextBoxRight, false, 55, 18);
    sld_p2_f->setColour (Slider::thumbColourId, Colour (0xff5a5a90));
    sld_p2_f->setColour (Slider::trackColourId, Colours::chocolate);
    sld_p2_f->setColour (Slider::rotarySliderFillColourId, Colours::coral);
    sld_p2_f->setColour (Slider::rotarySliderOutlineColourId, Colours::coral);
    sld_p2_f->addListener (this);
    sld_p2_f->setSkewFactor (0.55);
    sld_p2_f->setDoubleClickReturnValue(true, 2817.f);

    addAndMakeVisible (sld_p2_g = new Slider ("new slider"));
    sld_p2_g->setTooltip (TRANS("peak2 gain"));
    sld_p2_g->setRange (-18, 18, 0.1);
    sld_p2_g->setSliderStyle (Slider::Rotary);
    sld_p2_g->setTextBoxStyle (Slider::TextBoxRight, false, 45, 18);
    sld_p2_g->setColour (Slider::thumbColourId, Colour (0xff5a5a90));
    sld_p2_g->setColour (Slider::trackColourId, Colours::yellow);
    sld_p2_g->setColour (Slider::rotarySliderFillColourId, Colours::yellow);
    sld_p2_g->setColour (Slider::rotarySliderOutlineColourId, Colours::yellow);
    sld_p2_g->addListener (this);
    sld_p2_g->setDoubleClickReturnValue(true, 0.f);

    addAndMakeVisible (sld_p2_q = new Slider ("new slider"));
    sld_p2_q->setTooltip (TRANS("peak2 q"));
    sld_p2_q->setRange (0.2, 20, 0.1);
    sld_p2_q->setSliderStyle (Slider::Rotary);
    sld_p2_q->setTextBoxStyle (Slider::TextBoxRight, false, 40, 18);
    sld_p2_q->setColour (Slider::thumbColourId, Colour (0xff5a5a90));
    sld_p2_q->setColour (Slider::trackColourId, Colours::aqua);
    sld_p2_q->setColour (Slider::rotarySliderFillColourId, Colours::aqua);
    sld_p2_q->setColour (Slider::rotarySliderOutlineColourId, Colours::aqua);
    sld_p2_q->addListener (this);
    sld_p2_q->setSkewFactor (0.5);
    sld_p2_q->setDoubleClickReturnValue(true, 0.7f);

    addAndMakeVisible (sld_ls_f = new Slider ("new slider"));
    sld_ls_f->setTooltip (TRANS("low shelf frequency"));
    sld_ls_f->setRange (24, 21618, 1);
    sld_ls_f->setSliderStyle (Slider::Rotary);
    sld_ls_f->setTextBoxStyle (Slider::TextBoxRight, false, 55, 18);
    sld_ls_f->setColour (Slider::thumbColourId, Colour (0xff5a5a90));
    sld_ls_f->setColour (Slider::trackColourId, Colours::chocolate);
    sld_ls_f->setColour (Slider::rotarySliderFillColourId, Colours::coral);
    sld_ls_f->setColour (Slider::rotarySliderOutlineColourId, Colours::coral);
    sld_ls_f->addListener (this);
    sld_ls_f->setSkewFactor (0.55);
    sld_ls_f->setDoubleClickReturnValue(true, 94.f);

    addAndMakeVisible (sld_ls_g = new Slider ("new slider"));
    sld_ls_g->setTooltip (TRANS("low shelf gain"));
    sld_ls_g->setRange (-18, 18, 0.1);
    sld_ls_g->setSliderStyle (Slider::Rotary);
    sld_ls_g->setTextBoxStyle (Slider::TextBoxRight, false, 45, 18);
    sld_ls_g->setColour (Slider::thumbColourId, Colour (0xff5a5a90));
    sld_ls_g->setColour (Slider::trackColourId, Colours::yellow);
    sld_ls_g->setColour (Slider::rotarySliderFillColourId, Colours::yellow);
    sld_ls_g->setColour (Slider::rotarySliderOutlineColourId, Colours::yellow);
    sld_ls_g->addListener (this);
    sld_ls_g->setDoubleClickReturnValue(true, 0.f);

    addAndMakeVisible (sld_ls_q = new Slider ("new slider"));
    sld_ls_q->setTooltip (TRANS("low shelf q"));
    sld_ls_q->setRange (0.2, 20, 0.1);
    sld_ls_q->setSliderStyle (Slider::Rotary);
    sld_ls_q->setTextBoxStyle (Slider::TextBoxRight, false, 40, 18);
    sld_ls_q->setColour (Slider::thumbColourId, Colour (0xff5a5a90));
    sld_ls_q->setColour (Slider::trackColourId, Colours::aqua);
    sld_ls_q->setColour (Slider::rotarySliderFillColourId, Colours::aqua);
    sld_ls_q->setColour (Slider::rotarySliderOutlineColourId, Colours::aqua);
    sld_ls_q->addListener (this);
    sld_ls_q->setSkewFactor (0.5);
    sld_ls_q->setDoubleClickReturnValue(true, 0.7f);

    addAndMakeVisible (filtergraph = new FilterGraph(ownerFilter, ownerFilter));
    filtergraph->setName ("new component");
    
    filtergraph->analyzeron_ = ownerFilter->_freqanalysis;
    
    addAndMakeVisible (btn_analyzer = new ImageButton ("new button"));
    btn_analyzer->addListener (this);
    
    btn_analyzer->setImages (false, true, true,
                             ImageCache::getFromMemory (analyzer_off_png, analyzer_off_pngSize), 1.000f, Colour (0x00000000),
                             ImageCache::getFromMemory (analyzer_over_png, analyzer_over_pngSize), 1.000f, Colour (0x00000000),
                             ImageCache::getFromMemory (analyzer_on_png, analyzer_on_pngSize), 1.000f, Colour (0x00000000));
    btn_analyzer->setClickingTogglesState(true);
    btn_analyzer->setToggleState(ownerFilter->_freqanalysis, dontSendNotification);
    
    
    
    //[UserPreSize]
    //[/UserPreSize]

    setSize (630, 300);

    ownerFilter->_editorOpen = true;
    
    ownerFilter->addChangeListener(this);
    
    // turn on the fft
    // ownerFilter->freqanalysis(true);
    
    startTimer(80);
    
    //[Constructor] You can add your own custom stuff here..
    //[/Constructor]
}

LowhighpassAudioProcessorEditor::~LowhighpassAudioProcessorEditor()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    //[/Destructor_pre]
    // getProcessor()->freqanalysis(false);
    
    // remove me as listener for changes
    getProcessor()->removeChangeListener(this);
    
    
    getProcessor()->_editorOpen = false;
    
    lbl_gd = nullptr;
    btn_lc_on = nullptr;
    sld_lc_f = nullptr;
    btn_lc_order = nullptr;
    btn_hc_on = nullptr;
    sld_hc_f = nullptr;
    btn_hc_order = nullptr;
    sld_hs_f = nullptr;
    sld_hs_g = nullptr;
    sld_hs_q = nullptr;
    sld_p1_f = nullptr;
    sld_p1_g = nullptr;
    sld_p1_q = nullptr;
    sld_p2_f = nullptr;
    sld_p2_g = nullptr;
    sld_p2_q = nullptr;
    sld_ls_f = nullptr;
    sld_ls_g = nullptr;
    sld_ls_q = nullptr;
    filtergraph = nullptr;
    btn_analyzer = nullptr;


    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

void LowhighpassAudioProcessorEditor::timerCallback()
{
    if (changed_)
    {
        changed_ = false;
        updateSliders();
        // filtergraph->repaint();
    }
}
//==============================================================================
void LowhighpassAudioProcessorEditor::paint (Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    g.fillAll (Colour (0xff1a1a1a));

    g.setColour (Colour (0xff424242));
    g.fillRoundedRectangle (209.0f, 196.0f, 95.0f, 96.0f, 10.000f);

    g.setColour (Colour (0xff424242));
    g.fillRoundedRectangle (6.0f, 219.0f, 95.0f, 72.0f, 10.000f);

    g.setColour (Colour (0xff424242));
    g.fillRoundedRectangle (107.0f, 195.0f, 95.0f, 96.0f, 10.000f);

    g.setColour (Colours::white);
    g.setFont (Font (15.00f, Font::plain));
    g.drawText (TRANS("low shelf"),
                120, 195, 70, 23,
                Justification::centred, true);

    g.setColour (Colour (0xff424242));
    g.fillRoundedRectangle (322.0f, 196.0f, 95.0f, 96.0f, 10.000f);

    g.setColour (Colour (0xff424242));
    g.fillRoundedRectangle (423.0f, 196.0f, 95.0f, 96.0f, 10.000f);

    g.setColour (Colour (0xff424242));
    g.fillRoundedRectangle (523.0f, 220.0f, 95.0f, 72.0f, 10.000f);

    g.setColour (Colours::white);
    g.setFont (Font (15.00f, Font::plain));
    g.drawText (TRANS("peak1"),
                221, 194, 70, 23,
                Justification::centred, true);

    g.setColour (Colours::white);
    g.setFont (Font (15.00f, Font::plain));
    g.drawText (TRANS("peak2"),
                332, 194, 70, 23,
                Justification::centred, true);

    g.setColour (Colours::white);
    g.setFont (Font (15.00f, Font::plain));
    g.drawText (TRANS("high shelf"),
                434, 195, 70, 23,
                Justification::centred, true);

    g.setColour (Colours::white);
    g.setFont (Font (15.00f, Font::plain));
    g.drawText (TRANS("f"),
                306, 265, 15, 23,
                Justification::centred, true);

    g.setColour (Colours::white);
    g.setFont (Font (15.00f, Font::plain));
    g.drawText (TRANS("g"),
                306, 238, 15, 23,
                Justification::centred, true);

    g.setColour (Colours::white);
    g.setFont (Font (15.00f, Font::plain));
    g.drawText (TRANS("Q"),
                306, 213, 15, 23,
                Justification::centred, true);

    g.setColour (Colours::white);
    g.setFont (Font (15.00f, Font::plain));
    g.drawText (TRANS("low cut"),
                19, 218, 70, 23,
                Justification::centred, true);

    g.setColour (Colours::white);
    g.setFont (Font (15.00f, Font::plain));
    g.drawText (TRANS("high cut"),
                538, 219, 70, 23,
                Justification::centred, true);

    
    /* Version text */
    g.setColour (Colours::white);
    g.setFont (Font (10.00f, Font::plain));
    String version_string;
    version_string << "v" << QUOTE(VERSION);
    g.drawText (version_string,
                getWidth()-51, getHeight()-11, 50, 10,
                Justification::bottomRight, true);
    
    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void LowhighpassAudioProcessorEditor::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    lbl_gd->setBounds (0, 0, 115, 16);
    btn_lc_on->setBounds (36, 240, 27, 21);
    sld_lc_f->setBounds (7, 262, 86, 24);
    btn_lc_order->setBounds (65, 240, 27, 21);
    btn_hc_on->setBounds (534, 242, 27, 21);
    sld_hc_f->setBounds (535, 264, 86, 24);
    btn_hc_order->setBounds (562, 242, 27, 21);
    sld_hs_f->setBounds (423, 264, 85, 24);
    sld_hs_g->setBounds (423, 239, 75, 24);
    sld_hs_q->setBounds (423, 215, 70, 22);
    sld_p1_f->setBounds (213, 264, 85, 24);
    sld_p1_g->setBounds (213, 239, 75, 24);
    sld_p1_q->setBounds (213, 215, 70, 22);
    sld_p2_f->setBounds (324, 264, 85, 24);
    sld_p2_g->setBounds (324, 239, 75, 24);
    sld_p2_q->setBounds (324, 215, 70, 22);
    sld_ls_f->setBounds (109, 266, 85, 24);
    sld_ls_g->setBounds (109, 241, 75, 24);
    sld_ls_q->setBounds (109, 217, 70, 22);
    filtergraph->setBounds (26, 21, 580, 170);
    btn_analyzer->setBounds (27, 193, 58, 20);
    //[UserResized] Add your own custom resize handling here..
    //[/UserResized]
}

void LowhighpassAudioProcessorEditor::updateSliders()
{
    LowhighpassAudioProcessor* ourProcessor = getProcessor();
    
    float lc_f = param2freq(ourProcessor->getParameter(LowhighpassAudioProcessor::LCfreqParam));
    sld_lc_f->setValue(lc_f, dontSendNotification);
    float ls_f = param2freq(ourProcessor->getParameter(LowhighpassAudioProcessor::LSfreqParam));
    sld_ls_f->setValue(ls_f, dontSendNotification);
    float p1_f = param2freq(ourProcessor->getParameter(LowhighpassAudioProcessor::PF1freqParam));
    sld_p1_f->setValue(p1_f, dontSendNotification);
    float p2_f = param2freq(ourProcessor->getParameter(LowhighpassAudioProcessor::PF2freqParam));
    sld_p2_f->setValue(p2_f, dontSendNotification);
    float hs_f = param2freq(ourProcessor->getParameter(LowhighpassAudioProcessor::HSfreqParam));
    sld_hs_f->setValue(hs_f, dontSendNotification);
    float hc_f = param2freq(ourProcessor->getParameter(LowhighpassAudioProcessor::HCfreqParam));
    sld_hc_f->setValue(hc_f, dontSendNotification);
    
    
    sld_ls_q->setValue(param2q(ourProcessor->getParameter(LowhighpassAudioProcessor::LSQParam)), dontSendNotification);
    sld_p1_q->setValue(param2q(ourProcessor->getParameter(LowhighpassAudioProcessor::PF1QParam)), dontSendNotification);
    sld_p2_q->setValue(param2q(ourProcessor->getParameter(LowhighpassAudioProcessor::PF2QParam)), dontSendNotification);
    sld_hs_q->setValue(param2q(ourProcessor->getParameter(LowhighpassAudioProcessor::HSQParam)), dontSendNotification);
    
    float ls_g = param2db(ourProcessor->getParameter(LowhighpassAudioProcessor::LSGainParam));
    sld_ls_g->setValue(ls_g, dontSendNotification);
    float p1_g = param2db(ourProcessor->getParameter(LowhighpassAudioProcessor::PF1GainParam));
    sld_p1_g->setValue(p1_g, dontSendNotification);
    float p2_g = param2db(ourProcessor->getParameter(LowhighpassAudioProcessor::PF2GainParam));
    sld_p2_g->setValue(p2_g, dontSendNotification);
    float hs_g = param2db(ourProcessor->getParameter(LowhighpassAudioProcessor::HSGainParam));
    sld_hs_g->setValue(hs_g, dontSendNotification);
    
    
    filtergraph->setDragButton(0, lc_f, 0.f);
    filtergraph->setDragButton(1, ls_f, ls_g);
    filtergraph->setDragButton(2, p1_f, p1_g);
    filtergraph->setDragButton(3, p2_f, p2_g);
    filtergraph->setDragButton(4, hs_f, hs_g);
    filtergraph->setDragButton(5, hc_f, 0.f);
    
    
    if (ourProcessor->getParameter(LowhighpassAudioProcessor::LCOnParam) > 0.5f)
        btn_lc_on->setToggleState(true, dontSendNotification);
    else
        btn_lc_on->setToggleState(false, dontSendNotification);
    
    if (ourProcessor->getParameter(LowhighpassAudioProcessor::HCOnParam) > 0.5f)
        btn_hc_on->setToggleState(true, dontSendNotification);
    else
        btn_hc_on->setToggleState(false, dontSendNotification);
    
    
    if (ourProcessor->getParameter(LowhighpassAudioProcessor::LCorderParam) > 0.5f)
        btn_lc_order->setToggleState(true, dontSendNotification);
    else
        btn_lc_order->setToggleState(false, dontSendNotification);
    
    if (ourProcessor->getParameter(LowhighpassAudioProcessor::HCorderParam) > 0.5f)
        btn_hc_order->setToggleState(true, dontSendNotification);
    else
        btn_hc_order->setToggleState(false, dontSendNotification);
}

void LowhighpassAudioProcessorEditor::changeListenerCallback (ChangeBroadcaster *source)
{
    filtergraph->changed(true);
    
    changed_ = true;    
}

void LowhighpassAudioProcessorEditor::buttonClicked (Button* buttonThatWasClicked)
{
    LowhighpassAudioProcessor* ourProcessor = getProcessor();

    if (buttonThatWasClicked == btn_lc_on)
    {
        ourProcessor->setParameter(LowhighpassAudioProcessor::LCOnParam, btn_lc_on->getToggleState()==true ? 1.f : 0.f);
    }
    else if (buttonThatWasClicked == btn_lc_order)
    {
        ourProcessor->setParameter(LowhighpassAudioProcessor::LCorderParam, btn_lc_order->getToggleState()==true ? 1.f : 0.f);
    }
    else if (buttonThatWasClicked == btn_hc_on)
    {
        ourProcessor->setParameter(LowhighpassAudioProcessor::HCOnParam, btn_hc_on->getToggleState()==true ? 1.f : 0.f);
    }
    else if (buttonThatWasClicked == btn_hc_order)
    {
        ourProcessor->setParameter(LowhighpassAudioProcessor::HCorderParam, btn_hc_order->getToggleState()==true ? 1.f : 0.f);
    }
    else if (buttonThatWasClicked == btn_analyzer)
    {
        getProcessor()->freqanalysis(btn_analyzer->getToggleState());
        filtergraph->analyzeron_ = btn_analyzer->getToggleState();
    }

    //[UserbuttonClicked_Post]
    //[/UserbuttonClicked_Post]
}

void LowhighpassAudioProcessorEditor::sliderValueChanged (Slider* sliderThatWasMoved)
{
    LowhighpassAudioProcessor* ourProcessor = getProcessor();

    if (sliderThatWasMoved == sld_lc_f)
    {
        ourProcessor->setParameter(LowhighpassAudioProcessor::LCfreqParam, freq2param(sld_lc_f->getValue()));
    }
    else if (sliderThatWasMoved == sld_hc_f)
    {
        ourProcessor->setParameter(LowhighpassAudioProcessor::HCfreqParam, freq2param(sld_hc_f->getValue()));
    }
    else if (sliderThatWasMoved == sld_hs_f)
    {
        ourProcessor->setParameter(LowhighpassAudioProcessor::HSfreqParam, freq2param(sld_hs_f->getValue()));
    }
    else if (sliderThatWasMoved == sld_hs_g)
    {
        ourProcessor->setParameter(LowhighpassAudioProcessor::HSGainParam, db2param(sld_hs_g->getValue()));
    }
    else if (sliderThatWasMoved == sld_hs_q)
    {
        ourProcessor->setParameter(LowhighpassAudioProcessor::HSQParam, q2param(sld_hs_q->getValue()));
    }
    else if (sliderThatWasMoved == sld_p1_f)
    {
        ourProcessor->setParameter(LowhighpassAudioProcessor::PF1freqParam, freq2param(sld_p1_f->getValue()));
    }
    else if (sliderThatWasMoved == sld_p1_g)
    {
        ourProcessor->setParameter(LowhighpassAudioProcessor::PF1GainParam, db2param(sld_p1_g->getValue()));
    }
    else if (sliderThatWasMoved == sld_p1_q)
    {
        ourProcessor->setParameter(LowhighpassAudioProcessor::PF1QParam, q2param(sld_p1_q->getValue()));
    }
    else if (sliderThatWasMoved == sld_p2_f)
    {
        ourProcessor->setParameter(LowhighpassAudioProcessor::PF2freqParam, freq2param(sld_p2_f->getValue()));
    }
    else if (sliderThatWasMoved == sld_p2_g)
    {
        ourProcessor->setParameter(LowhighpassAudioProcessor::PF2GainParam, db2param(sld_p2_g->getValue()));
    }
    else if (sliderThatWasMoved == sld_p2_q)
    {
        ourProcessor->setParameter(LowhighpassAudioProcessor::PF2QParam, q2param(sld_p2_q->getValue()));
    }
    else if (sliderThatWasMoved == sld_ls_f)
    {
        ourProcessor->setParameter(LowhighpassAudioProcessor::LSfreqParam, freq2param(sld_ls_f->getValue()));
    }
    else if (sliderThatWasMoved == sld_ls_g)
    {
        ourProcessor->setParameter(LowhighpassAudioProcessor::LSGainParam, db2param(sld_ls_g->getValue()));
    }
    else if (sliderThatWasMoved == sld_ls_q)
    {
        ourProcessor->setParameter(LowhighpassAudioProcessor::LSQParam, q2param(sld_ls_q->getValue()));
    }

    //[UsersliderValueChanged_Post]
    //[/UsersliderValueChanged_Post]
}



//==============================================================================
// Binary resources - be careful not to edit any of these sections!

// JUCER_RESOURCE: lc_off_png, 687, "lc_off.png"
static const unsigned char resource_LowhighpassAudioProcessorEditor_lc_off_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,78,0,0,0,54,8,6,0,0,1,30,220,159,39,0,0,0,9,112,72,89,115,0,0,14,
197,0,0,14,198,1,108,65,191,60,0,0,2,97,73,68,65,84,120,218,237,154,57,142,194,48,20,64,63,136,22,177,181,108,5,162,71,72,208,208,81,178,137,142,181,166,0,78,3,41,40,89,4,92,128,69,154,51,112,3,182,3,
32,177,29,32,35,35,13,98,6,5,155,196,78,140,230,127,9,170,175,207,203,75,140,253,29,59,0,64,5,134,112,144,175,118,187,205,150,200,92,241,173,196,241,120,12,149,74,133,158,248,42,233,237,159,102,211,195,
162,70,223,85,143,70,35,168,213,106,244,196,87,73,247,196,233,116,10,165,82,137,158,72,75,186,235,97,189,114,174,118,12,21,84,20,5,154,205,38,159,130,221,110,23,90,173,22,31,66,163,197,158,10,26,45,38,
236,166,168,220,138,73,251,252,125,88,49,218,255,140,41,35,226,169,152,199,227,145,99,104,253,42,230,245,122,249,220,128,94,175,7,141,70,131,79,49,163,133,228,127,104,185,78,54,210,94,41,194,125,28,220,
233,116,130,225,112,40,20,230,239,179,207,4,183,219,237,96,54,155,129,217,3,135,10,183,221,110,97,62,159,115,89,161,112,133,179,18,236,37,220,102,179,129,197,98,97,25,152,38,220,122,189,134,229,114,105,
41,152,38,220,245,122,181,28,76,19,46,22,139,225,159,176,169,237,2,119,56,89,87,36,233,116,90,193,137,31,225,16,14,225,204,132,235,116,58,66,65,82,169,212,211,180,201,4,183,223,239,193,102,179,153,190,
24,96,130,35,75,116,183,219,45,223,109,37,253,131,170,170,186,118,134,132,195,17,107,70,54,136,132,246,16,36,170,213,170,124,112,164,185,49,186,129,37,4,238,199,26,237,245,141,37,112,86,91,211,132,35,
109,161,213,214,52,225,72,191,234,243,249,228,155,190,72,207,74,162,92,46,203,7,23,137,68,160,94,175,203,59,241,187,92,46,92,50,33,220,255,221,142,144,181,235,151,53,164,223,141,192,225,128,226,48,80,
28,138,67,113,40,14,197,233,140,195,225,0,147,201,228,35,133,36,18,9,72,38,147,230,139,35,39,207,142,199,35,62,113,122,164,217,237,118,200,100,50,16,14,135,81,28,74,19,32,142,156,107,34,231,155,136,180,
108,54,11,161,80,8,135,42,74,19,32,238,81,90,46,151,131,96,48,136,203,17,90,12,6,3,56,159,207,255,82,154,110,113,143,210,242,249,60,4,2,1,92,0,163,52,1,226,80,154,14,113,253,126,31,46,151,203,77,90,161,
80,0,191,223,143,189,42,45,86,171,21,56,157,206,219,113,141,98,177,40,205,75,58,233,197,197,227,241,219,7,67,240,238,8,138,195,64,113,40,78,38,113,228,205,52,170,96,143,104,52,250,245,13,87,26,219,170,
221,51,207,110,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* LowhighpassAudioProcessorEditor::lc_off_png = (const char*) resource_LowhighpassAudioProcessorEditor_lc_off_png;
const int LowhighpassAudioProcessorEditor::lc_off_pngSize = 687;

// JUCER_RESOURCE: _2nd_png, 1749, "2nd.png"
static const unsigned char resource_LowhighpassAudioProcessorEditor__2nd_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,78,0,0,0,54,8,6,0,0,1,30,220,159,39,0,0,0,9,112,72,89,115,0,0,14,
197,0,0,14,198,1,108,65,191,60,0,0,6,135,73,68,65,84,120,218,237,91,123,76,83,87,28,254,90,74,11,148,242,146,34,80,176,10,10,162,104,8,133,40,154,129,154,204,69,134,238,65,166,115,137,239,248,204,130,
89,150,225,36,202,162,102,40,238,143,101,26,153,198,141,196,37,206,231,220,195,215,28,78,139,36,58,35,16,140,111,64,1,17,148,71,228,81,40,165,133,222,157,222,190,75,145,11,82,104,245,254,72,195,61,231,
252,238,233,119,190,251,187,191,243,157,115,128,7,14,40,242,51,160,241,116,78,137,212,110,6,142,22,86,187,56,15,126,219,87,66,52,205,139,46,223,226,252,76,58,89,211,215,49,252,228,38,210,184,21,211,58,
118,227,229,167,89,164,38,210,126,143,58,39,35,140,208,179,57,8,237,239,171,95,133,149,7,134,166,115,164,24,57,50,161,166,223,193,244,246,104,209,219,210,1,190,216,231,213,131,233,248,254,40,124,191,90,
54,240,96,108,157,6,61,106,230,244,48,29,249,176,126,243,107,117,168,46,189,134,151,85,211,16,156,238,111,122,4,205,133,157,224,223,62,3,197,47,15,32,41,206,177,138,212,1,59,228,199,39,35,56,222,58,56,
3,83,132,64,202,50,248,100,24,31,225,26,230,8,91,119,229,163,34,187,162,79,196,15,121,200,126,219,87,35,113,187,19,61,20,106,216,58,115,218,248,27,153,206,116,225,97,78,17,90,104,154,218,224,46,246,39,
215,42,242,105,38,159,48,102,157,89,198,217,45,206,49,72,230,220,65,232,213,28,84,239,172,129,87,188,15,2,162,26,192,139,98,216,153,177,35,109,219,19,8,151,199,0,79,239,152,218,148,165,237,240,106,174,
134,119,148,140,249,48,53,229,119,81,22,125,212,208,113,28,93,55,62,91,106,104,149,50,231,172,243,204,95,168,248,86,128,193,134,141,221,206,238,167,223,48,240,37,31,112,210,97,204,217,91,16,180,67,206,
26,195,249,178,59,237,72,89,112,142,50,74,89,131,98,225,229,126,39,227,215,2,103,204,196,162,20,25,2,63,14,64,213,230,2,186,28,113,125,27,198,36,9,81,63,55,11,117,114,10,225,185,239,162,118,75,1,162,30,
238,132,183,247,35,148,134,29,197,248,131,11,81,189,225,44,241,158,232,8,230,180,136,107,204,52,76,11,122,11,204,152,71,3,126,50,235,56,198,24,216,208,137,111,15,162,90,60,57,117,104,57,81,143,206,171,
191,34,236,116,38,196,68,39,5,174,145,160,216,253,178,35,192,113,173,128,1,10,2,44,199,74,51,81,10,179,96,160,84,29,232,237,234,37,83,211,151,196,111,47,158,153,215,16,142,139,185,182,67,191,161,124,67,
49,185,18,247,73,211,58,181,104,150,89,27,225,55,114,233,92,207,20,55,98,38,70,42,47,50,6,167,250,175,6,227,127,88,72,95,55,237,187,110,29,141,42,10,99,51,103,143,30,56,143,153,177,228,195,38,97,215,1,
71,57,45,56,103,85,36,159,32,233,0,27,115,44,56,22,220,104,129,123,68,228,213,56,34,169,60,133,14,0,167,46,175,192,237,232,124,83,217,247,253,119,16,117,46,213,172,116,137,230,227,160,23,234,186,46,240,
37,230,109,69,117,93,11,120,18,127,34,186,64,218,29,192,156,146,172,175,239,25,150,197,210,131,31,66,113,248,79,188,60,95,68,148,74,145,65,165,168,80,204,217,1,110,228,20,4,198,52,161,241,92,19,93,175,
19,163,130,148,36,248,138,42,209,58,40,53,55,8,112,70,96,198,164,29,180,126,6,124,55,254,136,170,131,79,209,221,3,8,12,61,201,42,245,219,167,141,180,164,87,209,215,211,229,139,44,234,28,0,78,166,249,6,
154,6,173,85,157,104,197,100,128,128,211,118,27,123,122,245,250,192,207,81,47,4,151,231,1,129,196,186,174,58,233,31,189,156,98,24,224,14,123,172,182,86,191,48,11,237,58,105,158,255,133,69,144,119,219,
170,64,4,203,56,166,85,27,6,169,50,120,67,77,9,58,96,99,247,172,69,232,170,32,19,144,68,106,83,159,109,160,112,178,174,8,31,169,60,103,100,33,82,190,21,1,41,62,206,147,132,141,192,226,8,43,238,206,52,
67,212,38,100,141,40,176,65,129,123,81,162,15,229,50,59,185,106,130,124,155,254,112,98,180,192,141,203,77,3,215,195,254,228,227,53,145,63,186,204,57,98,93,202,74,38,22,28,11,238,141,218,142,112,214,85,
191,179,154,211,239,70,176,175,3,75,28,107,44,113,44,113,44,113,195,108,141,168,91,124,9,106,79,41,194,142,36,15,251,90,113,68,136,211,118,42,208,93,213,166,223,134,226,244,18,1,196,7,63,50,16,238,126,
142,91,250,82,61,93,80,158,186,143,86,168,17,146,71,136,19,186,4,113,90,180,126,119,18,21,153,183,153,45,55,247,172,197,184,45,17,22,53,42,212,205,221,137,122,249,100,76,60,23,141,230,180,63,232,109,51,
163,5,231,174,70,88,230,36,211,174,150,234,218,117,60,78,57,11,165,201,67,12,73,110,28,180,174,244,170,82,61,207,240,216,253,0,90,44,6,17,41,95,133,128,20,127,43,63,197,177,11,120,248,89,17,125,221,240,
245,97,180,94,72,67,108,225,108,122,47,91,103,122,82,30,160,118,111,24,166,16,129,174,7,170,193,139,197,251,81,187,37,31,92,159,12,72,54,132,160,101,87,62,42,179,43,32,72,94,128,248,194,100,184,25,252,
26,86,30,166,119,231,92,134,184,246,188,171,22,164,197,96,58,181,28,2,59,126,162,165,169,136,139,246,70,153,236,34,93,238,190,246,16,221,157,179,109,78,38,194,17,121,122,158,5,72,119,120,38,138,129,83,
77,208,52,170,233,200,236,186,82,73,183,72,246,200,12,164,233,253,130,126,90,132,246,35,7,172,34,213,169,137,243,205,88,134,196,12,102,190,202,127,43,77,215,220,9,33,118,242,144,0,110,94,3,68,184,241,
126,190,155,117,125,103,23,122,222,188,89,85,129,103,115,119,227,185,156,50,69,214,228,155,169,67,0,227,1,191,117,50,146,11,139,81,153,144,135,136,75,43,16,48,223,7,202,130,27,168,158,127,209,34,231,185,
56,113,148,242,57,158,38,239,71,99,137,249,0,66,188,99,53,164,217,147,108,206,232,248,16,125,254,1,164,31,9,192,179,137,66,207,5,9,144,82,17,228,183,152,46,11,151,166,35,113,233,2,180,236,147,67,241,251,
77,168,254,238,129,155,116,18,162,58,178,208,121,234,14,52,148,24,124,161,139,18,215,85,112,5,119,231,23,88,213,133,229,103,32,100,85,72,63,119,112,33,74,159,1,145,157,22,126,108,12,130,98,109,107,189,
224,159,145,10,127,155,90,191,149,179,92,51,226,218,246,29,71,249,102,75,73,50,29,83,27,151,192,75,204,101,87,14,246,76,83,94,130,178,232,211,230,57,46,126,14,166,150,188,55,98,39,61,46,73,92,215,249,
11,184,155,86,100,42,123,175,95,2,233,58,127,168,10,159,16,241,224,102,247,30,14,191,23,148,218,13,130,132,112,146,143,184,111,39,113,221,165,53,86,229,142,67,39,112,239,16,179,123,29,117,156,231,18,196,
233,254,222,118,168,255,214,196,238,142,176,219,74,172,177,196,177,196,177,196,177,196,189,181,196,233,78,166,89,42,152,219,76,72,11,254,7,215,46,34,169,125,124,169,207,0,0,0,0,73,69,78,68,174,66,96,130,
0,0};

const char* LowhighpassAudioProcessorEditor::_2nd_png = (const char*) resource_LowhighpassAudioProcessorEditor__2nd_png;
const int LowhighpassAudioProcessorEditor::_2nd_pngSize = 1749;

// JUCER_RESOURCE: hc_off_png, 787, "hc_off.png"
static const unsigned char resource_LowhighpassAudioProcessorEditor_hc_off_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,78,0,0,0,54,8,6,0,0,1,30,220,159,39,0,0,0,9,112,72,89,115,0,0,14,
197,0,0,14,198,1,108,65,191,60,0,0,2,197,73,68,65,84,120,218,237,154,203,142,105,81,16,134,75,104,38,110,79,224,22,60,1,26,83,19,183,48,23,116,8,51,158,166,157,153,16,183,153,145,68,68,206,3,184,62,129,
152,244,27,244,148,129,221,167,118,162,179,219,209,199,234,182,246,177,244,174,74,24,173,148,127,125,123,213,101,213,102,0,0,9,24,204,128,95,245,122,157,109,33,179,71,46,11,159,159,159,101,105,23,23,30,
245,127,233,167,217,240,176,160,225,183,235,227,142,47,46,84,202,186,248,211,205,102,19,42,149,202,229,133,184,232,29,15,235,206,185,210,81,221,225,104,52,130,76,38,195,207,33,58,27,143,199,144,74,165,
248,109,25,157,77,38,19,72,36,18,252,24,42,157,169,246,80,36,110,206,132,61,127,63,208,153,50,239,92,237,204,237,118,243,83,150,78,167,255,82,119,21,51,143,199,195,55,78,153,115,54,139,249,124,62,126,
71,35,30,143,223,201,161,229,90,108,132,221,41,137,19,90,28,6,178,154,150,205,102,193,225,112,124,79,156,218,1,210,104,52,224,241,241,17,130,193,160,120,143,181,86,171,201,2,37,73,130,80,40,36,222,153,
83,10,68,138,194,5,132,82,96,56,28,22,47,90,143,2,15,135,3,68,163,81,241,82,9,10,92,175,215,226,230,185,64,32,112,127,73,88,18,86,156,168,29,73,44,22,251,69,133,159,196,145,56,18,167,134,184,151,151,23,
121,72,166,166,125,123,168,234,116,58,85,237,132,177,3,193,34,127,174,150,222,252,177,226,221,97,54,155,137,41,14,199,160,72,111,185,92,254,179,69,191,89,64,184,92,46,88,44,22,98,138,195,41,23,210,67,
129,159,221,31,110,154,74,112,166,135,143,86,72,113,56,133,67,122,243,249,252,236,229,230,230,73,24,103,142,171,213,74,76,113,201,100,82,166,135,169,37,18,137,136,87,190,188,94,175,156,148,133,20,135,
51,214,237,118,123,54,114,133,40,252,213,106,21,76,38,147,152,93,201,57,97,212,207,145,184,155,141,35,68,189,245,139,106,194,79,35,40,28,8,28,25,129,35,112,4,142,192,17,56,133,109,54,27,152,78,167,119,
185,73,214,255,18,208,137,59,49,28,109,227,173,179,92,46,131,94,175,255,191,224,252,126,191,252,185,55,123,125,125,133,193,96,0,187,221,14,90,173,22,55,120,63,254,196,217,237,118,200,231,243,208,239,247,
223,225,149,74,37,48,24,12,4,238,146,217,108,182,15,240,218,237,246,213,240,52,83,85,79,225,29,195,246,187,240,52,213,142,32,188,66,161,0,189,94,15,246,251,253,85,240,52,215,199,89,173,86,46,240,52,217,
0,43,225,41,11,198,195,195,3,129,99,129,87,44,22,161,219,237,126,200,121,172,240,52,125,229,178,88,44,239,240,148,97,203,2,79,243,119,85,132,247,244,244,4,157,78,231,75,240,232,146,255,199,204,102,179,
252,150,114,56,28,130,78,167,147,223,148,159,254,5,152,192,125,98,70,163,17,114,185,28,21,7,181,141,192,17,56,2,119,63,224,240,205,52,161,96,55,191,223,255,251,13,168,80,12,127,21,214,123,194,0,0,0,0,
73,69,78,68,174,66,96,130,0,0};

const char* LowhighpassAudioProcessorEditor::hc_off_png = (const char*) resource_LowhighpassAudioProcessorEditor_hc_off_png;
const int LowhighpassAudioProcessorEditor::hc_off_pngSize = 787;

// JUCER_RESOURCE: _4th_png, 1568, "4th.png"
static const unsigned char resource_LowhighpassAudioProcessorEditor__4th_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,78,0,0,0,54,8,6,0,0,1,30,220,159,39,0,0,0,9,112,72,89,115,0,0,14,
197,0,0,14,198,1,108,65,191,60,0,0,5,210,73,68,65,84,120,218,237,90,123,76,83,87,24,255,245,129,181,84,140,29,161,10,5,215,1,9,176,140,45,50,37,48,94,169,76,35,232,31,168,36,146,204,213,45,89,179,76,150,
101,46,75,102,55,7,129,45,16,18,113,168,211,57,112,49,129,153,232,6,142,100,62,216,64,92,193,133,225,108,178,224,166,33,131,233,162,160,48,25,147,215,180,208,222,157,123,123,11,229,217,199,40,92,240,124,
105,238,185,231,220,239,158,243,187,191,243,157,239,124,231,156,74,1,17,3,144,159,19,145,178,74,133,121,46,41,142,23,67,110,38,180,186,131,88,231,111,134,114,197,83,83,43,22,231,138,200,53,25,242,37,190,
68,73,61,125,141,247,185,107,3,146,66,148,51,55,61,19,86,41,92,20,86,145,113,73,209,21,106,38,55,45,234,37,245,43,9,69,41,4,175,113,6,69,70,233,188,70,3,199,33,160,223,203,76,170,109,156,162,51,172,238,
209,227,234,151,187,207,142,183,43,52,240,70,199,146,117,233,86,55,190,63,177,210,67,110,196,173,248,236,219,118,238,101,182,47,89,209,106,84,208,122,50,94,56,177,70,224,141,205,17,222,225,112,42,219,
152,243,78,153,53,155,145,10,214,254,164,115,106,200,44,13,183,154,54,67,243,194,151,132,221,101,164,212,199,253,202,186,90,116,163,247,127,182,93,32,149,129,171,200,222,128,91,149,149,84,85,32,106,54,
62,211,222,122,57,239,224,82,94,182,142,26,144,71,110,222,238,41,103,250,44,151,42,179,191,204,34,211,185,97,135,51,246,166,206,77,131,94,160,70,235,145,215,152,205,193,46,216,47,165,224,60,22,166,27,
15,25,21,150,74,122,97,200,201,112,123,170,155,53,112,71,137,67,184,205,221,217,194,11,136,134,136,183,89,9,73,232,118,100,191,84,198,133,176,239,151,164,35,38,176,21,166,235,127,192,19,187,118,31,156,
184,3,134,15,131,225,243,204,41,68,253,154,133,27,163,172,249,98,75,184,8,230,164,163,8,244,177,129,46,120,251,60,247,200,196,187,74,175,130,99,231,173,207,107,206,227,173,92,6,129,98,226,78,9,56,71,89,
29,158,132,163,36,56,123,184,235,119,146,27,152,59,155,179,79,20,57,239,221,193,240,96,7,250,200,92,104,107,254,14,6,251,59,32,247,83,35,36,222,136,194,120,155,126,90,158,201,165,5,200,172,128,115,108,
64,206,167,203,184,107,4,20,126,106,225,141,214,53,105,5,208,136,163,132,233,74,162,227,12,212,9,11,22,28,35,88,112,66,141,72,226,18,113,132,218,28,5,71,193,121,11,92,247,95,173,80,5,68,192,88,38,70,200,
78,43,66,229,243,4,174,185,42,28,213,45,237,200,222,199,32,152,95,245,127,242,105,36,23,32,72,151,2,18,82,243,96,255,77,210,130,26,10,249,146,57,4,55,84,205,1,99,197,50,98,219,146,184,222,152,201,229,
43,207,228,34,144,164,199,62,22,65,187,62,7,151,234,243,177,53,123,0,177,42,197,220,128,51,20,109,157,84,246,116,50,9,205,235,170,144,185,45,15,63,86,124,4,173,174,11,27,195,84,136,242,189,138,218,107,
38,196,166,38,123,31,220,197,227,98,91,124,151,111,34,139,152,231,167,213,139,8,82,217,110,70,230,200,230,172,15,202,81,119,155,193,70,194,10,44,173,194,26,173,31,28,216,69,174,59,161,37,221,5,235,4,112,
252,246,57,27,206,111,10,22,193,194,23,91,44,247,240,96,120,216,187,224,206,29,226,187,51,175,194,165,80,222,46,154,68,19,246,120,147,185,127,239,29,192,229,30,6,241,89,215,208,71,22,51,172,72,204,157,
92,218,251,79,7,148,35,62,88,238,167,154,159,110,237,106,59,201,165,77,167,162,209,52,225,217,233,99,193,228,26,67,88,51,205,15,56,182,107,10,19,39,20,138,126,34,163,53,30,217,100,13,27,44,22,218,244,
101,177,25,185,229,145,195,90,81,48,224,68,161,216,146,86,140,0,185,64,92,201,120,112,106,36,196,189,67,67,38,10,142,130,163,224,32,240,115,8,161,138,224,119,35,232,112,160,196,81,161,196,81,226,40,113,
78,22,4,67,184,118,81,135,43,157,97,72,223,81,196,31,152,82,226,38,111,47,116,228,34,191,52,31,235,95,237,194,6,13,187,141,240,8,125,29,103,208,214,150,4,51,191,25,248,88,17,119,255,183,215,81,252,85,
169,67,73,50,244,123,141,99,251,198,162,94,24,75,253,81,115,199,22,91,214,159,88,137,122,86,199,80,205,43,40,208,123,251,56,140,39,245,99,71,247,68,214,174,175,195,214,148,84,136,23,29,113,162,110,24,
203,87,161,166,109,114,176,45,113,204,48,74,164,232,31,65,90,33,195,89,162,155,254,218,0,146,66,20,28,161,119,57,133,11,248,186,113,29,222,205,103,160,99,108,67,184,185,242,89,84,215,191,136,97,153,9,
89,113,49,139,136,184,254,211,40,222,159,197,255,81,26,208,102,150,161,167,82,143,150,105,73,30,251,63,197,136,101,144,179,178,49,137,193,43,89,121,80,218,249,103,124,17,20,20,1,180,180,99,104,112,96,
241,12,213,206,43,153,56,124,174,138,207,109,71,246,190,74,4,75,26,241,69,165,167,53,46,195,18,49,230,93,188,71,156,248,38,190,43,9,195,15,61,54,211,80,173,249,6,123,50,50,108,207,172,206,124,187,12,50,
223,80,146,182,163,171,243,42,122,150,71,65,161,92,177,248,195,17,243,223,71,144,123,240,205,209,124,26,153,25,147,53,110,108,176,147,225,183,118,155,17,178,85,135,112,119,196,136,230,159,155,17,25,187,
7,171,215,30,70,90,184,31,150,203,198,171,63,17,166,199,150,13,169,36,141,92,184,196,181,95,78,193,241,218,6,62,167,39,78,188,20,254,140,7,40,68,106,68,39,20,33,218,177,76,153,141,144,41,84,21,170,12,
36,168,22,176,197,221,106,36,164,213,53,56,148,148,97,127,78,153,147,183,26,184,195,88,187,227,215,239,53,253,239,35,237,5,71,92,64,248,110,164,137,55,65,34,145,205,172,56,252,11,206,214,141,29,232,61,
151,84,128,16,63,192,98,86,67,41,123,12,151,92,138,192,29,72,14,116,65,209,218,136,118,66,220,13,62,0,142,79,48,224,73,57,22,148,204,207,146,203,98,118,248,231,235,192,232,25,54,37,206,153,248,164,98,
247,2,223,174,167,219,74,148,56,74,28,37,142,18,71,133,18,55,171,196,177,39,211,148,10,215,69,163,65,237,127,70,87,210,87,174,48,145,106,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* LowhighpassAudioProcessorEditor::_4th_png = (const char*) resource_LowhighpassAudioProcessorEditor__4th_png;
const int LowhighpassAudioProcessorEditor::_4th_pngSize = 1568;

// JUCER_RESOURCE: hc_over_png, 832, "hc_over.png"
static const unsigned char resource_LowhighpassAudioProcessorEditor_hc_over_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,78,0,0,0,54,8,6,0,0,1,30,220,159,39,0,0,0,9,112,72,89,115,0,0,
14,197,0,0,14,198,1,108,65,191,60,0,0,2,242,73,68,65,84,120,218,237,154,73,78,106,65,20,134,143,98,8,77,2,3,194,144,38,52,10,196,41,27,32,24,186,16,54,32,235,48,68,164,111,4,198,238,128,49,129,72,104,
2,219,32,116,174,64,193,196,129,56,82,159,85,201,53,152,167,185,23,95,93,95,33,231,36,206,78,234,254,245,213,233,170,228,96,111,111,239,245,205,64,204,14,136,147,36,71,144,104,236,28,21,10,5,60,63,63,
139,59,18,167,141,63,253,42,201,81,10,26,118,187,22,118,44,234,40,56,73,250,180,94,175,135,135,135,7,113,71,226,244,142,71,234,206,153,210,145,125,65,191,223,15,195,225,144,221,130,100,177,112,56,12,157,
78,135,221,150,201,98,177,88,12,90,173,22,59,134,235,139,201,118,40,204,98,230,128,219,248,251,133,139,173,215,29,38,41,198,76,89,191,223,135,253,253,125,120,121,121,97,195,140,228,42,179,3,104,183,219,
210,107,182,20,35,73,207,44,52,26,141,198,150,4,45,211,102,195,237,78,81,28,215,226,222,70,102,89,63,218,237,118,33,24,12,126,79,156,220,9,66,170,88,50,153,132,108,54,203,223,177,146,242,42,148,217,66,
161,192,95,204,9,2,73,151,42,149,74,252,37,196,186,192,203,203,75,254,178,117,253,136,107,181,26,127,165,132,8,251,44,57,184,169,115,233,116,122,251,138,240,43,183,226,120,157,72,222,236,10,27,63,138,
67,113,40,78,14,113,228,233,41,18,137,200,250,209,111,63,170,146,203,187,156,133,153,76,32,185,92,14,82,169,20,127,199,234,243,249,232,4,194,165,184,193,96,64,233,93,92,92,64,62,159,231,47,33,78,78,78,
232,104,206,165,184,94,175,71,233,157,159,159,67,177,88,228,175,148,132,66,33,122,111,224,82,156,240,10,151,72,36,160,92,46,243,87,132,73,217,170,86,171,124,138,187,190,190,166,244,206,206,206,160,82,
169,240,215,190,162,209,40,189,18,114,41,174,217,108,126,153,185,92,52,254,251,251,123,250,15,64,46,167,146,207,132,225,60,135,226,254,219,115,4,199,183,126,124,141,248,109,134,224,16,28,130,67,112,8,
14,13,193,253,56,184,122,189,14,241,120,124,43,55,73,30,140,2,129,0,70,220,166,70,158,182,53,26,13,44,22,11,80,42,149,63,11,238,244,244,148,254,109,155,205,231,115,56,62,62,134,199,199,71,48,24,12,176,
92,46,153,192,251,245,17,231,116,58,97,52,26,129,219,237,134,213,106,69,225,221,221,221,129,74,165,66,112,98,230,112,56,96,50,153,128,203,229,162,240,140,70,227,63,195,219,153,174,106,183,219,41,60,143,
199,195,4,222,78,141,35,4,222,120,60,126,79,91,2,239,246,246,22,212,106,53,130,19,51,155,205,6,211,233,148,214,190,245,200,219,20,222,78,14,192,86,171,149,118,219,163,163,163,15,145,71,70,22,4,39,1,222,
108,54,131,195,195,195,15,145,39,21,222,78,95,185,44,22,11,141,60,146,182,79,79,79,27,193,219,249,187,170,217,108,134,155,155,27,58,178,8,240,72,218,106,181,90,4,39,102,38,147,137,70,154,215,235,165,191,
193,207,100,50,127,253,4,24,193,125,97,58,157,142,118,91,108,14,50,27,130,67,112,8,110,171,192,93,33,138,141,108,240,7,97,56,54,247,8,200,251,204,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* LowhighpassAudioProcessorEditor::hc_over_png = (const char*) resource_LowhighpassAudioProcessorEditor_hc_over_png;
const int LowhighpassAudioProcessorEditor::hc_over_pngSize = 832;

// JUCER_RESOURCE: hc_on_png, 868, "hc_on.png"
static const unsigned char resource_LowhighpassAudioProcessorEditor_hc_on_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,78,0,0,0,54,8,6,0,0,1,30,220,159,39,0,0,0,9,112,72,89,115,0,0,14,
197,0,0,14,198,1,108,65,191,60,0,0,3,22,73,68,65,84,120,218,237,154,91,79,19,81,16,199,255,123,40,186,237,82,171,49,36,26,229,14,138,198,7,95,252,2,202,69,139,134,240,168,209,240,49,12,177,20,202,29,125,
230,27,152,248,98,80,4,185,127,13,163,85,188,189,162,162,164,46,172,169,116,221,101,91,108,162,100,183,112,86,167,116,38,233,219,228,236,255,252,206,204,156,153,147,6,20,5,166,105,194,213,2,182,147,105,
134,221,29,225,209,228,57,150,149,165,176,181,21,118,119,180,157,10,254,180,233,201,209,11,26,121,187,206,237,216,213,49,231,228,233,211,145,72,10,235,235,30,56,218,78,59,120,188,238,92,42,29,223,23,108,
105,211,177,188,168,201,91,208,94,44,218,161,99,246,185,38,111,203,246,98,157,93,58,166,158,104,242,24,230,47,230,219,161,152,210,22,35,27,127,7,112,177,252,186,179,255,20,107,21,242,148,45,204,107,16,
34,133,76,38,44,135,89,244,186,144,119,0,51,207,52,239,53,219,139,117,118,9,121,161,49,249,88,43,146,160,149,122,217,144,221,41,139,35,45,78,81,82,190,126,116,110,41,136,171,45,129,189,137,243,59,65,236,
42,22,235,43,71,34,174,210,59,86,187,188,58,101,22,24,74,168,244,98,46,39,48,147,1,70,6,85,122,9,177,35,208,34,56,54,162,210,203,214,252,35,126,48,174,210,43,37,182,192,196,160,65,183,206,245,245,170,
116,197,249,62,46,72,23,71,181,35,1,186,39,248,226,103,113,44,142,197,249,33,110,118,241,39,58,218,55,125,253,232,158,31,85,163,109,1,95,59,97,187,3,25,24,50,16,143,169,244,142,245,114,139,64,162,47,77,
83,220,210,130,243,18,210,219,111,96,176,159,224,12,209,218,38,172,214,60,77,83,220,252,156,67,239,94,220,192,240,0,193,1,231,90,84,96,108,56,77,83,220,204,180,67,175,39,102,96,116,136,224,244,101,191,
57,222,31,77,211,20,55,61,229,208,187,219,99,96,124,148,224,12,113,163,83,88,35,97,154,166,184,167,147,187,103,46,137,139,127,45,85,129,72,72,161,217,149,252,77,24,247,115,44,238,191,61,71,208,157,250,
169,26,241,215,8,78,7,6,199,198,224,24,28,131,99,112,12,46,207,30,62,74,227,206,45,163,40,55,57,191,28,68,251,149,0,71,92,161,102,63,109,135,66,192,231,245,48,14,41,255,24,220,237,155,229,219,191,98,179,
55,239,51,184,208,172,67,215,129,227,145,20,190,72,130,119,224,35,174,169,78,224,69,82,195,185,38,29,27,89,120,159,190,134,161,150,49,56,87,107,172,21,120,245,86,67,115,131,3,175,242,216,254,225,149,204,
173,218,80,237,192,59,159,141,188,202,163,22,188,111,123,135,87,82,237,136,13,239,229,138,149,182,141,22,188,13,7,222,170,21,121,193,0,131,115,181,250,42,129,164,21,121,77,117,89,120,217,180,45,20,94,
73,54,192,181,167,133,117,219,106,56,155,87,243,86,215,42,16,42,87,24,156,23,120,175,223,105,56,83,159,171,121,223,173,154,231,29,94,73,143,92,53,167,172,200,251,96,165,109,173,142,205,205,194,224,149,
252,172,90,125,82,96,229,163,134,198,154,223,240,236,180,213,14,43,12,206,205,170,78,136,237,72,187,116,81,135,34,128,254,248,143,63,254,2,204,224,118,177,35,65,5,201,100,5,95,14,126,27,131,99,112,12,
174,152,192,117,79,48,138,66,172,117,233,23,144,85,18,8,0,181,223,225,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* LowhighpassAudioProcessorEditor::hc_on_png = (const char*) resource_LowhighpassAudioProcessorEditor_hc_on_png;
const int LowhighpassAudioProcessorEditor::hc_on_pngSize = 868;

// JUCER_RESOURCE: lc_over_png, 760, "lc_over.png"
static const unsigned char resource_LowhighpassAudioProcessorEditor_lc_over_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,78,0,0,0,54,8,6,0,0,1,30,220,159,39,0,0,0,9,112,72,89,115,0,0,
14,197,0,0,14,198,1,108,65,191,60,0,0,2,170,73,68,65,84,120,218,237,155,57,110,34,65,20,134,31,166,9,185,1,171,217,36,34,18,78,0,136,205,1,136,85,112,15,16,28,0,137,3,248,14,128,216,49,78,38,230,0,36,
32,64,44,98,187,0,33,72,182,171,2,75,179,64,117,15,13,93,204,188,95,178,58,121,122,254,250,239,87,123,33,168,84,170,143,47,1,75,2,9,18,21,8,34,37,61,208,225,112,192,100,50,97,7,94,10,146,252,175,63,68,
5,138,177,230,239,222,218,98,177,192,124,62,103,7,94,10,250,14,116,58,157,48,26,141,216,129,172,160,111,123,196,190,185,172,238,92,149,80,163,209,192,241,120,148,39,161,32,8,112,58,157,228,33,188,54,217,
111,9,175,77,118,179,143,34,91,205,8,220,214,223,131,37,51,155,205,176,92,46,149,109,17,194,159,168,184,104,90,63,37,179,90,173,242,124,0,173,86,11,135,195,65,158,100,215,38,226,191,104,101,29,108,184,
125,83,132,123,56,184,217,108,6,118,187,253,166,48,191,214,190,40,184,94,175,7,209,104,20,238,221,112,152,112,237,118,27,226,241,184,44,51,20,89,225,154,205,38,36,147,73,69,192,46,194,213,235,117,72,167,
211,138,129,157,133,171,86,171,144,205,102,21,5,59,11,183,90,173,20,7,59,11,151,207,231,177,19,190,235,114,65,118,56,94,103,36,95,122,197,129,31,225,16,14,225,238,9,167,82,169,110,10,82,46,151,33,151,
203,73,135,235,247,251,160,86,171,239,62,25,16,5,23,137,68,192,96,48,240,247,89,59,157,14,93,59,44,22,11,254,224,98,177,24,24,141,70,254,26,68,171,213,162,79,214,150,188,34,112,137,68,2,158,159,159,249,
235,74,26,141,6,125,78,167,83,254,224,82,169,20,61,138,225,174,19,174,213,106,244,201,58,244,82,4,46,147,201,128,205,102,227,111,248,170,84,42,244,57,30,143,249,131,35,171,124,183,219,205,239,192,175,
116,67,192,249,28,194,41,186,29,193,241,170,31,119,35,254,53,161,113,104,28,26,135,198,161,113,40,52,142,107,227,134,195,33,184,92,174,135,52,164,80,40,64,169,84,186,191,113,100,51,106,189,94,99,197,73,
17,185,24,71,76,123,122,122,162,151,60,94,94,94,208,56,41,166,145,11,40,225,112,24,43,142,37,147,201,4,155,205,134,154,70,142,3,66,161,16,54,85,150,200,177,196,118,187,165,166,117,187,93,8,6,131,56,170,
74,49,141,244,105,129,64,0,167,35,44,145,99,195,221,110,71,77,123,123,123,3,191,223,143,243,56,150,244,122,61,236,247,123,122,43,156,152,230,243,249,112,2,44,214,52,82,105,228,52,221,235,245,226,202,65,
74,165,253,207,166,73,50,78,167,211,81,211,200,47,181,222,223,223,193,227,241,224,90,149,37,178,118,35,131,1,185,224,50,24,12,184,57,164,227,222,184,98,177,72,255,80,55,222,29,65,227,80,104,28,26,199,
153,113,175,104,133,36,253,248,4,143,75,247,193,56,30,92,64,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* LowhighpassAudioProcessorEditor::lc_over_png = (const char*) resource_LowhighpassAudioProcessorEditor_lc_over_png;
const int LowhighpassAudioProcessorEditor::lc_over_pngSize = 760;

// JUCER_RESOURCE: lc_on_png, 799, "lc_on.png"
static const unsigned char resource_LowhighpassAudioProcessorEditor_lc_on_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,78,0,0,0,54,8,6,0,0,1,30,220,159,39,0,0,0,9,112,72,89,115,0,0,14,
197,0,0,14,198,1,108,65,191,60,0,0,2,209,73,68,65,84,120,218,237,154,75,111,18,81,20,199,255,83,6,17,202,124,3,25,16,250,72,212,133,27,215,70,91,180,245,89,45,125,68,23,253,16,26,173,198,157,73,163,137,
219,126,7,53,125,64,41,45,209,181,27,119,46,90,165,10,168,125,124,129,91,157,24,166,142,76,71,155,104,133,153,129,129,185,100,206,73,8,155,147,195,239,254,230,158,185,119,46,35,10,2,52,77,131,105,136,
122,146,166,73,230,137,176,24,246,19,251,251,119,81,40,132,205,19,235,37,217,254,105,205,82,162,21,53,141,141,58,145,96,40,22,37,243,196,122,73,7,137,39,78,238,98,125,205,130,30,179,164,3,61,86,71,238,
168,157,166,10,250,253,12,149,138,228,76,65,81,100,80,85,201,25,194,102,139,29,42,216,108,177,150,93,20,205,177,98,220,206,191,14,43,118,60,206,80,46,73,238,118,132,120,152,74,224,163,181,254,42,214,211,
43,56,115,1,36,137,129,49,135,26,190,217,66,252,79,90,71,23,27,110,71,74,112,29,7,247,177,252,19,125,241,111,45,133,249,119,238,91,130,203,174,168,184,113,77,65,187,27,199,20,46,189,172,34,53,162,56,178,
67,113,20,110,33,163,98,124,212,29,176,186,112,115,139,42,38,199,220,3,171,9,247,98,94,197,237,9,119,193,106,194,125,46,239,185,14,86,19,238,254,221,0,221,132,219,250,184,224,56,28,175,59,18,96,106,150,
22,126,130,35,56,130,107,39,156,32,176,150,130,60,125,118,4,247,238,4,236,195,229,242,42,124,62,180,125,51,96,9,110,228,170,2,57,202,225,101,205,228,84,232,71,255,165,162,196,31,220,232,117,5,209,152,
192,95,67,44,46,169,251,223,197,79,97,254,224,198,110,42,136,199,5,254,110,37,243,105,195,218,198,70,152,63,184,137,148,130,68,143,123,214,106,194,189,92,48,172,21,62,132,249,131,187,53,174,160,183,207,
93,107,255,133,123,62,103,88,123,191,30,230,15,110,50,37,226,76,169,155,223,133,63,33,119,209,150,137,224,188,123,28,193,239,83,63,175,193,249,105,4,181,3,137,163,32,113,36,142,196,145,56,18,215,96,188,
91,219,195,233,83,223,59,82,200,131,71,126,204,60,62,218,126,113,241,4,195,215,47,52,227,108,133,254,186,158,46,173,171,75,127,201,35,136,43,195,34,137,179,35,45,157,13,226,242,144,72,51,206,44,98,49,
134,205,77,67,90,38,23,196,165,11,34,181,170,89,68,163,12,91,91,134,180,165,106,123,14,39,189,177,80,139,78,73,203,174,6,49,52,232,157,221,77,195,35,149,171,210,182,127,75,91,206,7,113,113,192,91,91,194,
134,70,27,145,25,118,182,245,183,194,171,210,86,67,72,158,247,209,6,216,84,90,164,42,109,199,152,105,185,124,8,131,231,188,39,205,182,184,63,210,244,153,230,101,105,182,196,29,139,24,237,233,247,3,43,
175,66,24,56,235,93,105,150,197,205,60,249,1,89,22,160,86,52,188,121,219,205,205,159,116,220,139,123,56,29,216,255,80,180,248,116,132,196,81,144,56,18,199,151,184,169,89,82,97,39,146,175,127,1,130,241,
221,168,224,9,211,144,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* LowhighpassAudioProcessorEditor::lc_on_png = (const char*) resource_LowhighpassAudioProcessorEditor_lc_on_png;
const int LowhighpassAudioProcessorEditor::lc_on_pngSize = 799;


// JUCER_RESOURCE: analyzer_off_png, 5552, "analyzer_off.png"
static const unsigned char resource_LowhighpassAudioProcessorEditor_analyzer_off_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,184,0,0,0,64,8,6,0,0,1,222,245,141,37,0,0,0,9,112,72,89,115,
    0,0,17,137,0,0,17,136,1,252,205,31,202,0,0,21,98,73,68,65,84,120,218,237,93,249,91,20,199,214,126,89,6,68,80,100,149,69,16,148,32,32,200,34,139,8,10,168,40,24,80,130,24,141,154,69,205,189,70,147,185,127,
    193,247,91,254,131,16,189,154,39,70,111,162,81,35,16,16,23,68,4,21,92,16,69,5,151,113,71,16,65,20,16,21,148,101,134,175,170,103,186,233,158,233,25,102,212,81,48,253,62,207,76,111,213,167,79,157,62,117,
    234,212,118,218,250,225,195,135,177,183,110,221,90,8,51,192,154,18,254,251,239,191,127,52,11,113,83,18,191,124,124,15,14,19,167,188,57,241,151,77,39,225,224,147,132,220,220,92,230,88,46,151,51,251,105,
    203,214,194,225,241,85,236,248,235,16,119,142,5,123,76,183,157,228,216,73,140,56,155,224,37,239,38,22,37,249,59,144,147,56,153,59,199,191,70,239,243,244,244,68,241,249,199,240,158,228,2,39,47,107,93,226,
    222,51,115,152,237,14,30,87,124,206,180,153,248,179,248,42,86,101,134,50,231,114,114,114,208,54,0,60,108,123,205,145,21,16,207,158,237,169,195,21,68,114,225,22,182,148,217,178,132,217,107,238,132,154,
    187,215,152,55,123,161,44,86,38,251,26,247,66,203,203,203,255,15,102,130,181,185,116,220,100,177,116,171,0,123,203,55,36,78,181,192,53,40,3,95,164,250,227,202,233,98,188,118,154,133,184,16,55,78,59,94,
    12,88,192,222,102,144,73,123,144,156,187,207,211,164,228,140,85,176,111,58,128,67,87,94,114,47,216,90,84,229,82,229,8,79,200,100,246,227,66,132,154,163,93,184,40,168,142,159,56,248,167,166,28,172,213,
    229,252,90,105,46,166,47,148,235,168,161,154,235,111,120,12,124,71,244,89,134,99,154,220,80,4,196,166,161,165,104,135,126,177,80,194,134,116,155,194,131,136,164,36,119,43,210,200,249,84,222,181,8,95,7,
    68,136,148,13,147,245,60,77,132,136,65,61,55,167,58,142,8,53,31,53,140,63,186,148,135,252,170,22,81,27,57,28,90,46,29,128,197,244,165,140,126,189,103,198,123,56,166,115,121,165,69,108,95,223,117,54,3,
    39,95,70,96,229,28,95,188,104,56,137,67,77,1,100,223,91,125,81,245,20,185,155,247,112,52,54,146,173,181,134,6,221,127,66,238,205,171,122,32,42,56,189,140,231,230,110,135,179,179,51,118,239,222,205,108,
    181,25,50,22,158,145,75,240,132,218,159,57,114,236,44,174,35,52,146,184,12,174,147,11,77,141,181,136,85,72,88,246,47,227,37,94,87,87,167,195,100,123,67,61,26,218,251,145,148,148,52,68,84,179,47,151,127,
    143,188,162,34,36,36,46,98,238,59,126,252,56,98,166,7,194,210,122,80,199,235,160,168,37,244,41,157,187,100,75,49,99,198,12,230,218,141,218,179,184,120,250,62,150,110,88,137,113,54,150,24,231,21,12,155,
    113,54,198,51,78,9,105,195,197,47,12,46,204,206,208,181,40,46,157,37,114,150,46,229,206,207,159,63,127,40,195,143,31,98,208,214,69,32,136,40,17,250,20,193,81,241,204,143,133,195,196,79,224,240,161,172,
    138,203,196,73,230,41,156,230,244,87,204,106,14,71,99,229,243,241,86,64,255,72,198,111,220,184,129,224,224,224,15,195,184,218,113,245,35,166,44,211,100,194,101,101,101,31,134,113,229,243,27,176,114,159,
    11,101,219,41,238,220,211,107,133,24,12,206,194,222,205,185,130,26,174,73,113,30,133,199,170,117,125,97,101,51,114,183,156,35,231,150,113,77,132,232,13,255,193,254,109,63,241,158,20,64,174,167,163,255,
    197,35,108,221,153,47,160,193,122,252,113,89,255,70,172,143,173,113,140,111,249,95,25,67,224,124,222,41,92,110,237,71,132,135,12,253,125,150,200,219,172,174,1,187,238,149,99,203,190,43,216,180,34,156,
    97,154,121,152,5,241,61,126,226,185,6,86,212,39,121,196,209,164,237,154,12,226,116,241,25,91,75,152,166,160,76,139,249,59,27,200,214,230,77,116,60,54,71,93,93,179,45,5,214,119,112,156,146,0,212,40,152,
    253,229,153,201,130,230,58,31,65,228,119,239,249,32,166,140,239,99,212,142,5,109,230,88,77,140,99,106,198,22,226,133,82,228,229,229,233,220,111,99,138,170,236,167,68,53,14,214,112,160,30,223,254,226,62,
    157,215,203,34,85,227,167,56,241,213,136,120,133,52,203,242,207,99,57,246,252,102,175,68,230,76,183,183,211,241,86,74,116,245,106,238,120,240,229,45,242,240,124,210,170,148,233,182,161,95,60,131,108,146,
    159,40,211,124,116,242,11,189,198,149,229,123,144,13,244,222,153,226,174,177,113,140,171,94,34,33,33,85,112,202,194,33,144,120,115,175,49,110,162,29,207,91,179,68,98,196,68,120,76,11,71,112,99,33,241,
    14,155,153,135,157,60,122,20,205,207,149,2,47,114,205,60,31,92,80,37,115,199,244,90,157,198,51,28,24,32,78,87,148,218,59,164,111,216,201,195,143,99,154,79,99,120,198,45,29,8,161,32,189,30,227,144,183,
    38,195,140,105,30,234,7,124,154,53,196,212,162,69,234,238,43,158,7,184,171,188,137,48,51,193,160,247,73,177,154,247,150,13,165,123,47,53,103,227,195,198,55,106,128,124,240,42,223,119,146,239,251,173,128,
    70,188,175,50,90,251,85,36,231,80,130,36,112,73,224,18,222,70,224,253,196,131,219,202,236,165,172,220,132,80,55,171,247,194,112,139,166,19,116,57,105,69,153,171,23,120,68,10,156,21,182,63,249,85,236,221,
    2,127,226,67,217,75,10,108,30,129,151,104,218,56,172,150,209,182,202,111,34,237,21,86,27,89,76,11,137,66,127,231,125,220,107,81,183,132,210,191,252,1,1,19,44,152,253,11,121,185,56,219,162,73,55,35,10,
    182,189,79,80,119,179,137,57,14,158,255,37,22,132,76,48,208,121,2,166,151,93,248,194,85,228,218,102,174,191,160,144,164,107,210,155,35,47,174,15,226,81,253,97,228,159,184,171,225,35,22,174,182,175,113,
    186,70,221,220,177,11,74,199,183,169,1,130,188,57,187,0,29,237,208,219,15,241,214,2,191,119,122,47,110,147,237,39,201,171,185,34,45,223,148,141,220,45,5,36,131,197,162,189,70,161,68,96,41,156,192,18,48,
    216,223,140,159,183,22,160,244,104,29,2,86,132,51,103,163,115,228,136,214,186,47,105,161,90,160,55,142,255,65,4,46,222,10,161,163,162,180,180,105,191,240,66,141,176,215,105,206,101,201,197,135,106,227,
    150,124,139,216,201,118,204,126,111,91,13,35,108,118,12,153,69,212,172,36,52,86,231,163,232,252,17,156,15,28,74,207,116,189,120,101,64,190,202,223,60,26,222,223,118,9,135,106,159,48,251,183,79,236,38,
    63,237,20,13,248,245,216,29,78,11,88,56,78,24,35,108,116,203,92,245,106,42,211,218,247,245,133,147,131,3,198,216,219,26,193,149,12,63,172,95,132,159,183,31,37,52,170,136,208,19,209,169,40,101,180,153,
    246,253,104,155,185,167,228,218,158,99,55,117,123,219,8,58,154,91,52,105,14,34,87,33,254,180,251,205,207,4,2,15,12,244,52,151,73,233,194,214,125,85,162,140,242,59,165,20,138,35,184,60,125,35,34,188,140,
    39,77,139,39,197,156,229,223,49,61,124,92,55,14,41,9,181,53,151,134,189,223,98,108,32,150,37,214,33,191,234,18,46,183,4,161,146,8,84,230,157,130,40,175,49,195,106,53,31,174,222,62,228,255,1,211,237,38,
    151,167,106,245,214,244,2,150,182,239,207,134,231,230,254,206,108,55,24,232,96,160,157,108,10,146,169,202,252,255,34,192,132,142,8,207,200,116,160,106,43,42,247,147,31,95,119,137,208,104,97,189,175,17,
    86,194,178,141,208,167,79,94,145,57,8,170,34,207,206,219,195,28,127,151,29,202,187,218,75,238,255,133,59,170,62,240,43,170,117,76,147,28,50,247,72,252,176,206,5,63,255,86,68,210,235,170,184,173,223,2,
    252,59,51,248,253,8,220,216,158,28,126,58,135,200,37,144,71,138,165,178,213,162,39,51,64,63,84,75,178,44,77,93,151,80,161,151,87,91,163,249,183,176,247,53,42,173,167,222,188,125,228,13,31,161,247,65,71,
    173,87,73,45,77,115,34,203,12,125,168,146,192,165,166,189,132,55,22,248,104,157,159,50,26,17,24,24,88,106,61,111,222,188,31,229,163,204,14,142,86,16,215,246,71,201,164,72,54,92,18,184,4,73,224,146,192,
    37,72,2,151,4,254,78,112,245,234,13,88,88,217,97,122,176,223,63,75,224,245,135,115,161,25,137,194,251,244,227,43,42,202,64,39,134,254,227,4,206,10,155,226,81,159,5,188,70,225,8,250,168,17,56,93,31,77,
    17,20,19,15,69,205,89,20,236,61,143,31,190,138,145,164,105,46,129,239,40,84,143,98,39,205,138,102,4,62,216,117,14,42,196,192,82,183,41,11,102,214,253,198,120,228,254,247,15,222,149,161,81,242,33,244,226,
    224,238,95,112,191,131,199,156,71,40,54,46,79,209,203,7,219,23,254,217,250,255,96,210,88,173,18,166,108,65,238,150,60,198,252,200,55,69,107,246,197,177,98,163,156,89,17,206,226,124,233,95,168,190,249,
    152,59,118,241,8,194,170,229,169,34,121,35,231,211,108,241,103,201,21,163,77,171,233,2,87,106,230,50,144,140,208,57,215,75,99,93,81,116,254,41,142,215,63,67,106,216,4,145,27,238,16,97,223,161,108,35,42,
    202,29,181,181,55,168,17,210,154,6,254,130,28,239,228,94,70,76,140,55,106,106,106,48,208,122,149,156,191,170,55,35,11,179,99,176,189,160,6,133,121,186,37,76,113,92,45,224,200,140,121,228,255,153,193,44,
    13,168,232,84,141,65,158,32,213,72,74,73,193,205,138,10,180,182,42,152,33,55,118,101,38,239,41,68,216,102,214,240,11,127,171,51,146,178,60,141,217,250,198,45,37,42,177,29,138,19,127,16,129,139,11,38,104,
    238,74,164,134,171,23,10,36,36,44,224,50,245,138,252,232,80,238,131,211,251,153,99,26,143,128,13,27,48,107,214,44,245,160,52,217,103,151,143,104,99,172,247,44,242,95,195,148,48,64,40,112,205,192,60,18,
    253,233,184,189,189,206,75,59,65,104,215,147,45,93,137,193,214,63,247,78,169,23,156,187,6,47,196,23,11,166,49,251,51,66,67,209,118,237,16,246,149,223,195,111,197,55,116,198,52,249,105,205,34,112,118,178,
    78,40,39,128,177,220,64,239,195,30,11,221,162,77,192,10,155,5,29,27,167,166,128,157,28,55,57,97,29,228,9,186,207,10,77,156,12,69,213,3,168,84,74,208,113,79,49,36,79,85,87,224,85,119,186,145,24,160,158,
    20,49,248,170,145,217,90,56,206,18,189,135,78,149,168,215,236,111,226,86,144,128,137,73,192,152,24,45,1,186,79,255,20,40,207,69,111,3,245,144,132,2,55,69,216,38,11,188,243,246,81,245,91,13,202,16,156,
    79,95,147,130,45,187,42,80,148,119,6,223,127,21,63,44,29,63,162,196,77,141,208,177,225,39,75,143,112,179,173,140,69,216,167,95,224,196,79,123,112,233,72,17,18,53,227,153,87,42,138,152,109,86,78,44,116,
    6,155,85,79,185,121,41,235,244,152,170,130,34,82,226,250,45,134,78,200,100,31,166,210,220,85,114,75,163,33,226,19,101,84,93,23,72,229,25,15,75,19,153,232,186,123,10,191,31,190,194,29,79,11,9,129,173,181,
    53,154,234,234,4,203,131,68,49,232,202,172,125,234,68,59,158,147,237,120,242,171,212,184,172,98,165,141,46,49,162,160,115,83,244,205,133,108,105,108,253,240,94,202,224,235,6,110,159,174,194,215,70,71,
    135,218,189,40,173,239,64,90,152,179,73,76,176,194,214,158,17,219,50,190,75,48,55,81,31,62,203,138,194,111,133,181,56,124,162,17,43,98,95,171,75,209,236,207,117,210,29,211,212,29,86,238,9,162,19,129,88,
    152,179,33,103,180,192,171,242,139,153,109,244,167,223,34,126,138,8,179,204,122,199,61,204,20,184,180,176,55,99,88,123,250,241,69,141,176,45,45,13,79,133,182,247,161,21,64,45,158,212,23,161,182,75,125,
    110,209,204,137,58,118,155,45,148,155,86,68,25,164,215,174,34,62,149,86,49,237,238,25,128,253,88,235,247,37,240,126,92,214,248,199,162,194,22,20,109,253,149,231,112,96,189,22,138,235,167,246,49,21,49,
    69,83,243,51,226,165,24,94,14,201,86,158,103,152,186,33,64,184,198,147,103,183,13,205,28,91,147,22,200,152,205,63,55,11,39,134,62,107,56,141,63,138,107,193,174,228,53,187,192,91,46,29,81,215,29,147,230,
    26,76,183,52,115,6,19,172,227,239,237,103,8,99,241,70,51,49,143,84,244,229,68,30,191,106,45,245,220,176,105,5,182,109,217,135,134,51,123,145,123,198,112,81,15,75,35,149,167,198,62,207,95,41,140,165,118,
    177,120,15,183,191,77,100,57,41,59,205,216,233,147,69,240,39,2,191,15,241,101,167,27,222,82,216,70,11,188,219,106,18,146,146,252,48,57,100,134,193,116,227,252,146,144,144,228,4,107,186,110,148,241,185,
    147,120,206,31,207,205,10,158,139,36,127,37,247,112,26,83,201,35,248,58,142,20,30,135,45,169,31,38,6,70,96,110,204,116,230,218,170,172,249,184,112,189,9,14,246,238,67,52,173,68,74,153,229,208,140,220,
    16,173,213,24,126,225,243,33,243,31,208,203,183,187,211,80,121,200,160,145,215,58,30,225,244,185,243,120,218,217,13,11,91,91,132,71,196,99,122,128,183,224,30,125,121,123,39,2,15,152,17,101,52,65,126,80,
    20,186,86,87,12,94,129,225,164,61,41,132,139,79,8,214,200,67,116,210,210,243,139,124,66,134,165,217,221,116,154,107,60,233,208,240,11,81,7,125,49,18,14,206,94,88,180,56,203,112,62,163,102,188,223,222,
    194,145,132,222,174,187,140,151,66,97,108,128,187,81,215,61,59,18,192,174,164,224,250,86,86,109,250,56,123,11,71,10,44,100,238,200,200,200,96,178,225,239,239,243,241,118,207,142,28,200,136,160,253,71,
    21,199,210,32,178,36,112,73,224,18,36,129,143,98,129,211,249,225,116,202,178,36,10,9,31,181,69,145,230,228,75,248,216,64,251,31,165,42,83,130,228,19,74,144,32,41,184,4,9,146,130,75,144,32,41,184,4,9,146,
    130,75,144,48,114,21,92,213,143,87,3,131,130,249,74,54,54,54,31,165,128,155,47,252,137,130,179,154,144,157,240,194,178,13,57,210,74,191,143,89,193,7,123,110,97,247,246,163,58,243,235,221,194,50,176,50,
    217,255,163,19,176,165,21,253,92,6,171,224,54,38,175,81,144,48,154,20,92,245,20,101,26,229,182,114,1,148,237,67,151,158,212,31,68,153,187,254,216,201,18,36,140,112,5,239,197,229,226,61,220,90,15,101,187,
    11,150,124,155,131,190,202,109,40,209,44,164,164,177,170,61,156,191,227,173,67,52,84,88,84,24,32,63,181,153,180,132,181,37,207,54,14,244,160,189,189,11,221,61,175,65,87,25,90,88,201,96,231,224,8,23,231,
    113,38,103,184,183,251,25,158,117,62,71,79,191,82,243,88,21,172,100,246,112,114,118,134,163,195,219,185,85,42,226,170,169,84,22,226,121,48,194,205,27,16,185,87,64,211,164,170,198,154,208,24,62,89,255,
    171,231,232,104,127,134,151,189,253,228,177,150,140,10,141,25,107,7,135,113,19,48,206,168,117,56,228,189,13,168,116,158,57,216,247,28,173,173,237,120,77,196,44,27,51,22,78,110,19,97,111,61,138,20,188,
    177,186,0,149,188,245,152,97,11,51,49,217,142,40,72,234,10,220,189,185,143,9,252,78,81,177,191,4,206,27,150,12,235,167,182,92,57,200,91,213,22,132,229,223,132,65,177,115,63,183,200,213,16,188,163,151,
    34,43,222,87,212,93,24,236,107,65,217,182,60,40,76,200,155,119,88,58,50,146,3,96,154,186,247,227,90,217,86,156,184,57,116,38,40,121,53,82,141,88,245,247,178,233,52,118,104,230,192,83,208,200,211,235,178,
    67,97,163,108,198,225,45,5,220,122,40,211,224,71,218,7,226,114,127,114,251,20,246,150,92,49,154,18,93,145,157,181,96,26,196,86,128,181,92,18,190,183,197,89,110,56,92,88,169,147,142,198,123,215,14,65,62,
    98,21,156,126,73,145,198,60,224,251,219,201,211,198,169,15,44,220,145,250,117,10,110,255,175,66,115,181,129,137,0,242,205,87,49,122,63,205,171,11,5,246,239,84,171,100,64,116,6,230,198,249,11,62,101,223,
    219,113,27,71,118,151,112,129,151,155,47,20,225,144,213,50,100,198,10,215,186,244,52,159,99,226,35,12,193,5,179,51,22,33,220,223,69,40,168,129,103,184,114,170,8,167,174,61,87,211,171,63,130,109,245,97,
    88,243,125,50,156,140,54,194,50,132,45,90,131,246,155,187,184,66,169,56,177,27,158,30,134,63,18,67,215,29,148,241,148,155,46,166,203,94,18,170,46,92,86,222,88,188,113,35,84,195,88,234,190,167,151,176,
    93,19,111,127,168,208,135,235,172,212,84,62,191,138,45,220,123,97,149,55,5,139,231,132,194,209,86,88,59,63,168,59,133,3,39,213,239,224,233,141,82,252,74,126,179,179,254,133,153,62,250,148,212,133,121,
    111,135,11,213,247,216,250,207,198,146,57,65,112,180,81,226,85,247,0,236,156,199,188,19,221,51,187,130,43,59,175,224,247,67,215,134,228,235,24,77,172,157,176,49,105,53,62,20,107,210,218,176,171,68,157,
    142,198,131,216,123,204,21,235,82,253,77,104,148,69,99,173,60,94,180,80,216,58,127,130,172,77,14,56,184,37,143,179,110,205,205,79,208,7,47,158,213,237,71,251,115,25,98,98,34,209,221,221,141,23,173,47,
    48,53,37,3,97,98,86,196,122,2,194,231,125,13,215,241,249,40,56,203,126,222,180,30,247,91,103,193,201,20,171,51,232,132,164,245,139,241,112,251,97,174,209,93,177,183,20,238,27,211,5,17,91,248,86,255,234,
    177,2,193,247,89,232,162,72,126,90,75,107,107,3,50,235,135,162,124,39,142,93,123,193,59,231,133,37,107,115,48,217,97,80,171,51,224,46,10,4,202,77,210,173,39,233,68,87,220,218,98,242,140,84,200,195,35,
    113,236,167,33,55,244,76,97,1,28,191,254,2,1,227,197,92,166,118,174,128,174,208,202,175,157,221,40,241,193,169,181,41,222,117,74,32,164,165,57,179,137,18,234,10,201,233,147,121,248,180,229,1,23,51,227,
    149,226,32,142,123,24,87,101,83,204,89,30,109,216,226,91,90,9,50,171,234,211,181,168,62,193,51,225,35,226,235,246,246,14,128,186,141,212,229,84,113,126,63,177,248,202,183,151,145,197,216,169,200,206,142,
    36,53,7,251,53,145,59,40,56,112,85,237,114,232,212,132,149,130,224,98,244,131,70,33,70,126,18,76,217,121,149,9,241,33,180,218,217,200,142,167,11,78,117,223,71,123,227,93,240,99,58,248,197,37,233,81,110,
    254,11,119,69,2,201,139,130,203,75,59,234,21,109,8,136,157,168,247,150,184,172,121,122,10,243,136,87,240,23,56,251,151,208,218,140,13,11,133,172,171,9,15,59,196,210,91,193,110,106,36,112,101,200,31,163,
    85,182,155,219,119,162,209,127,116,27,108,250,131,214,168,53,201,17,30,190,192,237,70,195,116,218,27,46,160,188,248,44,90,241,254,48,214,59,17,57,137,29,156,111,218,223,92,129,163,23,221,144,201,11,200,
    48,248,250,46,74,121,53,225,152,128,197,152,103,84,143,19,181,218,187,181,172,182,31,62,91,187,4,147,28,6,13,52,38,95,11,142,27,170,247,32,183,250,221,43,152,181,149,121,199,4,204,164,224,42,220,46,207,
    199,69,45,69,238,169,47,197,95,245,166,81,170,36,141,78,215,245,75,222,40,24,133,73,181,205,171,70,20,253,90,164,245,121,182,0,164,229,36,96,242,196,241,176,17,169,247,219,234,139,176,239,68,227,59,121,
    62,253,116,81,114,203,86,206,66,55,156,249,11,151,189,217,194,221,139,218,163,135,121,133,46,18,43,210,167,14,235,190,153,106,181,13,33,52,229,115,196,250,219,49,61,55,198,186,141,180,182,179,150,125,
    216,47,32,154,69,193,219,174,29,65,9,207,98,112,173,124,35,239,239,105,174,226,85,217,13,76,228,172,175,191,138,55,161,209,105,58,30,93,171,17,40,119,80,242,151,122,130,0,14,89,198,39,45,141,239,144,3,
    25,19,112,164,117,243,144,15,91,185,255,4,38,203,83,161,188,86,166,137,250,162,110,156,165,127,157,192,4,150,50,217,106,155,96,40,156,61,233,23,192,134,98,47,117,190,24,128,189,253,120,140,54,188,115,
    5,239,109,61,199,4,244,227,169,10,150,103,133,154,212,133,70,171,236,101,137,173,200,175,82,7,232,163,145,202,246,30,115,35,141,206,0,179,141,4,90,90,9,221,27,199,9,134,90,58,61,184,88,180,157,167,116,
    26,31,249,25,169,214,223,166,107,203,210,21,11,214,46,194,227,29,236,72,175,2,187,180,66,183,69,166,47,213,211,104,123,183,86,219,214,35,8,113,158,231,80,221,194,246,60,21,160,194,145,255,77,78,113,60,
    56,253,27,14,212,118,195,197,103,26,166,250,7,32,52,108,138,160,71,107,84,43,56,109,121,31,216,95,35,56,183,240,203,5,58,193,185,140,129,87,100,38,230,52,254,194,245,157,191,82,28,65,169,219,106,164,69,
    56,155,69,16,238,83,167,0,188,104,113,213,133,191,224,142,127,28,146,102,7,195,217,206,26,170,129,30,180,53,222,194,133,242,11,122,253,243,235,181,53,8,32,133,211,211,206,10,150,54,111,54,44,111,225,16,
    136,207,178,158,112,177,94,248,112,139,200,230,2,95,138,187,133,191,11,106,78,198,88,132,204,65,240,248,167,168,171,107,55,248,92,26,176,201,43,48,12,30,14,108,225,25,135,216,229,107,208,251,211,46,92,
    214,156,185,122,252,15,242,35,142,91,228,124,196,132,251,99,156,53,125,170,18,221,93,173,184,93,87,139,11,188,216,188,237,77,55,225,25,24,251,65,149,251,221,42,184,234,41,142,110,63,44,120,249,145,233,
    235,48,109,130,197,27,18,180,69,196,146,21,120,248,243,80,32,183,219,149,187,225,236,178,30,177,62,99,161,82,190,20,190,32,229,240,207,25,232,231,177,251,90,9,165,86,87,165,124,163,7,206,151,238,65,181,
    198,15,110,191,95,141,130,251,186,45,43,218,23,156,185,32,148,113,153,154,47,29,64,129,166,96,168,58,21,40,252,93,161,233,29,80,199,40,19,242,217,103,184,143,90,3,26,1,48,59,190,149,215,5,169,142,224,
    154,57,199,219,160,91,210,217,241,66,183,174,185,94,137,178,235,198,73,60,206,109,26,81,112,91,65,55,230,28,185,28,137,125,109,168,41,43,33,114,81,135,35,188,115,233,56,249,233,167,51,45,58,29,115,226,
    3,68,7,122,222,228,189,141,12,5,39,213,107,26,17,70,218,187,228,206,194,157,137,167,38,6,239,232,85,144,71,155,86,96,196,62,105,45,148,134,43,98,23,203,17,107,2,85,239,97,62,89,170,203,167,49,110,66,15,
    154,27,30,9,252,231,236,149,196,26,26,188,215,22,177,57,166,241,110,244,107,176,113,39,114,249,234,157,208,54,253,189,141,216,110,66,9,111,228,230,245,55,227,208,86,254,112,59,105,84,126,153,33,77,177,
    149,20,124,20,66,213,133,139,21,149,104,199,120,12,246,62,198,173,187,218,222,125,16,86,108,76,53,235,64,136,164,224,18,204,103,173,149,93,184,119,253,190,78,163,213,198,39,2,139,231,205,134,207,120,43,
    73,72,146,130,143,94,88,200,124,177,92,10,184,36,41,184,4,9,146,130,75,144,32,41,184,4,73,193,37,72,248,152,20,60,48,48,176,148,108,127,20,251,18,144,4,9,163,29,255,15,252,23,251,203,94,62,16,19,0,0,0,
    0,73,69,78,68,174,66,96,130,0,0};

const char* LowhighpassAudioProcessorEditor::analyzer_off_png = (const char*) resource_LowhighpassAudioProcessorEditor_analyzer_off_png;
const int LowhighpassAudioProcessorEditor::analyzer_off_pngSize = 5552;

// JUCER_RESOURCE: analyzer_over_png, 5618, "analyzer_over.png"
static const unsigned char resource_LowhighpassAudioProcessorEditor_analyzer_over_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,184,0,0,0,64,8,6,0,0,1,222,245,141,37,0,0,0,9,112,72,89,
    115,0,0,17,137,0,0,17,136,1,252,205,31,202,0,0,21,164,73,68,65,84,120,218,237,93,7,88,20,215,22,254,23,17,65,81,17,163,66,236,45,65,108,40,138,5,21,1,27,22,172,40,88,243,236,6,196,6,136,53,137,32,10,34,
    10,98,98,55,49,234,162,98,44,81,35,136,96,65,77,196,134,13,141,237,89,34,177,161,17,65,1,205,190,123,103,119,135,153,221,217,101,23,89,21,222,252,223,183,59,51,119,102,238,156,57,115,238,185,231,156,185,
    247,140,177,76,38,115,0,208,13,6,128,177,162,226,32,67,85,174,51,30,156,222,137,26,14,3,11,95,249,253,131,227,81,179,199,26,72,36,18,102,155,176,141,89,223,124,244,30,234,156,138,70,205,54,131,216,50,
    37,148,219,116,121,141,108,219,8,85,174,60,224,30,231,36,37,134,59,213,66,242,226,206,108,25,119,31,61,175,101,203,150,232,50,247,15,56,119,109,140,57,78,230,234,149,183,157,126,140,89,214,230,80,197,
    165,76,149,136,47,186,69,227,207,120,31,166,236,236,217,179,56,147,13,196,165,60,23,102,203,169,165,29,213,168,130,192,93,52,250,207,126,102,169,172,88,185,175,85,89,242,115,170,84,184,7,170,196,213,13,
    61,117,123,160,228,170,115,97,32,24,27,74,198,245,102,75,122,30,96,93,186,144,149,83,41,176,241,220,133,52,105,63,44,157,214,13,207,108,22,33,100,130,61,43,29,119,94,145,202,21,207,171,7,41,139,227,72,
    210,242,95,82,81,61,206,25,30,171,51,216,7,108,44,40,114,82,25,102,44,139,103,214,67,38,240,37,71,181,113,81,80,25,159,58,160,185,162,29,36,169,83,190,202,67,130,137,59,100,106,98,40,167,250,30,135,128,
    87,68,158,203,97,168,226,110,40,134,124,183,7,231,250,212,212,204,22,90,177,54,217,166,104,79,88,210,95,98,142,93,164,124,43,103,95,64,239,26,8,16,104,27,122,203,249,46,129,74,10,146,115,131,137,227,39,
    33,230,197,134,240,164,208,150,112,9,60,47,168,35,11,194,137,80,103,72,198,39,49,242,245,129,9,127,196,18,45,225,180,22,161,117,77,251,149,55,48,254,190,63,174,68,247,196,253,189,19,208,35,222,131,172,
    119,145,239,124,151,10,137,177,29,91,71,54,89,154,41,234,160,235,231,200,185,29,2,143,8,50,78,35,225,18,137,21,234,213,171,135,250,245,235,51,75,85,130,116,133,227,204,36,92,165,250,39,90,134,90,125,215,
    144,58,86,179,55,152,46,227,171,26,51,1,173,16,124,36,67,119,142,47,91,182,76,141,200,139,123,162,240,235,229,76,68,68,68,176,101,139,20,235,50,89,14,233,54,157,176,124,229,47,204,121,35,71,142,196,183,
    227,71,194,216,92,221,234,160,88,76,234,167,245,72,201,146,98,218,180,105,204,190,53,139,252,177,120,246,110,36,101,164,161,118,37,99,212,118,30,135,222,117,42,232,78,56,173,72,21,205,250,250,162,25,93,
    105,146,191,47,144,61,206,4,103,142,30,101,203,55,109,218,196,174,95,254,35,1,121,22,118,60,70,4,10,212,79,49,126,214,18,230,167,68,13,135,161,168,241,177,180,74,147,54,93,12,211,56,13,105,175,24,90,29,
    6,21,87,194,33,18,94,18,8,95,183,110,29,198,142,29,251,113,8,151,27,174,206,68,149,37,234,93,241,184,113,227,62,14,225,57,55,215,193,204,54,28,175,175,250,177,101,169,63,116,194,219,177,199,208,202,68,
    194,235,225,14,173,159,139,110,99,23,170,219,194,111,14,67,98,22,72,202,82,88,23,97,126,134,12,142,150,92,231,211,157,236,223,131,172,187,71,96,94,199,153,87,135,210,226,159,27,159,129,160,174,149,116,
    35,220,180,225,56,166,130,57,246,126,8,63,245,10,126,237,204,241,234,101,41,116,48,145,247,128,55,99,71,160,108,227,165,200,190,50,131,33,90,126,177,11,124,211,192,212,149,252,157,97,235,164,126,205,193,
    74,124,194,238,18,162,41,40,209,66,246,206,11,178,172,88,24,25,95,120,86,222,93,251,41,42,82,218,14,13,6,69,2,223,108,100,214,79,238,142,228,185,235,92,120,145,223,206,59,255,98,96,221,23,140,216,41,65,
    221,156,178,77,230,160,22,99,136,181,100,202,236,237,237,213,206,175,168,143,168,180,32,149,154,18,227,138,26,88,5,129,90,124,237,251,189,84,123,188,74,108,85,216,41,117,185,98,68,172,66,41,221,190,20,
    204,146,215,101,193,57,28,154,215,226,253,100,252,2,173,244,230,77,118,251,223,123,63,147,139,183,38,94,165,185,218,177,247,238,92,71,249,182,125,4,137,230,226,14,183,209,27,243,109,23,106,65,38,208,115,
    231,9,155,198,186,17,254,246,1,66,22,174,231,21,25,213,26,65,172,185,167,168,237,88,141,99,173,149,65,72,64,59,56,142,154,129,33,241,29,209,170,83,18,115,177,113,131,6,33,233,206,27,158,21,121,229,251,
    142,8,202,203,175,147,238,91,166,176,12,223,188,1,102,205,146,91,135,244,9,55,106,223,151,37,154,91,71,193,132,27,215,192,172,217,163,53,90,140,249,214,90,57,76,29,213,158,89,91,187,255,56,91,186,54,54,
    150,89,58,115,44,192,198,95,31,39,196,52,212,106,125,82,220,186,117,171,64,43,245,131,245,156,7,14,239,47,148,3,242,209,187,252,158,174,189,68,91,165,68,196,85,68,227,80,132,200,112,145,225,34,222,135,
    225,89,196,130,147,91,138,43,206,228,192,199,222,228,131,16,124,66,17,4,61,65,188,40,67,69,129,63,73,134,43,153,221,157,252,38,183,42,131,65,196,134,178,18,5,216,48,12,239,175,240,113,148,82,70,125,21,
    107,1,127,69,41,141,74,12,26,62,13,175,174,238,194,193,115,255,149,59,116,105,239,224,101,99,196,172,7,181,36,190,255,121,197,113,227,166,162,210,243,20,98,213,159,144,123,172,171,174,99,235,132,47,180,
    4,79,192,68,217,249,15,60,151,236,43,195,198,11,58,144,227,78,104,188,163,86,108,12,226,200,242,94,196,139,56,160,160,35,16,45,44,159,98,78,232,58,102,251,51,79,41,158,72,61,121,247,70,253,90,165,3,33,
    20,135,120,111,134,199,78,179,193,110,178,236,23,121,137,109,210,178,215,9,144,152,117,33,55,232,34,24,53,26,77,24,182,158,101,88,4,100,47,19,97,84,209,21,99,6,46,131,215,149,25,76,233,188,115,50,204,
    83,57,111,141,130,161,210,137,95,18,134,11,123,33,244,173,40,109,109,170,15,188,131,130,217,233,138,178,100,153,240,171,218,57,191,62,70,112,239,42,204,250,179,148,249,12,179,149,239,144,149,152,189,120,
    45,226,2,29,208,35,212,11,115,135,185,178,199,83,152,56,253,66,28,243,254,134,145,240,204,148,69,240,88,126,157,89,223,61,165,41,36,83,84,143,72,66,21,175,24,86,10,148,168,103,83,133,127,179,21,236,52,
    74,42,19,87,233,212,9,182,181,107,195,210,90,23,37,93,14,239,210,127,66,41,235,81,164,14,31,194,244,104,92,221,224,193,72,51,141,253,168,170,185,84,178,207,110,76,172,122,180,141,224,90,162,220,103,190,
    22,211,31,146,24,225,171,237,78,186,198,99,248,176,145,157,13,165,82,110,162,130,195,108,65,66,185,65,41,105,140,23,194,39,246,134,159,147,185,94,29,33,197,146,147,153,76,132,143,149,94,210,18,34,195,
    162,10,60,223,200,106,36,18,23,47,135,75,224,74,132,31,31,11,127,194,80,243,182,209,152,227,84,169,64,169,230,162,153,11,29,206,68,85,160,39,185,71,41,127,103,94,6,80,218,242,195,233,112,137,68,30,13,
    121,161,37,192,64,131,108,82,114,83,254,157,203,99,176,30,129,8,199,153,251,128,64,115,248,183,47,15,127,78,57,101,26,237,148,227,20,204,10,62,146,9,77,242,228,60,243,28,188,2,201,181,59,201,131,119,153,
    167,188,57,123,51,200,249,149,217,173,133,125,170,98,161,154,106,146,161,124,235,89,164,181,180,32,173,197,141,28,175,46,226,21,187,172,193,139,67,227,62,12,195,117,141,228,112,143,171,53,51,9,178,153,
    66,71,89,170,212,87,78,75,253,222,252,77,39,77,117,2,82,141,180,90,234,76,191,145,85,15,157,142,117,156,169,153,142,18,237,248,240,173,143,6,132,89,55,68,79,211,144,72,54,64,12,85,100,184,232,218,139,
    40,52,195,139,235,248,148,98,138,120,99,98,110,5,201,138,153,30,44,174,160,188,22,85,138,168,195,69,134,139,16,25,46,50,92,132,200,112,145,225,69,130,149,43,215,65,98,82,21,95,143,115,255,255,98,120,100,
    111,9,166,202,103,66,226,67,218,241,62,62,52,60,234,252,255,199,112,37,179,41,142,62,7,156,42,137,234,194,96,12,167,243,163,41,6,7,124,131,237,97,223,193,181,241,60,188,125,40,142,150,51,24,195,107,185,
    173,101,150,107,66,191,101,24,254,46,61,24,121,8,66,105,117,87,22,116,212,253,191,217,139,96,84,182,49,103,79,254,91,242,124,100,160,71,131,202,136,227,12,161,45,219,252,43,100,93,216,168,145,14,101,44,
    60,238,145,12,221,170,170,236,124,147,12,137,89,71,70,253,200,94,47,80,172,11,35,37,75,198,204,8,87,98,158,71,51,4,199,94,98,183,235,219,13,198,205,243,219,4,238,205,19,169,63,91,160,249,136,85,58,171,
    86,253,25,78,110,68,14,103,102,192,252,193,192,134,232,177,248,6,70,70,221,128,212,183,161,192,9,123,9,179,247,130,190,28,152,50,181,13,34,151,111,1,157,97,192,31,6,126,159,108,215,98,31,70,64,128,11,
    194,194,194,144,157,250,35,41,255,81,227,141,196,38,6,192,218,37,12,61,237,212,91,216,134,17,114,6,251,236,164,83,235,174,107,189,165,172,28,250,116,185,140,148,35,34,58,26,155,124,124,112,225,194,118,
    82,190,157,157,153,153,143,24,194,108,3,75,120,80,123,249,141,68,157,220,203,44,187,47,58,14,44,182,66,204,148,47,8,195,133,25,227,25,118,22,82,127,249,76,135,229,203,54,179,55,245,148,252,62,163,143,
    100,186,92,250,105,62,2,101,218,128,208,208,80,249,75,105,178,174,156,62,162,10,43,231,80,242,31,198,180,48,213,1,192,138,23,243,88,49,128,142,105,175,161,246,208,198,144,186,55,80,62,55,153,195,246,63,
    177,19,229,239,61,109,188,126,68,218,214,81,204,250,52,111,111,164,252,208,19,14,95,255,6,235,174,107,213,222,105,114,143,53,8,195,149,131,117,38,179,12,168,198,190,232,141,127,12,245,166,77,160,100,182,
    18,142,228,71,85,129,82,5,185,71,188,132,76,96,218,128,247,226,206,144,6,30,65,78,110,158,70,122,150,247,146,119,224,62,210,191,16,237,85,93,222,180,159,29,100,150,165,172,133,35,207,116,168,196,6,165,
    116,179,51,72,192,228,36,160,184,168,194,192,214,147,14,0,95,75,240,79,2,237,187,248,12,215,135,217,122,51,252,234,150,65,242,167,234,185,139,87,190,231,106,52,76,109,125,208,171,133,63,242,254,90,82,
    96,61,238,157,8,195,143,65,77,135,143,247,112,103,71,91,233,138,41,251,206,99,170,164,5,86,14,237,76,24,46,127,159,25,62,202,141,89,30,184,32,208,145,191,75,101,199,165,164,107,80,85,237,58,183,128,44,
    211,40,95,95,155,155,163,168,160,23,195,27,15,223,201,44,53,13,148,121,251,48,156,116,158,75,80,90,79,34,110,238,152,136,134,131,87,179,219,131,70,140,128,165,153,25,14,173,89,195,155,30,36,12,59,102,
    238,211,29,220,4,237,111,233,140,170,0,133,201,42,212,218,232,20,35,10,58,54,69,211,88,200,179,71,47,124,124,43,69,246,108,55,187,78,103,225,171,226,246,237,219,114,83,49,234,50,118,249,54,209,139,8,37,
    179,85,71,196,158,168,247,39,111,108,162,38,156,252,205,23,214,110,81,232,51,250,0,46,135,60,101,202,186,4,157,82,59,110,168,162,239,48,107,28,44,56,16,136,189,87,3,58,114,58,51,124,178,131,124,252,156,
    223,206,199,88,50,64,136,88,58,223,177,5,51,4,14,190,133,35,88,117,248,113,144,130,217,101,76,180,183,25,171,30,145,180,27,71,218,198,94,88,168,48,43,99,231,182,85,211,219,202,113,43,217,151,231,104,173,
    239,34,233,50,154,169,92,242,225,227,215,248,188,170,217,135,98,120,22,86,202,5,88,3,179,185,77,91,115,231,89,16,148,86,11,197,234,137,182,76,71,76,17,151,112,131,88,41,218,167,67,42,59,207,249,76,223,
    224,206,159,227,201,209,219,218,70,142,93,217,60,144,81,155,205,77,248,3,67,211,246,78,133,109,223,72,40,103,242,26,156,225,39,66,123,51,203,242,109,195,181,30,119,116,207,120,38,89,71,247,106,1,132,176,
    48,157,137,248,129,244,197,147,8,63,170,168,76,245,124,254,250,52,42,153,57,32,97,126,75,72,230,107,111,234,83,246,92,192,84,133,126,94,125,102,7,191,165,184,216,177,235,22,2,211,73,149,195,140,109,135,
    197,162,251,112,9,59,180,78,21,47,222,147,217,58,51,252,190,73,87,68,68,184,163,207,132,105,90,143,171,233,190,26,139,34,108,96,66,231,141,18,132,132,16,91,79,102,170,118,92,155,177,75,16,209,47,87,233,
    107,48,57,149,28,127,91,141,190,61,39,194,130,244,15,14,195,252,176,106,193,36,121,243,62,176,10,223,173,63,132,26,159,183,206,175,179,140,64,43,43,213,156,93,29,175,50,27,163,239,244,85,48,239,151,173,
    145,110,7,219,252,36,39,7,201,67,253,235,202,81,248,6,204,197,133,107,15,97,100,97,129,233,254,225,152,228,233,204,59,71,211,189,21,9,195,61,167,205,214,185,66,110,82,20,58,87,87,8,78,35,252,224,164,82,
    214,212,109,2,110,203,38,168,29,75,203,99,221,38,20,88,231,223,7,167,176,206,147,42,154,245,157,32,79,250,162,35,170,55,118,194,78,206,116,95,33,104,162,195,96,209,194,79,9,25,55,118,48,86,10,227,43,232,
    152,224,174,216,133,103,63,5,40,103,82,40,177,241,226,155,146,25,45,252,84,32,169,208,6,187,118,81,175,183,12,250,245,115,43,185,225,217,79,7,229,8,163,251,21,43,138,197,151,200,34,195,69,134,139,16,25,
    94,140,25,78,199,135,211,97,180,34,43,68,148,104,141,34,142,201,23,81,210,64,227,143,98,151,41,66,180,9,69,136,16,5,92,132,8,81,192,69,136,16,5,92,132,8,81,192,69,136,248,116,5,60,47,11,79,178,242,120,
    195,153,45,44,44,74,36,131,147,22,212,135,203,55,138,1,153,104,133,35,25,41,226,76,191,146,44,224,255,254,189,9,13,172,71,169,141,175,111,52,250,23,92,93,223,191,196,49,216,164,12,157,35,166,20,240,242,
    122,207,81,16,81,156,4,252,93,42,134,43,132,219,172,1,240,58,255,147,53,72,219,48,0,67,29,52,231,78,22,33,226,19,23,240,12,132,185,216,177,115,61,94,223,108,128,125,143,207,224,197,215,22,24,174,152,72,
    73,115,85,183,107,150,201,153,135,168,205,204,201,67,118,158,98,14,97,233,210,40,91,58,95,55,202,178,31,227,202,165,107,184,151,158,129,92,178,45,49,169,0,235,90,245,209,180,73,109,232,59,83,228,249,195,
    107,184,158,118,11,127,103,202,175,149,147,147,3,83,243,234,104,100,215,20,95,84,175,248,126,150,26,49,213,242,242,36,130,247,160,139,153,151,45,112,46,175,78,125,80,186,44,169,163,224,195,178,158,220,
    198,165,139,105,184,251,244,21,202,148,161,121,194,203,160,178,149,21,106,214,179,65,29,157,230,225,228,34,59,251,173,218,53,101,47,110,227,248,201,84,100,228,74,80,190,178,53,108,237,219,192,186,108,
    49,18,240,184,89,109,49,147,51,31,115,244,198,120,244,170,66,4,36,230,15,196,26,183,97,18,191,83,248,182,119,71,179,140,196,2,237,212,19,17,221,56,179,218,60,145,252,95,95,108,168,211,158,157,228,170,
    13,109,103,236,195,177,240,94,130,230,130,236,159,100,12,179,232,8,169,30,247,230,48,90,138,248,245,158,208,79,220,179,240,253,48,115,76,229,204,136,241,140,188,4,169,14,179,254,238,31,156,130,90,110,
    249,169,189,105,230,233,7,167,188,81,241,205,97,244,49,235,194,206,135,210,15,206,196,63,16,230,251,153,205,147,208,90,145,67,64,23,208,25,217,199,183,142,98,231,104,241,158,91,104,119,222,115,219,246,
    91,11,12,113,83,207,148,76,243,189,207,41,2,103,229,131,8,56,253,146,34,205,121,192,181,183,215,127,85,87,190,81,202,1,49,55,86,192,180,225,100,165,91,198,100,0,185,253,48,8,181,116,190,66,12,58,212,145,
    79,206,237,51,99,39,86,47,26,192,251,148,125,70,218,102,184,219,142,96,19,47,255,190,180,55,220,76,143,33,33,152,159,84,226,239,164,153,76,126,132,124,52,192,130,95,118,194,175,127,51,158,230,151,189,
    190,138,165,19,157,225,191,233,49,179,125,122,131,23,44,54,28,66,90,238,122,216,232,172,132,203,97,202,246,203,184,40,105,194,54,202,152,41,77,225,232,168,253,35,49,116,222,129,151,27,55,111,189,59,146,
    14,123,203,27,151,169,43,126,205,34,26,188,0,77,157,121,97,17,172,28,102,171,52,250,25,106,51,53,115,110,70,115,158,139,66,120,135,70,97,111,244,100,52,172,196,239,157,247,45,159,132,62,211,182,51,91,
    215,164,95,161,10,249,5,197,103,96,174,198,207,96,52,96,158,219,16,55,249,115,179,232,22,132,95,163,198,224,75,203,55,120,116,63,27,86,77,139,198,19,55,184,128,231,164,45,69,67,143,205,249,23,252,220,
    15,7,85,156,201,50,13,124,112,229,231,211,104,60,226,103,185,169,158,30,140,150,94,246,72,151,246,211,195,41,243,199,93,89,152,96,163,176,108,52,28,201,175,235,160,135,89,71,86,187,157,62,124,26,255,16,
    1,175,200,209,168,151,110,149,71,64,128,47,210,211,31,224,78,242,93,12,222,120,8,147,5,180,136,196,204,22,126,63,61,130,125,189,86,112,249,246,172,162,116,3,118,158,12,215,83,235,52,198,218,244,109,72,
    178,30,194,58,221,147,91,121,160,109,214,30,94,198,22,174,214,143,26,234,202,251,62,11,157,20,201,61,182,116,217,178,90,120,150,133,141,163,170,98,244,166,39,156,178,86,248,245,65,10,122,87,87,13,6,108,
    71,91,158,112,183,194,190,71,41,232,37,56,227,214,18,189,167,110,131,108,234,44,12,149,180,96,123,191,121,221,28,80,255,246,117,120,213,53,18,82,123,108,3,77,81,185,223,42,85,138,137,13,78,181,141,171,
    173,31,143,73,251,207,47,17,20,66,219,225,155,176,35,121,63,155,51,227,89,76,127,140,108,167,91,151,77,177,228,228,124,237,26,191,180,9,79,11,191,125,169,174,81,187,142,157,139,174,2,182,238,243,23,217,
    120,157,199,152,186,196,190,85,218,253,50,164,191,49,122,111,30,25,89,13,198,201,164,100,88,59,175,80,148,236,133,179,235,74,185,201,161,214,19,78,228,37,23,163,31,52,26,175,227,39,193,114,210,86,50,41,
    62,120,90,123,250,33,156,90,42,252,29,249,75,7,118,48,31,175,85,194,117,246,26,13,194,205,133,29,34,146,38,67,202,222,203,77,172,88,159,2,175,224,54,26,207,152,27,255,163,134,198,252,201,11,248,125,248,
    217,241,181,77,213,49,62,168,112,61,1,135,47,9,57,65,38,168,54,36,128,168,164,192,124,195,131,116,217,246,173,51,5,179,255,168,61,192,220,60,237,7,148,106,128,118,157,128,221,199,180,31,118,113,239,2,
    140,234,251,13,239,225,26,26,86,157,163,144,188,248,18,107,155,190,250,221,7,131,130,237,113,136,147,144,65,246,108,27,60,56,61,97,37,247,24,252,164,83,196,137,106,237,186,42,90,219,25,113,15,18,209,173,
    186,230,179,94,61,121,202,219,62,28,210,18,146,16,253,239,173,32,217,53,53,240,39,251,12,36,224,185,216,50,202,30,17,42,193,238,199,235,191,66,187,245,250,213,228,79,157,206,71,137,133,74,70,161,87,111,
    243,236,32,58,126,230,166,242,121,54,119,108,62,186,20,189,218,53,128,133,64,191,159,18,217,25,14,83,143,22,201,245,233,167,139,150,31,55,103,53,116,194,188,118,8,119,85,54,238,12,4,15,240,228,52,58,111,
    164,236,25,82,160,249,166,175,214,214,134,255,68,157,68,240,64,43,188,202,149,64,87,153,204,205,205,133,185,69,205,146,23,38,76,249,161,31,134,115,52,6,235,229,235,120,254,223,71,124,57,93,118,18,147,
    57,235,214,95,75,244,112,58,245,199,145,232,185,60,225,246,140,252,83,67,18,192,124,205,120,54,249,104,17,82,80,142,73,56,242,135,113,126,40,213,191,253,56,244,150,73,145,243,195,112,69,214,23,185,115,
    182,245,118,20,147,88,74,111,173,173,135,162,176,233,208,9,242,143,162,201,145,246,223,44,124,254,121,93,20,55,20,185,128,63,59,17,192,36,244,227,136,10,78,28,243,214,43,132,70,187,236,196,197,201,112,
    9,148,39,232,163,153,202,236,137,211,249,80,234,105,176,55,129,38,166,229,249,62,126,163,207,180,28,253,8,65,78,86,28,161,147,227,246,181,39,239,151,113,183,84,115,108,190,187,9,191,215,30,169,112,58,
    99,208,72,37,117,155,247,214,35,26,156,182,162,213,218,149,29,199,98,118,203,5,8,57,167,136,60,69,116,197,152,47,184,223,228,20,198,222,233,21,208,119,89,38,26,118,244,192,192,190,131,225,235,59,136,23,
    209,42,214,2,78,61,239,46,29,248,57,10,127,76,219,162,150,156,75,167,168,236,204,4,132,30,168,204,198,206,159,198,120,193,163,101,83,236,246,111,108,16,70,180,26,56,16,224,100,139,11,238,102,137,237,221,
    103,99,85,248,120,52,169,86,22,185,89,143,112,250,192,70,44,240,142,208,104,159,111,14,153,143,193,157,87,162,67,181,82,48,177,176,40,84,99,52,170,53,2,39,127,59,195,230,122,225,57,226,222,135,216,196,
    151,194,102,97,13,94,207,201,68,36,134,133,97,116,189,139,88,182,236,146,214,235,210,132,77,157,70,250,194,177,122,41,69,73,77,44,60,123,25,47,36,77,240,189,50,78,52,241,75,242,3,122,79,94,133,160,105,
    3,81,219,92,134,183,228,186,247,175,31,133,52,114,49,194,57,185,121,111,28,223,129,199,195,130,63,170,112,23,173,128,191,75,197,64,235,33,188,135,239,189,245,1,70,217,20,54,210,96,137,128,196,63,144,104,
    220,134,13,237,237,9,104,130,121,118,68,123,118,173,138,220,156,123,252,7,148,171,67,36,33,147,179,254,44,135,23,47,166,161,202,127,179,59,98,158,135,29,22,42,236,224,63,227,66,224,18,167,238,89,209,88,
    112,220,150,201,140,201,148,20,234,76,122,26,121,195,200,189,183,13,61,108,182,41,162,3,242,28,101,124,58,51,145,167,75,15,214,35,18,137,223,158,224,132,32,229,25,92,15,71,107,211,194,175,112,245,242,
    19,181,210,39,91,2,48,126,139,110,28,159,219,234,43,34,224,149,120,97,204,149,50,25,162,255,73,193,188,161,253,177,240,192,95,76,233,190,21,19,153,159,38,12,246,219,138,149,75,188,4,95,244,20,230,185,
    125,26,2,78,186,215,93,69,61,113,185,148,3,147,79,77,80,195,207,191,5,217,124,253,26,140,208,39,173,185,144,152,53,71,240,62,25,130,245,234,105,180,127,178,84,127,58,229,38,80,194,222,179,60,251,249,240,
    149,160,2,62,88,111,73,52,174,76,237,155,183,69,1,73,197,214,8,222,255,64,47,190,20,45,63,62,201,48,161,136,66,69,115,94,38,194,173,162,43,231,117,59,113,42,211,18,196,33,182,162,128,23,67,188,189,133,
    224,209,222,72,149,213,129,236,249,239,216,185,63,85,229,0,79,164,100,73,13,250,34,68,20,112,17,134,211,214,217,127,98,231,207,113,106,78,107,249,14,147,176,99,83,4,186,215,53,21,153,36,10,120,241,133,
    164,130,27,206,139,9,151,68,1,23,33,66,20,112,17,34,68,1,23,33,10,184,8,17,37,76,192,227,201,47,72,232,75,64,34,68,20,119,252,15,232,54,58,219,158,236,65,147,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* LowhighpassAudioProcessorEditor::analyzer_over_png = (const char*) resource_LowhighpassAudioProcessorEditor_analyzer_over_png;
const int LowhighpassAudioProcessorEditor::analyzer_over_pngSize = 5618;

// JUCER_RESOURCE: analyzer_on_png, 5590, "analyzer_on.png"
static const unsigned char resource_LowhighpassAudioProcessorEditor_analyzer_on_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,184,0,0,0,64,8,6,0,0,1,222,245,141,37,0,0,0,9,112,72,89,115,
    0,0,17,137,0,0,17,136,1,252,205,31,202,0,0,21,136,73,68,65,84,120,218,237,93,9,88,20,87,182,254,155,93,154,157,200,98,64,131,184,0,137,38,81,212,8,206,184,76,20,51,70,29,39,98,220,6,159,154,24,201,232,
    167,198,141,231,68,179,160,65,242,25,33,113,18,48,201,152,193,168,188,167,76,22,162,243,68,103,132,44,56,147,79,52,99,162,128,27,134,69,20,12,160,64,17,186,65,250,221,91,213,213,85,213,93,221,116,163,
    173,66,234,255,62,232,170,238,170,91,183,254,123,206,185,231,158,186,231,150,147,78,87,60,18,248,118,18,236,0,39,174,224,101,201,118,42,220,122,84,157,96,16,50,66,221,245,194,43,243,24,132,198,169,161,
    82,49,236,190,78,199,109,239,41,236,135,135,190,38,191,141,100,12,223,241,224,247,233,231,57,82,228,96,180,155,22,206,31,80,9,71,195,73,60,230,199,150,227,155,173,194,119,226,223,232,121,195,134,169,49,
    241,245,126,24,255,100,7,54,196,84,154,22,62,58,41,138,252,47,71,95,85,163,228,110,140,107,202,87,98,240,148,62,56,119,168,154,253,238,228,73,242,167,173,67,94,145,151,60,45,199,83,202,77,106,37,190,0,
    143,168,23,184,74,240,5,243,191,13,119,105,198,240,152,230,174,53,40,143,179,153,229,214,53,168,78,247,209,203,176,19,156,236,37,227,54,211,114,77,231,138,32,149,166,107,133,83,41,136,76,136,66,113,86,
    57,222,122,137,65,221,144,129,120,99,97,181,65,58,46,51,110,8,242,224,10,159,76,190,203,19,73,210,219,185,253,208,231,96,57,226,223,23,26,216,201,84,228,138,129,44,53,86,111,167,219,23,72,225,82,201,49,
    86,46,10,42,227,43,166,73,245,64,82,120,102,60,131,165,7,212,38,98,200,213,90,92,1,42,207,129,152,171,170,49,28,59,59,37,24,167,226,46,154,167,133,22,108,73,182,89,37,243,184,137,25,42,224,83,93,51,246,
    137,126,91,59,233,42,214,202,232,134,205,114,254,169,206,6,195,197,201,185,253,196,241,190,16,243,110,83,241,252,84,6,19,146,228,109,100,103,40,36,231,58,252,177,15,43,95,119,185,226,66,165,121,221,23,
    247,18,226,109,115,191,243,55,176,180,38,2,63,108,175,68,101,46,131,223,22,68,145,109,189,189,214,49,80,57,8,215,104,209,121,160,23,116,134,237,83,169,205,24,99,134,56,179,21,87,17,45,236,31,222,11,225,
    3,58,216,79,227,10,89,139,216,245,106,156,81,149,2,196,78,245,157,78,43,81,110,184,193,171,58,47,178,127,203,112,44,173,180,96,21,184,237,45,133,3,201,255,106,235,42,158,150,238,108,82,201,31,114,129,
    47,74,123,99,123,154,51,217,107,99,191,75,73,243,97,183,117,58,103,68,143,87,225,237,12,31,214,166,37,44,112,196,171,75,72,225,234,86,19,175,131,98,43,41,127,59,57,55,59,157,238,57,96,213,202,54,246,183,
    15,72,235,164,36,121,34,191,201,29,253,60,154,209,111,2,48,101,64,139,245,162,66,11,50,198,144,105,106,12,193,117,32,66,248,46,201,112,156,11,138,242,41,67,13,236,222,238,44,202,34,173,164,6,103,138,24,
    180,251,5,73,12,118,146,76,249,20,207,147,214,121,126,125,7,217,226,124,7,234,215,133,224,198,189,177,42,143,68,211,27,104,186,243,202,105,79,127,197,206,230,176,251,117,62,61,183,3,250,69,86,252,195,
    143,156,240,220,194,246,123,83,113,206,113,13,32,166,140,177,185,224,231,23,221,36,21,87,223,253,138,107,202,24,168,135,13,4,115,234,130,222,38,3,167,51,136,77,78,244,71,180,170,78,226,247,30,221,197,
    96,210,98,25,95,88,75,186,116,87,63,242,157,48,68,120,165,169,15,98,60,197,61,97,48,249,189,17,76,37,3,143,190,210,50,120,143,127,99,254,96,188,62,174,202,186,138,187,133,211,2,170,241,167,97,192,91,69,
    15,98,117,244,21,52,147,193,238,24,82,105,90,240,197,28,114,161,225,125,208,124,178,154,173,52,119,49,70,234,26,184,208,207,122,195,141,211,113,205,97,226,116,137,43,86,161,111,77,90,105,57,127,231,134,
    206,19,222,168,178,93,198,183,156,162,221,245,121,172,214,23,196,251,14,3,102,210,62,95,63,210,254,28,146,225,186,24,115,200,223,39,85,1,248,125,200,101,86,236,232,205,81,204,37,199,123,12,11,67,40,106,
    89,71,140,29,113,15,55,61,223,27,29,214,139,202,227,164,80,183,112,119,214,193,234,12,212,227,139,153,30,66,24,106,144,52,47,143,125,172,159,114,25,97,172,8,48,6,175,48,155,126,156,172,53,28,55,49,245,
    97,28,89,247,163,126,79,221,53,25,255,15,45,244,162,138,86,131,221,239,168,164,21,114,37,163,74,83,86,43,202,8,43,99,221,100,43,45,198,101,177,210,59,72,117,129,122,144,71,85,103,129,117,242,174,177,117,
    21,39,108,188,177,53,152,108,8,209,27,135,80,53,241,230,58,208,47,86,234,173,109,94,173,69,236,92,53,226,143,94,38,222,161,23,235,72,45,153,229,128,130,74,31,137,23,121,246,61,114,172,234,97,178,197,49,
    74,61,195,180,116,174,140,86,141,35,254,123,125,43,91,81,218,194,145,177,158,6,135,108,187,222,251,180,174,226,42,53,41,168,209,140,199,40,246,214,212,88,57,247,39,118,235,131,67,148,113,45,187,253,254,
    254,14,86,33,199,173,20,206,125,248,69,202,240,143,102,188,79,97,251,210,69,7,131,14,152,243,82,239,90,207,249,127,5,93,27,250,221,243,46,255,169,113,138,175,210,51,226,42,138,115,168,64,33,92,33,92,193,
    237,16,206,176,177,69,138,29,167,7,98,217,208,234,187,82,97,234,183,211,32,232,241,38,251,69,129,239,75,194,121,178,227,200,223,242,71,47,96,166,206,11,65,184,165,136,176,61,8,159,161,31,227,240,82,70,
    199,42,193,170,70,19,199,149,151,70,30,51,19,2,208,124,166,22,135,79,113,251,217,101,225,152,29,118,141,221,78,30,198,96,211,119,220,247,241,47,248,195,167,190,14,31,28,224,246,231,238,138,196,222,133,
    21,22,130,39,96,163,236,210,6,215,146,223,218,12,241,130,49,228,184,66,179,119,36,196,32,10,210,25,140,95,197,215,163,55,30,243,189,142,63,109,229,246,3,18,34,80,147,85,41,185,183,254,225,64,217,37,152,
    141,67,220,54,225,57,171,24,124,70,73,207,136,32,100,115,23,167,117,37,99,81,114,131,106,217,168,209,98,66,216,135,44,97,12,59,100,210,53,51,112,240,4,158,155,233,128,217,39,245,149,61,165,198,70,195,
    25,173,236,113,239,235,9,221,183,168,132,16,46,63,10,161,227,59,170,109,198,13,62,70,79,54,109,8,144,134,248,70,39,255,168,246,229,188,193,72,158,196,145,84,119,130,35,155,127,134,12,180,176,245,216,144,
    2,228,37,49,152,156,90,138,141,243,132,227,217,248,205,196,8,50,48,175,212,239,85,221,89,9,111,34,21,138,215,15,110,63,77,44,133,42,209,248,136,90,4,46,16,164,128,71,255,193,210,216,161,202,195,188,164,
    178,113,149,113,30,136,234,235,0,255,62,214,196,28,213,232,184,70,26,48,136,150,209,155,144,126,29,197,187,56,105,166,177,159,32,163,231,70,167,201,111,143,73,162,109,2,73,165,199,184,207,146,221,197,
    80,237,150,191,218,231,249,142,132,112,97,127,222,31,110,217,203,164,48,240,26,41,19,22,20,129,6,165,178,119,151,98,219,11,17,88,19,83,105,83,71,72,177,237,196,32,54,194,199,61,104,187,69,52,161,5,233,
    91,173,232,79,2,213,56,182,149,62,213,252,17,219,142,7,98,237,98,6,222,99,7,99,67,76,85,167,82,45,198,208,9,252,150,16,215,18,71,107,104,224,131,78,117,185,43,54,156,239,36,105,232,17,102,162,120,52,200,
    150,77,110,106,109,108,41,158,213,121,89,93,137,216,245,228,31,177,135,107,70,156,199,26,113,180,144,144,22,135,115,108,136,148,146,181,165,48,2,99,81,42,91,198,248,245,106,204,73,162,215,174,225,234,
    89,80,37,235,81,177,225,171,184,115,216,108,98,154,212,240,28,193,107,75,149,228,120,30,190,83,66,80,127,176,234,238,16,46,72,117,135,149,199,221,66,40,33,65,71,201,52,121,28,172,214,199,214,170,141,246,
    141,81,5,105,212,147,104,77,12,95,166,169,75,152,13,115,26,104,174,124,121,109,49,127,108,149,72,72,204,221,91,15,31,248,72,189,15,233,124,0,101,164,105,7,72,189,143,251,127,28,160,12,237,21,194,123,56,
    225,221,117,126,74,247,196,168,35,78,42,213,194,100,123,60,75,82,32,59,184,75,86,76,138,98,195,21,194,21,40,132,43,132,43,80,8,87,8,191,35,120,55,211,13,42,87,87,188,184,240,230,47,139,240,183,167,48,
    88,249,119,110,251,110,250,241,203,18,233,244,221,0,66,248,47,76,194,121,178,41,190,108,246,193,88,143,27,138,189,176,23,225,52,63,154,98,86,146,63,246,111,173,195,228,71,221,240,243,37,133,76,187,17,
    222,119,50,247,249,126,138,150,16,14,180,150,93,66,59,60,72,97,58,227,161,44,216,89,247,154,171,236,67,102,1,194,83,114,1,12,38,135,3,121,101,194,55,30,195,250,160,233,164,121,91,205,199,194,143,212,135,
    98,162,111,189,244,71,54,35,0,172,249,209,105,106,141,174,47,69,145,38,144,205,8,231,177,113,22,131,205,7,132,223,7,12,11,196,133,147,205,50,247,22,130,211,123,170,240,232,124,235,77,171,237,132,107,249,
    135,189,1,240,38,36,29,94,15,76,78,5,18,50,251,97,223,210,31,101,78,224,201,246,194,138,85,141,120,59,141,110,215,179,143,176,132,10,138,31,129,249,97,93,82,59,222,220,218,136,230,83,213,70,199,73,145,
    115,140,52,231,4,96,90,180,139,137,134,237,154,199,125,46,207,245,5,125,184,109,9,205,90,39,154,145,39,34,146,195,246,12,111,236,78,188,137,255,156,170,97,235,193,103,102,10,16,200,182,155,132,39,63,193,
    125,238,56,225,195,18,21,71,31,242,18,194,179,19,207,18,194,229,137,153,147,22,137,125,43,233,52,9,53,210,183,11,55,85,7,7,248,163,3,185,250,121,32,116,61,2,126,217,128,212,20,53,247,80,26,66,250,136,
    49,130,198,115,41,35,173,236,196,16,233,181,23,231,112,159,239,76,229,30,211,25,63,54,91,76,202,222,197,106,81,24,233,127,184,6,201,121,129,171,87,100,194,64,20,103,209,71,103,237,88,69,238,233,68,6,131,
    145,47,2,15,62,253,160,201,51,77,225,88,59,17,206,79,214,89,102,32,64,141,56,112,57,254,71,27,252,76,85,155,128,35,91,64,44,249,43,20,93,124,90,26,33,132,149,124,233,19,241,63,146,198,204,78,2,52,90,243,
    15,36,211,127,203,117,224,203,255,214,23,59,158,225,174,163,171,231,136,115,163,179,116,112,205,228,28,58,85,98,151,126,187,73,148,65,66,215,36,160,248,62,235,170,228,248,17,137,164,49,95,100,208,112,
    232,156,73,195,218,66,182,205,132,23,239,229,37,32,74,66,206,231,37,228,230,34,169,106,59,89,213,121,78,251,53,33,252,43,152,216,240,37,179,96,152,109,101,45,86,28,34,132,19,117,255,243,204,43,216,161,
    111,151,109,127,224,62,115,139,180,166,39,232,132,121,41,252,36,33,99,60,49,190,21,186,70,193,232,171,188,84,247,166,211,124,88,111,175,204,77,148,105,45,187,44,219,121,118,6,154,93,54,48,94,216,159,153,
    224,5,191,94,68,99,118,54,74,210,131,228,161,70,24,105,172,203,104,196,37,210,79,132,19,2,215,233,93,86,57,109,163,41,70,20,116,110,74,144,153,153,82,39,11,218,89,115,114,79,189,20,94,77,41,104,22,190,
    49,202,46,253,204,185,138,153,33,248,100,105,165,77,149,224,201,22,102,196,114,82,87,24,6,201,220,68,115,56,126,152,116,158,196,115,154,182,52,4,103,94,43,230,200,78,165,201,234,210,122,204,213,247,29,
    234,97,3,100,39,2,25,238,85,119,15,146,53,141,177,60,154,251,92,155,27,133,55,167,202,207,62,162,61,57,157,2,135,165,93,171,176,241,244,227,100,61,217,174,46,150,85,58,40,142,235,60,139,119,22,99,115,
    9,247,221,129,117,87,76,236,54,63,111,165,249,228,85,139,229,253,0,55,12,65,171,212,215,106,112,71,176,111,203,221,34,156,193,187,122,221,54,71,182,160,218,230,59,207,206,192,123,45,20,59,151,114,29,49,
    69,222,49,103,226,165,88,62,151,239,60,55,177,125,67,48,113,89,27,101,237,182,165,153,99,103,247,112,102,115,168,62,145,150,71,73,46,131,168,233,250,49,133,174,209,254,132,23,166,114,159,222,99,7,176,
    126,181,57,124,249,57,216,197,58,38,249,185,88,61,211,137,34,99,38,144,72,220,184,7,84,210,85,28,110,144,177,145,15,233,187,142,174,63,11,213,122,203,170,190,226,32,33,92,111,159,119,158,166,179,69,5,
    98,146,199,9,199,249,168,76,87,138,224,167,25,71,205,35,30,215,124,198,48,181,206,24,55,116,205,119,71,194,43,93,233,130,33,106,76,125,209,114,235,134,78,83,35,37,205,25,46,26,206,213,122,99,171,63,249,
    239,8,113,246,37,197,168,37,100,80,49,163,23,245,142,217,125,186,166,82,108,30,131,233,196,14,251,244,239,133,81,11,124,145,177,137,51,47,223,31,102,240,218,95,60,17,18,194,45,171,65,203,84,185,202,84,
    91,165,54,92,103,137,81,54,198,239,86,19,127,123,134,143,217,122,143,138,18,76,197,97,210,168,87,74,25,172,88,227,140,239,74,157,224,224,237,128,151,54,120,33,241,153,70,137,102,152,187,183,59,66,248,
    236,149,116,24,214,6,115,249,181,98,136,23,69,161,185,186,114,24,75,36,105,44,167,235,134,239,134,16,59,92,102,216,189,41,249,62,39,174,3,252,26,38,92,153,166,229,94,211,199,119,248,197,220,196,224,22,
    125,177,84,119,169,249,123,48,130,92,243,160,248,27,83,65,51,119,111,118,139,22,222,79,104,40,99,88,47,133,181,195,153,229,247,117,93,187,53,225,124,38,5,143,191,150,244,39,255,107,20,194,237,5,154,73,
    241,89,174,55,217,114,198,244,169,63,223,247,100,247,0,147,162,38,68,219,111,84,168,16,222,3,160,16,174,16,174,16,174,224,78,18,78,231,135,211,41,203,10,21,10,122,180,69,81,230,228,43,232,105,160,241,
    71,165,203,84,160,248,132,10,20,40,2,174,64,129,34,224,10,20,40,2,174,64,129,34,224,10,20,220,191,2,174,99,240,19,227,37,185,184,143,71,207,92,62,53,255,53,6,19,94,229,247,252,80,208,212,75,201,244,235,
    201,2,174,171,97,16,30,4,118,222,188,24,81,75,35,112,54,163,178,199,17,236,226,38,165,219,197,198,28,5,5,221,73,192,137,229,158,23,196,189,91,134,190,218,168,85,148,5,84,156,89,138,121,35,205,175,157,
    172,64,193,125,46,224,12,222,28,39,172,81,218,122,201,11,7,235,93,113,99,201,117,204,215,39,82,210,181,170,71,15,25,36,202,67,180,164,44,14,104,105,227,220,26,149,179,35,122,169,68,169,12,90,6,103,190,
    119,70,69,181,59,218,104,138,138,171,35,130,251,58,97,72,68,139,81,54,107,231,104,32,61,206,185,98,119,212,52,59,179,251,26,45,41,206,211,21,81,143,1,3,3,152,219,98,164,141,40,124,91,155,187,252,61,88,
    97,44,90,100,206,21,151,105,11,84,206,174,164,12,109,167,199,181,212,51,248,254,180,59,42,234,157,225,202,102,16,59,195,63,184,3,161,225,142,232,103,85,30,142,22,45,90,103,147,107,234,154,24,124,253,47,
    55,52,104,92,225,233,239,132,200,104,7,4,187,180,116,31,1,207,75,2,214,139,242,49,23,239,13,193,20,223,114,96,63,144,227,0,118,225,119,138,229,35,110,96,72,83,231,43,82,20,190,217,36,202,106,11,193,55,
    149,85,216,21,10,67,146,43,151,48,32,159,155,63,58,105,16,190,74,169,150,77,105,164,211,158,231,121,10,138,168,111,86,139,117,25,245,194,32,228,101,94,53,251,190,56,115,10,255,222,179,192,202,3,66,217,
    115,50,34,176,207,138,172,63,186,160,4,183,198,1,119,46,93,121,186,188,160,26,222,218,38,76,117,165,47,233,235,138,96,120,144,241,129,187,44,239,69,123,25,140,152,111,61,31,52,35,251,235,172,107,134,28,
    45,73,187,165,182,145,118,107,51,140,73,246,231,53,96,86,28,255,171,144,96,65,215,123,223,16,211,77,4,156,230,186,78,78,149,250,219,31,206,213,39,39,168,212,248,159,75,12,235,178,112,168,197,228,71,195,
    113,254,146,35,66,173,94,175,183,10,99,66,185,173,105,73,3,176,243,141,122,201,171,236,235,75,25,76,139,132,97,225,229,127,109,61,143,167,122,69,224,232,38,169,48,93,203,103,216,245,17,4,120,33,57,215,
    23,171,167,214,73,45,63,233,33,182,61,15,172,213,231,3,127,187,243,60,124,118,246,69,9,177,158,17,176,54,251,68,141,21,251,137,53,84,9,74,153,157,88,138,152,24,203,47,137,161,10,56,103,178,248,155,96,
    252,243,200,13,78,185,92,212,248,66,163,179,152,79,67,173,102,99,81,3,130,70,26,43,189,15,98,61,164,215,165,175,246,20,218,133,23,222,48,228,190,171,193,0,73,86,39,131,131,233,192,84,253,50,9,37,187,47,
    224,1,194,205,230,252,193,248,147,217,215,96,120,177,237,198,11,183,239,148,135,240,197,59,63,99,144,111,13,106,43,253,16,56,164,174,123,184,40,154,82,105,34,183,91,255,48,28,206,144,18,233,218,95,141,
    179,123,24,67,50,57,93,15,34,122,65,20,174,100,85,216,144,56,30,142,10,221,117,162,20,166,137,140,126,17,106,124,163,33,74,230,10,67,182,231,137,99,237,184,185,201,65,100,117,25,252,64,6,7,235,146,220,
    113,181,90,135,203,223,56,99,214,199,129,88,30,35,35,108,68,144,214,100,1,195,251,139,35,36,21,248,228,56,181,58,182,44,111,160,198,135,215,24,228,7,9,239,188,92,254,104,51,70,107,60,36,43,182,136,5,233,
    157,103,33,121,63,11,77,138,28,238,34,92,211,217,69,5,103,11,189,198,71,9,45,88,244,177,132,29,28,172,241,194,148,128,43,38,193,128,39,194,141,142,171,247,32,189,110,173,236,125,60,189,146,156,179,146,
    193,92,149,208,251,189,60,254,42,194,43,3,48,59,68,238,156,70,131,130,22,105,58,200,61,92,55,148,213,219,87,211,61,124,112,106,109,126,19,41,37,41,183,168,93,214,50,211,44,212,3,95,49,134,53,51,106,119,
    23,35,97,180,117,93,54,197,182,19,157,88,124,210,234,226,37,1,218,155,84,38,141,52,113,17,48,145,179,115,224,18,252,171,89,95,247,70,131,154,248,187,206,68,120,218,208,166,229,210,109,85,100,251,106,235,
    237,115,68,223,121,112,252,152,184,231,184,138,223,76,162,46,71,139,137,203,115,49,71,186,184,24,125,161,209,146,161,21,86,27,26,183,72,99,171,29,137,227,41,244,252,235,38,199,127,127,136,123,121,45,143,
    39,95,235,67,132,251,114,167,10,187,157,220,75,246,4,65,136,119,236,10,193,236,77,230,207,216,152,239,69,132,187,202,110,50,104,71,1,103,176,102,168,212,218,4,45,245,131,87,73,57,254,169,117,147,149,192,
    192,57,100,208,246,190,240,13,237,178,135,71,15,146,93,253,199,164,1,181,157,88,122,34,179,163,127,77,124,253,175,44,31,246,67,46,131,132,233,210,198,181,53,187,216,86,208,149,140,232,27,143,249,49,197,
    205,47,207,33,254,205,8,28,89,39,40,55,93,189,35,94,212,19,62,16,31,137,172,133,149,86,181,195,71,9,48,178,218,1,200,171,113,199,164,0,243,202,209,108,36,243,255,120,229,12,84,175,216,126,111,157,13,119,
    221,92,58,236,202,173,157,4,92,139,189,132,212,237,70,10,127,45,243,34,158,200,228,7,128,214,97,13,25,116,14,173,239,218,98,20,54,245,54,68,128,126,229,15,163,215,179,5,99,79,161,51,158,30,221,4,111,153,
    8,195,137,52,6,35,95,186,51,215,167,175,46,74,255,74,176,208,71,215,151,226,173,9,188,114,51,216,60,67,172,116,15,225,223,251,175,116,234,190,89,182,218,182,41,237,162,140,1,72,158,81,134,102,173,154,
    95,126,173,115,41,104,83,193,211,247,122,207,11,19,158,200,104,195,124,145,197,48,140,242,173,140,50,72,7,123,181,152,22,29,102,227,160,211,118,20,236,144,10,247,156,140,135,205,44,2,40,88,198,147,199,
    239,100,13,200,160,243,32,131,111,29,4,31,118,205,136,22,60,173,115,66,107,6,191,234,11,55,56,203,174,84,177,11,75,217,106,181,143,212,187,18,67,97,157,75,19,49,70,186,95,82,238,140,62,129,212,201,179,
    213,226,182,244,44,1,175,43,228,22,244,19,16,130,175,11,126,178,41,132,70,187,108,238,205,117,252,160,243,50,25,116,70,144,65,103,149,205,171,149,89,11,23,35,175,105,64,68,187,69,1,74,30,43,22,58,14,101,
    231,8,157,49,183,227,144,171,177,167,130,193,191,251,242,131,206,42,68,24,13,21,150,229,60,72,6,109,21,118,183,218,254,177,192,134,199,129,55,190,227,35,79,37,120,110,16,255,78,78,243,200,93,197,96,122,
    58,48,112,92,111,60,51,221,13,43,86,48,146,136,86,183,22,112,58,242,126,210,72,243,255,90,230,138,33,104,176,185,172,241,164,203,78,253,187,16,59,175,221,93,138,248,199,163,240,233,74,251,172,125,18,77,
    253,91,209,106,113,201,227,207,225,127,167,132,34,115,155,6,143,4,50,104,35,3,230,111,201,192,235,245,68,99,255,92,192,222,205,55,17,63,206,11,191,234,221,12,23,15,93,151,148,209,33,148,12,58,15,11,107,
    189,136,241,200,170,40,236,120,166,220,130,91,40,237,57,89,187,189,160,47,22,245,43,71,90,186,101,199,162,85,227,136,177,11,221,17,19,80,111,232,81,182,156,34,3,108,162,96,239,233,191,249,203,162,18,242,
    71,195,129,253,240,250,170,159,241,144,154,240,162,106,71,101,137,6,217,68,168,183,137,214,151,188,80,112,29,215,19,34,137,112,215,227,94,226,206,9,184,142,193,239,131,164,141,191,44,39,18,11,194,186,
    250,232,93,141,117,5,12,142,57,8,161,189,207,86,21,99,211,99,131,240,250,184,43,208,26,69,48,90,245,209,13,139,62,169,104,41,177,91,55,164,235,123,208,80,165,78,195,224,101,226,235,110,209,251,193,231,
    15,85,98,194,33,211,114,104,44,56,47,171,142,117,153,242,83,197,61,77,53,38,247,231,163,3,220,26,101,210,122,182,19,49,116,238,188,7,139,35,61,216,171,226,16,36,183,130,235,209,237,150,162,13,109,40,62,
    99,250,109,109,86,5,158,207,178,206,85,216,56,234,1,145,128,115,109,240,46,209,209,63,19,229,222,248,172,192,203,23,105,229,228,207,124,57,179,146,194,240,94,202,79,240,135,105,219,119,165,221,238,15,
    1,39,221,235,167,38,6,171,226,182,203,60,108,82,38,23,81,25,255,10,17,72,201,168,190,170,83,133,217,120,10,162,87,90,203,199,183,55,19,129,222,220,105,197,106,69,61,13,255,202,82,99,84,201,212,83,163,
    255,235,60,242,241,143,207,165,254,243,225,211,173,157,188,176,158,90,92,34,132,183,69,184,60,135,42,15,107,121,49,229,199,164,103,182,185,221,238,219,48,161,130,46,117,132,196,90,62,229,41,244,90,236,
    160,178,204,19,99,61,174,41,228,40,2,222,221,164,153,193,150,255,162,110,157,31,116,117,245,248,155,137,59,20,130,34,77,27,134,187,40,194,173,8,120,247,148,111,228,236,166,2,46,29,136,121,143,235,131,
    253,31,59,98,82,72,189,66,146,34,224,221,23,212,183,253,78,54,208,114,83,33,71,17,112,5,10,20,1,87,160,8,184,2,5,138,128,43,80,208,93,5,124,212,17,242,153,44,247,38,32,5,10,186,59,254,31,68,134,124,163,
    151,108,187,225,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* LowhighpassAudioProcessorEditor::analyzer_on_png = (const char*) resource_LowhighpassAudioProcessorEditor_analyzer_on_png;
const int LowhighpassAudioProcessorEditor::analyzer_on_pngSize = 5590;


//[EndFile] You can add extra defines here...
//[/EndFile]
