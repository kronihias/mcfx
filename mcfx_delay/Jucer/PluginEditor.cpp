/*
  ==============================================================================

  This is an automatically generated GUI class created by the Introjucer!

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Created with Introjucer version: 4.1.0

  ------------------------------------------------------------------------------

  The Introjucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright (c) 2015 - ROLI Ltd.

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
    sld_del_ms->setRange (0, 500, 0.1);
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
    sld_del_smpl->setRange (0, 22050, 1);
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


    //[Constructor] You can add your own custom stuff here..
    //[/Constructor]
}

Mcfx_delayAudioProcessorEditor::~Mcfx_delayAudioProcessorEditor()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    //[/Destructor_pre]

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

    g.setColour (Colours::white);
    g.setFont (Font (10.00f, Font::plain));
    g.drawText (TRANS("v0.4.2"),
                137, 108, 50, 10,
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
    //[UsersliderValueChanged_Pre]
    //[/UsersliderValueChanged_Pre]

    if (sliderThatWasMoved == sld_del_ms)
    {
        //[UserSliderCode_sld_del_ms] -- add your slider handling code here..
        //[/UserSliderCode_sld_del_ms]
    }
    else if (sliderThatWasMoved == sld_del_smpl)
    {
        //[UserSliderCode_sld_del_smpl] -- add your slider handling code here..
        //[/UserSliderCode_sld_del_smpl]
    }

    //[UsersliderValueChanged_Post]
    //[/UsersliderValueChanged_Post]
}



//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...
//[/MiscUserCode]


//==============================================================================
#if 0
/*  -- Introjucer information section --

    This is where the Introjucer stores the metadata that describe this GUI layout, so
    make changes in here at your peril!

BEGIN_JUCER_METADATA

<JUCER_COMPONENT documentType="Component" className="Mcfx_delayAudioProcessorEditor"
                 componentName="" parentClasses="public AudioProcessorEditor"
                 constructorParams="Mcfx_delayAudioProcessor* ownerFilter" variableInitialisers="AudioProcessorEditor (ownerFilter)"
                 snapPixels="8" snapActive="0" snapShown="1" overlayOpacity="0.330"
                 fixedSize="1" initialWidth="190" initialHeight="120">
  <BACKGROUND backgroundColour="ff1a1a1a">
    <TEXT pos="137 108 50 10" fill="solid: ffffffff" hasStroke="0" text="v0.4.2"
          fontname="Default font" fontsize="10" bold="0" italic="0" justification="18"/>
  </BACKGROUND>
  <LABEL name="new label" id="b45e45d811b90270" memberName="lbl_g" virtualName=""
         explicitFocusOrder="0" pos="0 0 115 16" textCol="ffffff00" edTextCol="ff000000"
         edBkgCol="0" labelText="mcfx_delay" editableSingleClick="0" editableDoubleClick="0"
         focusDiscardsChanges="0" fontname="Default font" fontsize="15"
         bold="0" italic="0" justification="33"/>
  <SLIDER name="new slider" id="d5c1a3078147bf28" memberName="sld_del_ms"
          virtualName="" explicitFocusOrder="0" pos="22 49 93 24" tooltip="delay in milliseconds"
          rotarysliderfill="ffff0000" min="0" max="500" int="0.10000000000000000555"
          style="Rotary" textBoxPos="TextBoxLeft" textBoxEditable="1" textBoxWidth="60"
          textBoxHeight="18" skewFactor="1"/>
  <LABEL name="new label" id="a7997dcae0239c4c" memberName="label3" virtualName=""
         explicitFocusOrder="0" pos="11 24 109 16" textCol="ffffffff"
         edTextCol="ff000000" edBkgCol="0" labelText="delay all channels"
         editableSingleClick="0" editableDoubleClick="0" focusDiscardsChanges="0"
         fontname="Default font" fontsize="13" bold="0" italic="0" justification="34"/>
  <LABEL name="new label" id="8b44532ef9d588ae" memberName="label2" virtualName=""
         explicitFocusOrder="0" pos="110 53 35 16" textCol="ffffffff"
         edTextCol="ff000000" edBkgCol="0" labelText="[ms]" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
         fontsize="13" bold="0" italic="0" justification="33"/>
  <SLIDER name="new slider" id="8dbd8843ef4562aa" memberName="sld_del_smpl"
          virtualName="" explicitFocusOrder="0" pos="22 79 93 24" tooltip="delay in samples"
          rotarysliderfill="ffff0000" min="0" max="22050" int="1" style="Rotary"
          textBoxPos="TextBoxLeft" textBoxEditable="1" textBoxWidth="60"
          textBoxHeight="18" skewFactor="1"/>
  <LABEL name="new label" id="89f76018a1197423" memberName="label4" virtualName=""
         explicitFocusOrder="0" pos="112 82 68 16" textCol="ffffffff"
         edTextCol="ff000000" edBkgCol="0" labelText="[samples]" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
         fontsize="13" bold="0" italic="0" justification="33"/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif


//[EndFile] You can add extra defines here...
//[/EndFile]
