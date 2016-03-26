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
Mcfx_delayAudioProcessorEditor::Mcfx_delayAudioProcessorEditor (Mcfx_delayAudioProcessor* ownerFilter)
    : AudioProcessorEditor (ownerFilter)
{
    //[Constructor_pre] You can add your own custom stuff here..
    //[/Constructor_pre]
    tooltipWindow.setMillisecondsBeforeTipAppears (700); // tooltip delay
    
    
    addAndMakeVisible (lbl_g = new Label ("new label",
                                          TRANS("mcfx_delay")));
    lbl_g->setFont (Font (15.00f, Font::plain));
    lbl_g->setJustificationType (Justification::centredLeft);
    lbl_g->setEditable (false, false, false);
    lbl_g->setColour (Label::textColourId, Colours::yellow);
    lbl_g->setColour (TextEditor::textColourId, Colours::black);
    lbl_g->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    addAndMakeVisible (sld_del_ms = new Slider ("new slider"));
    sld_del_ms->setTooltip (TRANS("delay in milliseconds"));
    
    sld_del_ms->setRange (0, MAX_DELAYTIME_S*1000, 0.01);
    
    sld_del_ms->setSliderStyle (Slider::Rotary);
    sld_del_ms->setTextBoxStyle (Slider::TextBoxLeft, false, 60, 18);
    sld_del_ms->setColour (Slider::rotarySliderFillColourId, Colours::red);
    sld_del_ms->addListener (this);

    addAndMakeVisible (label3 = new Label ("new label",
                                           TRANS("delay all channels")));
    label3->setFont (Font (13.00f, Font::plain));
    label3->setJustificationType (Justification::centredRight);
    label3->setEditable (false, false, false);
    label3->setColour (Label::textColourId, Colours::white);
    label3->setColour (TextEditor::textColourId, Colours::black);
    label3->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    addAndMakeVisible (label2 = new Label ("new label",
                                           TRANS("[ms]")));
    label2->setFont (Font (13.00f, Font::plain));
    label2->setJustificationType (Justification::centredLeft);
    label2->setEditable (false, false, false);
    label2->setColour (Label::textColourId, Colours::white);
    label2->setColour (TextEditor::textColourId, Colours::black);
    label2->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    addAndMakeVisible (sld_del_smpl = new Slider ("new slider"));
    sld_del_smpl->setTooltip (TRANS("delay in samples"));
    
    sld_del_smpl->setRange (0, ownerFilter->_samplerate*MAX_DELAYTIME_S, 1);
    
    sld_del_smpl->setSliderStyle (Slider::Rotary);
    sld_del_smpl->setTextBoxStyle (Slider::TextBoxLeft, false, 60, 18);
    sld_del_smpl->setColour (Slider::rotarySliderFillColourId, Colours::red);
    sld_del_smpl->addListener (this);

    addAndMakeVisible (label4 = new Label ("new label",
                                           TRANS("[samples]")));
    label4->setFont (Font (13.00f, Font::plain));
    label4->setJustificationType (Justification::centredLeft);
    label4->setEditable (false, false, false);
    label4->setColour (Label::textColourId, Colours::white);
    label4->setColour (TextEditor::textColourId, Colours::black);
    label4->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    //[UserPreSize]
    //[/UserPreSize]

    setSize (190, 120);
    // setSize (300, 400);

    ownerFilter->addChangeListener(this);
    ownerFilter->sendChangeMessage(); // get status from dsp
    
    //[Constructor] You can add your own custom stuff here..
    //[/Constructor]
}

Mcfx_delayAudioProcessorEditor::~Mcfx_delayAudioProcessorEditor()
{
    Mcfx_delayAudioProcessor* ourProcessor = getProcessor();
    
    // remove me as listener for changes
    ourProcessor->removeChangeListener(this);

    lbl_g = nullptr;
    sld_del_ms = nullptr;
    label3 = nullptr;
    label2 = nullptr;
    sld_del_smpl = nullptr;
    label4 = nullptr;


    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void Mcfx_delayAudioProcessorEditor::paint (Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    g.fillAll (Colour (0xff1a1a1a));
    g.setGradientFill (ColourGradient (Colour (0xff4e4e4e),
                                       (float) (proportionOfWidth (0.6400f)), (float) (proportionOfHeight (0.6933f)),
                                       Colours::black,
                                       (float) (proportionOfWidth (0.1143f)), (float) (proportionOfHeight (0.0800f)),
                                       true));
    g.fillRect (0, 0, this->getBounds().getWidth(), this->getBounds().getHeight());
    
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

void Mcfx_delayAudioProcessorEditor::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    lbl_g->setBounds (0, 0, 115, 16);
    sld_del_ms->setBounds (22, 49, 93, 24);
    label3->setBounds (11, 24, 109, 16);
    label2->setBounds (110, 53, 35, 16);
    sld_del_smpl->setBounds (22, 79, 93, 24);
    label4->setBounds (112, 82, 68, 16);
    //[UserResized] Add your own custom resize handling here..
    //[/UserResized]
}

void Mcfx_delayAudioProcessorEditor::sliderValueChanged (Slider* sliderThatWasMoved)
{
    Mcfx_delayAudioProcessor* ourProcessor = getProcessor();

    if (sliderThatWasMoved == sld_del_ms)
    {
        ourProcessor->setParameter(0, sld_del_ms->getValue()/1000/MAX_DELAYTIME_S);
    }
    else if (sliderThatWasMoved == sld_del_smpl)
    {
        ourProcessor->setParameter(0, (double)sld_del_smpl->getValue()/ourProcessor->_samplerate/(double)MAX_DELAYTIME_S);
    }

    //[UsersliderValueChanged_Post]
    //[/UsersliderValueChanged_Post]
}

void Mcfx_delayAudioProcessorEditor::changeListenerCallback (ChangeBroadcaster *source)
{
    Mcfx_delayAudioProcessor* ourProcessor = getProcessor();
    sld_del_ms->setValue(ourProcessor->getDelayInMs(), dontSendNotification);
    sld_del_smpl->setValue(ourProcessor->getDelayInSmpls(), dontSendNotification);
}
