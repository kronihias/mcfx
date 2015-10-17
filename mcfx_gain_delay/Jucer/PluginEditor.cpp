/*
  ==============================================================================

  This is an automatically generated GUI class created by the Introjucer!

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Created with Introjucer version: 3.2.0

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
Mcfx_gain_delayAudioProcessorEditor::Mcfx_gain_delayAudioProcessorEditor (Mcfx_gain_delayAudioProcessor* ownerFilter)
    : AudioProcessorEditor (ownerFilter)
{
    //[Constructor_pre] You can add your own custom stuff here..
    //[/Constructor_pre]

    addAndMakeVisible (lbl_gd = new Label ("new label",
                                           TRANS("mcfx_gain_delay")));
    lbl_gd->setFont (Font (15.00f, Font::plain));
    lbl_gd->setJustificationType (Justification::centredLeft);
    lbl_gd->setEditable (false, false, false);
    lbl_gd->setColour (Label::textColourId, Colours::yellow);
    lbl_gd->setColour (TextEditor::textColourId, Colours::black);
    lbl_gd->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    addAndMakeVisible (sld_del = new Slider ("new slider"));
    sld_del->setTooltip (TRANS("delay ch 1"));
    sld_del->setRange (0, 500, 0.1);
    sld_del->setSliderStyle (Slider::Rotary);
    sld_del->setTextBoxStyle (Slider::TextBoxLeft, false, 50, 18);
    sld_del->setColour (Slider::rotarySliderFillColourId, Colours::red);
    sld_del->addListener (this);

    addAndMakeVisible (sld_gain = new Slider ("new slider"));
    sld_gain->setTooltip (TRANS("gain ch 1"));
    sld_gain->setRange (-18, 18, 0.01);
    sld_gain->setSliderStyle (Slider::Rotary);
    sld_gain->setTextBoxStyle (Slider::TextBoxLeft, false, 50, 18);
    sld_gain->setColour (Slider::thumbColourId, Colour (0xff5a5a90));
    sld_gain->setColour (Slider::trackColourId, Colours::yellow);
    sld_gain->setColour (Slider::rotarySliderFillColourId, Colours::yellow);
    sld_gain->setColour (Slider::rotarySliderOutlineColourId, Colours::yellow);
    sld_gain->addListener (this);

    addAndMakeVisible (label2 = new Label ("new label",
                                           TRANS("gain [dB]\n")));
    label2->setFont (Font (13.00f, Font::plain));
    label2->setJustificationType (Justification::centredRight);
    label2->setEditable (false, false, false);
    label2->setColour (Label::textColourId, Colours::white);
    label2->setColour (TextEditor::textColourId, Colours::black);
    label2->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    addAndMakeVisible (label3 = new Label ("new label",
                                           TRANS("delay [ms]\n")));
    label3->setFont (Font (13.00f, Font::plain));
    label3->setJustificationType (Justification::centredRight);
    label3->setEditable (false, false, false);
    label3->setColour (Label::textColourId, Colours::white);
    label3->setColour (TextEditor::textColourId, Colours::black);
    label3->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    addAndMakeVisible (lbl_ch = new Label ("new label",
                                           TRANS("1")));
    lbl_ch->setFont (Font (15.00f, Font::plain));
    lbl_ch->setJustificationType (Justification::centredRight);
    lbl_ch->setEditable (false, false, false);
    lbl_ch->setColour (Label::textColourId, Colours::white);
    lbl_ch->setColour (TextEditor::textColourId, Colours::black);
    lbl_ch->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    addAndMakeVisible (btn_paste_gain = new ImageButton ("new button"));
    btn_paste_gain->setTooltip (TRANS("paste gain values from clipboard"));
    btn_paste_gain->addListener (this);

    btn_paste_gain->setImages (false, true, true,
                               ImageCache::getFromMemory (clipboard35_grey_png, clipboard35_grey_pngSize), 1.000f, Colour (0x00000000),
                               ImageCache::getFromMemory (clipboard35_png, clipboard35_pngSize), 1.000f, Colour (0x00000000),
                               Image(), 1.000f, Colour (0x00000000));
    addAndMakeVisible (btn_paste_gain2 = new ImageButton ("new button"));
    btn_paste_gain2->setTooltip (TRANS("paste delay values from clipboard"));
    btn_paste_gain2->addListener (this);

    btn_paste_gain2->setImages (false, true, true,
                                ImageCache::getFromMemory (clipboard35_grey_png, clipboard35_grey_pngSize), 1.000f, Colour (0x00000000),
                                ImageCache::getFromMemory (clipboard35_png, clipboard35_pngSize), 1.000f, Colour (0x00000000),
                                Image(), 1.000f, Colour (0x00000000));
    addAndMakeVisible (btn_phase = new ImageButton ("new button"));
    btn_phase->setTooltip (TRANS("phase normal"));
    btn_phase->addListener (this);

    btn_phase->setImages (false, true, true,
                          ImageCache::getFromMemory (phase_symbol_png, phase_symbol_pngSize), 0.986f, Colour (0x00000000),
                          ImageCache::getFromMemory (phase_symbol_over_png, phase_symbol_over_pngSize), 1.000f, Colour (0x00000000),
                          ImageCache::getFromMemory (phase_symbol_inv_png, phase_symbol_inv_pngSize), 1.000f, Colour (0x00000000));
    addAndMakeVisible (btn_solo = new ImageButton ("new button"));
    btn_solo->setTooltip (TRANS("Not soloed"));
    btn_solo->addListener (this);

    btn_solo->setImages (false, true, true,
                         ImageCache::getFromMemory (solo_symbol_png, solo_symbol_pngSize), 1.000f, Colour (0x00000000),
                         ImageCache::getFromMemory (solo_symbol_over_png, solo_symbol_over_pngSize), 1.000f, Colour (0x00000000),
                         ImageCache::getFromMemory (solo_symbol_act_png, solo_symbol_act_pngSize), 1.000f, Colour (0x00000000));
    addAndMakeVisible (btn_mute = new ImageButton ("new button"));
    btn_mute->setTooltip (TRANS("Not muted"));
    btn_mute->addListener (this);

    btn_mute->setImages (false, true, true,
                         ImageCache::getFromMemory (mute_symbol_png, mute_symbol_pngSize), 1.000f, Colour (0x00000000),
                         ImageCache::getFromMemory (mute_symbol_over_png, mute_symbol_over_pngSize), 1.000f, Colour (0x00000000),
                         ImageCache::getFromMemory (mute_symbol_act_png, mute_symbol_act_pngSize), 1.000f, Colour (0x00000000));
    addAndMakeVisible (btn_mute2 = new ImageButton ("new button"));
    btn_mute2->setTooltip (TRANS("Not muted"));
    btn_mute2->addListener (this);

    btn_mute2->setImages (false, true, true,
                          ImageCache::getFromMemory (mute_symbol_png, mute_symbol_pngSize), 1.000f, Colour (0x00000000),
                          ImageCache::getFromMemory (mute_symbol_over_png, mute_symbol_over_pngSize), 1.000f, Colour (0x00000000),
                          ImageCache::getFromMemory (mute_symbol_act_png, mute_symbol_act_pngSize), 1.000f, Colour (0x00000000));
    addAndMakeVisible (btn_solo2 = new ImageButton ("new button"));
    btn_solo2->setTooltip (TRANS("Not soloed"));
    btn_solo2->addListener (this);

    btn_solo2->setImages (false, true, true,
                          ImageCache::getFromMemory (solo_symbol_png, solo_symbol_pngSize), 1.000f, Colour (0x00000000),
                          ImageCache::getFromMemory (solo_symbol_over_png, solo_symbol_over_pngSize), 1.000f, Colour (0x00000000),
                          ImageCache::getFromMemory (solo_symbol_act_png, solo_symbol_act_pngSize), 1.000f, Colour (0x00000000));

    //[UserPreSize]
    //[/UserPreSize]

    setSize (260, 210);


    //[Constructor] You can add your own custom stuff here..
    //[/Constructor]
}

Mcfx_gain_delayAudioProcessorEditor::~Mcfx_gain_delayAudioProcessorEditor()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    //[/Destructor_pre]

    lbl_gd = nullptr;
    sld_del = nullptr;
    sld_gain = nullptr;
    label2 = nullptr;
    label3 = nullptr;
    lbl_ch = nullptr;
    btn_paste_gain = nullptr;
    btn_paste_gain2 = nullptr;
    btn_phase = nullptr;
    btn_solo = nullptr;
    btn_mute = nullptr;
    btn_mute2 = nullptr;
    btn_solo2 = nullptr;


    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void Mcfx_gain_delayAudioProcessorEditor::paint (Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    g.fillAll (Colour (0xff1a1a1a));

    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void Mcfx_gain_delayAudioProcessorEditor::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    lbl_gd->setBounds (0, 0, 115, 16);
    sld_del->setBounds (109, 45, 80, 24);
    sld_gain->setBounds (28, 45, 80, 24);
    label2->setBounds (37, 26, 66, 16);
    label3->setBounds (115, 26, 75, 16);
    lbl_ch->setBounds (-2, 48, 29, 16);
    btn_paste_gain->setBounds (29, 26, 16, 16);
    btn_paste_gain2->setBounds (109, 26, 16, 16);
    btn_phase->setBounds (189, 48, 19, 19);
    btn_solo->setBounds (235, 48, 19, 19);
    btn_mute->setBounds (212, 48, 19, 19);
    btn_mute2->setBounds (212, 22, 19, 19);
    btn_solo2->setBounds (237, 22, 19, 19);
    //[UserResized] Add your own custom resize handling here..
    //[/UserResized]
}

void Mcfx_gain_delayAudioProcessorEditor::sliderValueChanged (Slider* sliderThatWasMoved)
{
    //[UsersliderValueChanged_Pre]
    //[/UsersliderValueChanged_Pre]

    if (sliderThatWasMoved == sld_del)
    {
        //[UserSliderCode_sld_del] -- add your slider handling code here..
        //[/UserSliderCode_sld_del]
    }
    else if (sliderThatWasMoved == sld_gain)
    {
        //[UserSliderCode_sld_gain] -- add your slider handling code here..
        //[/UserSliderCode_sld_gain]
    }

    //[UsersliderValueChanged_Post]
    //[/UsersliderValueChanged_Post]
}

void Mcfx_gain_delayAudioProcessorEditor::buttonClicked (Button* buttonThatWasClicked)
{
    //[UserbuttonClicked_Pre]
    //[/UserbuttonClicked_Pre]

    if (buttonThatWasClicked == btn_paste_gain)
    {
        //[UserButtonCode_btn_paste_gain] -- add your button handler code here..
        //[/UserButtonCode_btn_paste_gain]
    }
    else if (buttonThatWasClicked == btn_paste_gain2)
    {
        //[UserButtonCode_btn_paste_gain2] -- add your button handler code here..
        //[/UserButtonCode_btn_paste_gain2]
    }
    else if (buttonThatWasClicked == btn_phase)
    {
        //[UserButtonCode_btn_phase] -- add your button handler code here..
        //[/UserButtonCode_btn_phase]
    }
    else if (buttonThatWasClicked == btn_solo)
    {
        //[UserButtonCode_btn_solo] -- add your button handler code here..
        //[/UserButtonCode_btn_solo]
    }
    else if (buttonThatWasClicked == btn_mute)
    {
        //[UserButtonCode_btn_mute] -- add your button handler code here..
        //[/UserButtonCode_btn_mute]
    }
    else if (buttonThatWasClicked == btn_mute2)
    {
        //[UserButtonCode_btn_mute2] -- add your button handler code here..
        //[/UserButtonCode_btn_mute2]
    }
    else if (buttonThatWasClicked == btn_solo2)
    {
        //[UserButtonCode_btn_solo2] -- add your button handler code here..
        //[/UserButtonCode_btn_solo2]
    }

    //[UserbuttonClicked_Post]
    //[/UserbuttonClicked_Post]
}



//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...
//[/MiscUserCode]


//==============================================================================
#if 0
/*  -- Introjucer information section --

    This is where the Introjucer stores the metadata that describe this GUI layout, so
    make changes in here at your peril!

BEGIN_JUCER_METADATA

<JUCER_COMPONENT documentType="Component" className="Mcfx_gain_delayAudioProcessorEditor"
                 componentName="" parentClasses="public AudioProcessorEditor"
                 constructorParams="Mcfx_gain_delayAudioProcessor* ownerFilter"
                 variableInitialisers="AudioProcessorEditor (ownerFilter)" snapPixels="8"
                 snapActive="0" snapShown="1" overlayOpacity="0.330" fixedSize="1"
                 initialWidth="260" initialHeight="210">
  <BACKGROUND backgroundColour="ff1a1a1a"/>
  <LABEL name="new label" id="b45e45d811b90270" memberName="lbl_gd" virtualName=""
         explicitFocusOrder="0" pos="0 0 115 16" textCol="ffffff00" edTextCol="ff000000"
         edBkgCol="0" labelText="mcfx_gain_delay" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
         fontsize="15" bold="0" italic="0" justification="33"/>
  <SLIDER name="new slider" id="d5c1a3078147bf28" memberName="sld_del"
          virtualName="" explicitFocusOrder="0" pos="109 45 80 24" tooltip="delay ch 1"
          rotarysliderfill="ffff0000" min="0" max="500" int="0.10000000000000000555"
          style="Rotary" textBoxPos="TextBoxLeft" textBoxEditable="1" textBoxWidth="50"
          textBoxHeight="18" skewFactor="1"/>
  <SLIDER name="new slider" id="edc665213f83e06b" memberName="sld_gain"
          virtualName="" explicitFocusOrder="0" pos="28 45 80 24" tooltip="gain ch 1"
          thumbcol="ff5a5a90" trackcol="ffffff00" rotarysliderfill="ffffff00"
          rotaryslideroutline="ffffff00" min="-18" max="18" int="0.010000000000000000208"
          style="Rotary" textBoxPos="TextBoxLeft" textBoxEditable="1" textBoxWidth="50"
          textBoxHeight="18" skewFactor="1"/>
  <LABEL name="new label" id="e30a8f204d299927" memberName="label2" virtualName=""
         explicitFocusOrder="0" pos="37 26 66 16" textCol="ffffffff" edTextCol="ff000000"
         edBkgCol="0" labelText="gain [dB]&#10;" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
         fontsize="13" bold="0" italic="0" justification="34"/>
  <LABEL name="new label" id="a7997dcae0239c4c" memberName="label3" virtualName=""
         explicitFocusOrder="0" pos="115 26 75 16" textCol="ffffffff"
         edTextCol="ff000000" edBkgCol="0" labelText="delay [ms]&#10;"
         editableSingleClick="0" editableDoubleClick="0" focusDiscardsChanges="0"
         fontname="Default font" fontsize="13" bold="0" italic="0" justification="34"/>
  <LABEL name="new label" id="5189e110ab7987d1" memberName="lbl_ch" virtualName=""
         explicitFocusOrder="0" pos="-2 48 29 16" textCol="ffffffff" edTextCol="ff000000"
         edBkgCol="0" labelText="1" editableSingleClick="0" editableDoubleClick="0"
         focusDiscardsChanges="0" fontname="Default font" fontsize="15"
         bold="0" italic="0" justification="34"/>
  <IMAGEBUTTON name="new button" id="fb278a9f97bfef46" memberName="btn_paste_gain"
               virtualName="" explicitFocusOrder="0" pos="29 26 16 16" tooltip="paste gain values from clipboard"
               buttonText="new button" connectedEdges="0" needsCallback="1"
               radioGroupId="0" keepProportions="1" resourceNormal="clipboard35_grey_png"
               opacityNormal="1" colourNormal="0" resourceOver="clipboard35_png"
               opacityOver="1" colourOver="0" resourceDown="" opacityDown="1"
               colourDown="0"/>
  <IMAGEBUTTON name="new button" id="548c7fabb48b0c32" memberName="btn_paste_gain2"
               virtualName="" explicitFocusOrder="0" pos="109 26 16 16" tooltip="paste delay values from clipboard"
               buttonText="new button" connectedEdges="0" needsCallback="1"
               radioGroupId="0" keepProportions="1" resourceNormal="clipboard35_grey_png"
               opacityNormal="1" colourNormal="0" resourceOver="clipboard35_png"
               opacityOver="1" colourOver="0" resourceDown="" opacityDown="1"
               colourDown="0"/>
  <IMAGEBUTTON name="new button" id="35f65f745d301744" memberName="btn_phase"
               virtualName="" explicitFocusOrder="0" pos="189 48 19 19" tooltip="phase normal"
               buttonText="new button" connectedEdges="0" needsCallback="1"
               radioGroupId="0" keepProportions="1" resourceNormal="phase_symbol_png"
               opacityNormal="0.98593747615814208984" colourNormal="0" resourceOver="phase_symbol_over_png"
               opacityOver="1" colourOver="0" resourceDown="phase_symbol_inv_png"
               opacityDown="1" colourDown="0"/>
  <IMAGEBUTTON name="new button" id="9c6bc8d4ac97807c" memberName="btn_solo"
               virtualName="" explicitFocusOrder="0" pos="235 48 19 19" tooltip="Not soloed"
               buttonText="new button" connectedEdges="0" needsCallback="1"
               radioGroupId="0" keepProportions="1" resourceNormal="solo_symbol_png"
               opacityNormal="1" colourNormal="0" resourceOver="solo_symbol_over_png"
               opacityOver="1" colourOver="0" resourceDown="solo_symbol_act_png"
               opacityDown="1" colourDown="0"/>
  <IMAGEBUTTON name="new button" id="a9a7f5e5ad1d236c" memberName="btn_mute"
               virtualName="" explicitFocusOrder="0" pos="212 48 19 19" tooltip="Not muted"
               buttonText="new button" connectedEdges="0" needsCallback="1"
               radioGroupId="0" keepProportions="1" resourceNormal="mute_symbol_png"
               opacityNormal="1" colourNormal="0" resourceOver="mute_symbol_over_png"
               opacityOver="1" colourOver="0" resourceDown="mute_symbol_act_png"
               opacityDown="1" colourDown="0"/>
  <IMAGEBUTTON name="new button" id="10915824ad71c0ad" memberName="btn_mute2"
               virtualName="" explicitFocusOrder="0" pos="212 22 19 19" tooltip="Not muted"
               buttonText="new button" connectedEdges="0" needsCallback="1"
               radioGroupId="0" keepProportions="1" resourceNormal="mute_symbol_png"
               opacityNormal="1" colourNormal="0" resourceOver="mute_symbol_over_png"
               opacityOver="1" colourOver="0" resourceDown="mute_symbol_act_png"
               opacityDown="1" colourDown="0"/>
  <IMAGEBUTTON name="new button" id="45dceba6c8fb603e" memberName="btn_solo2"
               virtualName="" explicitFocusOrder="0" pos="237 22 19 19" tooltip="Not soloed"
               buttonText="new button" connectedEdges="0" needsCallback="1"
               radioGroupId="0" keepProportions="1" resourceNormal="solo_symbol_png"
               opacityNormal="1" colourNormal="0" resourceOver="solo_symbol_over_png"
               opacityOver="1" colourOver="0" resourceDown="solo_symbol_act_png"
               opacityDown="1" colourDown="0"/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif

//==============================================================================
// Binary resources - be careful not to edit any of these sections!

// JUCER_RESOURCE: clipboard35_png, 346, "clipboard35.png"
static const unsigned char resource_Mcfx_gain_delayAudioProcessorEditor_clipboard35_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,24,0,0,0,24,8,6,0,0,0,224,119,61,248,0,0,0,6,98,75,71,
68,0,255,0,255,0,255,160,189,167,147,0,0,0,9,112,72,89,115,0,0,11,19,0,0,11,19,1,0,154,156,24,0,0,0,7,116,73,77,69,7,222,12,19,21,17,27,160,237,73,232,0,0,0,231,73,68,65,84,72,199,237,149,177,109,195,
48,16,69,31,13,3,201,10,238,60,130,17,164,55,55,72,157,105,36,47,16,100,160,20,78,237,37,228,5,82,164,82,247,210,144,128,45,196,22,9,51,69,0,191,134,194,81,159,159,184,35,121,112,5,117,165,30,60,231,160,
174,40,100,57,51,255,10,60,3,187,147,88,151,226,111,220,138,218,171,78,98,170,125,237,66,81,29,189,141,81,141,151,82,20,129,135,244,253,9,236,43,246,23,129,109,210,199,169,246,183,26,236,67,8,125,77,26,
147,65,113,145,59,181,163,17,75,218,18,167,7,160,181,193,118,154,174,69,133,120,23,42,200,119,103,193,31,115,55,184,27,252,3,131,124,147,199,6,189,195,107,6,239,105,124,188,160,47,125,252,242,83,191,1,
94,128,99,241,238,230,186,88,254,71,125,82,191,212,65,93,183,174,193,6,248,0,190,129,24,66,24,66,69,126,231,58,93,78,227,49,47,94,85,192,66,6,117,125,170,253,1,108,188,230,234,114,55,239,250,0,0,0,0,73,
69,78,68,174,66,96,130,0,0};

const char* Mcfx_gain_delayAudioProcessorEditor::clipboard35_png = (const char*) resource_Mcfx_gain_delayAudioProcessorEditor_clipboard35_png;
const int Mcfx_gain_delayAudioProcessorEditor::clipboard35_pngSize = 346;

// JUCER_RESOURCE: clipboard35_grey_png, 363, "clipboard35_grey.png"
static const unsigned char resource_Mcfx_gain_delayAudioProcessorEditor_clipboard35_grey_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,24,0,0,0,24,8,6,0,0,0,224,119,61,248,0,0,0,6,98,75,
71,68,0,255,0,255,0,255,160,189,167,147,0,0,0,9,112,72,89,115,0,0,11,19,0,0,11,19,1,0,154,156,24,0,0,0,7,116,73,77,69,7,222,12,27,16,56,59,28,186,28,207,0,0,0,248,73,68,65,84,72,199,237,149,161,142,194,
64,16,134,191,146,38,135,194,243,22,21,231,187,18,5,122,94,226,94,161,197,157,186,220,91,140,71,33,16,197,243,16,84,225,79,21,5,102,54,217,144,94,187,133,34,72,248,205,38,179,153,255,223,249,39,59,147,
208,1,17,153,3,27,224,51,8,31,128,165,170,158,136,64,218,115,47,70,190,14,98,133,197,127,198,16,152,1,168,106,25,84,85,248,120,12,18,75,114,192,22,248,224,126,156,129,133,170,86,109,21,184,128,124,15,
84,3,136,29,144,91,190,187,205,109,179,168,10,45,233,131,136,148,38,16,221,131,194,124,30,5,41,227,194,89,69,79,19,200,111,237,26,34,176,190,163,55,197,132,39,227,45,240,22,120,1,1,255,209,154,71,137,
68,228,210,37,240,107,231,244,159,252,216,225,231,71,125,6,172,128,58,181,141,213,0,223,29,175,139,21,168,108,135,127,1,53,224,198,238,65,6,236,128,63,192,169,234,49,25,224,111,223,166,243,85,214,158,
124,232,52,205,187,54,87,27,57,192,21,140,163,68,213,64,152,116,144,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* Mcfx_gain_delayAudioProcessorEditor::clipboard35_grey_png = (const char*) resource_Mcfx_gain_delayAudioProcessorEditor_clipboard35_grey_png;
const int Mcfx_gain_delayAudioProcessorEditor::clipboard35_grey_pngSize = 363;

// JUCER_RESOURCE: phase_symbol_png, 2992, "phase_symbol.png"
static const unsigned char resource_Mcfx_gain_delayAudioProcessorEditor_phase_symbol_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,49,0,0,0,49,8,6,0,0,1,4,155,113,138,0,0,0,9,112,72,89,
115,0,0,14,197,0,0,14,197,1,71,108,236,255,0,0,11,98,73,68,65,84,120,218,205,90,105,80,84,217,21,190,224,67,5,65,20,69,140,27,98,185,239,104,66,80,131,226,194,104,33,113,139,34,238,90,174,196,45,150,63,
76,25,171,172,82,113,41,117,84,220,17,84,68,203,224,174,40,50,46,131,203,104,20,151,148,107,92,10,40,13,229,50,136,34,8,42,42,228,251,238,244,109,187,155,126,77,55,50,73,78,213,211,238,190,239,221,123,
238,185,231,124,231,59,231,161,149,148,148,8,37,131,6,13,186,126,228,200,145,223,170,239,26,255,25,53,106,84,73,173,90,181,68,163,70,141,196,172,89,179,228,221,209,209,209,78,114,144,3,148,248,248,120,
49,122,244,104,225,228,228,36,94,191,126,237,165,113,42,62,193,129,188,188,60,81,169,82,37,121,227,136,17,35,126,212,184,6,167,10,14,14,22,190,190,190,34,46,46,78,76,156,56,81,132,133,133,197,105,106,
113,14,80,56,64,153,57,115,230,122,77,45,206,53,56,21,159,224,128,212,86,109,229,202,149,43,131,30,60,120,224,55,126,252,248,53,198,109,40,213,41,55,111,222,252,94,110,65,253,184,126,253,122,97,34,37,
70,37,48,183,200,202,202,18,13,26,52,248,106,145,13,27,54,200,141,182,104,209,226,235,0,231,227,163,166,243,12,27,54,172,187,81,229,234,213,171,231,123,123,123,191,74,79,79,247,51,83,151,226,225,225,81,
50,97,194,4,97,41,247,238,221,19,103,207,158,117,50,59,17,117,179,181,7,40,109,218,180,145,219,253,240,225,131,136,137,137,113,210,104,16,211,155,185,93,238,140,82,84,84,36,182,110,221,106,252,94,181,
106,85,65,85,53,90,208,114,102,238,124,198,140,25,162,114,229,202,162,101,203,150,102,99,220,155,70,147,39,36,36,124,79,21,148,253,76,37,36,36,196,236,59,141,33,247,164,54,57,101,202,148,18,170,96,77,
104,206,160,160,160,139,165,172,167,36,50,50,50,249,253,251,247,85,118,238,220,217,219,114,204,120,164,105,105,105,127,130,138,7,120,130,46,46,46,242,82,126,0,115,95,134,38,221,140,15,96,63,255,132,53,
58,242,102,29,83,119,197,113,20,230,231,231,187,201,7,120,179,40,67,112,20,174,231,206,157,11,214,92,93,93,75,38,79,158,108,28,128,222,226,227,199,143,226,203,151,47,242,154,54,109,154,60,27,154,124,207,
158,61,169,154,233,205,133,133,133,34,34,34,66,40,107,241,80,183,109,219,38,111,166,96,242,175,155,166,108,223,190,93,14,242,196,225,22,242,108,44,28,223,252,129,128,128,0,121,226,136,101,227,172,60,121,
179,7,78,156,56,33,250,247,239,47,245,133,105,5,207,133,87,113,113,177,216,188,121,179,152,62,125,186,249,3,56,114,39,218,155,27,85,179,42,177,188,249,210,165,75,55,53,3,190,20,142,27,55,206,205,150,89,
221,221,221,79,223,184,113,227,59,249,192,219,183,111,171,213,173,91,247,231,240,240,112,111,107,55,103,100,100,156,61,126,252,248,119,102,155,126,241,226,69,29,254,15,84,57,14,132,241,171,82,165,74,209,
195,135,15,125,158,61,123,86,207,108,15,214,156,79,201,162,69,139,18,151,45,91,22,30,24,24,40,218,181,107,87,106,60,55,55,151,251,162,186,153,183,110,221,106,162,55,143,102,249,3,125,241,229,203,151,29,
155,53,107,38,191,155,58,131,165,212,168,81,67,158,10,196,79,57,55,109,72,179,232,70,197,194,133,11,15,112,114,248,176,40,175,240,144,184,224,190,125,251,178,149,57,53,101,150,236,236,236,112,165,189,
53,161,75,39,39,39,11,236,82,186,53,205,71,39,52,149,148,148,20,209,181,107,87,169,36,15,183,118,237,218,185,175,94,189,170,33,23,73,77,77,13,183,102,115,10,99,131,136,72,179,13,29,58,212,108,12,193,197,
180,37,63,187,185,185,25,115,142,146,145,35,71,122,210,41,52,34,140,222,2,7,14,28,144,94,111,137,121,199,142,29,19,79,158,60,145,19,51,148,144,253,74,45,160,132,94,167,17,194,136,74,214,4,7,40,67,141,
114,244,232,81,241,244,233,211,82,26,83,9,91,66,183,214,136,145,99,199,142,45,161,167,88,10,108,42,39,222,184,113,99,169,144,84,194,49,127,127,127,221,69,24,55,242,76,224,227,153,61,122,244,48,230,146,
189,123,247,202,131,54,213,152,32,71,88,82,66,148,105,219,182,109,41,83,154,10,206,242,36,2,51,84,45,210,164,119,239,222,151,136,199,6,154,81,234,129,169,83,167,58,228,202,6,136,8,53,139,19,149,29,144,
16,74,108,105,87,150,248,248,248,36,195,124,27,67,67,67,147,117,35,30,48,35,211,14,147,5,92,246,200,224,193,131,61,9,235,182,132,102,1,10,250,224,234,108,21,86,108,97,151,146,79,159,62,185,1,249,151,95,
191,126,189,19,188,209,165,113,227,198,25,112,253,69,8,222,127,149,245,172,166,55,128,137,60,91,181,106,245,76,211,52,183,110,221,186,9,79,79,79,35,127,252,252,249,115,0,114,81,196,237,219,183,9,69,197,
112,146,97,3,7,14,60,100,247,2,224,98,249,96,140,238,3,6,12,176,169,93,251,246,237,121,57,159,62,125,250,32,8,235,103,4,166,143,151,151,215,107,221,5,16,108,46,192,154,34,189,136,213,19,103,103,103,38,
122,109,248,240,225,57,72,211,147,241,124,172,213,5,96,142,162,111,241,36,152,84,96,55,219,176,224,23,44,184,195,108,1,82,197,111,153,92,73,157,58,117,24,184,49,240,192,195,64,140,92,185,64,84,84,212,
94,84,7,162,162,4,140,82,131,167,253,27,25,210,67,46,176,106,213,170,136,49,99,198,216,124,232,234,213,171,226,238,221,187,18,82,234,213,171,39,16,80,194,52,70,224,202,130,94,213,185,243,47,225,128,67,
119,71,16,247,214,224,102,67,112,56,182,98,64,108,217,178,69,50,125,30,38,76,41,114,114,114,68,108,108,172,17,113,21,180,179,210,81,11,240,94,184,238,49,13,154,111,239,219,183,175,213,201,9,209,156,156,
254,143,228,98,25,39,114,17,114,51,46,14,155,27,203,12,37,189,122,245,202,209,128,126,158,122,218,115,114,106,98,57,57,5,222,98,252,60,105,210,36,97,141,111,227,28,26,106,180,167,158,48,9,129,144,155,
253,166,106,9,165,53,115,249,201,147,39,229,103,171,145,172,7,102,42,131,129,193,25,127,179,150,237,88,85,62,127,254,92,31,139,120,136,122,209,169,167,181,169,173,121,22,32,106,250,11,208,35,116,7,53,
77,236,218,181,75,230,108,107,172,194,0,241,18,147,116,231,40,40,40,96,94,172,108,57,64,173,129,154,114,114,107,30,66,97,93,77,233,212,169,147,30,17,184,167,33,193,199,33,120,34,77,7,76,109,205,131,62,
124,248,176,104,221,186,53,221,78,154,137,99,73,73,73,82,123,61,130,96,224,239,239,181,229,203,151,255,25,4,32,178,67,135,14,242,199,71,143,30,9,48,65,51,173,207,159,63,47,80,112,138,251,247,239,155,145,
128,178,114,54,206,230,141,132,10,104,244,2,11,72,119,105,222,188,185,188,76,5,10,200,203,17,193,238,142,157,58,117,106,160,92,32,51,51,243,55,160,36,95,96,2,231,138,0,59,56,71,97,86,86,214,71,51,184,
94,189,122,117,232,142,29,59,82,8,183,223,42,32,120,31,65,39,195,205,22,0,30,253,0,143,137,72,76,76,252,123,253,250,245,203,61,57,106,137,60,204,227,101,53,163,129,150,39,34,167,230,204,155,55,111,31,
18,125,77,71,38,6,41,120,132,201,93,49,121,35,155,73,191,79,159,62,103,192,113,188,16,60,119,176,19,31,80,19,111,91,19,35,135,167,163,4,240,128,43,79,88,188,120,241,101,187,105,11,146,135,228,251,107,
215,174,253,11,92,249,175,136,137,79,192,251,92,160,102,209,187,119,239,156,64,61,155,245,236,217,51,53,33,33,97,204,146,37,75,222,234,30,184,61,196,203,150,48,97,69,71,71,207,187,118,237,90,11,120,142,
39,49,172,90,181,106,146,71,17,106,88,50,112,13,162,2,113,143,109,55,40,200,128,45,106,218,180,233,115,156,125,242,220,185,115,23,122,123,123,103,151,219,107,29,185,25,138,84,94,177,98,69,252,186,117,
235,134,65,161,74,29,59,118,148,61,51,248,134,208,75,140,54,132,16,232,75,164,90,176,96,65,36,193,2,150,47,174,89,179,230,207,49,49,49,227,67,66,66,126,168,208,77,160,94,221,191,114,229,202,161,126,126,
126,178,222,100,171,165,34,5,216,43,136,136,184,136,39,117,81,61,166,204,153,51,167,24,69,115,62,202,209,225,140,212,114,109,130,132,21,153,230,214,254,253,251,91,177,220,212,235,18,254,26,194,77,25,0,
210,19,167,146,210,175,95,63,1,61,134,163,120,217,103,247,38,104,121,184,205,80,246,47,28,101,221,21,45,196,72,242,231,131,7,15,38,130,42,197,225,148,6,19,121,116,55,65,159,71,17,154,3,75,184,255,175,
149,183,20,4,190,64,201,239,78,92,198,198,126,2,123,29,80,106,19,172,210,80,72,229,32,88,43,89,182,120,191,69,20,165,51,101,94,246,10,81,204,146,133,49,177,128,73,4,19,255,21,68,107,38,53,209,51,156,66,
185,55,64,101,83,83,83,37,85,49,156,170,46,195,51,84,46,2,57,192,90,11,65,156,57,115,70,242,84,178,236,46,93,186,136,38,77,204,91,126,96,42,30,128,104,31,196,205,143,144,94,154,161,74,185,202,58,89,175,
215,84,6,236,202,254,46,137,27,173,77,194,198,102,10,41,145,41,161,166,160,132,16,143,31,63,150,239,25,88,69,241,98,16,115,51,52,0,155,180,170,247,141,28,34,97,91,175,23,206,12,124,241,226,197,38,76,148,
36,222,110,72,181,1,150,157,54,123,4,229,153,145,8,50,185,193,103,109,222,207,50,133,23,249,29,45,206,150,52,21,103,171,208,160,152,124,241,160,222,65,149,37,48,150,15,51,189,198,22,8,203,62,42,225,136,
176,38,165,235,208,82,36,247,221,187,119,183,167,219,36,93,69,89,156,23,123,115,111,222,188,145,76,152,193,107,239,6,40,72,140,85,65,85,94,105,236,225,168,54,139,35,66,87,160,43,113,81,91,27,176,84,220,
210,226,164,36,124,139,192,130,133,115,170,58,217,94,33,215,98,75,212,69,5,155,35,65,204,174,128,106,234,58,170,184,169,112,156,115,112,62,162,145,163,66,178,200,222,70,6,44,16,224,104,27,137,190,205,
64,165,162,252,159,69,41,99,196,30,197,77,133,85,155,66,178,242,144,127,178,93,118,192,23,1,93,28,38,67,236,203,240,13,20,221,0,217,212,33,197,77,243,0,91,46,116,41,198,4,231,116,68,192,144,11,73,215,
53,246,49,113,61,65,226,240,181,85,125,43,73,79,79,47,101,113,85,189,51,49,5,5,5,149,185,1,42,207,141,147,150,27,10,18,190,45,113,248,20,114,114,114,110,0,158,127,105,193,161,60,253,29,178,245,11,208,
106,103,226,182,181,46,205,161,67,135,248,102,203,170,197,57,206,134,60,81,134,167,195,123,104,89,110,84,197,12,221,77,253,166,98,1,232,34,91,75,229,201,230,64,212,12,20,185,254,168,97,26,104,6,94,146,
205,134,47,172,177,31,137,207,217,114,82,6,29,255,26,128,117,3,255,100,192,210,210,252,206,63,62,48,180,191,101,50,163,175,211,210,5,5,5,106,81,9,227,236,212,49,25,90,190,199,116,68,8,171,177,177,177,
238,73,73,73,127,196,156,111,141,176,196,142,53,232,110,232,144,33,67,142,195,58,154,233,137,80,1,203,119,89,54,208,66,194,164,163,80,233,72,145,13,72,174,131,68,217,31,181,205,229,82,44,150,213,20,59,
227,160,12,25,254,254,254,108,219,255,95,49,89,156,248,145,248,248,248,206,208,209,23,113,148,167,91,79,176,181,207,183,131,113,113,113,147,102,207,158,189,33,44,44,172,74,69,52,87,190,69,224,9,63,45,
93,186,244,15,112,249,196,53,107,214,12,182,187,178,227,43,4,94,8,158,9,40,21,163,225,78,85,217,51,255,111,41,206,150,21,16,239,20,44,223,99,211,166,77,27,241,57,168,220,53,54,95,85,240,202,205,205,173,
1,130,183,235,194,133,11,61,3,3,3,75,192,50,61,202,131,42,101,5,44,128,224,242,238,221,187,187,195,181,79,35,120,39,193,242,121,21,214,237,224,43,17,211,106,138,47,24,162,162,162,254,150,150,150,246,123,
192,109,118,195,134,13,157,17,116,222,36,101,246,36,41,24,32,51,59,59,251,53,72,164,235,157,59,119,90,162,110,248,199,252,249,243,151,2,253,206,89,254,201,70,89,242,31,12,97,188,28,94,18,23,150,0,0,0,
0,73,69,78,68,174,66,96,130,0,0};

const char* Mcfx_gain_delayAudioProcessorEditor::phase_symbol_png = (const char*) resource_Mcfx_gain_delayAudioProcessorEditor_phase_symbol_png;
const int Mcfx_gain_delayAudioProcessorEditor::phase_symbol_pngSize = 2992;

// JUCER_RESOURCE: phase_symbol_inv_png, 3564, "phase_symbol_inv.png"
static const unsigned char resource_Mcfx_gain_delayAudioProcessorEditor_phase_symbol_inv_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,49,0,0,0,49,8,6,0,0,1,4,155,113,138,0,0,0,9,112,72,
89,115,0,0,14,197,0,0,14,197,1,71,108,236,255,0,0,13,158,73,68,65,84,120,218,205,90,9,88,83,87,22,62,47,121,129,36,144,133,196,16,196,29,247,86,235,90,171,88,84,22,69,16,89,20,21,247,34,58,86,172,83,139,
90,25,29,107,107,91,165,90,145,177,138,99,11,162,197,22,5,21,5,4,119,105,181,90,23,212,106,157,186,140,40,46,152,4,9,97,13,18,147,55,247,222,152,64,36,1,162,78,103,206,247,93,17,239,114,150,123,238,127,
150,39,205,48,12,152,232,237,217,49,23,47,36,197,15,52,253,78,227,63,100,219,175,49,116,167,222,0,211,227,161,245,79,64,86,63,30,14,20,153,36,19,136,148,225,238,224,154,86,4,192,102,129,90,93,46,161,241,
81,120,7,158,96,74,149,64,113,56,100,161,239,251,139,78,208,152,7,62,74,180,100,27,112,223,25,13,138,96,23,112,203,42,131,89,94,253,147,105,19,115,60,129,9,79,96,90,176,96,193,55,180,137,185,90,173,150,
224,163,240,14,60,65,100,49,169,114,224,250,189,208,194,63,126,239,244,81,120,208,6,179,26,70,209,59,34,125,58,194,218,159,32,158,168,96,210,71,49,130,5,64,81,100,119,235,147,122,198,44,132,91,190,1,234,
10,142,131,195,0,223,122,139,40,188,217,0,44,22,240,188,39,212,79,224,243,240,86,104,64,233,172,83,195,204,34,11,68,162,202,86,174,110,79,238,222,190,217,9,192,171,94,92,76,44,137,43,35,207,84,193,139,
84,147,179,21,202,215,205,165,44,110,196,180,216,218,6,76,252,160,185,68,93,67,133,26,148,99,37,20,141,13,210,112,177,194,155,5,110,39,13,228,239,140,182,18,148,1,66,164,169,81,26,150,80,2,88,84,26,91,
16,27,203,76,104,94,225,195,6,183,19,122,160,120,2,224,250,78,182,224,138,117,163,177,201,63,93,178,53,30,139,96,178,95,67,18,175,248,209,226,119,108,12,162,147,73,73,121,182,154,193,34,88,35,108,78,47,
47,175,83,22,151,221,144,198,172,88,151,91,87,93,233,120,52,126,149,239,139,115,230,43,221,87,88,50,62,250,190,116,15,69,161,59,247,91,2,245,206,1,208,57,46,232,204,233,188,156,161,230,13,3,63,77,188,
252,200,59,186,47,69,89,149,8,238,196,230,120,242,197,146,154,26,141,154,79,54,224,197,208,12,137,14,168,121,249,249,249,35,104,202,73,192,184,229,86,154,39,148,227,221,200,125,192,51,61,128,174,14,228,
185,21,160,28,35,34,38,159,84,209,235,36,221,112,177,94,173,0,89,202,117,116,97,82,48,121,165,50,68,70,22,147,139,20,181,170,87,26,83,73,120,91,52,249,204,120,227,129,98,228,1,122,228,248,148,117,43,97,
114,158,254,55,114,170,100,253,81,178,152,16,143,111,185,161,44,54,0,92,226,242,128,209,233,160,42,117,53,114,19,116,227,140,30,253,208,131,114,148,19,184,29,171,181,220,80,123,54,143,34,222,89,89,106,
150,213,252,220,94,88,236,30,51,224,18,17,169,34,84,90,35,220,95,202,111,202,172,253,254,157,119,52,183,160,96,20,217,80,93,86,234,228,218,186,141,138,189,235,145,204,218,226,94,9,19,142,231,102,102,140,
178,80,90,245,248,145,43,254,25,20,20,148,83,164,53,116,226,240,120,117,138,75,103,229,197,197,197,238,48,60,163,94,7,107,206,103,162,41,73,251,119,167,125,56,125,162,112,206,106,224,135,45,104,52,255,
236,193,77,168,220,188,16,220,13,85,119,11,207,157,242,176,117,14,253,226,63,96,95,188,99,16,247,229,250,78,1,232,26,10,13,157,161,209,230,118,221,201,173,104,1,58,153,156,27,219,16,155,197,230,171,136,
218,126,116,15,215,55,26,184,240,242,132,47,9,51,212,71,180,41,49,153,147,54,153,229,68,151,224,137,68,122,27,132,93,90,189,60,8,244,183,126,3,112,18,128,115,216,124,112,142,252,220,98,141,102,69,40,8,
230,111,0,182,91,39,192,151,43,118,117,211,104,84,10,49,97,114,240,247,7,19,249,93,89,86,15,87,4,10,17,158,86,130,107,214,19,144,109,185,104,49,167,154,214,25,12,15,11,81,12,64,123,5,66,115,204,49,17,
47,67,33,194,78,65,99,132,225,135,45,177,202,160,228,125,28,56,245,102,36,53,81,233,34,111,208,93,250,153,28,44,89,127,4,212,139,252,27,49,48,17,246,58,26,67,152,77,19,21,23,26,225,1,31,28,131,14,190,
252,115,35,137,241,211,3,176,237,161,216,173,105,140,145,178,157,55,25,236,41,141,22,116,233,131,36,206,7,133,47,7,220,142,235,172,30,162,244,161,129,63,225,67,155,76,240,187,33,119,34,248,102,246,93,
237,218,83,157,204,182,158,217,3,12,101,74,11,137,21,1,200,43,181,218,122,169,249,124,224,143,153,211,200,148,13,105,64,238,23,121,57,197,197,129,132,9,126,72,239,6,4,253,130,241,24,255,238,186,227,70,
163,13,110,121,213,118,185,50,134,136,156,204,140,64,139,119,98,138,14,20,69,49,77,73,215,28,13,82,157,201,157,235,164,217,28,152,153,145,107,243,197,35,152,33,240,140,131,69,232,196,136,253,142,73,191,
139,48,172,55,69,216,44,143,143,101,202,11,10,10,6,0,120,54,70,134,166,176,203,156,118,24,128,191,236,135,3,113,151,47,94,236,95,87,93,197,241,104,223,166,240,239,83,195,87,245,236,220,241,143,230,246,
210,182,38,202,117,140,168,237,192,161,197,58,129,148,47,136,222,0,116,251,16,0,60,16,221,67,22,57,241,16,34,106,254,145,0,85,201,43,12,25,187,119,79,24,23,20,184,175,197,12,196,158,254,149,48,60,194,
217,121,227,153,38,165,227,143,95,136,7,43,218,160,223,27,225,210,234,153,226,206,45,185,68,34,81,219,100,160,99,128,227,232,34,173,147,31,40,181,235,130,41,22,27,164,251,159,208,30,107,183,149,126,221,
21,230,204,142,154,149,100,149,129,3,139,170,123,21,79,226,5,204,130,216,155,23,191,99,167,164,232,35,35,35,83,44,24,176,164,242,87,114,83,51,34,116,31,8,49,185,87,190,13,211,104,50,197,98,177,134,48,
152,154,154,151,38,219,121,27,94,23,113,3,103,211,237,60,164,15,42,213,165,2,194,32,237,227,247,35,228,187,138,154,220,84,149,188,28,170,14,108,65,225,173,12,216,111,12,2,233,154,131,208,240,141,48,218,
42,168,217,187,17,156,166,45,35,191,59,237,81,58,31,63,126,220,151,206,188,173,24,39,219,118,205,230,193,134,218,26,80,141,118,70,54,68,111,144,230,0,37,149,131,254,222,117,80,133,160,160,38,68,233,83,
150,218,12,237,156,126,195,204,12,40,54,13,193,65,161,89,116,212,180,169,219,28,215,30,183,122,56,134,104,213,104,39,96,117,232,134,176,234,166,37,99,141,10,84,161,114,99,110,134,2,146,228,235,35,230,
50,195,68,242,168,229,165,180,230,214,85,145,220,134,244,202,0,36,57,237,216,232,112,76,101,159,79,50,70,58,148,170,185,30,40,1,107,249,182,54,44,182,29,237,216,199,203,182,225,159,214,130,52,209,242,
177,153,50,75,76,88,106,245,138,16,80,255,61,24,90,109,60,109,29,42,88,146,214,54,205,67,220,238,141,33,245,97,212,74,180,163,56,142,240,236,70,129,109,44,98,170,203,109,190,78,171,82,163,84,215,161,191,
79,253,92,165,6,40,23,87,219,12,234,10,175,54,225,208,124,80,78,106,7,140,170,216,106,86,97,170,170,156,199,219,14,163,52,187,228,81,29,70,137,70,251,112,157,80,171,5,166,246,33,72,226,143,91,72,109,118,
130,16,41,225,224,52,37,214,234,225,238,143,11,174,211,81,49,31,39,103,2,204,179,72,89,76,182,22,138,80,182,241,12,212,49,190,192,243,159,14,194,216,237,128,235,166,167,231,15,65,217,178,16,146,232,203,
109,36,8,4,126,18,23,107,233,196,79,150,70,255,176,32,126,30,63,60,198,232,90,71,83,65,119,235,146,133,173,43,54,204,131,154,35,223,131,246,112,170,233,134,16,86,243,80,142,251,180,201,215,239,194,229,
148,17,168,112,58,146,172,96,194,99,220,8,34,142,156,78,134,69,94,250,209,22,50,236,161,193,249,241,89,153,71,142,132,16,6,197,55,175,183,118,10,152,161,23,198,126,207,122,29,96,135,80,165,134,115,253,
215,167,150,89,248,194,169,129,83,111,94,60,132,225,246,85,73,27,42,121,154,174,86,79,180,96,224,239,239,127,120,115,250,158,136,232,43,85,187,28,250,142,120,233,195,107,130,197,21,229,229,26,137,213,
136,54,105,98,248,110,233,177,99,165,147,191,154,150,78,47,221,233,98,207,193,114,157,234,150,122,214,64,30,58,188,125,147,65,223,207,207,239,88,137,159,159,164,247,91,125,174,149,13,159,44,215,135,199,
202,154,58,88,86,167,188,163,156,214,71,144,152,185,47,210,243,254,253,51,45,78,91,174,93,253,141,244,184,18,18,18,22,198,197,197,197,114,4,98,157,164,123,47,13,135,239,92,103,40,85,80,183,207,159,233,
234,237,61,226,100,106,106,234,116,145,82,81,110,27,139,152,87,139,195,56,96,173,219,154,188,244,198,169,19,221,203,110,92,22,97,12,99,201,218,0,199,221,3,40,71,39,160,156,80,129,99,48,16,100,96,106,171,
65,87,124,7,12,79,138,129,195,48,117,238,189,250,63,14,24,229,151,251,217,188,89,43,101,50,89,201,203,202,64,219,179,184,142,1,135,200,157,121,59,246,172,248,104,130,238,169,150,45,152,180,24,184,254,
51,129,53,118,57,56,162,225,102,31,111,135,90,128,14,24,169,246,158,43,155,87,123,56,30,106,210,227,13,50,177,72,149,178,121,227,123,254,126,190,135,95,171,18,17,219,178,51,210,151,204,14,231,121,6,131,
115,244,122,144,88,169,40,94,233,253,11,92,0,35,34,26,44,228,23,110,51,42,203,14,85,5,204,48,56,92,62,86,185,107,71,202,36,252,82,95,74,9,156,176,14,92,189,253,183,171,107,99,122,74,19,242,65,190,79,9,
127,22,97,165,158,3,164,104,202,229,147,135,212,163,41,72,207,216,51,105,66,248,248,244,22,43,65,44,31,243,94,184,100,77,14,73,178,254,151,228,208,207,155,20,161,243,110,21,236,158,37,145,38,103,166,239,
14,195,200,99,83,9,236,243,174,67,253,75,117,210,246,206,246,150,12,255,109,226,116,27,0,156,204,82,103,140,203,67,54,126,115,58,43,235,64,112,35,37,112,149,230,218,181,103,41,111,242,82,182,112,116,228,
107,99,110,74,233,26,102,94,45,37,125,201,67,96,203,218,90,186,14,10,44,103,179,55,143,192,248,111,130,104,179,18,184,204,228,134,206,103,243,94,82,1,70,255,12,42,214,70,130,54,127,239,243,100,87,107,
51,195,35,85,165,239,100,16,46,73,106,52,93,123,106,15,104,214,32,25,48,84,139,165,32,156,183,30,184,67,67,45,21,25,59,95,80,246,180,82,238,227,227,115,2,145,15,81,194,115,241,151,231,112,157,236,98,165,
129,216,172,240,40,51,87,6,163,168,171,211,17,198,248,155,12,31,25,2,39,112,13,19,106,226,174,215,78,65,237,145,84,148,123,161,113,48,153,12,224,59,129,104,81,18,148,175,159,131,178,136,26,227,135,17,
20,187,184,239,6,131,104,213,94,146,0,90,189,37,20,129,239,93,58,234,129,3,37,141,59,8,103,191,91,63,168,213,150,243,118,43,80,190,102,58,74,4,119,26,221,69,222,22,228,187,31,52,253,72,123,123,145,33,
92,244,173,209,226,159,160,250,166,166,26,202,63,159,76,18,73,174,87,8,136,86,238,2,138,118,108,17,255,234,168,245,242,184,101,163,99,105,220,2,97,203,219,3,221,166,139,93,10,224,154,148,184,14,11,89,
62,120,46,8,23,38,54,187,199,236,42,38,139,99,209,221,59,2,83,140,234,95,30,15,56,61,6,182,88,1,2,197,157,251,114,105,129,248,9,141,123,56,14,207,63,211,217,165,4,46,170,177,223,59,56,54,169,128,133,224,
172,231,174,226,133,92,229,19,163,197,25,84,101,42,125,56,100,190,106,223,38,112,154,186,220,46,57,112,174,69,227,38,20,197,115,182,31,113,42,158,23,138,2,113,243,130,27,44,5,183,168,23,177,207,59,163,
252,10,213,131,76,185,253,49,9,39,139,52,238,162,93,185,253,203,32,123,219,72,184,173,161,255,215,121,210,202,192,15,214,160,86,128,230,171,89,45,18,220,162,98,71,85,27,232,117,196,189,232,94,131,237,
86,2,103,187,52,110,3,166,109,136,139,176,119,51,238,203,168,198,183,37,15,83,189,96,152,17,153,152,150,9,222,48,14,148,197,6,145,125,192,229,129,100,117,182,93,50,176,217,80,131,211,117,26,247,49,59,
188,217,175,72,189,55,161,3,238,8,54,251,56,243,211,65,179,54,170,222,226,166,18,21,185,24,37,113,5,1,122,31,205,41,128,133,127,18,253,14,48,37,197,198,221,173,90,131,124,79,177,221,183,208,37,107,117,
129,208,123,132,177,5,119,62,243,199,183,221,58,122,40,184,163,102,178,112,242,213,232,13,160,24,240,228,131,193,160,191,117,165,222,85,134,33,56,92,145,102,124,156,104,94,53,179,27,48,143,139,160,100,
92,27,35,242,32,180,193,205,120,74,40,38,143,23,170,42,137,181,137,242,38,213,221,59,129,107,234,45,210,236,178,151,90,213,41,11,11,182,172,233,247,240,225,195,182,198,255,97,128,10,18,220,240,157,20,
213,59,67,186,171,136,245,98,138,96,80,63,6,222,224,64,208,35,20,19,46,222,218,248,113,114,56,32,255,241,174,113,109,133,26,180,217,91,64,123,46,15,244,143,238,144,6,14,110,129,80,46,50,96,183,235,6,188,
65,1,192,11,141,6,22,95,248,242,29,87,120,250,68,53,173,143,115,118,118,246,88,145,72,84,110,54,1,238,88,231,252,176,35,112,236,56,121,142,75,234,109,186,225,141,224,56,242,226,183,44,155,216,45,148,16,
152,180,23,42,237,41,178,139,38,244,112,61,120,48,103,140,167,167,231,153,70,89,44,174,166,112,103,220,227,141,55,11,225,189,47,68,184,199,252,255,68,158,191,108,218,127,225,159,107,7,20,21,221,235,32,
20,10,43,108,214,19,184,181,175,81,20,139,147,146,183,205,254,32,88,188,201,121,221,49,199,215,209,92,121,21,234,249,232,244,233,147,83,189,222,157,144,150,182,123,239,253,251,97,45,174,236,240,39,4,60,
82,82,82,34,255,26,230,191,145,158,243,21,23,247,204,255,180,234,142,130,154,193,167,55,29,57,249,245,39,195,255,146,152,184,249,4,195,120,189,116,141,141,63,85,224,161,209,104,196,51,102,132,124,127,
242,244,105,111,238,172,85,12,59,112,174,224,101,80,165,185,7,219,39,127,243,153,252,132,47,134,141,28,233,119,52,41,41,105,182,112,217,7,21,175,173,219,129,63,137,52,172,166,240,7,134,47,63,251,114,249,
185,11,23,222,145,191,53,168,4,222,30,201,170,126,107,180,12,39,101,45,9,82,242,135,5,119,5,151,14,171,171,127,61,202,187,113,249,98,143,33,67,134,156,93,182,108,217,234,17,43,99,242,97,101,140,93,202,
255,7,222,4,219,115,95,50,230,255,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* Mcfx_gain_delayAudioProcessorEditor::phase_symbol_inv_png = (const char*) resource_Mcfx_gain_delayAudioProcessorEditor_phase_symbol_inv_png;
const int Mcfx_gain_delayAudioProcessorEditor::phase_symbol_inv_pngSize = 3564;

// JUCER_RESOURCE: phase_symbol_over_png, 3018, "phase_symbol_over.png"
static const unsigned char resource_Mcfx_gain_delayAudioProcessorEditor_phase_symbol_over_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,49,0,0,0,49,8,6,0,0,1,4,155,113,138,0,0,0,9,112,
72,89,115,0,0,14,197,0,0,14,197,1,71,108,236,255,0,0,11,124,73,68,65,84,120,218,205,89,7,80,84,235,21,254,193,139,60,16,4,81,196,174,160,177,141,142,138,134,40,70,99,23,137,81,95,84,4,123,111,232,24,176,
151,49,51,168,216,198,130,117,198,8,138,58,40,118,159,141,103,175,56,177,98,155,216,11,96,5,236,162,162,178,249,190,63,123,215,187,203,238,178,11,228,37,103,230,194,238,254,237,156,243,159,243,157,114,
21,157,78,39,84,138,140,140,188,184,120,241,226,38,234,119,133,127,110,220,184,161,171,87,175,158,192,0,191,170,179,29,228,32,7,72,21,42,84,16,143,31,63,22,142,142,142,226,237,219,183,94,138,126,43,57,
240,226,197,11,225,228,228,36,39,142,31,63,254,152,162,63,67,23,23,23,39,130,130,130,68,169,82,165,196,235,215,175,133,191,191,127,172,162,30,206,1,18,7,72,99,199,142,93,174,14,58,188,122,245,202,139,
91,113,5,7,36,183,170,40,143,30,61,234,6,174,125,59,119,238,188,196,32,6,207,171,86,173,154,224,3,90,172,138,32,151,144,109,7,7,7,57,235,251,247,239,58,3,19,185,185,185,226,232,209,163,162,109,219,182,
63,52,82,172,88,49,185,162,103,207,158,70,3,14,92,42,52,116,250,244,233,150,6,150,61,60,60,222,151,43,87,46,243,246,237,219,190,45,90,180,248,193,46,169,108,217,178,186,151,47,95,10,83,90,179,102,141,
24,62,124,184,131,209,141,168,147,205,45,32,97,129,20,23,74,18,94,94,94,14,10,21,162,157,76,230,41,25,233,253,251,247,162,100,201,146,66,229,6,11,4,89,85,168,65,189,178,12,68,201,33,164,112,119,119,23,
97,97,97,70,99,148,77,161,202,193,243,98,61,11,134,83,84,74,72,72,48,250,78,101,40,122,158,165,144,224,89,71,22,204,17,213,9,205,157,54,186,108,45,45,92,184,240,0,228,113,142,138,138,106,107,58,102,184,
210,140,140,140,238,101,202,148,217,206,251,158,56,113,162,250,179,220,13,236,39,239,219,183,175,185,97,193,170,85,171,174,140,30,61,186,161,176,64,152,28,8,118,179,193,182,171,92,96,109,178,74,152,236,
114,226,196,137,86,10,212,168,163,254,53,42,148,247,65,21,231,228,228,24,238,134,223,225,105,199,21,237,228,231,207,159,139,155,55,111,138,210,165,75,27,238,6,114,201,201,36,126,86,180,199,86,170,84,73,
124,251,246,77,238,234,233,233,41,39,170,70,159,71,75,164,169,83,167,202,93,15,31,62,108,216,213,213,213,213,120,65,167,78,157,196,193,131,7,197,215,175,95,69,116,116,180,188,113,62,92,80,162,68,9,241,
249,243,103,227,5,152,204,51,117,89,89,89,134,93,85,50,157,220,184,113,227,203,146,37,8,153,141,5,174,214,212,138,141,15,95,186,116,169,131,92,128,201,37,42,86,172,248,242,201,147,39,222,230,38,195,119,
143,110,219,182,173,131,145,208,152,92,86,111,6,251,32,131,175,139,139,75,206,185,115,231,124,158,62,125,90,1,147,127,200,96,206,248,84,218,189,123,119,98,191,126,253,66,168,12,64,83,158,241,59,119,238,
136,113,227,198,137,15,31,62,60,132,69,251,89,218,71,49,253,129,182,136,123,107,216,187,119,111,209,173,91,55,161,53,6,83,170,89,179,166,188,21,144,175,106,220,122,29,150,176,232,21,184,231,237,176,95,
81,24,210,95,146,14,186,207,80,213,169,168,106,233,218,181,107,8,185,183,68,52,105,232,94,92,189,122,85,66,76,120,120,184,152,53,107,150,209,28,74,190,116,233,82,137,227,188,92,248,204,27,172,243,148,
135,164,165,165,133,152,154,180,74,116,36,170,44,51,51,83,92,188,120,209,104,172,122,245,234,226,193,131,7,18,24,57,79,141,57,26,198,60,104,20,10,17,70,3,26,70,212,164,73,19,105,208,166,198,209,166,77,
27,113,242,228,73,185,49,93,169,67,135,14,121,14,208,196,27,95,122,185,179,37,21,145,75,213,107,90,183,110,45,78,157,58,149,135,99,115,76,104,137,102,173,16,35,97,138,58,90,138,41,53,104,208,64,0,160,
100,208,166,95,155,53,79,69,145,102,108,137,232,55,242,78,134,12,25,66,59,247,85,7,106,215,174,45,51,2,45,199,4,138,79,159,62,25,184,38,202,12,27,54,204,170,20,179,103,207,62,8,199,12,86,244,161,193,15,
23,116,150,120,204,239,183,110,221,202,179,224,227,199,143,118,153,178,30,34,130,141,252,68,141,14,176,50,157,53,238,242,163,228,228,228,3,111,222,188,89,137,3,14,88,244,120,28,32,109,153,193,34,52,52,
116,55,66,178,7,97,221,26,81,45,187,118,237,242,1,18,54,14,12,12,204,123,111,54,114,237,186,103,207,158,121,240,19,127,224,148,19,188,249,65,143,30,61,162,224,116,255,202,111,161,98,105,0,7,123,52,111,
222,252,41,112,200,117,201,146,37,2,104,32,31,61,5,224,9,141,137,137,17,51,102,204,200,77,76,76,236,25,28,28,188,211,230,3,58,118,236,248,30,42,114,131,94,173,114,71,147,197,227,8,31,217,1,85,126,131,
11,248,32,128,191,178,118,128,19,56,206,97,184,178,135,24,56,1,53,10,114,92,46,28,54,120,240,224,181,102,15,128,37,229,20,198,146,176,49,241,236,31,235,214,173,251,62,104,208,160,117,70,7,248,248,248,
20,202,76,181,216,150,146,146,178,6,38,187,11,49,232,141,162,15,184,155,239,222,189,43,138,138,134,14,29,170,64,213,105,80,181,187,60,96,228,200,145,161,172,42,172,209,244,233,211,197,234,213,171,37,156,
4,4,4,136,253,251,247,11,173,143,208,235,105,85,211,166,77,147,223,1,61,110,200,232,219,42,192,244,191,94,191,126,221,226,198,217,217,217,194,205,205,77,166,72,4,66,168,82,230,91,200,190,101,250,196,236,
90,133,246,150,45,91,26,14,32,48,34,72,253,162,244,233,211,39,142,181,131,57,82,179,35,162,47,114,96,163,49,132,98,121,8,15,102,64,58,116,232,144,161,204,208,72,157,165,92,187,118,205,195,18,247,228,220,
217,217,57,207,230,164,94,189,122,25,74,0,30,102,46,223,158,50,101,74,101,133,101,138,37,98,170,6,140,55,250,77,205,44,73,228,154,222,221,165,75,23,113,230,204,25,243,80,81,190,124,121,139,234,33,53,109,
218,212,240,155,185,104,71,9,1,116,150,177,8,181,168,69,239,52,199,53,227,51,47,85,37,216,187,188,11,139,7,88,179,32,70,177,202,149,43,11,68,41,179,89,133,22,147,44,30,144,158,158,158,131,255,197,77,7,
200,53,67,38,198,101,133,170,229,90,37,150,6,244,126,92,166,217,205,47,95,190,124,83,153,52,105,82,44,62,143,210,14,168,186,70,33,40,75,6,154,31,114,90,17,31,31,47,205,50,41,41,73,94,46,239,201,82,130,
160,175,246,63,41,147,39,79,30,13,188,31,21,17,17,33,127,220,180,105,19,79,54,210,245,168,81,163,196,134,13,27,196,198,141,27,85,80,100,154,34,190,124,249,98,213,251,225,152,175,37,84,172,93,187,246,57,
14,40,199,207,125,251,246,149,143,150,8,17,124,236,33,48,253,11,204,184,171,60,0,174,95,190,127,255,254,223,193,165,99,17,225,93,54,252,231,11,181,98,128,107,64,70,48,176,60,137,112,91,88,130,87,127,1,
70,133,24,197,3,132,201,95,183,111,223,30,138,160,190,165,85,171,86,5,222,28,254,242,14,190,225,101,54,162,33,83,72,60,114,228,72,22,238,96,43,46,187,148,61,27,3,143,238,160,234,115,193,230,85,172,6,253,
118,237,218,29,193,227,133,92,245,122,88,88,152,15,108,220,219,218,198,192,253,251,152,235,190,115,231,206,65,169,169,169,201,54,167,45,40,70,234,243,63,138,142,191,205,155,55,111,10,68,255,138,98,252,
13,16,54,7,49,196,1,25,199,239,160,202,227,48,221,126,248,254,214,162,39,23,54,14,51,96,197,198,198,78,62,118,236,88,173,43,87,174,120,16,195,144,152,9,63,63,63,25,75,8,49,132,116,34,3,163,30,75,5,84,
72,68,128,28,127,127,255,103,208,214,1,36,11,127,247,246,246,206,40,40,15,138,157,243,139,35,71,136,135,137,247,4,84,21,155,48,97,130,24,48,96,128,12,217,124,236,221,11,79,85,98,5,108,99,20,19,200,69,
139,22,229,2,161,94,46,91,182,108,32,16,234,215,34,21,2,137,254,54,148,46,61,24,191,112,144,217,138,162,48,196,136,67,223,199,67,60,41,7,161,146,128,47,185,112,156,247,72,229,122,209,83,11,42,132,211,
250,245,235,175,70,70,70,214,97,17,199,90,235,183,34,10,165,7,72,143,227,199,143,39,17,198,1,55,189,186,119,239,190,213,102,33,168,121,152,73,15,102,114,76,178,254,151,196,64,72,191,69,188,74,68,52,141,
69,93,242,51,145,199,154,16,197,113,117,89,85,170,84,113,179,183,100,248,111,19,64,128,253,25,55,226,242,242,229,203,207,160,194,235,146,71,8,86,105,117,234,212,201,66,212,46,134,114,162,200,14,87,83,
58,109,230,101,43,17,197,136,116,90,98,96,89,185,114,101,43,226,191,10,209,6,33,88,102,134,135,135,23,88,0,230,60,92,187,99,199,14,249,157,137,150,165,12,143,196,94,56,50,144,60,227,92,63,112,224,64,41,
52,147,49,2,9,27,88,90,2,159,238,128,108,31,164,66,64,246,99,109,164,16,115,230,204,249,39,235,100,115,13,196,252,136,153,57,223,168,48,49,83,95,85,80,24,160,139,81,66,173,239,155,200,156,139,15,98,139,
124,24,75,40,12,27,48,44,130,232,196,244,1,34,33,162,167,176,212,96,99,4,70,78,231,199,64,73,33,92,33,109,192,249,243,231,237,22,128,153,38,147,68,181,105,157,150,150,102,117,62,203,20,62,124,89,66,141,
135,132,132,200,0,200,91,33,179,204,84,225,184,162,120,241,226,54,157,15,190,125,130,130,130,166,40,108,129,192,145,69,141,26,53,236,18,128,193,141,140,80,243,35,70,140,96,231,54,223,53,170,169,168,26,
39,177,199,201,250,151,153,48,83,66,91,5,32,53,108,216,240,39,192,113,166,194,30,78,253,250,245,237,190,5,102,203,180,123,214,88,214,4,48,101,92,53,21,85,227,252,206,154,152,227,43,86,172,176,59,242,51,
215,82,216,132,98,73,107,47,226,168,37,151,90,223,217,195,184,73,147,70,230,87,172,7,11,18,147,152,44,42,236,162,157,61,123,54,192,222,54,18,219,26,244,35,40,65,58,44,35,58,29,218,22,198,181,196,170,141,
160,192,53,166,64,96,99,2,234,160,176,13,136,52,56,212,222,197,140,230,116,102,58,38,219,37,20,204,86,198,181,113,128,239,7,184,142,62,177,119,239,94,187,107,46,166,235,10,251,152,141,26,53,122,28,19,
19,83,213,90,245,173,210,214,173,91,217,199,54,114,78,254,167,137,177,61,64,255,200,79,0,50,207,155,100,55,128,196,222,135,250,217,30,138,142,142,190,132,122,227,63,45,184,132,132,132,223,35,255,127,142,
124,201,209,156,141,243,186,121,213,41,41,41,6,83,33,28,110,217,178,69,50,204,113,246,160,136,50,140,176,106,221,77,199,231,126,106,61,193,223,40,188,74,190,190,190,242,69,22,29,219,94,130,249,62,152,
59,119,110,163,244,244,244,74,114,53,11,18,54,124,225,233,219,192,136,163,105,138,240,236,217,51,17,28,28,44,136,98,196,120,83,77,179,59,247,240,225,67,249,153,206,73,228,226,203,175,251,247,239,75,135,
165,208,56,67,10,202,215,149,124,199,69,103,46,40,229,228,228,100,34,237,112,131,249,253,5,245,199,91,131,10,216,177,142,143,143,15,246,241,241,217,119,239,222,61,69,123,35,140,35,166,239,178,172,148,
238,5,45,146,108,46,178,107,213,170,85,22,153,246,159,3,3,3,147,243,100,177,172,166,216,25,175,91,183,238,131,217,179,103,123,176,199,252,255,68,136,35,187,23,44,88,208,248,209,163,71,85,113,147,239,44,
214,19,108,237,195,201,60,227,226,226,134,226,54,86,160,186,114,46,138,230,74,97,232,12,8,233,202,31,55,111,222,156,152,154,154,250,179,205,149,29,95,33,240,65,121,56,8,53,198,178,249,243,231,255,196,
158,249,111,200,123,54,52,127,104,230,204,153,127,2,226,173,132,95,181,40,112,141,205,87,21,124,224,160,158,64,164,13,80,74,235,168,168,40,29,242,37,247,130,160,74,126,14,139,90,33,25,254,215,18,213,219,
97,100,183,67,199,140,25,243,174,200,186,29,124,37,162,173,166,248,130,1,41,252,244,11,23,46,252,1,152,159,209,190,125,123,71,100,148,222,76,202,108,209,50,202,205,135,136,214,175,144,78,187,32,127,171,
221,172,89,179,115,211,166,77,139,142,136,136,56,161,182,81,109,165,127,3,18,33,62,210,0,40,7,24,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* Mcfx_gain_delayAudioProcessorEditor::phase_symbol_over_png = (const char*) resource_Mcfx_gain_delayAudioProcessorEditor_phase_symbol_over_png;
const int Mcfx_gain_delayAudioProcessorEditor::phase_symbol_over_pngSize = 3018;

// JUCER_RESOURCE: solo_symbol_png, 3173, "solo_symbol.png"
static const unsigned char resource_Mcfx_gain_delayAudioProcessorEditor_solo_symbol_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,49,0,0,0,49,8,6,0,0,1,4,155,113,138,0,0,0,9,112,72,89,
115,0,0,14,197,0,0,14,197,1,71,108,236,255,0,0,12,23,73,68,65,84,120,218,205,89,123,80,83,103,22,63,132,11,18,94,193,32,4,17,65,80,131,136,162,40,176,65,170,168,104,113,128,21,80,68,11,62,43,214,161,148,
118,59,253,195,157,110,103,156,177,213,218,233,195,22,180,86,132,42,165,172,11,226,98,21,108,209,10,190,80,86,16,71,20,171,236,2,21,40,15,33,148,135,128,70,52,123,206,133,27,19,184,137,36,98,119,207,204,
133,36,223,227,60,190,243,248,157,239,50,74,165,18,56,138,136,136,40,59,126,252,184,15,247,157,161,63,177,177,177,74,91,91,91,112,118,118,134,183,223,126,155,157,157,148,148,100,196,14,210,0,71,201,201,
201,144,152,152,8,237,237,237,98,134,182,162,21,220,128,173,155,47,251,249,181,215,94,43,100,136,199,192,86,221,176,53,62,1,14,236,223,135,67,50,8,11,11,75,99,84,251,129,21,152,226,55,218,146,8,255,39,
51,28,115,226,65,91,209,10,26,96,165,229,84,41,41,41,137,184,115,231,142,235,198,141,27,247,168,212,224,68,39,42,47,47,255,130,85,129,251,113,64,244,4,252,36,96,39,170,132,72,140,127,29,7,247,169,132,
80,13,36,239,255,22,98,163,194,84,178,50,180,31,45,229,102,18,173,90,181,106,129,74,100,107,107,235,110,59,59,187,182,234,234,106,87,13,113,89,85,173,172,148,155,54,109,130,161,84,89,89,9,103,207,158,
53,210,56,17,110,178,250,130,150,166,38,48,179,180,6,145,149,5,120,122,122,178,90,60,124,248,16,82,82,82,140,24,50,200,208,221,185,211,26,74,102,102,102,64,162,50,100,193,161,131,234,11,134,110,64,186,
49,100,242,140,140,140,47,72,4,62,26,202,145,140,193,234,196,41,249,198,27,111,40,73,4,62,34,115,206,159,63,255,226,48,235,113,20,31,31,127,170,175,175,111,204,225,195,135,131,134,142,169,78,238,234,213,
171,43,81,196,28,35,35,35,48,49,49,97,31,206,15,208,220,151,81,146,0,213,2,212,231,58,90,99,54,77,230,35,212,115,30,30,71,111,119,119,183,57,187,128,38,171,79,120,218,175,128,86,249,239,32,145,72,84,191,
225,81,8,207,157,59,183,144,17,10,133,202,45,91,182,104,152,52,54,46,129,157,156,131,159,189,95,79,132,201,22,3,99,153,153,153,69,140,250,100,142,172,133,2,246,127,212,16,211,226,230,192,240,217,255,124,
126,14,84,212,52,241,158,7,195,167,100,96,104,20,4,170,68,252,1,23,133,63,91,144,159,159,15,161,161,161,26,58,120,202,150,192,98,95,119,54,114,252,194,55,106,114,192,35,55,82,15,70,117,17,134,138,83,92,
92,92,206,138,148,158,158,222,187,97,195,6,115,208,65,150,150,150,103,174,93,187,246,42,187,160,179,179,211,194,193,193,225,126,116,116,180,29,223,228,154,154,154,179,121,121,121,175,106,40,221,220,220,
108,79,255,49,171,228,97,134,113,29,51,102,140,226,238,221,187,146,198,198,70,71,13,29,248,156,143,163,29,59,118,100,125,252,241,199,209,50,153,12,102,206,156,57,108,188,163,163,131,244,34,113,107,111,
220,184,225,166,109,159,97,71,65,190,216,210,210,50,123,234,212,169,236,119,62,103,224,200,198,198,134,59,21,87,206,216,100,67,50,139,214,168,216,190,125,123,14,109,142,62,12,134,18,29,18,49,204,206,206,
110,229,204,201,112,102,105,109,109,141,230,164,31,70,138,22,72,62,144,205,59,100,59,125,9,196,4,121,12,251,157,14,119,220,184,113,29,109,109,109,54,44,147,162,162,162,104,62,155,15,114,96,25,24,79,13,
134,55,151,73,135,140,117,195,217,226,187,208,210,169,0,137,200,116,216,202,152,152,24,17,57,5,67,25,70,59,3,34,83,136,244,119,134,220,43,5,144,252,239,2,246,23,233,244,57,48,219,219,11,36,98,43,8,10,
240,209,105,62,242,58,134,82,24,101,37,93,228,228,19,14,137,67,246,234,110,111,192,144,59,60,96,50,239,112,136,121,197,153,119,45,185,53,67,57,114,253,250,245,74,242,20,62,186,117,124,47,20,213,43,33,
120,125,2,72,69,130,103,149,74,236,196,134,41,197,185,252,250,117,0,45,76,40,110,216,51,65,31,175,13,12,12,116,229,155,52,35,226,45,152,209,223,137,213,116,31,20,240,140,47,142,92,15,158,78,34,94,6,88,
221,126,196,192,12,225,152,184,5,5,5,21,83,62,230,143,38,17,111,181,211,69,131,41,34,68,35,78,184,234,128,5,65,169,239,134,234,132,41,254,148,183,183,247,190,144,144,144,83,90,35,30,211,12,91,118,168,
88,68,69,69,29,143,140,140,20,81,90,215,69,100,22,204,130,18,124,230,242,26,66,87,238,226,232,241,227,199,230,123,247,238,221,93,86,86,54,7,189,209,100,210,164,73,53,232,250,59,48,120,127,121,222,90,70,
219,0,110,36,242,240,240,104,100,24,198,60,32,32,0,68,34,145,10,63,246,247,247,251,161,71,173,169,168,168,160,84,244,244,200,145,35,171,194,195,195,255,57,98,6,238,238,238,221,46,46,46,150,203,151,47,
215,41,157,151,151,23,61,130,51,103,206,28,91,187,118,109,255,189,123,247,36,98,177,184,93,43,131,39,79,158,152,96,66,84,108,222,188,89,175,3,22,8,4,84,232,153,213,171,87,203,215,172,89,179,5,215,167,
242,50,64,115,40,94,196,147,208,164,128,218,28,68,134,79,144,225,33,13,6,4,21,95,100,115,142,236,237,237,1,207,36,5,61,48,23,51,70,7,203,96,231,206,157,71,176,59,120,238,226,238,206,118,232,125,248,24,
63,25,131,173,221,56,96,4,252,243,166,77,155,198,160,167,213,99,133,180,98,25,124,246,217,103,107,214,173,91,167,117,227,75,57,123,225,122,19,191,59,111,69,173,77,121,126,199,67,183,196,32,14,98,208,205,
86,224,225,104,221,188,163,170,144,221,92,224,22,0,9,161,115,158,253,222,80,9,25,185,133,112,32,57,19,83,74,44,239,193,163,235,158,96,80,242,111,131,131,131,181,231,151,95,110,15,160,86,181,205,217,218,
237,228,9,50,233,175,96,238,58,77,235,218,197,139,23,203,25,204,126,34,93,118,119,156,56,17,160,174,14,78,94,169,133,112,127,205,4,236,27,28,170,243,204,240,28,38,50,142,142,142,58,39,57,204,65,32,88,
156,12,117,101,121,144,92,70,191,140,135,192,224,185,48,93,234,170,61,13,168,187,254,243,146,25,135,9,155,170,43,33,231,84,33,126,107,130,243,5,121,248,12,2,128,184,68,144,232,216,130,193,68,54,34,255,
30,63,217,19,25,121,114,152,29,42,174,156,133,243,229,85,144,157,154,172,179,110,48,114,185,92,255,104,18,48,224,21,16,12,19,108,140,224,239,133,119,161,73,129,2,152,106,97,208,211,211,163,96,97,5,47,
245,99,29,222,207,11,157,137,30,245,245,233,148,3,129,64,37,147,144,144,144,214,219,219,27,175,45,155,251,140,53,130,178,223,149,200,40,19,162,98,151,195,120,132,49,253,138,62,184,126,241,52,148,220,174,
27,48,159,22,241,16,231,246,49,187,119,239,126,19,1,64,252,172,89,179,120,39,249,175,125,11,108,74,10,224,231,210,42,200,201,60,172,49,102,36,118,131,183,98,67,117,97,254,223,89,79,171,171,171,107,70,
6,14,90,179,164,44,152,125,244,33,172,148,39,78,159,62,29,206,50,168,173,173,29,63,99,198,140,39,24,121,2,24,5,194,180,223,219,208,208,240,72,35,93,127,254,249,231,33,135,14,29,250,137,210,237,139,18,
2,188,71,8,39,163,53,24,96,62,42,192,30,96,77,86,86,214,63,38,76,152,96,240,230,216,75,116,225,62,98,222,138,134,176,60,11,107,170,124,219,182,109,217,88,232,199,234,179,49,130,130,42,220,92,136,155,59,
235,44,250,75,150,44,249,25,49,142,24,139,249,77,212,68,130,208,196,78,215,198,88,195,171,15,30,60,104,149,155,155,187,233,195,15,63,188,60,98,216,130,144,132,197,251,95,126,249,229,95,208,149,255,106,
110,110,254,216,217,217,185,195,204,204,76,241,224,193,3,35,132,158,83,23,45,90,84,148,145,145,177,238,163,143,62,234,212,122,224,35,1,94,186,136,10,86,82,82,210,182,210,210,82,119,244,28,17,21,26,11,
11,11,22,71,161,55,177,23,25,196,3,177,20,1,56,232,234,234,2,20,144,160,170,98,202,148,41,77,120,246,167,222,123,239,189,237,118,118,118,173,6,123,173,62,147,81,16,211,79,62,249,36,253,171,175,190,90,
133,2,25,207,158,61,155,112,26,160,111,128,174,194,168,181,179,2,112,161,76,245,193,7,31,196,223,185,115,135,64,255,211,177,99,199,222,79,73,73,217,184,116,233,210,130,81,85,2,251,213,163,159,126,250,
105,148,171,171,43,204,155,55,15,16,195,193,104,18,230,94,160,140,136,15,229,19,135,156,156,156,159,222,125,247,221,167,216,52,119,103,102,102,174,166,72,53,72,9,2,172,8,54,111,28,61,122,212,35,34,34,
2,248,174,21,95,22,145,82,131,9,82,132,167,242,211,178,101,203,0,229,88,141,205,75,246,136,149,32,203,163,219,68,209,253,133,190,168,123,180,137,114,36,85,214,99,199,142,101,197,197,197,165,225,41,69,
82,230,209,170,4,249,60,54,161,114,180,132,229,255,90,248,161,132,129,15,216,242,91,82,94,70,197,46,157,56,113,98,249,48,37,168,75,195,70,74,142,193,106,140,128,220,64,86,79,17,38,255,7,110,85,254,2,215,
171,234,116,206,156,60,221,23,220,103,120,194,100,137,149,94,28,168,176,84,85,85,45,164,252,207,165,104,70,173,39,106,196,83,48,80,129,30,40,61,122,8,74,154,53,211,245,120,169,23,184,77,176,7,145,208,
20,20,136,127,250,122,58,161,254,110,57,212,97,239,88,125,187,148,125,88,212,237,19,14,43,253,157,71,204,77,42,149,90,97,138,150,96,220,20,34,45,102,6,187,148,127,81,159,172,251,174,73,59,53,150,253,172,
166,128,23,196,38,6,130,88,203,220,57,62,236,141,6,116,85,95,128,244,83,55,6,215,255,0,199,152,40,88,233,59,126,196,60,169,2,95,188,120,209,141,10,37,1,111,115,44,181,126,24,249,6,186,144,2,218,91,234,
159,1,65,137,57,8,71,176,202,122,242,2,12,216,5,47,20,39,8,243,36,84,233,25,186,2,193,14,154,173,176,134,145,41,76,245,89,0,69,53,231,7,0,101,75,9,164,38,151,176,168,86,54,107,26,76,114,113,4,177,149,
16,4,47,33,216,177,48,154,33,84,105,99,232,14,71,253,53,157,65,121,93,226,133,86,245,130,182,170,18,40,40,40,5,186,46,81,182,215,192,149,34,124,120,87,136,65,234,229,1,158,51,221,193,73,108,241,66,188,
9,107,209,149,168,9,97,156,209,160,113,82,25,196,226,243,236,21,77,31,116,96,27,215,214,214,12,245,191,254,6,183,107,184,140,213,14,85,21,197,236,51,24,170,16,181,53,88,107,179,163,139,8,44,210,221,70,
205,173,91,183,252,94,70,110,23,48,66,16,75,156,216,71,234,233,3,234,239,191,30,181,87,193,143,153,5,48,16,77,216,100,29,232,135,232,248,80,144,232,105,79,66,187,116,3,190,3,75,187,193,96,168,187,173,
1,154,90,238,195,253,134,122,144,155,121,64,104,160,116,68,128,108,140,88,10,161,209,15,224,155,236,98,67,176,40,75,136,144,123,9,174,51,116,143,137,207,61,44,28,46,116,35,168,47,25,61,184,7,5,133,229,
131,223,234,224,155,251,237,176,110,165,12,68,207,137,228,222,166,10,72,203,41,86,203,106,98,176,214,83,15,185,92,126,13,251,141,129,43,56,108,79,125,177,90,55,35,172,22,16,248,210,135,44,39,5,64,98,156,
11,20,164,230,162,83,96,64,55,151,194,119,251,74,245,218,195,69,22,9,203,125,157,244,90,131,25,181,6,155,92,111,236,97,156,152,65,92,210,74,23,190,27,54,108,56,138,133,79,64,141,141,94,36,116,130,96,4,
105,108,71,209,223,3,45,141,191,65,93,125,19,220,111,235,128,62,181,11,32,19,19,33,152,89,90,130,173,157,61,56,56,58,129,227,56,43,131,82,47,165,213,212,212,84,203,147,39,79,254,25,75,67,167,234,0,233,
198,26,225,110,200,138,21,43,242,98,98,98,24,125,79,228,25,164,180,0,137,179,148,125,94,6,81,147,141,181,205,62,63,63,63,20,123,155,203,195,162,137,186,41,186,25,71,108,82,227,237,237,77,215,246,255,87,
72,214,216,216,248,120,122,122,250,92,148,209,5,27,251,46,173,253,4,93,237,211,219,193,180,180,180,184,119,222,121,103,111,88,88,216,152,209,184,92,121,17,66,255,191,180,107,215,174,87,208,229,179,246,
236,217,19,57,226,206,142,94,33,208,131,193,179,9,91,197,36,63,63,63,51,186,51,255,163,4,167,43,43,165,82,121,26,45,31,248,245,215,95,239,195,207,243,13,238,177,233,85,5,61,29,29,29,54,216,148,124,119,
225,194,133,69,50,153,76,57,101,202,20,43,189,19,192,8,2,182,167,167,231,242,247,223,127,191,0,93,251,12,6,111,28,90,190,107,212,110,59,232,149,136,122,55,69,47,24,118,238,220,249,183,171,87,175,254,9,
235,76,235,196,137,19,5,24,116,118,4,202,70,82,164,208,0,181,173,173,173,237,216,224,8,111,222,188,57,205,223,223,255,202,251,239,191,191,107,225,194,133,231,232,197,171,62,244,95,103,164,1,103,57,59,
205,7,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* Mcfx_gain_delayAudioProcessorEditor::solo_symbol_png = (const char*) resource_Mcfx_gain_delayAudioProcessorEditor_solo_symbol_png;
const int Mcfx_gain_delayAudioProcessorEditor::solo_symbol_pngSize = 3173;

// JUCER_RESOURCE: solo_symbol_over_png, 3175, "solo_symbol_over.png"
static const unsigned char resource_Mcfx_gain_delayAudioProcessorEditor_solo_symbol_over_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,49,0,0,0,49,8,6,0,0,1,4,155,113,138,0,0,0,9,112,72,
89,115,0,0,14,197,0,0,14,197,1,71,108,236,255,0,0,12,25,73,68,65,84,120,218,205,89,11,84,212,101,22,191,131,127,40,94,205,96,14,227,163,20,200,32,21,55,69,5,211,64,4,45,197,18,12,80,82,129,53,95,41,184,
6,190,16,203,20,141,4,54,4,20,246,44,34,134,120,52,18,212,82,30,137,8,130,226,250,0,20,100,87,176,16,16,89,229,161,152,130,167,49,152,189,247,207,204,48,3,255,25,103,0,107,239,57,159,14,243,61,238,227,
187,223,239,62,134,145,72,36,32,163,192,192,192,171,145,145,145,19,101,127,51,244,207,141,27,55,36,214,214,214,128,19,244,167,108,53,143,157,164,9,25,241,120,60,160,211,30,60,120,48,144,145,30,37,159,
24,57,107,35,251,121,221,186,117,103,25,41,15,60,170,26,30,182,138,193,196,80,15,255,12,3,27,27,155,253,140,252,60,48,3,129,1,128,76,192,53,107,214,236,145,77,242,136,7,29,69,59,104,130,149,86,182,178,
186,186,218,13,165,54,255,224,131,15,118,203,213,32,126,102,102,102,64,3,41,82,166,130,164,75,116,49,126,210,101,23,202,133,144,180,222,197,73,61,185,16,242,9,158,225,48,40,203,63,14,138,166,226,1,43,
109,151,61,11,10,10,28,228,34,243,249,252,199,131,7,15,110,170,168,168,48,183,183,183,239,18,151,200,212,212,84,210,208,208,0,221,41,62,62,30,86,172,88,193,83,186,17,217,98,197,13,151,10,10,192,228,245,
55,192,210,108,40,109,96,101,71,35,193,192,129,3,121,12,25,164,251,233,178,219,234,78,184,1,72,84,134,44,40,53,150,156,20,55,116,63,128,116,99,200,228,40,115,164,84,132,30,212,157,35,25,131,213,73,166,
36,202,44,33,17,184,136,204,137,150,43,80,186,108,69,138,136,136,200,120,252,248,241,75,33,33,33,206,221,231,228,55,215,216,216,232,62,104,208,160,84,146,127,195,134,13,114,201,232,31,20,191,240,212,169,
83,83,229,27,226,226,226,74,86,175,94,61,14,84,16,46,158,130,226,182,161,216,6,236,134,238,139,159,181,181,64,113,89,5,216,217,217,201,191,195,197,250,121,121,121,142,140,177,177,177,4,229,85,50,233,141,
6,49,187,216,6,63,111,190,43,1,207,161,157,115,248,210,114,25,197,197,50,50,23,178,62,11,197,221,12,130,58,2,195,101,255,21,115,38,192,190,140,98,206,251,96,184,148,140,79,47,130,120,185,136,211,112,211,
185,174,13,179,103,207,134,204,204,76,37,29,124,182,238,131,164,237,190,236,203,9,58,85,165,204,1,23,243,20,112,68,73,132,238,226,76,152,48,161,152,21,233,213,87,95,109,107,110,110,54,0,53,132,7,103,23,
21,21,189,199,110,192,197,134,195,134,13,107,184,123,247,174,144,107,177,167,167,103,206,209,163,71,223,83,82,26,23,155,74,221,224,84,71,71,135,185,190,190,190,248,226,197,139,162,250,250,250,161,184,
184,75,7,46,231,147,209,137,19,39,82,188,189,189,231,135,134,134,18,102,245,152,175,172,172,132,181,107,215,194,147,39,79,110,163,71,91,168,58,167,199,85,144,47,10,4,130,113,11,23,46,4,55,55,55,224,114,
6,25,89,90,90,202,110,197,92,102,108,169,13,13,85,190,138,236,236,236,84,244,95,232,11,73,47,73,130,182,111,148,153,147,145,153,197,213,213,117,62,73,207,73,143,46,1,79,48,153,115,106,228,162,120,184,
117,104,121,143,239,233,114,17,118,90,238,221,187,39,96,153,220,185,115,103,62,57,32,55,181,176,12,12,220,146,160,245,184,79,183,185,106,240,249,236,0,92,170,104,1,59,43,65,143,157,200,128,79,78,193,16,
194,40,128,6,7,9,32,251,203,105,48,147,245,126,95,246,27,247,197,107,97,67,208,103,96,55,198,12,14,70,109,87,107,62,242,58,122,229,47,61,207,206,51,182,229,129,100,155,242,119,183,203,115,144,169,57,251,
249,205,53,233,80,25,227,194,185,151,220,154,33,140,68,87,148,144,167,112,209,94,7,93,88,83,240,59,36,223,20,195,98,43,221,46,184,25,227,204,62,83,50,243,173,61,97,0,42,152,208,187,97,239,100,233,210,
165,228,231,230,92,139,252,243,159,129,255,211,74,224,25,232,129,55,199,124,236,153,155,176,218,217,138,147,193,206,157,59,51,241,97,186,48,210,208,96,129,23,116,129,240,152,91,103,75,80,247,104,213,64,
132,139,210,59,145,69,7,84,95,162,237,129,138,84,88,88,152,209,210,210,18,139,12,50,84,190,120,100,192,250,50,5,11,47,47,175,19,24,146,249,4,235,234,136,204,114,252,248,113,17,34,225,132,41,83,122,26,
131,209,80,106,131,31,126,248,97,215,213,171,87,109,16,167,116,241,53,87,121,120,120,132,96,62,240,159,231,109,100,84,77,32,99,254,212,169,83,235,17,135,12,118,239,222,13,136,6,236,144,146,45,14,175,232,
232,104,248,252,243,207,59,82,82,82,60,93,92,92,142,105,204,224,253,247,223,127,140,38,50,66,187,170,149,142,144,23,135,78,123,123,123,26,154,242,119,124,2,34,12,224,15,212,49,208,69,137,197,8,108,90,
93,240,128,1,3,160,169,169,137,73,76,76,164,141,203,63,249,228,147,4,78,6,232,73,226,190,120,18,30,12,120,79,251,14,28,56,208,190,100,201,146,3,74,12,68,34,81,159,220,84,70,19,39,78,132,107,215,174,197,
163,203,30,199,24,212,194,72,3,238,145,91,183,110,61,119,115,117,101,57,220,127,248,4,63,233,193,88,155,241,96,160,203,189,110,217,178,101,12,154,250,14,154,218,152,101,240,233,167,159,122,213,212,212,
168,190,76,155,1,16,83,210,193,57,247,16,181,22,112,124,127,255,254,125,163,156,156,28,103,6,49,253,163,178,178,50,149,135,87,36,251,176,135,235,207,10,133,182,204,205,93,223,159,141,133,183,156,253,193,
132,247,6,186,244,47,61,221,147,97,40,60,255,200,44,90,180,40,17,57,169,100,144,182,239,112,103,224,83,56,156,200,202,201,15,182,126,148,1,166,174,62,42,247,110,217,178,165,153,41,45,45,229,171,179,187,
211,236,119,17,25,207,193,204,141,39,32,47,220,77,105,110,123,90,186,218,59,11,10,10,122,157,161,50,69,29,77,222,156,7,16,204,131,115,17,243,128,23,65,223,216,64,100,210,23,176,210,199,13,12,52,240,42,
102,200,144,33,207,93,68,238,155,159,18,11,211,188,252,41,203,133,64,223,121,56,164,85,79,131,4,108,133,106,24,60,122,244,72,35,255,118,88,224,7,18,28,210,156,29,34,55,248,192,186,232,52,176,51,229,169,
141,27,140,58,15,82,73,186,6,16,24,149,10,239,89,122,194,88,191,84,56,223,2,240,174,64,5,131,186,186,58,49,251,114,56,169,13,225,195,144,51,117,102,11,141,134,70,181,114,20,23,23,151,51,27,55,110,220,
143,159,87,169,8,3,176,193,76,7,34,170,59,144,209,27,144,127,35,7,236,49,141,105,107,105,132,176,191,121,64,72,114,62,187,74,149,244,88,162,63,101,54,109,218,180,26,241,126,85,64,64,0,231,162,240,219,
237,240,230,38,15,88,17,158,6,14,214,202,121,130,142,249,108,104,175,202,80,109,73,93,221,135,44,84,36,36,36,220,67,6,131,85,45,92,30,150,138,67,187,107,66,161,127,60,125,250,180,43,203,160,188,188,124,
136,143,143,79,251,193,131,7,117,160,127,168,13,115,164,223,200,42,114,184,70,200,112,65,44,207,34,184,237,43,97,84,251,13,43,205,249,74,241,0,195,228,79,169,169,169,94,24,212,191,115,116,116,236,245,
225,24,3,126,197,88,48,144,51,162,97,166,144,114,230,204,153,230,197,139,23,127,127,232,208,33,19,109,14,198,58,164,18,171,62,125,60,124,184,218,160,63,99,198,140,51,56,6,190,253,246,219,101,31,127,252,
177,8,1,75,168,238,96,196,253,95,112,173,241,177,99,199,150,212,214,214,22,106,156,182,92,191,126,125,44,253,31,21,21,245,217,174,93,187,130,80,245,103,88,140,183,24,25,25,137,49,134,240,48,227,120,19,
77,153,155,156,156,236,141,127,171,196,27,166,175,113,152,2,214,254,253,251,55,157,61,123,214,170,164,164,132,79,25,6,38,102,96,97,97,1,134,134,134,240,202,43,175,80,253,192,214,133,173,173,173,80,85,
85,69,21,18,33,131,216,198,198,230,191,104,173,12,76,22,190,20,10,133,141,189,149,129,209,114,189,30,230,8,73,232,226,158,79,159,62,29,176,126,253,122,240,245,245,165,224,197,14,109,207,194,49,130,144,
10,125,99,21,37,144,223,124,243,77,7,159,207,111,136,137,137,249,171,179,179,243,79,253,170,4,38,250,71,177,116,241,152,59,119,46,49,130,155,55,111,66,127,18,186,17,208,219,199,65,120,50,24,149,202,66,
124,233,192,135,243,24,83,185,5,244,82,123,171,132,238,183,223,126,123,61,48,48,112,20,22,14,228,248,240,71,17,41,37,5,72,126,110,110,110,22,21,137,8,55,11,220,221,221,191,215,88,9,178,60,186,137,71,122,
122,58,181,202,224,207,164,233,211,167,179,145,21,35,101,10,166,166,251,177,46,153,71,200,163,78,9,61,188,186,230,225,195,135,27,105,91,50,188,104,66,16,160,254,140,17,225,242,158,61,123,206,99,133,55,
183,135,18,84,165,141,26,53,170,25,163,246,0,44,39,122,201,234,25,84,228,124,7,113,255,72,128,152,180,124,181,43,93,22,111,4,95,191,85,48,127,178,153,86,28,40,176,196,198,198,58,18,254,203,32,90,174,4,
149,153,126,126,126,189,84,160,30,182,142,31,14,59,174,181,43,247,34,221,87,128,251,244,73,96,49,200,24,33,182,9,26,235,110,65,102,114,12,20,84,73,32,227,80,56,59,22,80,169,180,62,29,174,68,184,104,204,
13,229,52,70,200,22,57,57,57,33,178,159,117,98,149,248,234,171,175,46,81,157,204,213,64,212,132,242,183,45,84,80,96,57,148,75,226,97,180,138,181,155,183,69,117,182,143,190,95,9,22,11,58,27,189,87,255,
62,7,38,233,159,131,43,33,14,26,243,164,8,156,157,157,109,65,129,146,148,48,64,216,180,189,124,249,114,47,93,168,5,74,47,95,232,130,180,191,12,1,161,6,187,204,231,255,19,36,56,250,66,40,183,104,214,172,
89,65,12,181,64,240,33,195,200,145,35,123,11,136,176,104,91,56,172,201,12,236,124,21,165,33,96,202,11,97,179,218,47,214,249,130,235,28,39,176,54,19,130,238,11,120,236,227,198,141,123,25,225,184,137,161,
30,206,216,177,99,251,116,152,137,109,0,2,67,0,20,39,111,2,79,159,112,160,150,126,199,237,76,216,238,79,131,107,135,5,184,47,95,6,171,214,250,130,243,152,161,125,226,77,185,22,67,77,40,76,184,250,7,6,
189,195,224,23,239,174,90,224,89,91,35,84,148,149,66,113,81,1,100,167,231,194,161,12,25,98,85,97,141,30,204,142,78,114,135,130,135,169,42,139,29,117,68,201,34,67,93,180,11,23,46,216,190,8,108,215,53,16,
130,181,157,51,59,124,86,111,131,100,197,86,200,191,147,193,117,140,47,20,176,191,3,164,129,189,137,11,92,106,205,0,91,3,237,120,80,182,203,80,27,16,211,96,175,222,10,122,187,36,7,10,10,47,67,81,110,22,
92,55,89,10,25,251,124,52,234,21,152,140,246,134,204,75,117,96,100,23,44,47,77,123,83,115,81,186,206,80,31,115,252,248,241,53,209,209,209,35,168,35,168,117,42,90,123,18,124,253,163,101,96,11,130,203,229,
112,227,106,24,88,62,231,37,55,156,223,13,34,251,96,5,84,27,3,230,90,234,17,26,26,90,132,245,70,103,11,238,240,225,195,147,48,255,191,135,249,146,14,37,95,218,208,48,215,40,144,52,126,8,238,194,153,112,
12,93,227,89,105,56,88,233,133,107,117,134,227,23,103,32,183,231,143,173,240,156,74,172,234,235,175,191,30,95,87,87,247,26,171,4,21,36,212,240,197,151,126,180,166,166,70,135,10,27,173,104,144,51,164,73,
164,109,192,182,122,248,87,94,14,100,158,62,15,87,74,110,194,253,39,79,186,30,161,225,96,16,154,189,6,214,227,38,129,189,227,12,112,176,49,235,21,244,138,197,226,38,76,59,140,78,158,60,249,33,214,31,143,
228,105,7,117,172,147,146,146,92,68,34,209,169,159,127,254,153,209,246,70,186,186,46,67,97,178,139,55,59,94,4,81,145,109,101,101,101,138,153,246,156,41,83,166,20,246,200,98,169,154,162,206,248,232,209,
163,171,118,238,220,201,167,30,243,255,19,237,221,187,247,68,120,120,248,132,234,234,234,17,88,246,254,170,178,158,160,214,126,125,125,189,32,49,49,113,25,222,198,94,172,174,94,234,143,230,74,95,232,60,
146,189,189,253,187,71,142,28,73,169,173,173,157,167,113,101,71,63,33,208,192,242,112,9,214,24,49,97,97,97,47,83,207,252,15,148,189,13,45,127,122,235,214,173,211,226,226,226,98,177,84,176,239,117,141,
77,63,85,208,192,186,87,224,234,234,122,16,141,50,61,36,36,68,178,114,229,74,99,106,161,247,39,209,131,197,90,161,112,199,142,29,14,88,189,101,39,36,36,44,243,247,247,255,181,223,186,29,244,147,136,98,
53,69,63,48,96,10,191,229,202,149,43,118,182,182,182,141,51,103,206,212,193,140,82,72,73,153,38,86,198,114,243,118,86,86,214,3,76,167,245,49,127,123,235,157,119,222,185,24,28,28,28,26,16,16,144,167,170,
141,170,138,254,7,127,25,105,77,112,230,42,29,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* Mcfx_gain_delayAudioProcessorEditor::solo_symbol_over_png = (const char*) resource_Mcfx_gain_delayAudioProcessorEditor_solo_symbol_over_png;
const int Mcfx_gain_delayAudioProcessorEditor::solo_symbol_over_pngSize = 3175;

// JUCER_RESOURCE: solo_symbol_act_png, 3257, "solo_symbol_act.png"
static const unsigned char resource_Mcfx_gain_delayAudioProcessorEditor_solo_symbol_act_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,49,0,0,0,49,8,6,0,0,1,4,155,113,138,0,0,0,9,112,72,
89,115,0,0,14,197,0,0,14,197,1,71,108,236,255,0,0,12,107,73,68,65,84,120,218,205,89,9,88,83,87,22,62,47,121,9,100,129,132,29,7,91,4,171,168,31,84,5,171,117,129,34,210,98,41,20,149,77,139,130,160,173,74,
101,220,171,216,106,45,45,74,91,69,112,97,58,86,68,197,78,101,145,106,65,16,113,161,160,56,150,69,81,59,69,173,224,138,32,38,6,67,130,36,36,111,222,125,49,36,129,36,178,216,206,252,223,119,9,112,183,115,
206,61,123,112,130,32,64,141,149,171,199,85,37,111,173,26,167,254,27,71,63,174,213,113,8,215,17,116,72,222,138,254,50,127,190,250,41,70,77,162,9,53,48,76,12,4,97,6,66,161,208,18,87,29,165,153,120,45,0,
163,126,95,181,122,218,25,92,117,135,234,168,39,50,46,88,48,219,168,73,247,177,49,233,56,104,129,207,192,168,35,17,226,226,226,118,226,234,203,209,29,232,40,180,3,77,80,212,170,89,185,125,231,232,140,
107,215,26,156,2,222,91,177,189,139,13,116,223,16,71,0,52,0,62,79,126,206,130,138,8,53,233,234,133,93,68,16,50,51,157,201,174,9,140,41,134,171,231,217,160,37,170,167,24,218,170,57,6,160,188,252,184,87,
23,201,60,158,153,216,222,222,250,241,245,235,13,78,158,158,90,228,34,216,218,97,196,163,102,51,232,142,61,233,50,248,104,193,51,76,231,69,212,139,181,55,92,188,160,0,139,87,48,24,62,152,70,110,96,82,
92,8,197,4,88,154,137,49,28,9,164,251,233,186,44,107,96,105,134,1,34,21,71,18,84,9,75,3,237,13,221,15,64,188,225,72,228,123,210,227,147,85,36,244,68,247,27,145,48,40,158,212,76,10,197,102,4,34,65,31,144,
56,61,61,61,203,117,30,91,27,223,110,243,47,20,139,101,38,9,155,78,77,235,62,215,245,114,45,130,35,193,214,86,209,185,232,142,53,171,160,235,205,209,207,128,64,183,138,130,252,243,147,187,54,164,253,211,
253,82,236,162,63,198,128,1,20,228,95,157,100,105,201,150,10,133,82,54,181,161,251,98,185,28,160,230,138,2,38,120,104,12,69,40,196,89,165,165,165,222,184,153,57,70,136,159,234,138,244,218,19,14,181,216,
157,252,61,254,17,27,66,109,84,27,93,199,6,158,197,181,23,171,225,196,167,81,159,53,221,68,107,205,195,0,215,39,255,143,222,147,194,247,133,10,189,239,129,235,99,114,207,113,54,236,49,240,234,248,187,
254,18,40,42,228,232,240,16,153,128,195,129,13,44,234,247,117,39,77,117,111,40,42,84,96,26,63,162,75,66,119,114,60,60,134,213,80,36,89,89,41,164,2,1,157,13,70,80,116,114,98,73,117,117,241,59,212,6,129,
64,194,113,112,176,125,244,224,193,51,27,125,139,67,195,198,159,206,201,46,126,71,135,233,7,15,30,217,82,106,16,16,80,160,36,26,156,88,44,166,236,66,69,179,93,99,99,227,223,114,178,181,120,208,167,124,
106,28,205,15,207,154,23,145,29,182,57,201,4,226,98,123,154,193,141,122,37,44,91,218,14,109,109,142,13,229,101,245,206,134,206,233,241,20,72,23,249,252,186,49,31,132,227,48,35,16,64,159,50,168,49,220,
153,246,252,85,30,59,169,133,173,146,161,132,99,208,42,74,78,205,203,141,93,132,27,82,131,94,65,245,72,230,132,131,131,105,139,90,156,184,90,44,65,129,69,97,136,122,189,104,35,0,51,107,211,59,245,90,20,
13,110,238,231,244,248,63,122,92,251,65,124,81,211,67,17,159,58,245,222,189,99,97,24,48,13,82,135,46,96,147,43,36,89,220,30,115,145,43,219,225,98,61,105,169,206,244,30,115,77,15,149,60,164,20,56,242,48,
107,86,49,141,138,160,100,147,41,188,189,233,25,96,217,98,234,239,224,40,58,172,137,55,129,9,46,116,56,152,204,50,186,23,105,29,142,92,216,139,228,236,251,57,3,8,114,104,163,225,186,146,50,57,132,97,43,
233,112,99,155,126,123,65,106,141,35,31,121,163,158,75,32,77,209,135,93,94,237,16,87,222,9,153,245,108,152,235,164,17,137,147,11,141,50,83,116,209,205,100,210,139,108,211,79,32,178,27,234,77,22,204,183,
37,245,28,169,97,79,44,45,99,193,82,185,42,154,206,211,51,191,187,148,5,177,111,233,87,152,175,18,167,20,53,54,22,250,83,179,200,144,2,2,39,159,71,254,88,239,106,6,232,141,118,198,160,114,17,133,254,58,
118,162,142,14,24,134,17,125,61,80,27,21,191,122,21,138,30,199,238,206,201,246,47,52,104,241,228,13,84,36,68,193,98,246,156,25,71,175,213,41,120,200,173,27,3,18,203,79,121,205,118,213,213,213,30,147,198,
235,113,43,198,124,151,6,82,246,177,252,117,73,85,85,151,220,219,196,50,134,195,96,231,250,144,89,27,19,134,12,25,249,251,139,118,26,244,29,4,209,202,155,236,233,208,104,101,213,206,222,158,202,130,160,
64,26,57,212,179,191,147,180,30,159,157,186,75,6,159,173,151,43,179,14,231,132,250,251,207,202,235,245,5,126,211,205,197,179,63,144,115,43,206,33,155,224,24,164,110,217,82,38,26,52,5,204,63,98,109,29,
222,121,227,70,179,157,165,165,165,208,200,5,114,134,149,21,83,38,16,152,169,212,168,151,64,22,243,248,49,11,223,119,192,65,0,138,157,31,198,196,44,220,171,247,2,12,99,202,6,162,73,49,81,12,168,186,188,
236,251,140,12,186,34,58,58,58,67,231,2,59,251,129,169,169,26,227,198,208,225,242,165,165,123,68,162,153,63,241,249,124,17,174,10,184,115,126,188,217,192,125,225,230,219,100,196,107,126,162,210,58,55,
50,181,49,20,197,23,70,211,113,43,171,87,238,9,4,98,51,234,130,197,31,30,158,125,231,142,97,234,151,185,75,96,199,37,165,222,185,39,36,215,124,61,255,111,22,96,220,211,167,79,79,195,155,30,229,205,186,
90,103,152,250,235,135,20,212,225,44,178,114,145,230,107,214,93,47,85,192,136,169,82,176,48,144,64,35,202,103,204,120,255,103,60,34,34,102,223,233,18,195,214,122,100,79,7,245,217,154,175,75,132,139,55,
29,54,134,224,96,59,211,112,24,254,116,163,181,0,191,82,219,202,3,48,44,30,31,127,50,80,149,183,195,219,235,165,80,186,89,87,234,95,228,24,15,68,235,214,60,121,5,247,244,162,25,93,244,230,58,146,194,120,
128,95,182,40,0,219,162,10,60,201,63,152,192,162,15,152,192,238,133,86,225,131,6,209,94,184,8,201,184,44,183,19,222,10,109,87,21,169,17,29,212,160,170,30,50,89,30,207,55,124,6,222,218,74,244,74,191,189,
72,121,107,63,102,242,138,103,176,42,69,14,19,44,36,70,227,6,126,245,138,178,95,6,181,114,187,41,188,51,28,7,183,216,118,56,39,81,194,20,142,126,46,240,251,247,24,50,242,211,96,202,161,14,248,250,168,
20,54,27,231,190,166,118,200,111,248,39,107,23,164,3,28,90,98,104,209,154,33,52,248,246,182,42,179,40,171,99,131,39,153,198,72,201,100,237,235,143,159,65,194,193,78,106,141,33,234,87,45,55,105,199,215,
126,146,22,187,61,53,125,201,138,101,250,179,154,111,26,56,48,44,190,3,62,74,146,129,215,8,169,206,28,109,40,128,226,15,195,242,103,48,44,158,80,86,178,247,123,203,166,21,203,36,246,134,22,126,184,197,
132,26,125,193,246,20,159,159,79,158,60,26,68,93,240,219,181,135,131,34,231,51,21,7,247,155,210,224,165,0,151,94,168,96,118,172,88,174,229,174,35,230,228,251,87,93,158,117,2,185,219,129,194,210,18,58,
132,194,236,48,157,120,224,231,231,87,156,155,187,111,118,91,91,204,97,239,41,253,191,132,207,199,158,138,68,66,75,189,17,45,36,36,60,235,212,41,43,193,220,121,225,217,135,50,229,22,125,57,184,229,137,
195,13,143,209,18,150,72,116,247,85,163,65,223,215,215,247,148,175,175,192,114,244,104,183,171,115,230,182,216,173,91,35,181,49,118,112,179,112,208,173,209,163,68,102,121,121,233,209,119,239,78,170,232,
117,218,82,91,123,213,13,125,166,164,164,44,79,74,74,90,199,231,51,228,174,110,124,17,151,203,148,53,53,17,88,197,249,155,195,188,189,167,158,205,204,204,156,215,212,196,107,53,248,220,189,75,188,12,3,
5,172,244,244,111,214,158,57,83,231,114,169,186,149,71,39,73,118,112,192,192,121,40,6,28,14,6,230,230,24,40,73,111,36,22,19,32,145,16,80,127,139,128,7,247,9,210,51,48,101,238,30,131,30,250,250,250,23,
198,68,127,241,185,141,141,77,75,191,117,182,111,203,101,204,162,147,81,7,86,44,203,13,109,151,118,210,87,175,53,129,168,104,6,124,26,143,145,3,205,247,41,49,33,93,160,208,17,121,42,81,91,230,146,237,
169,50,216,182,181,83,201,227,217,62,218,145,186,127,254,180,105,126,197,47,149,137,130,194,176,156,5,49,57,33,239,7,209,97,91,10,11,234,126,103,193,203,4,159,139,1,242,136,228,32,253,137,196,94,212,22,
114,34,114,126,135,242,212,73,182,56,35,35,43,28,89,106,63,153,144,51,246,103,142,169,93,185,252,63,35,75,203,217,208,220,100,6,127,21,16,83,42,7,169,228,157,45,159,117,2,195,164,144,155,155,21,30,28,
28,150,221,107,38,144,228,163,230,229,132,28,63,193,6,161,224,175,35,94,31,166,122,210,169,200,90,115,37,58,203,202,106,65,122,86,214,79,51,145,231,49,194,132,140,233,55,221,90,240,170,163,148,43,248,
31,19,223,29,238,175,211,65,32,0,46,242,203,59,119,78,62,119,236,216,207,239,247,96,2,85,105,35,71,89,11,214,198,227,244,232,72,118,191,47,67,105,114,218,238,14,216,145,171,48,186,206,63,138,14,81,113,
38,16,230,209,183,104,132,2,203,238,239,202,188,145,255,87,187,232,46,38,80,153,249,113,28,157,100,128,209,47,226,55,142,149,194,151,151,117,9,247,8,165,65,176,15,3,156,109,104,32,126,74,64,203,61,37,
20,29,144,67,121,61,1,133,7,20,228,144,66,56,42,149,226,233,80,185,185,247,130,251,120,177,210,76,44,110,177,243,241,241,57,67,194,135,98,34,113,203,155,23,81,157,28,23,203,233,23,3,101,155,228,58,12,
252,70,112,97,20,232,175,69,226,55,170,210,162,134,28,5,56,135,169,242,175,42,50,163,127,131,37,133,202,13,189,103,4,69,224,146,226,6,103,20,40,113,212,65,216,246,237,197,241,191,214,112,250,173,66,87,
126,237,212,36,122,30,24,216,0,246,194,61,78,161,116,24,104,193,187,45,69,104,55,221,55,105,29,142,90,32,175,58,98,240,218,144,254,167,124,17,9,38,16,87,164,98,68,94,77,128,45,153,70,163,172,118,195,106,
38,4,5,48,192,117,48,13,24,127,130,177,143,113,37,76,201,84,229,49,142,122,56,110,175,15,44,103,181,24,167,106,250,213,252,208,9,161,115,219,161,30,181,42,111,145,85,228,18,25,53,244,33,120,49,29,98,151,
155,128,143,203,192,114,89,148,107,225,168,9,197,229,98,47,199,13,70,224,112,43,66,163,34,232,43,154,235,181,10,168,169,82,66,73,129,12,14,29,215,148,116,71,190,83,144,67,83,147,148,183,113,12,22,59,198,
128,146,69,28,117,209,206,159,175,26,255,103,248,118,6,169,67,174,227,232,212,136,92,204,128,76,237,86,72,157,18,130,70,74,161,28,84,9,168,39,87,2,23,229,100,229,140,247,141,17,148,237,226,168,13,152,
180,249,240,236,254,18,218,64,22,204,229,21,10,168,62,45,135,90,107,128,194,127,176,123,213,43,176,24,65,131,162,74,54,112,223,144,12,168,230,66,233,58,142,250,152,99,221,29,239,164,238,106,114,68,29,
193,190,130,121,135,128,168,37,207,52,121,79,101,27,92,171,226,192,240,23,120,168,71,36,227,118,147,165,58,94,205,169,143,175,176,57,201,181,218,219,251,121,11,238,95,63,84,190,225,60,212,190,41,106,62,
131,198,239,163,125,56,4,146,174,82,200,133,96,75,9,228,145,170,129,188,147,11,214,214,167,51,188,19,112,56,187,161,111,153,113,179,208,190,126,75,226,165,177,247,239,223,31,76,49,129,10,18,212,240,117,
29,25,154,115,231,30,135,214,103,127,97,129,193,17,226,121,19,141,52,230,127,159,81,64,81,177,28,42,107,20,208,220,70,104,25,33,121,23,233,202,93,221,233,224,57,21,7,47,55,122,191,92,175,140,224,60,30,
61,170,149,155,159,159,31,200,227,241,90,187,210,14,212,177,62,176,191,208,223,206,58,160,224,143,219,166,56,191,191,30,139,164,234,77,63,58,53,254,12,160,34,219,101,104,147,109,65,193,241,247,38,77,82,
213,194,58,89,44,170,166,80,103,124,212,112,231,250,175,182,116,240,80,143,249,255,9,187,210,124,143,126,147,84,229,113,251,246,29,71,115,115,243,167,6,235,9,212,218,111,108,20,241,247,237,219,187,144,
111,177,116,215,169,179,116,147,151,209,92,25,8,206,93,24,115,206,115,82,249,148,31,127,12,203,186,123,55,111,102,175,43,59,244,21,2,26,25,25,25,209,126,211,254,190,227,235,173,74,83,212,51,255,235,72,
199,165,187,210,188,79,110,252,172,244,173,180,180,216,221,4,81,230,217,239,26,27,125,85,129,134,72,36,226,7,5,69,30,60,119,238,236,212,132,68,58,177,136,76,135,95,54,71,200,96,119,167,78,168,248,50,225,
23,47,178,122,43,217,187,119,239,194,165,177,26,181,25,112,183,3,125,37,162,93,77,161,47,24,18,19,19,63,173,172,188,56,97,252,4,219,150,183,253,148,180,233,239,138,108,80,82,214,27,41,215,212,14,110,56,
81,196,23,150,20,183,179,170,170,234,70,76,156,56,241,194,250,245,235,55,175,88,238,93,138,26,134,125,193,127,1,140,113,41,29,111,6,190,237,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* Mcfx_gain_delayAudioProcessorEditor::solo_symbol_act_png = (const char*) resource_Mcfx_gain_delayAudioProcessorEditor_solo_symbol_act_png;
const int Mcfx_gain_delayAudioProcessorEditor::solo_symbol_act_pngSize = 3257;

// JUCER_RESOURCE: mute_symbol_png, 3122, "mute_symbol.png"
static const unsigned char resource_Mcfx_gain_delayAudioProcessorEditor_mute_symbol_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,49,0,0,0,49,8,6,0,0,1,4,155,113,138,0,0,0,9,112,72,89,
115,0,0,14,197,0,0,14,197,1,71,108,236,255,0,0,11,228,73,68,65,84,120,218,205,90,121,80,84,103,18,239,129,135,220,14,34,48,28,2,162,8,42,136,66,208,128,68,145,195,99,145,8,40,32,226,125,151,171,36,107,
249,135,91,174,85,86,121,69,43,49,26,48,217,90,20,21,149,32,135,1,21,80,193,91,81,2,72,10,13,6,216,18,34,97,17,3,67,0,5,21,25,102,187,31,243,134,153,97,46,142,61,186,234,201,204,123,223,235,235,235,254,
117,247,55,50,98,177,24,56,10,15,15,47,205,206,206,246,230,190,51,244,207,242,229,203,197,163,71,143,6,7,7,7,248,236,179,207,216,213,241,241,241,60,246,33,61,72,72,72,128,184,184,56,233,223,150,150,22,
115,134,88,209,27,142,62,145,44,171,217,246,60,246,239,178,101,203,110,49,36,131,99,149,146,146,0,238,198,189,15,67,67,67,147,24,78,248,162,233,54,0,211,227,164,202,33,235,4,134,19,78,50,136,21,189,65,
15,88,109,57,83,138,138,138,194,43,43,43,157,214,172,89,115,84,106,6,39,143,168,172,172,236,107,214,4,217,155,28,209,61,169,18,100,147,68,120,159,71,100,111,72,93,69,252,20,217,69,69,69,205,150,170,60,
114,228,200,215,150,150,150,205,207,159,63,119,146,83,151,200,212,212,84,188,118,237,90,69,93,160,162,162,2,110,222,188,201,147,219,17,110,177,178,23,136,220,220,220,88,43,222,189,123,7,137,137,137,60,
134,28,34,187,56,175,188,30,90,91,223,67,172,255,120,40,201,56,14,134,35,1,220,231,111,99,159,25,24,24,0,169,202,144,7,101,185,118,119,233,194,39,204,79,248,201,22,122,124,215,131,168,52,73,78,42,217,
198,144,203,207,157,59,247,53,169,32,221,68,232,13,129,143,199,224,63,99,182,201,189,68,206,96,109,226,140,220,180,105,147,152,84,80,70,228,206,89,179,102,221,239,231,61,142,182,108,217,146,247,246,237,
91,253,51,103,206,4,41,62,147,238,92,113,113,241,18,84,49,147,199,227,129,158,158,30,123,113,27,135,238,126,136,154,248,73,95,64,123,126,66,111,76,163,197,42,92,61,19,183,163,243,245,235,215,70,236,11,
180,24,52,16,110,133,225,157,59,119,230,48,134,134,134,226,141,27,55,202,68,154,13,6,86,164,210,200,75,73,73,185,205,112,139,251,232,165,74,41,200,188,207,104,105,216,174,9,129,202,182,30,112,104,47,134,
117,200,249,148,68,74,63,47,245,69,230,120,40,144,170,226,211,239,49,147,155,155,11,11,23,46,148,187,105,143,87,189,192,71,169,90,12,110,185,52,81,56,227,194,227,228,242,92,250,185,176,176,176,140,85,
41,57,57,185,115,245,234,213,70,234,220,106,98,98,82,240,248,241,227,121,236,11,109,109,109,198,214,214,214,191,71,71,71,91,42,91,92,83,83,115,51,39,39,103,158,156,209,141,141,141,86,18,28,202,65,132,
113,210,215,215,239,170,170,170,18,52,52,52,216,202,217,160,44,248,56,218,187,119,111,218,23,95,124,17,237,227,227,3,83,166,76,233,247,188,181,181,149,236,34,117,107,203,203,203,199,169,226,211,111,43,
40,22,95,189,122,53,109,194,132,9,236,247,254,193,208,71,102,102,102,220,174,56,113,206,38,31,146,91,84,102,197,158,61,123,50,137,57,198,48,12,150,104,147,72,96,122,122,122,19,231,78,134,115,75,83,83,
83,52,167,189,34,194,122,133,173,3,63,7,227,126,12,235,139,46,65,86,73,157,82,208,165,205,181,176,176,104,109,110,110,54,99,133,220,190,125,59,90,153,207,165,192,127,233,20,120,32,19,83,217,155,111,235,
164,2,84,81,108,108,44,159,130,130,33,132,81,39,192,209,39,26,248,69,233,112,70,82,11,165,86,158,188,4,182,158,97,224,220,124,25,238,253,166,58,120,40,234,24,130,48,66,37,213,212,3,254,200,252,9,10,73,
72,184,137,130,130,32,83,2,31,75,62,113,128,242,108,245,123,68,97,205,16,70,174,90,181,74,76,145,162,142,226,226,214,161,144,83,120,61,147,124,143,212,42,16,40,111,216,61,193,24,175,245,247,247,119,82,
182,232,77,183,72,242,201,24,54,69,250,66,98,230,35,88,188,185,207,109,162,15,170,5,96,117,187,138,137,25,194,9,25,23,20,20,84,72,120,44,175,189,124,196,232,219,120,227,61,111,185,123,94,81,219,192,75,
53,68,132,200,229,9,87,29,176,32,136,21,153,15,132,4,2,65,158,167,167,231,183,33,33,33,121,42,51,30,97,134,45,59,84,44,34,35,35,179,35,34,34,248,4,235,234,136,220,130,40,40,192,235,35,165,176,162,14,187,
56,250,240,225,131,209,241,227,199,15,149,150,150,122,97,52,234,141,29,59,182,6,67,127,47,38,239,47,154,222,101,84,61,64,70,252,73,147,38,53,48,12,99,228,231,231,7,124,62,159,237,31,217,230,162,187,123,
6,34,65,204,147,39,79,8,138,122,82,83,83,163,194,194,194,126,208,90,128,171,171,235,107,71,71,71,147,69,139,22,169,213,206,195,195,131,46,157,130,130,130,139,43,86,172,232,126,241,226,133,192,220,220,
188,69,165,0,145,72,164,135,128,216,181,126,253,250,1,109,176,142,142,14,21,122,102,233,210,165,194,152,152,152,141,248,254,73,165,2,208,29,93,67,137,36,116,41,160,53,39,80,160,8,5,158,150,19,64,173,226,
80,152,115,100,101,101,5,184,39,137,24,129,89,136,24,173,172,128,3,7,14,164,226,116,0,195,69,19,39,78,100,48,210,126,195,10,105,202,10,248,234,171,175,98,86,174,92,41,183,168,177,44,15,50,10,159,43,205,
110,197,154,226,27,185,1,188,109,228,243,5,55,221,4,147,56,136,193,48,91,140,155,163,82,27,115,115,128,95,59,0,198,42,212,164,206,127,21,1,62,130,63,104,129,138,141,199,208,189,204,160,230,167,230,207,
159,175,6,235,1,114,126,40,132,109,43,253,228,238,231,253,80,2,45,220,2,21,20,24,24,40,100,16,253,248,234,252,249,39,47,27,184,90,86,134,159,100,5,116,176,61,163,111,196,50,120,148,149,170,242,93,220,
7,123,198,214,214,86,237,134,57,251,161,117,101,103,224,86,85,27,4,186,242,37,245,250,42,251,215,123,140,25,60,210,4,21,154,192,12,3,152,245,117,69,254,89,20,208,187,217,89,37,47,129,103,54,157,173,130,
26,177,8,129,76,227,162,176,79,189,224,244,149,50,104,65,126,230,239,123,27,129,176,72,234,94,187,52,11,16,10,133,26,23,153,140,37,255,151,193,133,252,106,240,108,206,239,109,145,13,181,203,9,166,163,
163,131,212,24,161,105,161,143,53,15,138,254,121,29,74,105,64,243,142,208,138,57,54,2,21,204,214,173,91,147,58,59,59,183,104,90,60,61,44,28,138,254,145,197,126,14,241,29,163,149,0,236,115,223,50,135,14,
29,250,51,54,0,91,166,78,157,42,223,196,244,116,202,175,30,193,49,117,0,69,239,136,68,42,123,254,63,88,168,168,171,171,107,68,1,214,178,15,109,189,35,65,161,214,43,129,140,17,42,97,4,43,229,229,252,252,
252,48,86,64,109,109,173,141,187,187,187,8,51,79,103,56,192,14,97,191,179,190,190,254,189,28,92,31,57,114,36,228,244,233,211,215,8,110,135,74,216,224,189,199,118,50,90,78,0,226,209,117,156,1,98,210,210,
210,46,216,217,217,13,154,57,206,18,237,200,199,92,105,69,195,182,60,13,107,170,112,231,206,157,233,88,232,71,13,132,49,54,5,213,200,220,16,153,59,168,45,250,193,193,193,55,176,199,49,199,98,254,20,45,
17,96,107,98,169,142,49,214,240,231,39,78,156,48,205,202,202,90,187,111,223,190,135,90,183,45,216,146,176,253,254,177,99,199,254,130,161,252,87,35,35,163,15,14,14,14,173,6,6,6,93,111,222,188,225,97,235,
57,33,32,32,224,246,185,115,231,86,238,223,191,191,77,229,134,107,211,120,169,35,42,88,241,241,241,59,75,74,74,92,49,114,248,84,104,140,141,141,217,62,10,163,137,61,200,32,25,216,75,81,3,7,237,237,237,
128,10,82,171,218,229,236,236,252,18,247,62,111,199,142,29,123,44,45,45,155,6,29,181,3,89,140,138,140,56,124,248,112,242,55,223,124,19,133,10,233,78,155,54,141,250,52,172,172,230,160,174,48,170,32,130,
64,71,66,170,221,187,119,111,169,172,172,164,166,191,103,212,168,81,191,39,38,38,174,153,59,119,238,245,97,53,2,231,213,140,47,191,252,50,210,201,201,9,102,206,156,9,216,195,193,112,18,98,47,16,34,226,
69,120,98,157,153,153,121,109,251,246,237,61,56,52,191,78,73,73,89,74,153,58,40,35,168,97,197,102,179,60,35,35,99,82,120,120,184,202,83,194,255,4,145,81,18,128,228,227,174,92,91,176,96,1,160,30,75,113,
120,73,215,218,8,242,60,134,77,36,157,95,12,180,235,30,110,34,140,164,242,112,241,226,197,180,13,27,54,36,225,46,69,16,242,168,52,130,98,30,135,80,33,122,194,228,127,173,188,34,97,226,3,142,252,38,132,
203,104,216,131,203,151,47,47,234,103,4,77,105,56,72,9,49,89,117,177,33,87,203,240,213,147,60,72,191,251,92,230,206,100,88,190,53,8,204,7,80,33,69,109,191,192,133,179,55,64,182,215,245,143,222,12,30,2,
245,45,15,21,150,234,234,234,57,132,255,28,68,51,50,51,81,3,238,130,70,3,216,42,223,173,120,231,25,164,230,9,96,115,168,187,118,72,209,211,2,119,20,12,32,234,234,17,105,229,0,23,23,23,83,132,104,1,230,
205,45,164,64,70,50,165,252,72,115,178,186,179,38,85,67,11,141,13,60,252,208,83,123,27,242,203,45,33,100,170,64,19,80,195,47,5,223,195,51,153,59,60,28,203,197,194,129,133,23,85,224,251,247,239,143,163,
66,73,141,183,17,150,218,25,152,249,3,142,83,50,96,188,151,47,232,254,250,8,170,233,71,130,123,233,240,147,245,102,240,84,19,18,194,202,91,112,163,186,183,192,142,158,236,3,227,186,171,161,164,186,101,
80,121,130,109,158,128,42,61,67,71,32,56,65,179,21,118,48,212,109,50,22,130,230,143,128,234,212,187,236,247,7,233,87,193,122,115,24,216,40,177,67,212,82,1,223,23,84,113,65,1,11,130,60,161,33,251,199,65,
39,59,22,70,3,108,85,154,25,58,195,225,142,89,6,69,93,31,64,223,194,3,162,252,234,37,131,119,29,92,204,46,131,245,209,94,242,109,122,79,51,220,72,185,213,55,162,46,67,32,192,25,237,87,222,208,80,139,122,
45,58,18,213,163,30,103,168,100,237,21,12,179,95,214,192,189,26,49,136,95,21,66,222,35,43,88,34,29,102,40,15,46,176,33,199,30,41,5,44,3,55,11,134,157,3,117,135,214,186,209,175,106,93,116,182,81,243,243,
207,63,207,24,58,146,143,128,169,243,99,160,241,239,169,172,178,13,165,89,240,163,221,58,248,216,193,24,132,21,125,121,160,59,33,24,2,221,45,134,173,126,80,183,75,39,224,123,177,180,15,79,51,196,88,64,
240,242,64,168,150,132,77,241,165,34,176,139,182,135,187,183,170,164,245,36,102,222,36,96,134,201,0,236,144,59,169,93,103,232,28,19,175,23,88,56,28,233,68,112,168,164,107,238,6,177,193,175,224,251,27,
21,24,243,207,32,43,253,25,11,161,128,16,58,119,85,192,128,10,162,38,18,10,133,143,113,222,232,61,130,195,241,116,58,86,235,70,108,171,117,168,249,26,42,141,158,52,7,230,53,188,132,252,103,189,208,73,
53,192,109,222,42,152,200,31,62,11,16,81,107,112,200,245,196,25,102,12,35,233,75,154,232,192,119,245,234,213,25,88,248,116,104,176,81,123,158,98,231,138,229,127,12,134,69,55,24,219,153,41,59,131,195,228,
13,7,158,101,21,188,99,179,207,6,220,93,248,74,251,79,171,201,1,224,55,14,43,117,183,14,216,153,105,231,64,130,213,147,39,79,154,92,185,114,229,83,44,13,109,210,240,164,19,107,108,119,67,22,47,94,156,
19,27,27,203,168,219,17,19,193,120,240,210,84,152,117,140,193,197,195,75,211,34,176,113,113,3,155,1,14,217,88,219,172,114,115,115,23,226,108,243,176,95,23,75,211,20,157,140,99,111,82,227,233,233,73,199,
246,255,87,157,172,174,174,110,118,114,114,242,71,168,163,35,14,246,237,42,231,9,58,218,167,95,7,147,146,146,54,124,254,249,231,199,67,67,67,245,135,227,112,101,136,241,255,224,224,193,131,159,96,200,
167,29,61,122,52,66,235,201,142,126,66,160,11,147,103,45,142,138,241,51,102,204,48,160,51,243,255,150,226,116,100,37,22,139,243,209,243,254,223,125,247,221,183,248,121,214,160,103,108,250,169,130,174,
214,214,86,51,28,74,206,222,187,119,47,192,199,199,71,236,236,236,108,170,9,0,6,74,148,176,29,29,29,15,207,159,63,63,27,67,187,0,147,119,3,122,190,125,216,78,59,232,39,17,217,105,138,126,96,56,112,224,
192,223,138,139,139,63,198,58,211,100,111,111,175,131,73,103,73,77,153,54,69,10,29,80,219,212,212,212,130,3,142,225,211,167,79,39,250,250,250,62,218,181,107,215,193,57,115,230,220,73,80,248,47,27,154,
232,223,211,9,225,84,105,180,164,89,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* Mcfx_gain_delayAudioProcessorEditor::mute_symbol_png = (const char*) resource_Mcfx_gain_delayAudioProcessorEditor_mute_symbol_png;
const int Mcfx_gain_delayAudioProcessorEditor::mute_symbol_pngSize = 3122;

// JUCER_RESOURCE: mute_symbol_over_png, 3148, "mute_symbol_over.png"
static const unsigned char resource_Mcfx_gain_delayAudioProcessorEditor_mute_symbol_over_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,49,0,0,0,49,8,6,0,0,1,4,155,113,138,0,0,0,9,112,72,
89,115,0,0,14,197,0,0,14,197,1,71,108,236,255,0,0,11,254,73,68,65,84,120,218,205,89,121,88,20,87,18,175,129,70,195,149,33,24,64,37,30,160,43,129,232,6,6,131,10,66,16,48,81,60,192,168,136,137,138,70,60,
193,85,16,69,241,92,36,160,18,65,60,248,67,1,65,76,92,34,42,70,68,20,196,131,8,70,5,188,72,60,34,1,20,214,3,16,130,74,28,132,217,170,102,122,152,129,185,56,54,187,245,125,61,211,211,239,245,171,227,85,
253,170,234,13,35,18,137,128,163,192,192,192,235,81,81,81,195,185,223,12,125,220,185,115,71,52,116,232,80,192,1,250,201,205,230,177,131,52,192,227,241,128,86,225,190,107,106,106,12,25,241,82,224,188,225,
34,59,253,91,71,118,62,172,92,185,50,135,17,243,96,151,26,52,88,3,150,246,213,100,7,5,2,65,60,195,49,63,31,234,4,16,218,44,17,110,217,178,101,187,185,65,30,241,160,165,232,13,26,96,165,229,84,41,45,45,
245,68,169,205,38,78,156,24,45,81,131,248,13,28,56,16,232,66,138,226,84,104,213,190,149,68,18,33,72,39,246,137,120,105,201,128,180,217,184,1,94,219,229,114,115,115,157,36,34,243,249,252,250,222,189,123,
87,221,187,119,207,204,209,209,177,85,92,34,99,99,99,209,179,103,207,218,73,178,111,223,62,88,184,112,33,79,102,71,184,201,242,94,32,194,23,88,17,209,72,96,104,104,200,99,200,32,210,147,39,69,157,131,
123,191,86,193,253,253,51,96,163,13,3,198,131,120,224,159,218,200,142,225,11,64,162,50,100,65,177,177,88,122,89,171,5,209,186,219,241,206,5,26,35,43,225,77,168,169,12,87,210,141,33,147,163,204,81,98,17,
90,54,17,10,216,251,8,55,252,112,107,148,121,137,140,193,136,101,102,149,68,153,69,36,130,60,34,115,162,229,114,101,54,91,154,34,35,35,51,234,235,235,123,134,134,134,186,182,29,147,236,220,243,231,207,
167,190,255,254,251,169,180,181,171,86,173,146,108,40,125,160,248,121,233,233,233,14,146,23,98,99,99,139,150,46,93,106,13,10,8,39,219,163,184,175,81,108,29,246,5,101,147,57,194,201,218,23,46,92,112,102,
244,245,245,69,40,175,148,167,9,208,177,10,228,122,30,70,218,121,134,155,220,74,133,10,185,160,142,173,74,115,212,92,154,2,241,247,26,97,82,201,6,168,192,149,77,121,60,249,86,146,4,192,0,47,240,149,136,
178,181,29,23,102,252,248,241,112,250,244,105,153,135,142,24,10,87,254,190,65,174,88,12,78,150,4,10,167,220,37,81,107,156,75,111,172,173,173,109,33,43,82,175,94,189,94,87,87,87,235,40,51,43,46,156,85,
80,80,240,25,251,2,78,214,53,53,53,125,86,81,81,97,36,111,242,244,233,211,207,29,57,114,228,51,25,165,113,178,177,216,13,210,155,155,155,205,180,181,181,133,249,249,249,38,149,149,149,125,113,114,171,
14,242,156,143,163,180,180,180,148,217,179,103,123,133,135,135,19,102,181,27,191,127,255,62,44,95,190,28,94,190,124,249,59,122,180,185,162,117,218,109,5,249,162,129,129,129,245,151,95,126,9,158,158,158,
208,222,25,90,105,200,144,33,220,174,152,113,198,22,219,80,87,97,84,100,101,101,165,162,255,66,87,72,188,73,34,180,253,115,206,156,12,103,22,15,15,15,47,146,94,198,193,196,206,21,144,94,1,81,19,250,182,
91,48,59,216,25,198,110,191,40,23,116,105,115,17,118,106,159,60,121,98,192,50,121,244,232,145,23,175,141,75,75,83,244,68,83,248,7,46,50,80,250,97,85,134,132,129,34,66,6,124,114,10,134,16,70,10,52,218,
145,243,250,124,24,28,230,0,102,226,92,40,209,210,104,2,12,95,118,10,188,111,120,64,80,238,91,133,239,147,215,81,148,247,84,42,14,79,8,251,69,77,16,135,76,120,188,217,200,40,25,4,98,248,184,182,203,29,
118,56,41,223,35,114,107,134,48,18,93,81,68,158,162,140,68,162,10,100,98,138,215,33,241,239,2,181,28,129,226,134,221,147,249,243,231,147,159,155,201,155,84,217,192,165,131,190,80,147,187,9,12,29,255,9,
231,94,180,154,237,77,189,226,56,11,11,11,59,141,129,233,206,136,83,131,57,110,208,101,194,99,89,233,101,23,120,111,244,102,124,182,89,230,89,72,209,91,8,81,12,17,238,50,113,194,101,7,244,50,145,50,20,
80,69,121,121,121,25,181,181,181,123,145,65,134,194,136,71,6,172,47,83,178,240,246,246,78,195,148,204,39,88,87,70,100,150,227,199,143,155,32,18,218,218,219,219,183,135,21,53,165,214,57,113,226,196,214,
235,215,175,11,16,167,180,48,154,75,166,77,155,22,138,245,192,175,170,94,100,20,123,146,136,239,224,224,80,137,56,164,19,29,29,13,136,6,236,37,38,59,188,188,99,98,98,96,253,250,245,205,41,41,41,211,221,
221,221,143,169,205,224,243,207,63,175,71,19,233,161,93,149,74,71,200,139,151,70,83,83,211,81,52,229,91,12,1,19,76,224,53,202,24,104,161,196,66,4,182,14,109,176,166,166,38,84,85,85,49,9,9,9,244,226,130,
175,191,254,58,78,46,3,244,36,97,87,60,9,23,6,220,167,253,7,14,28,104,154,55,111,222,1,25,6,38,38,38,93,114,83,142,134,15,31,14,55,110,220,216,135,46,123,28,115,80,45,35,78,184,135,31,60,120,0,221,69,
190,190,190,12,154,250,17,154,90,159,101,176,120,241,98,239,178,178,50,153,73,87,34,38,192,168,144,12,185,209,221,54,167,108,190,248,20,54,57,25,203,140,61,125,250,84,239,220,185,115,174,12,98,250,23,
183,111,223,86,40,141,57,22,197,39,42,1,60,218,228,164,103,57,107,128,146,118,233,32,249,169,155,97,24,74,207,63,50,95,125,245,85,2,114,82,200,160,228,161,8,190,24,185,2,154,202,119,202,60,31,231,186,
13,74,232,230,97,137,194,119,215,173,91,87,205,220,186,117,139,175,204,158,135,151,11,96,38,6,20,128,52,131,74,40,34,211,100,23,194,102,55,129,194,119,215,172,89,211,143,161,54,69,25,121,239,60,138,12,
204,96,110,210,61,72,244,177,16,231,235,73,236,247,38,87,11,216,172,10,42,250,244,233,163,98,202,64,214,214,73,115,63,68,6,45,155,61,118,123,33,104,244,91,141,119,66,149,30,197,212,213,213,169,156,116,
41,109,57,124,224,25,3,191,96,46,178,170,107,241,172,244,130,109,248,89,171,154,129,50,15,226,200,212,131,236,31,3,159,120,29,4,191,27,243,216,103,227,141,212,139,9,230,241,227,199,164,103,15,85,19,55,
88,107,194,150,52,31,136,164,104,13,202,86,107,241,194,194,194,98,102,245,234,213,241,120,191,68,213,228,208,11,103,96,139,129,27,123,159,17,233,170,22,3,108,209,27,152,224,224,224,165,136,247,75,2,2,
2,100,6,133,111,158,202,206,230,115,139,58,65,91,235,252,169,96,175,181,180,180,94,176,80,17,23,23,247,4,25,244,150,30,116,218,124,29,218,228,122,57,144,97,160,16,70,80,232,31,207,158,61,235,193,50,40,
46,46,238,51,103,206,156,166,131,7,15,106,116,19,222,189,198,26,233,13,89,69,2,215,8,25,238,136,229,153,4,183,93,37,204,106,111,176,211,244,146,201,7,152,38,207,164,166,166,122,99,82,255,151,179,179,115,
167,23,199,28,240,7,230,2,67,185,25,13,43,133,148,236,236,236,234,89,179,102,253,112,232,208,161,247,58,178,48,246,33,247,177,235,211,198,197,251,43,77,250,110,110,110,217,120,25,126,252,241,199,183,103,
206,156,105,130,128,165,52,164,16,247,31,226,92,253,99,199,142,205,43,47,47,207,83,187,108,185,121,243,230,48,250,222,185,115,231,138,173,91,183,174,65,213,27,177,25,175,213,211,211,19,98,14,225,97,197,
241,55,52,229,249,228,228,228,217,248,91,33,222,48,93,205,195,148,176,226,227,227,131,115,114,114,44,138,138,138,248,84,97,96,97,6,230,230,230,160,171,171,11,239,190,251,46,245,15,108,95,248,234,213,43,
40,41,41,161,14,137,220,91,40,16,8,254,141,214,202,192,98,97,147,145,145,209,243,206,202,192,116,112,126,15,172,17,146,208,197,167,55,52,52,104,6,5,5,129,143,143,15,37,47,246,234,232,90,120,13,32,164,
66,223,88,66,5,228,142,29,59,154,249,124,254,179,93,187,118,205,117,117,117,61,211,173,74,96,161,127,4,91,151,105,147,39,79,38,70,112,247,238,93,232,78,66,55,2,138,125,188,8,79,122,163,82,153,136,47,205,
24,56,245,88,202,205,160,72,237,172,18,90,137,137,137,55,3,3,3,45,177,113,32,199,135,191,138,72,41,49,64,242,207,159,63,159,73,133,24,194,205,140,169,83,167,254,160,182,18,100,121,116,147,105,167,78,157,
162,163,50,248,95,210,152,49,99,216,244,128,153,50,5,75,211,120,236,75,166,16,242,40,83,162,7,110,93,117,255,254,253,245,58,218,50,252,183,9,65,128,206,103,244,8,151,119,239,222,253,19,118,120,147,219,
41,65,93,154,165,165,101,53,102,109,77,108,39,148,46,120,37,26,107,241,192,12,169,39,179,160,88,152,12,86,90,234,11,37,124,16,7,150,67,22,128,116,173,27,157,95,3,43,70,42,207,27,148,88,246,238,221,235,
76,248,207,65,180,68,9,106,51,253,252,252,84,42,208,82,91,180,91,26,134,79,26,9,85,153,126,160,163,142,6,111,139,97,254,144,133,208,182,88,175,19,54,170,101,0,148,83,31,33,219,196,197,197,5,145,61,199,
133,85,226,155,111,190,249,153,250,100,121,7,136,202,136,154,22,234,43,52,176,57,105,56,227,15,51,118,216,194,201,149,35,85,21,34,176,223,219,6,213,110,205,79,26,131,0,154,31,118,204,189,40,3,103,101,
101,153,83,162,36,37,116,16,54,237,174,94,189,218,97,63,37,5,38,44,223,8,61,79,110,1,58,1,72,15,26,5,219,28,106,32,88,137,75,220,137,247,129,133,71,91,44,62,120,214,70,152,242,231,119,16,153,250,176,83,
113,130,114,155,140,27,55,110,13,67,71,32,24,200,48,120,240,224,78,45,244,170,159,7,36,31,53,128,99,54,129,45,22,26,229,9,14,47,46,194,104,3,57,113,240,75,44,12,243,77,21,255,154,10,39,146,131,33,199,
41,188,211,193,110,109,109,253,14,194,113,21,67,103,56,195,134,13,235,60,108,212,189,132,247,172,3,32,63,60,91,220,120,95,2,23,167,8,168,184,181,86,182,76,127,91,4,51,63,242,111,61,174,46,56,8,86,216,
163,165,65,215,202,30,170,181,24,58,132,194,130,171,203,16,56,114,237,119,240,109,174,17,4,157,126,11,141,183,67,192,125,149,29,92,147,52,51,20,7,35,208,229,90,4,158,190,187,0,150,8,116,216,70,179,39,
240,186,196,151,138,69,134,78,209,46,95,190,108,215,13,121,22,86,30,189,10,121,58,182,172,176,215,191,117,131,181,206,21,16,49,161,47,220,137,109,141,3,29,207,125,144,232,47,232,182,252,65,213,46,67,199,
128,88,6,123,119,203,138,218,54,112,184,120,15,244,252,200,143,253,185,117,98,48,184,253,252,25,44,244,75,149,228,147,107,63,44,80,15,134,213,236,185,168,92,103,232,28,211,198,198,166,44,38,38,102,0,157,
8,118,149,122,88,45,133,219,251,175,192,176,5,201,96,142,64,234,54,226,16,11,161,128,0,20,119,55,161,67,9,81,21,133,135,135,23,96,191,209,114,4,247,253,247,223,127,130,245,255,19,172,151,52,168,248,234,
42,13,245,141,135,196,75,121,48,55,185,5,58,41,7,248,36,222,133,249,22,221,167,1,22,164,37,17,17,17,54,143,31,63,254,128,85,130,26,18,58,240,197,72,63,82,86,86,166,65,141,141,50,234,231,226,131,86,112,
195,160,252,19,62,24,99,41,175,0,6,159,132,75,192,179,78,130,106,138,219,94,163,193,127,142,133,156,121,186,240,137,239,78,8,159,34,4,104,232,1,174,150,234,181,170,66,161,176,10,203,14,189,147,39,79,78,
194,254,163,78,82,118,208,137,117,82,82,146,187,137,137,73,250,111,191,253,198,40,219,17,211,17,94,176,118,132,170,78,165,47,204,9,92,171,98,146,22,56,205,241,3,167,14,54,217,22,22,22,198,88,105,79,176,
183,183,207,107,87,197,82,55,69,39,227,86,86,86,37,97,97,97,124,58,99,254,127,162,61,123,246,164,109,223,190,221,182,180,180,116,0,182,189,127,40,236,39,232,104,191,178,178,210,32,33,33,193,23,119,99,
15,118,87,61,187,227,112,165,43,244,19,146,163,163,227,232,195,135,15,167,148,151,151,79,81,187,179,163,191,16,232,194,246,112,30,246,24,187,182,109,219,246,14,157,153,255,133,178,191,70,203,159,221,184,
113,227,167,177,177,177,123,177,85,112,236,116,143,77,127,85,208,133,125,175,129,135,135,199,65,52,202,152,208,208,80,209,162,69,139,244,233,8,189,59,137,2,22,123,133,188,45,91,182,56,97,247,150,21,23,
23,231,235,239,239,255,71,183,157,118,208,95,34,210,221,20,253,193,128,37,252,186,107,215,174,141,176,179,179,123,62,118,236,88,13,172,40,141,168,40,83,199,202,216,110,254,158,153,153,89,131,229,180,54,
214,111,31,142,26,53,42,63,36,36,36,60,32,32,224,66,219,99,84,85,244,31,239,69,76,61,134,98,164,23,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* Mcfx_gain_delayAudioProcessorEditor::mute_symbol_over_png = (const char*) resource_Mcfx_gain_delayAudioProcessorEditor_mute_symbol_over_png;
const int Mcfx_gain_delayAudioProcessorEditor::mute_symbol_over_pngSize = 3148;

// JUCER_RESOURCE: mute_symbol_act_png, 2890, "mute_symbol_act.png"
static const unsigned char resource_Mcfx_gain_delayAudioProcessorEditor_mute_symbol_act_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,49,0,0,0,49,8,6,0,0,1,4,155,113,138,0,0,0,9,112,72,
89,115,0,0,14,197,0,0,14,197,1,71,108,236,255,0,0,10,252,73,68,65,84,120,218,197,25,9,88,20,231,117,22,6,4,4,118,57,197,139,203,136,177,21,227,109,193,168,128,80,16,81,185,15,171,18,4,191,42,70,19,163,
13,169,182,49,49,85,49,49,106,109,188,34,72,208,70,5,22,150,128,130,26,65,188,80,131,136,134,38,158,129,136,8,202,229,114,45,133,5,166,239,141,51,56,12,123,1,251,165,239,251,102,119,118,231,255,223,125,
253,111,72,138,162,8,22,162,167,76,187,149,120,251,214,52,246,55,137,31,71,5,36,229,64,232,16,75,225,254,162,64,159,94,237,78,117,8,232,135,248,192,131,144,19,249,132,94,207,119,67,67,131,57,137,168,112,
199,36,6,85,44,44,68,248,179,187,71,62,137,52,88,84,203,96,215,98,230,225,156,152,232,68,146,37,190,27,80,113,97,237,218,181,255,34,89,226,72,3,81,225,14,124,64,115,203,138,242,31,73,166,255,189,242,114,
135,144,15,214,239,233,17,131,165,103,137,247,27,226,118,211,34,176,127,114,1,255,235,97,2,101,66,200,103,152,233,121,144,207,227,142,68,124,124,116,228,229,188,57,61,44,11,77,76,154,173,45,45,235,30,
1,103,189,216,69,48,23,8,168,116,30,74,132,51,68,23,177,139,234,18,244,178,8,187,88,209,6,132,5,132,46,45,69,19,65,17,139,41,185,128,68,133,112,23,111,34,58,137,167,240,240,56,252,151,12,247,34,66,0,54,
208,165,159,153,194,61,178,74,162,6,45,57,88,255,11,27,214,50,139,228,204,197,5,148,141,68,149,111,220,176,113,247,2,102,33,215,136,49,175,181,215,3,168,12,250,95,86,200,239,4,122,20,178,160,8,80,157,
179,103,207,190,210,71,123,44,108,246,241,205,145,117,180,15,217,147,159,55,175,207,102,246,230,177,56,61,168,34,36,66,140,52,60,57,54,199,239,157,174,211,11,207,94,187,54,171,103,195,199,19,39,151,184,
151,254,52,73,49,67,4,17,87,88,228,42,50,50,146,73,101,50,163,87,150,134,197,132,26,144,180,117,26,22,20,20,184,145,67,193,144,167,25,13,241,61,141,255,91,234,238,117,145,60,173,196,226,138,64,8,90,236,
163,112,196,150,11,62,229,2,17,151,6,143,67,192,250,10,181,196,133,47,136,110,248,236,238,227,243,244,134,191,2,134,29,188,125,168,129,59,74,216,34,111,80,221,61,129,194,98,228,186,9,151,202,198,41,206,
183,105,212,129,70,122,178,12,153,220,72,149,192,143,231,185,125,95,124,225,252,31,233,13,47,91,91,135,14,183,182,174,57,81,43,181,82,180,120,223,188,57,121,18,88,220,75,232,234,154,26,107,252,246,243,
243,59,45,3,207,52,212,211,239,40,169,121,49,172,170,170,106,132,59,87,6,69,206,199,194,145,144,176,148,247,197,169,161,209,160,226,0,38,12,184,80,9,113,180,31,76,64,217,219,151,223,44,47,115,84,134,167,
143,41,208,23,169,210,31,39,121,0,210,55,224,183,42,103,24,5,198,167,173,242,107,165,3,171,108,212,33,170,69,105,84,36,135,132,138,61,104,142,117,137,129,2,26,9,9,46,177,18,213,178,234,36,89,181,140,17,
75,66,61,120,200,89,63,15,6,142,99,21,248,95,9,56,222,6,80,151,162,164,139,198,181,22,137,164,53,82,169,136,222,89,42,22,135,190,161,130,123,49,232,62,16,46,27,94,134,97,9,40,131,148,70,153,16,157,130,
196,12,19,160,130,0,122,254,104,64,190,4,162,130,203,45,74,57,14,190,231,129,83,28,160,195,73,49,160,215,145,152,194,212,233,121,61,168,42,27,144,178,69,119,53,163,198,131,112,47,86,35,13,186,53,137,57,
242,56,36,219,81,132,64,229,98,182,178,243,243,145,58,192,184,161,109,242,149,253,232,242,120,112,67,69,139,106,57,247,217,32,209,66,80,219,151,28,245,202,85,16,200,245,241,204,173,202,205,241,165,137,
96,32,249,204,154,117,13,243,49,159,123,46,12,5,105,249,255,69,0,193,8,5,54,165,83,4,16,232,21,39,108,117,16,64,234,206,239,71,54,230,67,141,183,87,142,201,186,53,251,37,190,190,57,74,35,30,210,12,109,
28,44,22,161,254,254,153,95,55,182,10,133,106,236,133,106,201,3,221,23,23,23,79,85,152,86,84,229,46,22,186,160,156,253,59,238,163,248,226,219,183,167,180,202,229,122,182,142,142,101,75,183,124,188,117,
204,248,241,247,212,237,37,149,61,144,55,54,10,167,13,31,81,101,210,214,102,180,10,244,109,11,146,216,178,15,139,74,102,84,164,164,135,239,1,215,77,210,33,186,83,210,210,66,252,2,3,51,52,38,224,50,212,
180,121,174,172,213,120,47,109,72,165,60,16,65,240,60,168,155,208,233,14,10,79,55,35,169,206,95,94,188,24,102,110,110,222,160,148,0,5,226,139,244,245,59,50,105,99,107,158,24,177,163,205,232,20,144,59,
45,172,234,157,142,28,94,25,29,19,147,160,144,128,14,32,31,140,39,249,0,83,15,87,174,58,146,164,171,219,21,21,21,149,212,139,128,197,32,221,148,5,39,144,39,119,197,170,175,165,1,1,18,17,100,92,154,64,
114,88,196,201,99,42,116,221,95,152,79,80,164,227,232,209,79,27,154,155,77,104,172,31,165,158,10,63,193,227,254,62,100,207,88,37,245,128,95,83,34,193,195,34,121,12,166,182,180,27,231,229,229,205,35,31,
166,103,4,38,168,224,126,36,92,215,128,208,44,158,209,165,80,63,240,217,51,37,251,112,245,162,69,139,178,200,165,81,43,142,238,84,17,173,136,224,239,116,91,213,155,0,118,84,207,212,168,106,133,200,178,
158,124,212,220,40,36,84,24,119,11,24,237,83,5,197,230,1,163,154,100,66,121,38,240,175,170,30,77,78,84,147,107,230,210,156,119,19,59,129,227,56,70,149,37,12,65,212,123,178,202,4,14,110,106,174,129,87,
160,174,207,1,167,113,26,214,234,94,4,90,53,88,180,151,233,131,127,5,206,237,153,147,104,188,134,145,78,150,19,234,179,169,5,163,198,21,192,249,18,70,61,51,24,66,106,9,212,66,122,32,58,40,125,117,11,89,
131,158,128,107,156,134,234,169,118,26,247,19,185,126,69,116,34,113,40,97,181,122,2,175,13,186,67,195,168,63,100,106,208,70,110,57,120,32,118,205,161,195,171,131,121,58,85,229,27,34,158,231,41,91,107,
96,102,246,146,102,229,130,141,245,243,224,231,245,54,220,135,206,160,227,124,158,158,21,165,12,101,105,228,210,194,249,89,231,179,190,91,76,19,120,80,93,61,220,71,64,118,197,17,58,58,218,72,118,2,98,
136,236,103,131,33,237,189,210,245,250,179,103,124,31,250,44,56,235,68,12,158,70,160,153,94,123,67,106,106,104,47,2,222,222,222,231,82,79,125,27,126,55,252,79,167,222,26,4,17,127,83,195,166,198,134,6,
115,133,21,45,52,44,44,229,130,133,69,253,231,65,161,169,31,54,181,152,245,7,177,220,214,225,97,12,213,102,216,88,81,97,171,178,232,123,122,122,94,240,108,108,48,159,232,236,92,58,231,89,205,176,160,151,
47,173,84,33,110,31,105,247,75,164,188,201,68,114,50,41,170,194,213,181,80,227,182,229,199,210,82,103,58,77,236,221,251,126,124,124,252,71,38,164,158,220,9,74,160,17,4,102,3,52,104,63,60,126,52,214,221,
221,253,226,241,227,199,151,189,16,10,27,149,70,178,38,141,151,42,192,130,117,56,126,103,220,149,7,15,198,61,128,212,175,195,76,179,70,160,47,209,189,45,65,39,151,54,122,88,3,209,13,153,160,14,59,24,96,
116,130,205,240,106,47,104,53,99,183,126,186,197,202,202,170,118,160,60,244,171,216,83,29,29,250,199,150,69,38,111,129,102,174,157,234,210,13,1,135,243,134,203,15,24,246,163,87,244,163,49,193,20,88,81,
101,135,153,234,198,161,35,171,207,130,168,98,1,209,45,26,102,93,179,255,155,111,222,241,4,199,214,170,16,137,65,33,105,31,102,136,131,93,128,217,213,144,185,146,232,104,209,74,232,211,96,12,120,49,35,
6,83,128,20,18,87,139,143,223,89,31,130,234,190,99,106,220,156,156,154,18,230,173,70,32,165,66,96,195,250,143,9,111,221,221,245,240,222,248,221,64,64,172,133,246,175,63,66,197,17,2,29,162,73,38,188,3,
121,204,7,42,94,90,74,74,88,112,104,104,170,198,66,160,230,63,0,205,239,0,230,51,127,67,230,21,159,231,95,149,144,71,97,75,82,204,163,163,19,83,37,146,0,204,60,202,143,11,224,243,174,102,150,245,22,178,
22,227,255,55,243,124,24,11,130,164,67,251,139,121,121,223,220,183,175,102,101,101,45,234,35,4,158,210,198,154,89,212,135,83,148,174,183,154,80,225,246,226,44,28,5,171,217,247,35,78,112,247,59,80,223,
185,29,47,190,44,8,86,211,129,97,97,57,157,125,201,13,243,63,155,162,123,184,197,99,230,34,90,128,129,77,178,176,83,203,1,95,54,80,211,0,179,176,75,65,203,222,74,104,150,238,253,136,54,147,118,40,96,30,
30,30,249,0,30,180,16,27,166,207,188,137,231,228,128,126,30,175,248,7,147,173,32,200,118,13,112,224,76,248,28,49,184,250,132,21,184,164,172,204,17,11,37,137,19,132,132,91,63,204,216,63,128,243,33,10,240,
7,90,251,4,81,0,76,221,128,235,36,48,24,161,194,154,216,188,127,193,244,214,35,233,163,137,128,110,133,7,2,81,79,106,135,253,13,42,61,137,35,16,107,26,161,96,64,136,240,149,201,102,80,64,1,51,101,63,2,
12,78,0,92,206,10,226,163,139,113,59,22,62,3,97,239,210,2,12,76,136,49,132,220,0,90,149,58,18,103,56,14,3,20,128,59,54,59,0,12,177,193,254,30,124,103,192,127,252,54,125,27,231,117,194,123,32,36,38,130,
235,253,56,175,41,28,61,224,184,1,135,80,134,90,72,129,111,2,67,177,160,81,118,6,139,103,237,131,156,52,141,113,80,192,104,220,141,243,86,108,176,137,28,155,69,18,167,104,143,138,138,102,104,35,151,99,
122,252,25,24,69,102,241,60,159,0,130,224,11,54,110,28,208,105,114,16,179,124,62,96,183,75,226,24,112,111,202,169,112,109,33,125,21,31,175,206,119,24,176,83,129,249,221,28,151,193,122,98,48,72,247,125,
61,154,49,144,97,187,78,226,28,243,247,182,118,79,210,43,158,218,5,105,65,67,186,12,163,24,192,35,121,51,138,191,48,113,160,45,200,158,53,169,216,221,156,25,193,73,110,21,77,183,183,177,121,238,221,77,
233,24,107,65,75,200,232,38,176,194,118,142,11,121,3,222,249,90,116,163,246,17,182,101,135,238,150,76,174,172,172,28,69,11,129,7,18,28,248,70,7,135,164,157,160,116,213,158,175,173,128,161,149,192,168,
30,125,175,24,60,25,134,165,204,111,101,47,109,198,49,184,16,38,107,172,64,81,93,100,103,179,113,118,118,246,66,33,156,248,122,42,28,78,172,191,205,205,241,13,246,243,59,125,172,147,32,85,89,4,135,66,
17,26,104,213,83,131,53,88,79,156,251,121,200,14,107,170,177,62,35,57,179,192,149,57,11,247,42,211,120,154,194,201,248,239,28,28,203,34,155,154,133,62,90,52,191,54,160,112,241,130,204,195,183,139,167,
62,121,242,196,206,212,212,180,73,233,121,2,71,251,207,27,165,162,196,132,132,24,255,119,223,253,234,243,246,206,33,78,90,12,198,129,192,51,151,153,87,151,93,191,250,246,201,240,176,148,138,76,73,128,
198,39,59,124,133,128,87,82,82,82,84,208,186,117,251,98,90,58,12,112,102,254,91,49,142,35,171,107,139,61,207,127,121,249,210,220,3,235,214,236,167,10,175,204,30,240,25,27,95,85,224,37,149,74,69,203,151,
47,63,118,245,226,69,247,168,22,29,106,62,180,195,218,119,54,81,221,165,133,46,133,255,188,114,121,142,151,151,215,247,9,224,13,155,57,110,51,232,105,7,190,18,225,158,166,240,5,195,103,219,182,109,46,
186,121,115,230,68,200,110,211,228,148,142,115,85,157,21,54,101,154,20,169,74,39,187,242,18,43,179,134,155,237,109,134,37,247,239,191,233,226,226,114,125,211,166,77,219,63,113,115,43,248,164,159,162,255,
15,27,135,89,239,35,210,43,58,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* Mcfx_gain_delayAudioProcessorEditor::mute_symbol_act_png = (const char*) resource_Mcfx_gain_delayAudioProcessorEditor_mute_symbol_act_png;
const int Mcfx_gain_delayAudioProcessorEditor::mute_symbol_act_pngSize = 2890;


//[EndFile] You can add extra defines here...
//[/EndFile]
