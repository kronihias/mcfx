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

#include "PluginProcessor.h"
#include "PluginEditor.h"


//==============================================================================
Mcfx_convolverAudioProcessorEditor::Mcfx_convolverAudioProcessorEditor (Mcfx_convolverAudioProcessor* ownerFilter)
    : AudioProcessorEditor (ownerFilter),
label ("new label", "Input channels: "),
txt_preset("new text editor"),
label5 ("new label", "Preset"),
txt_debug ("new text editor"),
btn_open ("new button"),
label2 ("new label", "Output channels: "),
label3 ("new label", "Impulse responses: "),
label4 ("new label", "debug window"),
lbl_skippedcycles ("new label", "skipped cycles: "),
num_ch ("new label", "0"),
num_spk ("new label", "0"),
num_hrtf ("new label", "0"),
btn_preset_folder ("new button"),
box_conv_buffer ("new combobox"),
box_maxpart("new combobox")
{
    LookAndFeel::setDefaultLookAndFeel(&MyLookAndFeel);

    tooltipWindow.setMillisecondsBeforeTipAppears (700); // tooltip delay

    addAndMakeVisible (label);
    label.setFont (Font (15.0000f, Font::plain));
    label.setJustificationType (Justification::centredRight);
    label.setEditable (false, false, false);
    label.setColour (Label::textColourId, Colours::white);
    label.setColour (TextEditor::textColourId, Colours::black);
    label.setColour (TextEditor::backgroundColourId, Colour (0x0));

    addAndMakeVisible (txt_preset);
    txt_preset.setReadOnly(true);
    txt_preset.setPopupMenuEnabled(false);
    
    addAndMakeVisible (label5);
    label5.setFont (Font (15.0000f, Font::plain));
    label5.setJustificationType (Justification::centredRight);
    label5.setEditable (false, false, false);
    label5.setColour (Label::textColourId, Colours::white);
    label5.setColour (TextEditor::textColourId, Colours::white);
    label5.setColour (TextEditor::backgroundColourId, Colour (0x0));
    
    addAndMakeVisible (txt_debug);
    txt_debug.setMultiLine (true);
    txt_debug.setReturnKeyStartsNewLine (false);
    txt_debug.setReadOnly (true);
    txt_debug.setScrollbarsShown (true);
    txt_debug.setCaretVisible (false);
    txt_debug.setPopupMenuEnabled (true);
    txt_debug.setText ("debug window");
    txt_debug.setFont(Font(10,1));
    
    addAndMakeVisible (btn_open);
    btn_open.setTooltip ("browse presets or open from file");
    btn_open.setButtonText ("open");
    btn_open.addListener (this);
    btn_open.setColour (TextButton::buttonColourId, Colours::white);
    btn_open.setColour (TextButton::buttonOnColourId, Colours::blue);
    
    addAndMakeVisible (label2);
    label2.setFont (Font (15.0000f, Font::plain));
    label2.setJustificationType (Justification::centredRight);
    label2.setEditable (false, false, false);
    label2.setColour (Label::textColourId, Colours::white);
    label2.setColour (TextEditor::textColourId, Colours::black);
    label2.setColour (TextEditor::backgroundColourId, Colour (0x0));
    
    addAndMakeVisible (label3);
    label3.setFont (Font (15.0000f, Font::plain));
    label3.setJustificationType (Justification::centredRight);
    label3.setEditable (false, false, false);
    label3.setColour (Label::textColourId, Colours::white);
    label3.setColour (TextEditor::textColourId, Colours::black);
    label3.setColour (TextEditor::backgroundColourId, Colour (0x0));
    
    addAndMakeVisible (label4);
    label4.setFont (Font (10.0000f, Font::plain));
    label4.setJustificationType (Justification::centredLeft);
    label4.setEditable (false, false, false);
    label4.setColour (Label::textColourId, Colours::white);
    label4.setColour (TextEditor::textColourId, Colours::black);
    label4.setColour (TextEditor::backgroundColourId, Colour (0x0));
    
    addAndMakeVisible(lbl_skippedcycles);
    lbl_skippedcycles.setFont(Font(10.0000f, Font::plain));
    lbl_skippedcycles.setJustificationType(Justification::centredLeft);
    lbl_skippedcycles.setEditable(false, false, false);
    lbl_skippedcycles.setColour(Label::textColourId, Colours::yellow);
    lbl_skippedcycles.setColour(TextEditor::textColourId, Colours::black);
    lbl_skippedcycles.setColour(TextEditor::backgroundColourId, Colour(0x0));

    addAndMakeVisible (num_ch);
    num_ch.setFont (Font (15.0000f, Font::plain));
    num_ch.setJustificationType (Justification::centredRight);
    num_ch.setEditable (false, false, false);
    num_ch.setColour (Label::textColourId, Colours::white);
    num_ch.setColour (TextEditor::textColourId, Colours::black);
    num_ch.setColour (TextEditor::backgroundColourId, Colour (0x0));
    
    addAndMakeVisible (num_spk);
    num_spk.setFont (Font (15.0000f, Font::plain));
    num_spk.setJustificationType (Justification::centredRight);
    num_spk.setEditable (false, false, false);
    num_spk.setColour (Label::textColourId, Colours::white);
    num_spk.setColour (TextEditor::textColourId, Colours::black);
    num_spk.setColour (TextEditor::backgroundColourId, Colour (0x0));
    
    addAndMakeVisible (num_hrtf);
    num_hrtf.setFont (Font (15.0000f, Font::plain));
    num_hrtf.setJustificationType (Justification::centredRight);
    num_hrtf.setEditable (false, false, false);
    num_hrtf.setColour (Label::textColourId, Colours::white);
    num_hrtf.setColour (TextEditor::textColourId, Colours::black);
    num_hrtf.setColour (TextEditor::backgroundColourId, Colour (0x0));
    
    addAndMakeVisible (btn_preset_folder);
    btn_preset_folder.setTooltip ("choose another preset folder");
    btn_preset_folder.setButtonText ("preset folder");
    btn_preset_folder.addListener (this);
    btn_preset_folder.setColour (TextButton::buttonColourId, Colours::white);
    btn_preset_folder.setColour (TextButton::buttonOnColourId, Colours::blue);
    
    addAndMakeVisible(box_conv_buffer);
    box_conv_buffer.setTooltip("set higher buffer size to optimize CPU performance but increased latency");
    box_conv_buffer.addListener(this);
    box_conv_buffer.setEditableText (false);
    box_conv_buffer.setJustificationType (Justification::centredLeft);
  
    addAndMakeVisible(box_maxpart);
    box_maxpart.setTooltip("set maximum partition size for CPU load optimizations, leave it at 8192 if you don't know what you are doing!");
    box_maxpart.addListener(this);
    box_maxpart.setEditableText (false);
    box_maxpart.setJustificationType (Justification::centredLeft);
    box_maxpart.setColour(ComboBox::backgroundColourId, Colour (0xffa8a8a8));

    addAndMakeVisible(txt_rcv_port);
    txt_rcv_port.setTooltip(TRANS("OSC receive port"));
    txt_rcv_port.setMultiLine(false);
    txt_rcv_port.setReturnKeyStartsNewLine(false);
    txt_rcv_port.setScrollbarsShown(false);
    txt_rcv_port.setCaretVisible(false);
    txt_rcv_port.setReadOnly(false);
    txt_rcv_port.setPopupMenuEnabled(true);
    txt_rcv_port.setText(TRANS("7200"));
    txt_rcv_port.addListener(this);

    addAndMakeVisible(tgl_rcv_active);// = new ToggleButton("new toggle button"));
    tgl_rcv_active.setButtonText(TRANS("OSC receive port: "));
    tgl_rcv_active.setTooltip(TRANS("enable OSC receive, supported messages: /reload, /load <preset.conf> (preset needs to be within the search path)"));
    tgl_rcv_active.addListener(this);
    tgl_rcv_active.setToggleState(true, dontSendNotification);
    tgl_rcv_active.setColour(ToggleButton::textColourId, Colours::white);

    addAndMakeVisible(tgl_save_preset);// = new ToggleButton("new toggle button"));
    tgl_save_preset.setButtonText(TRANS("Save preset within project"));
    tgl_save_preset.setTooltip(TRANS("this will save the preset and data within the project and reload it from there; CAUTION: may slow down saving/opening your project and increases project file size!"));
    tgl_save_preset.addListener(this);
    tgl_save_preset.setToggleState(true, dontSendNotification);
    tgl_save_preset.setColour(ToggleButton::textColourId, Colours::white);

    setSize (350, 330);
    
    UpdateText();
    
    UpdatePresets();
    
    txt_preset.setText(ownerFilter->box_preset_str);
    txt_preset.setCaretPosition(txt_preset.getTotalNumChars()-1);
    txt_preset.setTooltip(txt_preset.getText());
    
    ownerFilter->addChangeListener(this); // listen to changes of processor

    timerCallback();

    startTimer(1000);
}

Mcfx_convolverAudioProcessorEditor::~Mcfx_convolverAudioProcessorEditor()
{
    Mcfx_convolverAudioProcessor* ourProcessor = getProcessor();
    
    ourProcessor->removeChangeListener(this);
}

//==============================================================================
void Mcfx_convolverAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (Colours::white);
    
    g.setGradientFill (ColourGradient (Colour (0xff4e4e4e),
                                       (float) (proportionOfWidth (0.6400f)), (float) (proportionOfHeight (0.6933f)),
                                       Colours::black,
                                       (float) (proportionOfWidth (0.1143f)), (float) (proportionOfHeight (0.0800f)),
                                       true));
    g.fillRect (0, 0, 350, 330);
    
    g.setColour (Colours::black);
    g.drawRect (0, 0, 350, 330, 1);
    
    g.setColour (Colour (0x410000ff));
    g.fillRoundedRectangle (18.0f, 132.0f, 190.0f, 77.0f, 10.0000f);
    
    g.setColour (Colours::white);
    g.setFont (Font (17.2000f, Font::bold));

    g.drawText ("MCFX-CONVOLVER",
                1, 3, 343, 25,
                Justification::centred, true);
    
    g.setFont (Font (12.4000f, Font::plain));
    g.drawText ("multichannel non-equal partioned convolution matrix",
                1, 23, 343, 25,
                Justification::centred, true);
    
    g.setFont (Font (12.4000f, Font::plain));
    g.drawText ("First Partition Size",
                200, 104, 135, 30,
                Justification::centredRight, true);
    g.drawText ("Maximum Partition Size",
              200, 145, 135, 30,
              Justification::centredRight, true);
  
  
    /* Version text */
    g.setColour (Colours::white);
    g.setFont (Font (10.00f, Font::plain));
    String version_string;
    version_string << "v" << QUOTE(VERSION);
    g.drawText (version_string,
                getWidth()-51, getHeight()-11, 50, 10,
                Justification::bottomRight, true);
}

void Mcfx_convolverAudioProcessorEditor::resized()
{
    label.setBounds (16, 134, 140, 24);
    txt_preset.setBounds (72, 50, 200, 24);
    label5.setBounds (8, 50, 56, 24);
    txt_debug.setBounds (16, 214, 320, 96);
    btn_open.setBounds (280, 50, 56, 24);
    label2.setBounds (16, 158, 140, 24);
    label3.setBounds (16, 182, 140, 24);
    label4.setBounds (24, 310, 64, 16);
    lbl_skippedcycles.setBounds(110, 310, 150, 16);
    num_ch.setBounds (150, 134, 40, 24);
    num_spk.setBounds (150, 158, 40, 24);
    num_hrtf.setBounds (150, 182, 40, 24);
    btn_preset_folder.setBounds (245, 80, 94, 24);
    
    box_conv_buffer.setBounds (270, 126, 67, 20);
    box_maxpart.setBounds (270, 169, 65, 20);

    tgl_save_preset.setBounds(16, 79, 200, 24);

    txt_rcv_port.setBounds(146, 108, 40, 18);
    tgl_rcv_active.setBounds(16, 105, 130, 24);
}

void Mcfx_convolverAudioProcessorEditor::timerCallback()
{
    Mcfx_convolverAudioProcessor* ourProcessor = getProcessor();
    String text("skipped cycles: ");
    text << ourProcessor->getSkippedCyclesCount();

    lbl_skippedcycles.setText(text, dontSendNotification);

    txt_debug.setText(ourProcessor->getDebugString(), true);
}

void Mcfx_convolverAudioProcessorEditor::UpdateText()
{
    Mcfx_convolverAudioProcessor* ourProcessor = getProcessor();

    
    num_ch.setText(String(ourProcessor->_min_in_ch), dontSendNotification);
    
    num_spk.setText(String(ourProcessor->_min_out_ch), dontSendNotification);

    num_hrtf.setText(String(ourProcessor->_num_conv), dontSendNotification);

    txt_preset.setText(ourProcessor->box_preset_str);
    txt_preset.setCaretPosition(txt_preset.getTotalNumChars()-1);
    txt_preset.setTooltip(txt_preset.getText());

    box_conv_buffer.clear(dontSendNotification);
    
    unsigned int buf = jmax(ourProcessor->getBufferSize(), (unsigned int) 1);
    unsigned int conv_buf = jmax(ourProcessor->getConvBufferSize(), buf);
  
    int sel = 0;
    unsigned int val = 0;
    
    for (int i=0; val < 8192; i++) {
        
        val = (unsigned int)floor(buf*pow(2,i));
        
        box_conv_buffer.addItem(String(val), i+1);
        
        if (val == conv_buf)
            sel = i;
    }
    box_conv_buffer.setSelectedItemIndex(sel, dontSendNotification);
  
    box_maxpart.clear(dontSendNotification);
    sel = 0;
    val = 0;
    unsigned int max_part_size = ourProcessor->getMaxPartitionSize();
    for (int i=0; val < 8192; i++) {
      
      val = (unsigned int)floor(conv_buf*pow(2,i));
      
      box_maxpart.addItem(String(val), i+1);
      
      if (val == max_part_size)
        sel = i;
    }
    box_maxpart.setSelectedItemIndex(sel, dontSendNotification);
    
    tgl_rcv_active.setToggleState(ourProcessor->getOscIn(), dontSendNotification);
    txt_rcv_port.setText(String(ourProcessor->getOscInPort()), dontSendNotification);

    tgl_save_preset.setToggleState(ourProcessor->_storeConfigDataInProject.get(), dontSendNotification);
}

void Mcfx_convolverAudioProcessorEditor::UpdatePresets()
{
    Mcfx_convolverAudioProcessor* ourProcessor = getProcessor();
    
    popup_submenu.clear(); // contains submenus
    
    popup_presets.clear(); // main menu
    
    String lastSubdir;
    StringArray Subdirs; // hold name of subdirs
    
    int j = 1;
    
    for (int i=0; i < ourProcessor->_presetFiles.size(); i++) {
        
        // add separator for new subfolder
        String subdir = ourProcessor->_presetFiles.getUnchecked(i).getParentDirectory().getFileNameWithoutExtension();
        
        if (!lastSubdir.equalsIgnoreCase(subdir)) {
            
            popup_submenu.add(new PopupMenu());
            Subdirs.add(subdir);
            
            j++;
            lastSubdir = subdir;
        }
        
        // add item to submenu
        // check if this preset is loaded
        
        if (ourProcessor->_configFile == ourProcessor->_presetFiles.getUnchecked(i))
        {
            popup_submenu.getLast()->addItem(i+1, ourProcessor->_presetFiles.getUnchecked(i).getFileNameWithoutExtension(), true, true);
        } else {
            popup_submenu.getLast()->addItem(i+1, ourProcessor->_presetFiles.getUnchecked(i).getFileNameWithoutExtension());
        }
        
    }
    
    // add all subdirs to main menu
    for (int i=0; i < popup_submenu.size(); i++) {
        if (Subdirs.getReference(i) == ourProcessor->_configFile.getParentDirectory().getFileName())
        {
            popup_presets.addSubMenu(Subdirs.getReference(i), *popup_submenu.getUnchecked(i), true, nullptr, true);
        } else {
            popup_presets.addSubMenu(Subdirs.getReference(i), *popup_submenu.getUnchecked(i));
        }
    }
    

    if (ourProcessor->activePreset.isNotEmpty())
    {
        popup_presets.addSeparator();
        popup_presets.addItem(-2, String("save preset to .zip file..."), ourProcessor->_readyToSaveConfiguration.get());
    }

    popup_presets.addSeparator();
    popup_presets.addItem(-1, String("open preset from file..."));
}


void Mcfx_convolverAudioProcessorEditor::menuItemChosenCallback (int result, Mcfx_convolverAudioProcessorEditor* demoComponent)
{
    // std::cout << "result: " << result << std::endl;
    
    Mcfx_convolverAudioProcessor* ourProcessor = demoComponent->getProcessor();
    
    
    // file chooser....
    if (result == 0)
    {
        // do nothing
    }
    else if (result == -1)
    {
        FileChooser myChooser ("Please select the preset file to load...",
                               ourProcessor->lastDir,
                               "*.conf");
        if (myChooser.browseForFileToOpen())
        {
            
            File mooseFile (myChooser.getResult());
            //ourProcessor->ScheduleConfiguration(mooseFile);
            ourProcessor->LoadConfigurationAsync(mooseFile);
            
            ourProcessor->lastDir = mooseFile.getParentDirectory();
        }
    }
    else if (result == -2)
    {
        FileChooser myChooser("Save the loaded preset as .zip file...",
            ourProcessor->lastDir.getChildFile(ourProcessor->activePreset),
            "*.zip");
        if (myChooser.browseForFileToSave(true))
        {
            File mooseFile(myChooser.getResult());
            ourProcessor->SaveConfiguration(mooseFile);

            ourProcessor->lastDir = mooseFile.getParentDirectory();
        }
    }
    else // load preset
    {
        ourProcessor->LoadPreset(result - 1);
        
    }
    
}

void Mcfx_convolverAudioProcessorEditor::buttonClicked (Button* buttonThatWasClicked)
{
    Mcfx_convolverAudioProcessor* ourProcessor = getProcessor();
    
    if (buttonThatWasClicked == &btn_open)
    {
        popup_presets.showMenuAsync(PopupMenu::Options().withTargetComponent (btn_open), ModalCallbackFunction::forComponent (menuItemChosenCallback, this));
    }
    else if (buttonThatWasClicked == &btn_preset_folder)
    {
        FileChooser myChooser ("Please select the new preset folder...",
                               ourProcessor->presetDir,
                               "");
        
        if (myChooser.browseForDirectory())
        {
            
            File mooseFile (myChooser.getResult());
            ourProcessor->presetDir = mooseFile;
            
            ourProcessor->SearchPresets(mooseFile);
            
            ourProcessor->lastDir = mooseFile.getParentDirectory();
            UpdatePresets();
        }
    }
    else if (buttonThatWasClicked == &tgl_rcv_active)
    {
      ourProcessor->setOscIn(tgl_rcv_active.getToggleState());
    }
    else if (buttonThatWasClicked == &tgl_save_preset)
    {
        ourProcessor->_storeConfigDataInProject = tgl_save_preset.getToggleState();
    }

}

void Mcfx_convolverAudioProcessorEditor::changeListenerCallback (ChangeBroadcaster *source)
{
    UpdateText();
    UpdatePresets();
    repaint();
}

void Mcfx_convolverAudioProcessorEditor::comboBoxChanged (ComboBox* comboBoxThatHasChanged)
{
    Mcfx_convolverAudioProcessor* ourProcessor = getProcessor();
    
    if (comboBoxThatHasChanged == &box_conv_buffer)
    {
        int val = box_conv_buffer.getText().getIntValue();
        
        // std::cout << "set size: " << val << std::endl;
        ourProcessor->setConvBufferSize(val);
    }
    else if (comboBoxThatHasChanged == &box_maxpart)
    {
        int val = box_maxpart.getText().getIntValue();
        ourProcessor->setMaxPartitionSize(val);
    }
    
}

void Mcfx_convolverAudioProcessorEditor::textEditorFocusLost(TextEditor & ed)
{
  Mcfx_convolverAudioProcessor* ourProcessor = getProcessor();
  if (&ed == &txt_rcv_port)
  {
    ourProcessor->setOscInPort(txt_rcv_port.getText().getIntValue());
  }
}

void Mcfx_convolverAudioProcessorEditor::textEditorReturnKeyPressed(TextEditor & ed)
{
  textEditorFocusLost(ed);
}
