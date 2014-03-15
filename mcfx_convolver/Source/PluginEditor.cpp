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
label (0),
txt_preset(0),
label5 (0),
txt_debug (0),
btn_open (0),
label2 (0),
label3 (0),
label4 (0),
num_ch (0),
num_spk (0),
num_hrtf (0),
btn_preset_folder (0)
{
    
    tooltipWindow.setMillisecondsBeforeTipAppears (700); // tooltip delay
    
    addAndMakeVisible (label = new Label ("new label",
                                          "Input channels: "));
    label->setFont (Font (15.0000f, Font::plain));
    label->setJustificationType (Justification::centredRight);
    label->setEditable (false, false, false);
    label->setColour (Label::textColourId, Colours::white);
    label->setColour (TextEditor::textColourId, Colours::black);
    label->setColour (TextEditor::backgroundColourId, Colour (0x0));
    
    
    addAndMakeVisible (txt_preset = new TextEditor ("new text editor"));
    txt_preset->setReadOnly(true);
    txt_preset->setPopupMenuEnabled(false);
    
    addAndMakeVisible (label5 = new Label ("new label",
                                           "Preset"));
    label5->setFont (Font (15.0000f, Font::plain));
    label5->setJustificationType (Justification::centredRight);
    label5->setEditable (false, false, false);
    label5->setColour (Label::textColourId, Colours::white);
    label5->setColour (TextEditor::textColourId, Colours::white);
    label5->setColour (TextEditor::backgroundColourId, Colour (0x0));
    
    addAndMakeVisible (txt_debug = new TextEditor ("new text editor"));
    txt_debug->setMultiLine (true);
    txt_debug->setReturnKeyStartsNewLine (false);
    txt_debug->setReadOnly (true);
    txt_debug->setScrollbarsShown (true);
    txt_debug->setCaretVisible (false);
    txt_debug->setPopupMenuEnabled (true);
    txt_debug->setText ("debug window");
    txt_debug->setFont(Font(10,1));
    
    addAndMakeVisible (btn_open = new TextButton ("new button"));
    btn_open->setTooltip ("browse presets or open from file");
    btn_open->setButtonText ("open");
    btn_open->addListener (this);
    btn_open->setColour (TextButton::buttonColourId, Colours::white);
    btn_open->setColour (TextButton::buttonOnColourId, Colours::blue);
    
    addAndMakeVisible (label2 = new Label ("new label",
                                           "Output channels: "));
    label2->setFont (Font (15.0000f, Font::plain));
    label2->setJustificationType (Justification::centredRight);
    label2->setEditable (false, false, false);
    label2->setColour (Label::textColourId, Colours::white);
    label2->setColour (TextEditor::textColourId, Colours::black);
    label2->setColour (TextEditor::backgroundColourId, Colour (0x0));
    
    addAndMakeVisible (label3 = new Label ("new label",
                                           "Impulse responses: "));
    label3->setFont (Font (15.0000f, Font::plain));
    label3->setJustificationType (Justification::centredRight);
    label3->setEditable (false, false, false);
    label3->setColour (Label::textColourId, Colours::white);
    label3->setColour (TextEditor::textColourId, Colours::black);
    label3->setColour (TextEditor::backgroundColourId, Colour (0x0));
    
    addAndMakeVisible (label4 = new Label ("new label",
                                           "debug window"));
    label4->setFont (Font (10.0000f, Font::plain));
    label4->setJustificationType (Justification::centredLeft);
    label4->setEditable (false, false, false);
    label4->setColour (Label::textColourId, Colours::white);
    label4->setColour (TextEditor::textColourId, Colours::black);
    label4->setColour (TextEditor::backgroundColourId, Colour (0x0));
    
    addAndMakeVisible (num_ch = new Label ("new label",
                                           "0"));
    num_ch->setFont (Font (15.0000f, Font::plain));
    num_ch->setJustificationType (Justification::centredRight);
    num_ch->setEditable (false, false, false);
    num_ch->setColour (Label::textColourId, Colours::white);
    num_ch->setColour (TextEditor::textColourId, Colours::black);
    num_ch->setColour (TextEditor::backgroundColourId, Colour (0x0));
    
    addAndMakeVisible (num_spk = new Label ("new label",
                                            "0"));
    num_spk->setFont (Font (15.0000f, Font::plain));
    num_spk->setJustificationType (Justification::centredRight);
    num_spk->setEditable (false, false, false);
    num_spk->setColour (Label::textColourId, Colours::white);
    num_spk->setColour (TextEditor::textColourId, Colours::black);
    num_spk->setColour (TextEditor::backgroundColourId, Colour (0x0));
    
    addAndMakeVisible (num_hrtf = new Label ("new label",
                                             "0"));
    num_hrtf->setFont (Font (15.0000f, Font::plain));
    num_hrtf->setJustificationType (Justification::centredRight);
    num_hrtf->setEditable (false, false, false);
    num_hrtf->setColour (Label::textColourId, Colours::white);
    num_hrtf->setColour (TextEditor::textColourId, Colours::black);
    num_hrtf->setColour (TextEditor::backgroundColourId, Colour (0x0));
    
    addAndMakeVisible (btn_preset_folder = new TextButton ("new button"));
    btn_preset_folder->setTooltip ("choose another preset folder");
    btn_preset_folder->setButtonText ("preset folder");
    btn_preset_folder->addListener (this);
    btn_preset_folder->setColour (TextButton::buttonColourId, Colours::white);
    btn_preset_folder->setColour (TextButton::buttonOnColourId, Colours::blue);
    
    setSize (350, 300);
    
    UpdateText();
    
    UpdatePresets();
    
    txt_preset->setText(ownerFilter->box_preset_str);
    txt_preset->setCaretPosition(txt_preset->getTotalNumChars()-1);
    txt_preset->setTooltip(txt_preset->getText());
    
    ownerFilter->addChangeListener(this); // listen to changes of processor
}

Mcfx_convolverAudioProcessorEditor::~Mcfx_convolverAudioProcessorEditor()
{
    Mcfx_convolverAudioProcessor* ourProcessor = getProcessor();
    
    ourProcessor->removeChangeListener(this);
    
    label = nullptr;
    txt_preset = nullptr;
    label5 = nullptr;
    txt_debug = nullptr;
    btn_open = nullptr;
    label2 = nullptr;
    label3 = nullptr;
    label4 = nullptr;
    num_ch = nullptr;
    num_spk = nullptr;
    num_hrtf = nullptr;
    btn_preset_folder = nullptr;
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
    g.fillRect (0, 0, 350, 300);
    
    g.setColour (Colours::black);
    g.drawRect (0, 0, 350, 300, 1);
    
    g.setColour (Colour (0x410000ff));
    g.fillRoundedRectangle (18.0f, 100.0f, 222.0f, 76.0f, 10.0000f);
    
    g.setColour (Colours::white);
    g.setFont (Font (17.2000f, Font::bold));

    g.drawText ("MCFX-CONVOLVER",
                1, 4, 343, 30,
                Justification::centred, true);
    
    g.setFont (Font (12.4000f, Font::plain));
    g.drawText ("multichannel convolution matrix",
                1, 28, 343, 30,
                Justification::centred, true);
}

void Mcfx_convolverAudioProcessorEditor::resized()
{
    label->setBounds (16, 104, 184, 24);
    txt_preset->setBounds (72, 64, 200, 24);
    label5->setBounds (8, 64, 56, 24);
    txt_debug->setBounds (16, 184, 320, 96);
    btn_open->setBounds (280, 64, 56, 24);
    label2->setBounds (72, 128, 127, 24);
    label3->setBounds (48, 152, 152, 24);
    label4->setBounds (24, 280, 64, 16);
    num_ch->setBounds (192, 104, 40, 24);
    num_spk->setBounds (192, 128, 40, 24);
    num_hrtf->setBounds (192, 152, 40, 24);
    btn_preset_folder->setBounds (248, 96, 94, 24);
    
}


void Mcfx_convolverAudioProcessorEditor::UpdateText()
{
    Mcfx_convolverAudioProcessor* ourProcessor = getProcessor();
	
    
    num_ch->setText(String(ourProcessor->_min_in_ch), dontSendNotification);
    
    num_spk->setText(String(ourProcessor->_min_out_ch), dontSendNotification);

    num_hrtf->setText(String(ourProcessor->_my_convolvers.size()), dontSendNotification);

    
    txt_debug->setText(ourProcessor->_DebugText, true);
    
    txt_preset->setText(ourProcessor->box_preset_str);
    txt_preset->setCaretPosition(txt_preset->getTotalNumChars()-1);
    txt_preset->setTooltip(txt_preset->getText());

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
        popup_submenu.getLast()->addItem(i+1, ourProcessor->_presetFiles.getUnchecked(i).getFileNameWithoutExtension());
        
    }
    
    // add all subdirs to main menu
    for (int i=0; i < popup_submenu.size(); i++) {
        popup_presets.addSubMenu(Subdirs.getReference(i), *popup_submenu.getUnchecked(i));
    }
    
    popup_presets.addItem(-1, String("open from file..."));
    
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
            ourProcessor->LoadConfiguration(mooseFile);
            
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
    
    if (buttonThatWasClicked == btn_open)
    {
        popup_presets.showMenuAsync(PopupMenu::Options().withTargetComponent (btn_open), ModalCallbackFunction::forComponent (menuItemChosenCallback, this));
    }
    else if (buttonThatWasClicked == btn_preset_folder)
    {
        FileChooser myChooser ("Please select the new preset folder...",
                               ourProcessor->presetDir,
                               "*.conf");
        
        if (myChooser.browseForDirectory())
        {
            
            File mooseFile (myChooser.getResult());
            ourProcessor->presetDir = mooseFile;
            
            ourProcessor->SearchPresets(mooseFile);
            
            ourProcessor->lastDir = mooseFile.getParentDirectory();
            UpdatePresets();
        }
    }
    
}

void Mcfx_convolverAudioProcessorEditor::changeListenerCallback (ChangeBroadcaster *source)
{
    UpdateText();
    repaint();
}