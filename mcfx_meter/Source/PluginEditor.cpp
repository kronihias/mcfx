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

extern float iec_scale(float dB);

//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
Ambix_meterAudioProcessorEditor::Ambix_meterAudioProcessorEditor (Ambix_meterAudioProcessor* ownerFilter) :
AudioProcessorEditor (ownerFilter),
_scale_left(163, false),
_scale_right(163, true)
{
    LookAndFeel::setDefaultLookAndFeel(&MyLookAndFeel);

    tooltipWindow.setMillisecondsBeforeTipAppears (500); // tooltip delay

    String label_text = "mcfx_meter";
    label_text << NUM_CHANNELS;

    addAndMakeVisible (label);
    label.setText(label_text, dontSendNotification);
    label.setFont (Font (15.0000f, Font::plain));
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
    label2.setFont (Font (15.0000f, Font::plain));
    label2.setJustificationType (Justification::centredLeft);
    label2.setEditable (false, false, false);
    label2.setColour (Label::textColourId, Colours::white);
    label2.setColour (TextEditor::textColourId, Colours::black);
    label2.setColour (TextEditor::backgroundColourId, Colour (0x0));

    addAndMakeVisible (label3);
    label3.setText("fall [dB/s]\n", dontSendNotification);
    label3.setFont (Font (15.0000f, Font::plain));
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

    cachedImage_meter_scale_png = ImageCache::getFromMemory (meter_scale_png, meter_scale_pngSize);

    addAndMakeVisible (sld_offset);
    sld_offset.setTooltip ("offset scale");
    sld_offset.setRange (-36, 18, 1);
    sld_offset.setSliderStyle (Slider::LinearVertical);
    sld_offset.setTextBoxStyle (Slider::NoTextBox, true, 40, 18);
    sld_offset.setColour (Slider::rotarySliderFillColourId, Colours::white);
    sld_offset.setColour (Slider::thumbColourId, Colours::grey);
    sld_offset.addListener (this);
    sld_offset.setDoubleClickReturnValue(true, 0.f);

    // create meters and labels!
    for (int i=0; i<NUM_CHANNELS; i++)
    {
        if (MeterComponent* const METER = new MeterComponent())
        {
            _meters.add(METER);
            addChildComponent(_meters.getUnchecked(i));
            _meters.getUnchecked(i)->setVisible(true);

            if (Label* const LABEL = new Label ("new label", String (i+1)))
            {
                _labels.add(LABEL);
                addChildComponent(_labels.getUnchecked(i));
                _labels.getUnchecked(i)->setVisible(true);
                _labels.getUnchecked(i)->setFont (Font (11.0000f, Font::plain));
                _labels.getUnchecked(i)->setColour (Label::textColourId, Colours::white);
                _labels.getUnchecked(i)->setJustificationType (Justification::centred);
            }
        }
    }

    addAndMakeVisible(_scale_left);
    addAndMakeVisible(_scale_right);

    //[UserPreSize]
    //[/UserPreSize]

    // make space between each group of eight
    int gr_of_eight = NUM_CHANNELS / GROUP_CHANNELS;

    _width = 50 + METER_WIDTH * NUM_CHANNELS + 50 + gr_of_eight*METER_GROUP_SPACE;
    setSize (_width, 220);

    // register as change listener (gui/dsp sync)
    ownerFilter->addChangeListener(this);
    ownerFilter->sendChangeMessage(); // get status from dsp

    //[Constructor] You can add your own custom stuff here..
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
    g.fillRect (0, 0, _width, 220);

    /* Version text */
    g.setColour (Colours::white);
    g.setFont (Font (10.00f, Font::plain));
    String version_string;
    version_string << "v" << QUOTE(VERSION);
    g.drawText (version_string,
                getWidth()-51, getHeight()-11, 50, 10,
                Justification::bottomRight, true);

    // draw left scale
    /*
    g.drawImage (cachedImage_meter_scale_png,
                 25, 23, 20, 170,
                 0, 0, cachedImage_meter_scale_png.getWidth(), cachedImage_meter_scale_png.getHeight());
    */

    // draw right scale
    /*
    g.drawImage (cachedImage_meter_scale_png,
                 _width-60, 23, 20, 170,
                 0, 0, cachedImage_meter_scale_png.getWidth(), cachedImage_meter_scale_png.getHeight());
     */
    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void Ambix_meterAudioProcessorEditor::resized()
{

    label.setBounds (0, 0, 104, 16);
    sld_hold.setBounds (166, 0, 70, 24);
    sld_fall.setBounds (307, 0, 70, 24);
    label2.setBounds (111, 3, 64, 16);
    label3.setBounds (239, 3, 77, 16);
    tgl_pkhold.setBounds (382, 0, 91, 24);

    sld_offset.setBounds (4, 23, 18, 167);

    for (int i=0; i<NUM_CHANNELS; i++)
    {
        int group = i / GROUP_CHANNELS;

        int x = 50 + i * METER_WIDTH + group * METER_GROUP_SPACE;
        _meters.getUnchecked(i)->setBounds(x, 30, 8, 163);

        _labels.getUnchecked(i)->setBounds((int)(x-METER_WIDTH*0.75), 195, (int)(METER_WIDTH*2), 14);
    }

    _scale_left.setBounds(20, 23, 50, 200);
    _scale_right.setBounds(_width-65, 23, 50, 200);

    //[UserResized] Add your own custom resize handling here..
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


    for (int i=0; i<NUM_CHANNELS; i++)
    {
        // _meters.getUnchecked(i)->setValue(ourProcessor->_rms.getUnchecked(i), ourProcessor->_peak.getUnchecked(i));
        _meters.getUnchecked(i)->setValue(ourProcessor->_my_meter_dsp.getUnchecked(i)->getRMS(), ourProcessor->_my_meter_dsp.getUnchecked(i)->getPeak(), ourProcessor->_my_meter_dsp.getUnchecked(i)->getPeakHold());
    }

}

void Ambix_meterAudioProcessorEditor::changeListenerCallback (ChangeBroadcaster *source)
{
    Ambix_meterAudioProcessor* ourProcessor = getProcessor();

    sld_hold.setValue(ourProcessor->_hold,dontSendNotification);
    sld_fall.setValue(ourProcessor->_fall,dontSendNotification);
    tgl_pkhold.setToggleState(ourProcessor->_pk_hold, dontSendNotification);

    sld_offset.setValue(ourProcessor->_offset, dontSendNotification);

    _scale_left.offset(sld_offset.getValue());
    _scale_right.offset(sld_offset.getValue());

    for (int i=0; i<NUM_CHANNELS; i++)
    {
        _meters.getUnchecked(i)->offset( (int)ourProcessor->_offset);
        _meters.getUnchecked(i)->_peak_hold = ourProcessor->_pk_hold;
    }

}

void Ambix_meterAudioProcessorEditor::mouseDown (const MouseEvent& e)
{
    Ambix_meterAudioProcessor* ourProcessor = getProcessor();
    //[UserCode_mouseDown] -- Add your code here...
    for (int i=0; i<NUM_CHANNELS; i++)
    {

        // ourProcessor->_kmdsp.getUnchecked(i)->reset();

        ourProcessor->_my_meter_dsp.getUnchecked(i)->reset();

        _meters.getUnchecked(i)->reset();

    }
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

//==============================================================================
// Binary resources - be careful not to edit any of these sections!


// JUCER_RESOURCE: meter_scale_png, 2204, "meter_scale.png"
static const unsigned char resource_Ambix_meterAudioProcessorEditor_meter_scale_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,20,0,0,0,170,8,6,0,0,0,173,73,111,8,0,0,4,36,105,67,67,80,
    73,67,67,32,80,114,111,102,105,108,101,0,0,56,17,133,85,223,111,219,84,20,62,137,111,82,164,22,63,32,88,71,135,138,197,175,85,83,91,185,27,26,173,198,6,73,147,165,237,74,22,165,233,216,42,36,228,58,55,
    137,169,27,7,219,233,182,170,79,123,129,55,6,252,1,64,217,3,15,72,60,33,13,6,98,123,217,246,192,180,73,83,135,42,170,73,72,123,232,196,15,33,38,237,5,85,225,187,118,98,39,83,196,92,245,250,203,57,223,
    57,231,59,231,94,219,68,61,95,105,181,154,25,85,136,150,171,174,157,207,36,149,147,167,22,148,158,77,138,210,179,212,75,3,212,171,233,78,45,145,203,205,18,46,193,21,247,206,235,225,29,138,8,203,237,145,
    238,254,78,246,35,191,122,139,220,209,137,34,79,192,110,21,29,125,25,248,12,81,204,212,107,182,75,20,191,7,251,248,105,183,6,220,243,28,240,211,54,4,2,171,2,151,125,156,18,120,209,199,39,61,78,33,63,9,
    142,208,42,235,21,173,8,188,6,60,188,216,102,47,183,97,95,3,24,200,147,225,85,110,27,186,34,102,145,179,173,146,97,114,207,225,47,143,113,183,49,255,15,46,155,117,244,236,93,131,88,251,156,165,185,99,
    184,15,137,222,43,246,84,190,137,63,215,181,244,28,240,75,176,95,175,185,73,97,127,5,248,143,250,210,124,2,120,31,81,244,169,146,125,116,222,231,71,95,95,173,20,222,6,222,5,123,209,112,167,11,77,251,106,
    117,49,123,28,24,177,209,245,37,235,152,200,35,56,215,117,103,18,179,164,23,128,239,86,248,180,216,99,232,145,168,200,83,105,224,97,224,193,74,125,170,153,95,154,113,86,230,132,221,203,179,90,153,204,
    2,35,143,100,191,167,205,228,128,7,128,63,180,173,188,168,5,205,210,58,55,51,162,22,242,75,87,107,110,174,169,65,218,174,154,89,81,11,253,50,153,59,94,143,200,41,109,187,149,194,148,31,203,14,186,118,
    161,25,203,22,74,198,209,233,38,127,173,102,122,103,17,218,216,5,187,158,23,218,144,159,221,210,236,116,6,24,121,216,63,188,58,47,230,6,28,219,93,212,82,98,182,163,192,71,232,68,68,35,78,22,45,98,213,
    169,74,59,164,80,158,50,148,196,189,70,54,60,37,50,200,132,133,195,203,97,49,34,79,210,18,108,221,121,57,143,227,199,132,140,178,23,189,45,162,81,165,59,199,175,112,191,201,177,88,63,83,217,1,252,31,98,
    179,236,48,27,103,19,164,176,55,216,155,236,8,75,193,58,193,14,5,10,114,109,58,21,42,211,125,40,245,243,188,79,117,84,22,58,78,80,242,252,64,125,40,136,81,206,217,239,26,250,205,143,254,134,154,176,75,
    43,244,35,79,208,49,52,91,237,19,32,7,19,104,117,55,210,62,167,107,107,63,236,14,60,202,6,187,248,206,237,190,107,107,116,252,113,243,141,221,139,109,199,54,176,110,198,182,194,248,216,175,177,45,252,
    109,82,2,59,96,122,138,150,161,202,240,118,194,9,120,35,29,61,92,1,211,37,13,235,111,224,89,208,218,82,218,177,163,165,234,249,129,208,35,38,193,63,200,62,204,210,185,225,208,170,254,162,254,169,110,168,
    95,168,23,212,223,59,106,132,25,59,166,36,125,42,125,43,253,40,125,39,125,47,253,76,138,116,89,186,34,253,36,93,149,190,145,46,5,57,187,239,189,159,37,216,123,175,95,97,19,221,138,93,104,213,107,159,53,
    39,83,78,202,123,228,23,229,148,252,188,252,178,60,27,176,20,185,95,30,147,167,228,189,240,236,9,246,205,12,253,29,189,24,116,10,179,106,77,181,123,45,241,4,24,52,15,37,6,157,6,215,198,84,197,132,171,
    116,22,89,219,159,147,166,82,54,200,198,216,244,35,167,118,92,156,229,150,138,120,58,158,138,39,72,137,239,139,79,196,199,226,51,2,183,158,188,248,94,248,38,176,166,3,245,254,147,19,48,58,58,224,109,44,
    76,37,200,51,226,157,58,113,86,69,244,10,116,155,208,205,93,126,6,223,14,162,73,171,118,214,54,202,21,87,217,175,170,175,41,9,124,202,184,50,93,213,71,135,21,205,52,21,207,229,40,54,119,184,189,194,139,
    163,36,190,131,34,142,232,65,222,251,190,69,118,221,12,109,238,91,68,135,255,194,59,235,86,104,91,168,19,125,237,16,245,191,26,218,134,240,78,124,230,51,162,139,7,245,186,189,226,231,163,72,228,6,145,
    83,58,176,223,251,29,233,75,226,221,116,183,209,120,128,247,85,207,39,68,59,31,55,26,255,174,55,26,59,95,34,255,22,209,101,243,63,89,0,113,120,108,27,43,19,0,0,0,9,112,72,89,115,0,0,11,19,0,0,11,19,1,
    0,154,156,24,0,0,4,30,73,68,65,84,104,5,237,91,205,107,83,65,16,79,76,211,8,165,69,168,167,82,63,81,4,21,161,23,61,136,34,136,30,4,193,131,208,63,192,131,104,65,188,120,42,248,7,244,228,127,160,158,196,
    136,23,189,84,80,106,173,120,49,209,10,138,66,139,120,178,74,90,34,77,109,62,250,17,127,147,118,151,237,203,236,246,77,26,10,194,44,76,119,118,103,118,222,188,223,182,59,51,239,189,38,235,245,122,162,
    157,109,71,59,141,145,45,53,184,57,162,149,74,101,31,54,242,13,168,4,26,47,151,203,123,220,85,98,12,59,59,59,71,96,232,197,220,220,92,31,250,87,153,76,102,196,53,152,192,164,136,86,87,87,103,230,231,231,
    119,211,58,234,49,254,233,218,16,123,8,111,122,71,71,71,139,228,85,62,159,255,67,99,226,109,115,173,199,225,225,209,175,98,177,184,171,157,30,78,244,244,244,12,205,206,206,118,119,117,117,93,135,103,111,
    173,119,196,196,241,202,213,193,174,30,128,151,19,160,5,234,49,222,239,202,147,52,104,103,107,101,83,130,215,87,131,65,120,98,9,21,195,88,48,5,149,20,195,32,60,177,132,138,97,44,152,130,74,138,97,16,158,
    88,66,197,48,22,76,65,37,197,48,8,79,44,225,127,142,225,244,244,116,102,121,121,249,52,114,200,133,232,253,122,203,11,55,251,100,120,76,217,182,33,219,93,89,89,121,12,26,46,20,10,221,232,239,130,30,65,
    51,118,74,76,86,55,24,68,58,204,150,23,238,166,80,110,108,40,122,135,220,152,47,47,162,87,246,140,57,15,217,242,194,245,144,243,34,52,199,151,23,30,143,12,94,228,89,180,53,100,190,242,66,203,138,208,30,
    196,147,109,101,151,217,43,168,65,22,22,209,164,98,40,130,139,85,86,12,89,88,68,147,138,161,8,46,86,89,49,100,97,17,77,42,134,34,184,88,101,197,144,133,69,52,25,196,16,153,254,45,164,175,121,80,25,52,
    137,154,229,162,177,222,82,157,2,131,79,171,213,234,177,169,169,41,42,128,174,98,252,27,134,27,41,241,86,235,148,4,61,162,135,193,175,198,32,248,214,235,148,82,169,212,139,23,49,247,225,213,77,115,203,
    232,91,171,83,112,203,199,225,205,120,173,86,59,105,188,163,30,115,108,157,98,202,7,182,135,71,55,168,134,51,111,120,34,6,159,64,102,106,189,97,92,32,75,114,214,144,179,16,108,83,107,172,209,58,197,110,
    126,240,47,197,106,9,24,53,40,0,203,163,170,24,122,128,17,76,43,134,2,176,60,170,138,161,7,24,193,180,98,40,0,203,163,250,159,99,136,124,229,14,18,145,143,160,10,232,11,114,153,203,230,78,91,74,56,97,
    224,33,178,175,19,235,9,231,37,92,160,0,195,141,220,6,178,214,31,140,231,114,185,52,50,216,65,24,124,102,12,130,103,19,78,55,251,130,174,109,238,60,241,117,24,152,116,63,52,193,184,150,205,102,83,16,37,
    198,198,198,58,48,174,18,31,93,200,142,105,225,210,210,210,57,44,250,70,139,136,192,179,9,103,240,215,6,11,159,3,195,35,3,3,3,29,201,100,178,15,27,178,211,108,10,122,249,131,113,0,127,27,158,124,6,253,
    5,189,67,90,124,202,120,168,9,167,133,54,184,41,86,75,192,168,65,1,88,30,85,197,208,3,140,96,90,49,20,128,229,81,221,126,12,113,106,15,194,25,122,171,107,155,55,38,147,134,57,210,185,30,241,228,40,142,
    254,25,200,168,89,93,95,76,38,29,171,228,46,32,158,222,108,195,88,14,209,238,12,198,212,172,46,93,196,60,41,137,126,11,107,148,214,150,172,253,108,204,193,139,7,48,118,118,221,16,73,140,46,133,80,54,38,
    147,142,85,114,23,172,243,232,154,90,67,31,6,217,152,12,237,224,247,216,73,64,108,136,224,38,222,52,62,38,147,148,172,198,32,114,213,234,249,98,50,233,232,11,107,131,121,235,253,246,255,45,75,125,85,15,
    165,136,53,235,43,134,205,152,72,103,54,197,16,39,247,53,28,75,223,65,139,174,113,111,160,114,207,185,40,15,99,67,56,157,223,99,241,33,70,38,47,30,97,236,19,226,10,125,247,101,15,87,195,67,198,6,42,247,
    150,41,246,26,50,119,119,48,149,74,221,131,17,122,65,243,3,30,95,49,2,244,242,183,21,240,98,1,183,123,152,170,77,120,122,1,99,251,175,35,224,217,64,229,122,232,92,220,178,47,211,233,244,249,254,254,254,
    20,138,199,52,102,221,79,222,248,64,101,48,225,122,4,163,189,240,228,53,104,17,244,193,125,167,226,11,84,109,15,82,255,0,52,253,150,77,58,174,154,44,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* Ambix_meterAudioProcessorEditor::meter_scale_png = (const char*) resource_Ambix_meterAudioProcessorEditor_meter_scale_png;
const int Ambix_meterAudioProcessorEditor::meter_scale_pngSize = 2204;

//[EndFile] You can add extra defines here...
//[/EndFile]
