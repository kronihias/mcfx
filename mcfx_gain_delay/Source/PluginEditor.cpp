/*
  ==============================================================================

  This is an automatically generated GUI class created by the Introjucer!

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Created with Introjucer version: 3.1.1

  ------------------------------------------------------------------------------

  The Introjucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-13 by Raw Material Software Ltd.

  ==============================================================================
*/

//[Headers] You can add your own extra header files here...
//[/Headers]

#include "PluginEditor.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

#define LINE_WIDTH 20
#define GROUP_CHANNELS 4
#define METER_GROUP_SPACE 6

//==============================================================================
Mcfx_gain_delayAudioProcessorEditor::Mcfx_gain_delayAudioProcessorEditor (Mcfx_gain_delayAudioProcessor* ownerFilter)
    : AudioProcessorEditor (ownerFilter)
{
    
    tooltipWindow.setMillisecondsBeforeTipAppears (700); // tooltip delay
    
    addAndMakeVisible (lbl_gd = new Label ("new label",
                                           TRANS("mcfx_gain_delay")));
    lbl_gd->setFont (Font (15.00f, Font::plain));
    lbl_gd->setJustificationType (Justification::centredLeft);
    lbl_gd->setEditable (false, false, false);
    lbl_gd->setColour (Label::textColourId, Colours::yellow);
    lbl_gd->setColour (TextEditor::textColourId, Colours::black);
    lbl_gd->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

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

    //[UserPreSize]
    //[/UserPreSize]
/*
    addAndMakeVisible (lbl_ch = new Label ("new label",
                                           TRANS("1")));
    lbl_ch->setFont (Font (15.00f, Font::plain));
    lbl_ch->setJustificationType (Justification::centredRight);
    lbl_ch->setEditable (false, false, false);
    lbl_ch->setColour (Label::textColourId, Colours::white);
    lbl_ch->setColour (TextEditor::textColourId, Colours::black);
    lbl_ch->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    
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
    sld_gain->setColour (Slider::rotarySliderFillColourId, Colours::yellow);
    sld_gain->addListener (this);
*/
    
    // create labels and knobs!
    for (int i=0; i<NUM_CHANNELS; i++)
    {
        if (Label* const LABEL = new Label ("new label", String (i+1)))
        {
            lbl_ch.add(LABEL);
            addChildComponent(lbl_ch.getUnchecked(i));
            lbl_ch.getUnchecked(i)->setVisible(true);
            lbl_ch.getUnchecked(i)->setFont (Font (15.00f, Font::plain));
            lbl_ch.getUnchecked(i)->setJustificationType (Justification::centredRight);
            lbl_ch.getUnchecked(i)->setEditable (false, false, false);
            lbl_ch.getUnchecked(i)->setColour (Label::textColourId, Colours::white);
            lbl_ch.getUnchecked(i)->setColour (TextEditor::textColourId, Colours::black);
            lbl_ch.getUnchecked(i)->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
        }
        if (Slider* const DELAY = new Slider (String(2*i+1)))
        {
            sld_del.add(DELAY);
            addChildComponent(sld_del.getUnchecked(i));
            sld_del.getUnchecked(i)->setVisible(true);
            
            String tooltip = "delay ch";
            tooltip.append(String(i+1), 3);
            
            sld_del.getUnchecked(i)->setTooltip(tooltip);
            
            sld_del.getUnchecked(i)->setRange (0, MAX_DELAYTIME_S*1000.f, 0.1);
            sld_del.getUnchecked(i)->setSliderStyle (Slider::Rotary);
            sld_del.getUnchecked(i)->setTextBoxStyle (Slider::TextBoxLeft, false, 50, 18);
            sld_del.getUnchecked(i)->setColour (Slider::rotarySliderFillColourId, Colours::red);
            sld_del.getUnchecked(i)->addListener (this);
            sld_del.getUnchecked(i)->setExplicitFocusOrder(NUM_CHANNELS+i);
            sld_del.getUnchecked(i)->setDoubleClickReturnValue(true, 0.f);
        }
        if (Slider* const GAIN = new Slider (String(2*i)))
        {
            sld_gain.add(GAIN);
            addChildComponent(sld_gain.getUnchecked(i));
            sld_gain.getUnchecked(i)->setVisible(true);
            
            String tooltip = "gain ch";
            tooltip.append(String(i+1), 3);
            
            sld_gain.getUnchecked(i)->setTooltip(tooltip);
            
            sld_gain.getUnchecked(i)->setRange (-18, 18, 0.01);
            sld_gain.getUnchecked(i)->setSliderStyle (Slider::Rotary);
            sld_gain.getUnchecked(i)->setTextBoxStyle (Slider::TextBoxLeft, false, 50, 18);
            sld_gain.getUnchecked(i)->setColour (Slider::rotarySliderFillColourId, Colours::yellow);
            sld_gain.getUnchecked(i)->addListener (this);
            sld_gain.getUnchecked(i)->setExplicitFocusOrder(i);
            sld_gain.getUnchecked(i)->setDoubleClickReturnValue(true, 0.f);
        }
    }
    
    if (NUM_CHANNELS > GROUP_CHANNELS)
        setSize (420, 50+NUM_CHANNELS/2*LINE_WIDTH + METER_GROUP_SPACE * NUM_CHANNELS/GROUP_CHANNELS);
    else
        setSize (200, 50+NUM_CHANNELS*LINE_WIDTH + METER_GROUP_SPACE*NUM_CHANNELS/GROUP_CHANNELS);
    


    // register as change listener (gui/dsp sync)
    ownerFilter->addChangeListener(this);
    ownerFilter->sendChangeMessage(); // get status from dsp
    //[Constructor] You can add your own custom stuff here..
    //[/Constructor]
}

Mcfx_gain_delayAudioProcessorEditor::~Mcfx_gain_delayAudioProcessorEditor()
{
    Mcfx_gain_delayAudioProcessor* ourProcessor = getProcessor();
    
    // remove me as listener for changes
    ourProcessor->removeChangeListener(this);
    
    lbl_gd = nullptr;
    label2 = nullptr;
    label3 = nullptr;
    btn_paste_gain = nullptr;
    btn_paste_gain2 = nullptr;

}

//==============================================================================
void Mcfx_gain_delayAudioProcessorEditor::paint (Graphics& g)
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
    
    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void Mcfx_gain_delayAudioProcessorEditor::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    lbl_gd->setBounds (0, 0, 115, 16);
    label2->setBounds (40, 26, 66, 16);
    label3->setBounds (123, 26, 75, 16);
    
    btn_paste_gain->setBounds (32, 26, 16, 16);
    btn_paste_gain2->setBounds (117, 26, 16, 16);
    
    
    for (int i=0; i < NUM_CHANNELS; i++)
    {
        int group = i / GROUP_CHANNELS;
        
        int column = (float)group / (NUM_CHANNELS/GROUP_CHANNELS) + 0.5f;
        
        int y = 48 + (i - NUM_CHANNELS/2*column - 1*column) * LINE_WIDTH + group * METER_GROUP_SPACE;
        
        int x_offset = column * 220;
        
        //int y=10;
        if (lbl_ch.size() > i)
            lbl_ch.getUnchecked(i)->setBounds(-2+x_offset, y, 29, 16);
        if (sld_del.size() > i)
            sld_del.getUnchecked(i)->setBounds(117+x_offset, y-3, 84, 24);
        if (sld_gain.size() > i)
            sld_gain.getUnchecked(i)->setBounds(30+x_offset, y-3, 77, 24);
    }
    
    //lbl_ch->setBounds (-2, 48, 29, 16);
    //sld_del->setBounds (117, 45, 84, 24);
    //sld_gain->setBounds (30, 45, 77, 24);
}

void Mcfx_gain_delayAudioProcessorEditor::sliderValueChanged (Slider* sliderThatWasMoved)
{
    Mcfx_gain_delayAudioProcessor* ourProcessor = getProcessor();
    
    int slider_nr = sliderThatWasMoved->getName().getIntValue();
    
    if (slider_nr % 2 == 0)
        ourProcessor->setParameterNotifyingHost(slider_nr, (float)sliderThatWasMoved->getValue()/36.f+0.5f);
    else
        ourProcessor->setParameterNotifyingHost(slider_nr, (float)sliderThatWasMoved->getValue()/(MAX_DELAYTIME_S*1000.f));
    
    /*
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
     */
    //[UsersliderValueChanged_Post]
    //[/UsersliderValueChanged_Post]
}

void Mcfx_gain_delayAudioProcessorEditor::buttonClicked (Button* buttonThatWasClicked)
{
    //[UserbuttonClicked_Pre]
    //[/UserbuttonClicked_Pre]
    
    Array<float> values;
    
    // get string from clipboard and remove any nonnumbers
    String clip (SystemClipboard::getTextFromClipboard());
    
    if (clip.isNotEmpty())
    {
        
        clip = clip.replaceCharacter(',', ' ');
        clip = clip.replaceCharacter(';', ' ');
        clip = clip.replaceCharacter('\n', ' ');
        clip = clip.replaceCharacter('\t', ' ');
        clip = clip.retainCharacters("-0123456789. ");
        
        while (clip.isNotEmpty()) {
            float nr = clip.upToFirstOccurrenceOf(" ", false, true).getFloatValue();
            values.add(nr);
            clip = clip.fromFirstOccurrenceOf(" ", false, true).trim();
        }
    }
    
    if (buttonThatWasClicked == btn_paste_gain)
    {
        if (values.size() > 0)
        {
            for (int i=0; i < jmin(NUM_CHANNELS, values.size()); i++) {
                sld_gain.getUnchecked(i)->setValue(values.getUnchecked(i), sendNotification);
            }
        }
        
    }
    else if (buttonThatWasClicked == btn_paste_gain2)
    {
        if (values.size() > 0)
        {
            for (int i=0; i < jmin(NUM_CHANNELS, values.size()); i++) {
                sld_del.getUnchecked(i)->setValue(values.getUnchecked(i), sendNotification);
            }
        }
    }

    //[UserbuttonClicked_Post]
    //[/UserbuttonClicked_Post]
}

void Mcfx_gain_delayAudioProcessorEditor::changeListenerCallback (ChangeBroadcaster *source)
{
    Mcfx_gain_delayAudioProcessor* ourProcessor = getProcessor();
    
    for (int i=0; i < NUM_CHANNELS; i++)
    {
        sld_del.getUnchecked(i)->setValue(ourProcessor->getParameter(2*i+1) * MAX_DELAYTIME_S*1000,dontSendNotification);
        sld_gain.getUnchecked(i)->setValue((ourProcessor->getParameter(2*i)-0.5f)*36.f,dontSendNotification);
        
    }
    
    /*
    sld_az->setValue((ourProcessor->getParameter(Ambix_encoderAudioProcessor::AzimuthParam) - 0.5f) * 360.f, dontSendNotification);
    sld_el->setValue((ourProcessor->getParameter(Ambix_encoderAudioProcessor::ElevationParam) - 0.5f) * 360.f, dontSendNotification);
    sld_size->setValue(ourProcessor->getParameter(Ambix_encoderAudioProcessor::SizeParam), dontSendNotification);
     */
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
                 initialWidth="200" initialHeight="210">
  <BACKGROUND backgroundColour="ff1a1a1a"/>
  <LABEL name="new label" id="b45e45d811b90270" memberName="lbl_gd" virtualName=""
         explicitFocusOrder="0" pos="0 0 115 16" textCol="ffffff00" edTextCol="ff000000"
         edBkgCol="0" labelText="mcfx_gain_delay" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
         fontsize="15" bold="0" italic="0" justification="33"/>
  <SLIDER name="new slider" id="d5c1a3078147bf28" memberName="sld_del"
          virtualName="" explicitFocusOrder="0" pos="117 45 84 24" tooltip="delay ch 1"
          rotarysliderfill="ffff0000" min="0" max="500" int="0.10000000000000000555"
          style="Rotary" textBoxPos="TextBoxLeft" textBoxEditable="1" textBoxWidth="50"
          textBoxHeight="18" skewFactor="1"/>
  <SLIDER name="new slider" id="edc665213f83e06b" memberName="sld_gain"
          virtualName="" explicitFocusOrder="0" pos="30 45 77 24" tooltip="gain ch 1"
          thumbcol="ff5a5a90" trackcol="ffffff00" rotarysliderfill="ffffff00"
          rotaryslideroutline="ffffff00" min="-18" max="18" int="0.010000000000000000208"
          style="Rotary" textBoxPos="TextBoxLeft" textBoxEditable="1" textBoxWidth="50"
          textBoxHeight="18" skewFactor="1"/>
  <LABEL name="new label" id="e30a8f204d299927" memberName="label2" virtualName=""
         explicitFocusOrder="0" pos="40 26 66 16" textCol="ffffffff" edTextCol="ff000000"
         edBkgCol="0" labelText="gain [dB]&#10;" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
         fontsize="13" bold="0" italic="0" justification="34"/>
  <LABEL name="new label" id="a7997dcae0239c4c" memberName="label3" virtualName=""
         explicitFocusOrder="0" pos="123 26 75 16" textCol="ffffffff"
         edTextCol="ff000000" edBkgCol="0" labelText="delay [ms]&#10;"
         editableSingleClick="0" editableDoubleClick="0" focusDiscardsChanges="0"
         fontname="Default font" fontsize="13" bold="0" italic="0" justification="34"/>
  <LABEL name="new label" id="5189e110ab7987d1" memberName="lbl_ch" virtualName=""
         explicitFocusOrder="0" pos="-2 48 29 16" textCol="ffffffff" edTextCol="ff000000"
         edBkgCol="0" labelText="1" editableSingleClick="0" editableDoubleClick="0"
         focusDiscardsChanges="0" fontname="Default font" fontsize="15"
         bold="0" italic="0" justification="34"/>
  <IMAGEBUTTON name="new button" id="fb278a9f97bfef46" memberName="btn_paste_gain"
               virtualName="" explicitFocusOrder="0" pos="32 26 16 16" tooltip="paste gain values from clipboard"
               buttonText="new button" connectedEdges="0" needsCallback="1"
               radioGroupId="0" keepProportions="1" resourceNormal="clipboard35_png"
               opacityNormal="1" colourNormal="0" resourceOver="" opacityOver="1"
               colourOver="0" resourceDown="" opacityDown="1" colourDown="0"/>
  <IMAGEBUTTON name="new button" id="548c7fabb48b0c32" memberName="btn_paste_gain2"
               virtualName="" explicitFocusOrder="0" pos="117 26 16 16" tooltip="paste delay values from clipboard"
               buttonText="new button" connectedEdges="0" needsCallback="1"
               radioGroupId="0" keepProportions="1" resourceNormal="clipboard35_png"
               opacityNormal="1" colourNormal="0" resourceOver="" opacityOver="1"
               colourOver="0" resourceDown="" opacityDown="1" colourDown="0"/>
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


//[EndFile] You can add extra defines here...
//[/EndFile]
