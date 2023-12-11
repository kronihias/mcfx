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
                const float font_size = (i < 99) ? 11.f : 9.f;
                _labels.getUnchecked(i)->setFont (Font (font_size, Font::plain));
                _labels.getUnchecked(i)->setColour (Label::textColourId, Colours::white);
                _labels.getUnchecked(i)->setJustificationType (Justification::centred);
            }
        }
    }

    int rows = floor((NUM_CHANNELS - 1.0) / 64.0) + 1;
    
    for (int i = 0; i < rows; i++) {
        auto* const scale_left = new MeterScaleComponent (163, false);
        _scales.add(scale_left);
        addAndMakeVisible(_scales.getUnchecked(i * 2));
        
        // TODO: For some reason the right scale is not visible...!
        auto* const scale_right = new MeterScaleComponent (163, true);
        _scales.add(scale_right);
        addAndMakeVisible(_scales.getUnchecked(i * 2 + 1));
    }

    //[UserPreSize]
    //[/UserPreSize]

    // make space between each group of eight
    int gr_of_eight = std::min(64, NUM_CHANNELS) / GROUP_CHANNELS;

    _width = 50 + METER_WIDTH * std::min(64, NUM_CHANNELS) + 50 + gr_of_eight*METER_GROUP_SPACE;
    _height = 220 * (floor((NUM_CHANNELS - 1.0) / 64.0) + 1);

    setSize (_width, _height);

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
    g.fillRect (0, 0, _width, _height);

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

void Ambix_meterAudioProcessorEditor::resized()
{

    label.setBounds (0, 0, 104, 16);
    sld_hold.setBounds (166, 0, 70, 24);
    sld_fall.setBounds (307, 0, 70, 24);
    label2.setBounds (111, 3, 64, 16);
    label3.setBounds (239, 3, 77, 16);
    tgl_pkhold.setBounds (382, 0, 91, 24);

    sld_offset.setBounds (4, 23, 18, 167);

    const int row_y_offset = 215;

    for (int i = 0; i < NUM_CHANNELS; i++)
    {
        const int row = (int)floor(i / 64);
        const int group = (i - row * 64) / GROUP_CHANNELS;

        const int x = 50 + (i - row * 64) * METER_WIDTH + group * METER_GROUP_SPACE;
        const int y = 30 + row * row_y_offset;

        _meters.getUnchecked(i)->setBounds(x, y, 8, 163);

        _labels.getUnchecked(i)->setBounds((int)(x-METER_WIDTH*0.75), y - 30 + 195, (int)(METER_WIDTH*2), 14);
    }

    const int rows = floor((NUM_CHANNELS - 1.0) / 64.0) + 1;
    
    for (int i = 0; i < rows; i++) {
        _scales.getUnchecked(i * 2)->setBounds(20, 23 + i * row_y_offset, 50, 200); // left
        _scales.getUnchecked(i * 2 + 1)->setBounds(_width-65, 23 + i * row_y_offset, 50, 200); // right
    }
    

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

    for (int i = 0; i < _scales.size(); i++) {
        _scales.getUnchecked(i)->offset(sld_offset.getValue());
    }

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

//[EndFile] You can add extra defines here...
//[/EndFile]
