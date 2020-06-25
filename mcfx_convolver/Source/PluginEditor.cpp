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

#include "PluginView.h"

//==============================================================================

Mcfx_convolverAudioProcessorEditor::Mcfx_convolverAudioProcessorEditor(Mcfx_convolverAudioProcessor& processorToUse)
    : AudioProcessorEditor (&processorToUse), processor(processorToUse)
{
    LookAndFeel::setDefaultLookAndFeel(&MyLookAndFeel);

    tooltipWindow.setMillisecondsBeforeTipAppears (700); // tooltip delay
    
    view.presetManagingBox.chooseButton.addListener(this);
    view.presetManagingBox.saveToggle.addListener(this);
    view.presetManagingBox.selectFolderButton.addListener(this);
    
    view.irMatrixBox.loadUnloadButton.addListener(this);
    
    view.oscManagingBox.activeReceiveToggle.addListener(this);
    view.oscManagingBox.receivePortText.addListener(this);
    
    view.convManagingBox.bufferCombobox.addListener(this);
    view.convManagingBox.maxPartCombobox.addListener(this);
    
    view.inputChannelDialog.OKButton.addListener(this);
    
    addAndMakeVisible(view);

    setSize (400, 600); //originally 350, 330
    
    UpdateText();
    
    UpdatePresets();
    
//    ownerFilter->addChangeListener(this); // listen to changes of processor
    processor.addChangeListener(this);

    timerCallback();

    startTimer(1000);
}

Mcfx_convolverAudioProcessorEditor::~Mcfx_convolverAudioProcessorEditor()
{
    processor.removeChangeListener(this);
}

//==============================================================================
void Mcfx_convolverAudioProcessorEditor::paint (Graphics& g)
{
}

void Mcfx_convolverAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    view.setBounds(getBounds());
}

void Mcfx_convolverAudioProcessorEditor::timerCallback()
{
    String text("skipped cycles: ");
    text << processor.getSkippedCyclesCount();

    view.skippedCyclesLabel.setText(text, dontSendNotification);
    
    view.debugText.setText(processor.getDebugString(), true);   //notify for what purpose? critical section? 
    view.debugText.moveCaretToEnd();
}

/// update the overall plugin text based on the processor data and the stored one
void Mcfx_convolverAudioProcessorEditor::UpdateText()
{
    view.ioDetailBox.inputNumber.setText(String(processor._min_in_ch), dontSendNotification);
    view.ioDetailBox.outputNumber.setText(String(processor._min_out_ch), dontSendNotification);
    view.ioDetailBox.IRNumber.setText(String(processor._num_conv), dontSendNotification);
    
    view.presetManagingBox.textEditor.setText(processor.presetName);
    view.presetManagingBox.textEditor.setCaretPosition(0);
    view.presetManagingBox.textEditor.setTooltip(view.presetManagingBox.textEditor.getText()); //to see all the string
//    view.presetManagingBox.textEditor.setCaretPosition(view.presetManagingBox.textEditor.getTotalNumChars()-1);
    //view.presetManagingBox.textEditor.setScrollToShowCursor(true);
    
//  ---------------------------------------------------------------------------------------
    view.convManagingBox.bufferCombobox.clear(dontSendNotification);
    
    unsigned int buf = jmax(processor.getBufferSize(), (unsigned int) 1);
    unsigned int conv_buf = jmax(processor.getConvBufferSize(), buf);
  
    int sel = 0;
    unsigned int val = 0;
    
    for (int i=0; val < 8192; i++) {
        
        val = (unsigned int)floor(buf*pow(2,i));
        
        view.convManagingBox.bufferCombobox.addItem(String(val), i+1);
        
        if (val == conv_buf)
            sel = i;
    }
    // view.convManagingBox.bufferCombobox.setSelectedItemIndex(sel, dontSendNotification);
    
//  ---------------------------------------------------------------------------------------
    view.convManagingBox.maxPartCombobox.clear(dontSendNotification);
    sel = 0;
    val = 0;
    unsigned int max_part_size = processor.getMaxPartitionSize();
    for (int i=0; val < 8192; i++) {
      
      val = (unsigned int)floor(conv_buf*pow(2,i));
      
      view.convManagingBox.maxPartCombobox.addItem(String(val), i+1);
      
      if (val == max_part_size)
        sel = i;
    }
    view.convManagingBox.maxPartCombobox.setSelectedItemIndex(sel, dontSendNotification);
    
//  ---------------------------------------------------------------------------------------
    view.oscManagingBox.activeReceiveToggle.setToggleState(processor.getOscIn(), dontSendNotification);
    view.oscManagingBox.receivePortText.setText(String(processor.getOscInPort()), dontSendNotification);

    view.presetManagingBox.saveToggle.setToggleState(processor._storeConfigDataInProject.get(), dontSendNotification);
    
    view.presetManagingBox.pathText.setText(processor.presetDir.getFullPathName(), dontSendNotification);
}

/// Update the popup menu presets based on a predefined preset folder
void Mcfx_convolverAudioProcessorEditor::UpdatePresets()
{
    popup_submenu.clear(); // contains submenus
    
    popup_presets.clear(); // main menu
    
    String lastSubdir;
    StringArray Subdirs; // hold name of subdirs
    
    int j = 1;
    
    for (int i=0; i < processor._presetFiles.size(); i++)
    {
        // add separator for new subfolder
        String subdir = processor._presetFiles.getUnchecked(i).getParentDirectory().getFileNameWithoutExtension();
        
        if (!lastSubdir.equalsIgnoreCase(subdir))
        {
            popup_submenu.add(new PopupMenu());
            Subdirs.add(subdir);
            
            j++;
            lastSubdir = subdir;
        }
        
        // add item to submenu
        // check if this preset is loaded
        if (processor.configFileLoaded == processor._presetFiles.getUnchecked(i))
            popup_submenu.getLast()->addItem(i+1, processor._presetFiles.getUnchecked(i).getFileName(), true, true);
        else
            popup_submenu.getLast()->addItem(i+1, processor._presetFiles.getUnchecked(i).getFileName());
    }
    
    // add all subdirs to main menu
    for (int i=0; i < popup_submenu.size(); i++)
    {
        if (Subdirs.getReference(i) == processor.configFileLoaded.getParentDirectory().getFileName())
            popup_presets.addSubMenu(Subdirs.getReference(i), *popup_submenu.getUnchecked(i), true, nullptr, true);
        else
            popup_presets.addSubMenu(Subdirs.getReference(i), *popup_submenu.getUnchecked(i));
    }
    

    if (processor.activePresetName.isNotEmpty())
    {
        popup_presets.addSeparator();
        popup_presets.addItem(-2, String("save preset to .zip file..."), processor._readyToSaveConfiguration.get());
    }

    popup_presets.addSeparator();
    popup_presets.addItem(-1, String("open preset from file..."));
}

void Mcfx_convolverAudioProcessorEditor::buttonClicked (Button* buttonThatWasClicked)
{
    if (buttonThatWasClicked == &(view.presetManagingBox.chooseButton))
    {
        popup_presets.showMenuAsync(PopupMenu::Options().withTargetComponent (view.presetManagingBox.chooseButton), ModalCallbackFunction::forComponent (menuItemChosenCallback, this));
    }
    else if (buttonThatWasClicked == &(view.presetManagingBox.selectFolderButton))
    {
        FileChooser myChooser ("Please select the new preset folder...",
                               processor.presetDir,
                               "");
        
        if (myChooser.browseForDirectory())
        {
            
            File mooseFile (myChooser.getResult());
            processor.presetDir = mooseFile;
            
            processor.SearchPresets(mooseFile);
            
            processor.presetLastDirectory = mooseFile.getParentDirectory();
            processor.sendChangeMessage();
        }
    }
    else if (buttonThatWasClicked == &view.oscManagingBox.activeReceiveToggle)
    {
        processor.setOscIn(view.oscManagingBox.activeReceiveToggle.getToggleState());
    }
    else if (buttonThatWasClicked == &(view.presetManagingBox.saveToggle))
    {
        processor._storeConfigDataInProject = view.presetManagingBox.saveToggle.getToggleState();
    }
    else if (buttonThatWasClicked == &(view.irMatrixBox.loadUnloadButton))
    {
        FileChooser chooser (   "Please select the IR filter matrix file to load...",
                                processor.IRlastDirectory,
                                "*.wav"
                            );
        if (chooser.browseForFileToOpen())
        {
            File pickedFile (chooser.getResult());
            processor.LoadIRMatrixFilterAsync(pickedFile);
            processor.IRlastDirectory = pickedFile.getParentDirectory();
            view.presetManagingBox.setEnabled(false);
        }
    }
    else if (buttonThatWasClicked == &(view.inputChannelDialog.OKButton))
    {
        if (getInputChannelFromDialog() > 0 && getInputChannelFromDialog() <= NUM_CHANNELS)
        {
            processor._min_in_ch = getInputChannelFromDialog();
            processor.inputChannelRequired = false;
            processor.notify();
            
            view.inputChannelDialog.setVisible(false);
            view.inputChannelDialog.resetState();
        }
        else
        {
            view.inputChannelDialog.invalidState();
        }
    }
}

void Mcfx_convolverAudioProcessorEditor::menuItemChosenCallback (int result, Mcfx_convolverAudioProcessorEditor* demoComponent)
{
    // std::cout << "result: " << result << std::endl;
    
    // file chooser....
    if (result == 0)
    {
        // do nothing
    }
    else if (result == -1)
    {
        FileChooser myChooser ("Please select the preset file to load...",
                               demoComponent->processor.presetLastDirectory, //old version: ourProcessor->presetLastDirectory,
                               "*.conf");
        if (myChooser.browseForFileToOpen())
        {
            File mooseFile (myChooser.getResult());
//            demoComponent->processor.LoadConfigurationAsync(mooseFile);
            demoComponent->processor.LoadSetupFromFile(mooseFile);
            demoComponent->processor.presetLastDirectory = mooseFile.getParentDirectory();
        }
    }
    else if (result == -2)
    {
        FileChooser myChooser("Save the loaded preset as .zip file...",
            demoComponent->processor.presetLastDirectory.getChildFile(demoComponent->processor.activePresetName),"*.zip");
        if (myChooser.browseForFileToSave(true))
        {
            File mooseFile(myChooser.getResult());
            demoComponent->processor.SaveConfiguration(mooseFile);

            demoComponent->processor.presetLastDirectory = mooseFile.getParentDirectory();
        }
    }
    else // load preset
    {
//        demoComponent->processor.LoadPreset(result - 1);
        File empty;
        demoComponent->processor.LoadPresetFromMenu(result - 1);
    }
}

void Mcfx_convolverAudioProcessorEditor::changeListenerCallback (ChangeBroadcaster *source)
{
    UpdateText();
    UpdatePresets();
    repaint();
    if (processor.inputChannelRequired)
    {
        view.inputChannelDialog.setVisible(true);
    }
}

void Mcfx_convolverAudioProcessorEditor::comboBoxChanged (ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &view.convManagingBox.bufferCombobox)
    {
        int val = view.convManagingBox.bufferCombobox.getText().getIntValue();
        
        // std::cout << "set size: " << val << std::endl;
        processor.setConvBufferSize(val);
    }
    else if (comboBoxThatHasChanged == &view.convManagingBox.maxPartCombobox)
    {
        int val = view.convManagingBox.maxPartCombobox.getText().getIntValue();
        processor.setMaxPartitionSize(val);
    }
}

void Mcfx_convolverAudioProcessorEditor::textEditorFocusLost(TextEditor & ed)
{
    if (&ed == &view.oscManagingBox.receivePortText)
    {
        processor.setOscInPort(view.oscManagingBox.receivePortText.getText().getIntValue());
    }
}

void Mcfx_convolverAudioProcessorEditor::textEditorReturnKeyPressed(TextEditor & ed)
{
    textEditorFocusLost(ed);
}
  
int Mcfx_convolverAudioProcessorEditor::getInputChannelFromDialog()
{
    int value;
    
    if (!view.inputChannelDialog.textEditor.isEmpty())
    {
        String temp =  view.inputChannelDialog.textEditor.getText();
        value = temp.getIntValue();
    }
    else
    {
        return -1;
    }
    return value;
}
