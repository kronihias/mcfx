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
    
    view.irMatrixBox.newInChannelsButton.addListener(this);
    view.irMatrixBox.confModeButton.addListener(this);
    view.irMatrixBox.wavModeButton.addListener(this);
    
    view.oscManagingBox.activeReceiveToggle.addListener(this);
    view.oscManagingBox.receivePortText.addListener(this);
    
    view.convManagingBox.bufferCombobox.addListener(this);
    view.convManagingBox.maxPartCombobox.addListener(this);
    
    view.inputChannelDialog.OKButton.addListener(this);
    
    addAndMakeVisible(view);

    setSize (400, 530); //originally 350, 330
    
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
    auto test = getLocalBounds();
    view.setBounds(test);
}

void Mcfx_convolverAudioProcessorEditor::timerCallback()
{
    String text("skipped cycles: ");
    text << processor.getSkippedCyclesCount();

    view.skippedCyclesLabel.setText(text, dontSendNotification);
    
    view.debugText.setText(processor.getDebugString(), true);   //notify for what purpose?
    view.debugText.moveCaretToEnd();
}

void Mcfx_convolverAudioProcessorEditor::changeListenerCallback (ChangeBroadcaster *source)
{
    UpdateText();
    UpdatePresets();
    repaint();
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
    view.presetManagingBox.saveToggle.setToggleState(processor._storeConfigDataInProject.get(), dontSendNotification);
    view.presetManagingBox.pathText.setText(processor.defaultPresetDir.getFullPathName(), dontSendNotification);
    
    view.oscManagingBox.activeReceiveToggle.setToggleState(processor.getOscIn(), dontSendNotification);
    view.oscManagingBox.receivePortText.setText(String(processor.getOscInPort()), dontSendNotification);
    
    if (processor.presetType == Mcfx_convolverAudioProcessor::PresetType::conf)
    {
        view.irMatrixBox.confModeButton.setToggleState(true, dontSendNotification);
        view.irMatrixBox.lastState = View::IRMatrixBox::modeState::conf;
    }
    else
    {
        view.irMatrixBox.wavModeButton.setToggleState(true, dontSendNotification);
        view.irMatrixBox.lastState = View::IRMatrixBox::modeState::wav;
    }
    
    switch (processor.getConvolverStatus()) {
        case Mcfx_convolverAudioProcessor::ConvolverStatus::Unloaded :
            view.statusLed.setStatus(View::StatusLed::State::red);
            break;
        case Mcfx_convolverAudioProcessor::ConvolverStatus::Loading:
            view.statusLed.setStatus(View::StatusLed::State::yellow);
            break;
        case Mcfx_convolverAudioProcessor::ConvolverStatus::Loaded:
            view.statusLed.setStatus(View::StatusLed::State::green);
            break;
        default:
            break;
    }
    
    if (processor.newStatusText)
    {
        view.statusText.setText(processor.getStatusText());
        processor.newStatusText = false;
    }
    
    switch (processor.inChannelStatus) {
        case Mcfx_convolverAudioProcessor::InChannelStatus::missing:
            view.inputChannelDialog.setVisible(true);
            view.inputChannelDialog.textEditor.grabKeyboardFocus();
            break;
        case Mcfx_convolverAudioProcessor::InChannelStatus::notFeasible:
            view.inputChannelDialog.setVisible(true);
            view.inputChannelDialog.invalidState(View::InputChannelDialog::InvalidType::notFeasible, processor.getTotalNumInputChannels()) ;
            view.inputChannelDialog.textEditor.grabKeyboardFocus();
            break;
        case Mcfx_convolverAudioProcessor::InChannelStatus::notMultiple:
            view.inputChannelDialog.setVisible(true);
            view.inputChannelDialog.invalidState(View::InputChannelDialog::InvalidType::notMultiple) ;
            view.inputChannelDialog.textEditor.grabKeyboardFocus();
            break;
        
        default:
            break;
    }
    
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
     view.convManagingBox.bufferCombobox.setSelectedItemIndex(sel, dontSendNotification);
    
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
}

/// Update the popup menu presets based on a predefined preset folder
void Mcfx_convolverAudioProcessorEditor::UpdatePresets()
{
    presetMenu.clear(); // main menu
    presetSubmenu.clear(); // contains submenus
    
    StringArray subDirectories; // hold name of subdirectories, they will be the just the first parent directory name
    String lastSubdirectory;
    int j = 1;
    
    for (int i=0; i < processor.presetFilesList.size(); i++)
    {
        // add separator for new subfolder
        String currentPresetDirectory = processor.presetFilesList.getUnchecked(i).getParentDirectory().getFileNameWithoutExtension();
        
        //check if the current preset File has the same parent directory as the precedent one
        if (!lastSubdirectory.equalsIgnoreCase(currentPresetDirectory))
        {
            presetSubmenu.add(new PopupMenu());
            subDirectories.add(currentPresetDirectory);
            
            j++;
            lastSubdirectory = currentPresetDirectory;
        }
        
        // add item to submenu
        // check if this preset is the target of the loading configuration stage (even if it fails to load)
        if (processor.getTargetPreset() == processor.presetFilesList.getUnchecked(i))
            presetSubmenu.getLast()->addItem(i+1, processor.presetFilesList.getUnchecked(i).getFileName(), true, true);
        else
            presetSubmenu.getLast()->addItem(i+1, processor.presetFilesList.getUnchecked(i).getFileName());
    }
    
    // add all subdirs to main menu
    for (int i=0; i < presetSubmenu.size(); i++)
    {
        if (subDirectories.getReference(i) == processor.getTargetPreset().getParentDirectory().getFileName())
            presetMenu.addSubMenu(subDirectories.getReference(i), *presetSubmenu.getUnchecked(i), true, nullptr, true);
        else
            presetMenu.addSubMenu(subDirectories.getReference(i), *presetSubmenu.getUnchecked(i));
    }
    
    //need review
    if (processor.activePresetName.isNotEmpty())
    {
        presetMenu.addSeparator();
        presetMenu.addItem(-2, String("save preset to .zip file..."), processor._readyToSaveConfiguration.get());
    }

    presetMenu.addSeparator();
    presetMenu.addItem(-1, String("open preset from file..."));
}

void Mcfx_convolverAudioProcessorEditor::buttonClicked (Button* buttonThatWasClicked)
{
    if (buttonThatWasClicked == &(view.presetManagingBox.chooseButton))
    {
        presetMenu.showMenuAsync(PopupMenu::Options().withTargetComponent (view.presetManagingBox.chooseButton), ModalCallbackFunction::forComponent (menuItemChosenCallback, this));
    }
    else if (buttonThatWasClicked == &(view.presetManagingBox.selectFolderButton))
    {
        FileChooser myChooser ("Please select the new preset folder...",
                               processor.defaultPresetDir,
                               "");
        
        if (myChooser.browseForDirectory())
        {
            
            File mooseFile (myChooser.getResult());
            processor.defaultPresetDir = mooseFile;
            
            processor.SearchPresets(mooseFile);
            
            processor.lastSearchDir = mooseFile.getParentDirectory();
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
    else if (buttonThatWasClicked == &(view.irMatrixBox.confModeButton))
    {
        if (view.irMatrixBox.confModeButton.getToggleState())
        {
            if (view.irMatrixBox.lastState != View::IRMatrixBox::conf)
            {
                processor.changePresetType(Mcfx_convolverAudioProcessor::PresetType::conf);
                view.irMatrixBox.lastState = View::IRMatrixBox::conf;
                view.irMatrixBox.newInChannelsButton.setEnabled(false);
//            std::cout << "conf was clicked" << std::endl;
            }
        }
        else
        {
            if (view.irMatrixBox.lastState != View::IRMatrixBox::wav)
            {
                processor.changePresetType(Mcfx_convolverAudioProcessor::PresetType::wav);
                view.irMatrixBox.lastState = View::IRMatrixBox::wav;
                view.irMatrixBox.newInChannelsButton.setEnabled(true);
//            std::cout << "conf was clicked" << std::endl;
            }
        }
    }
    else if (buttonThatWasClicked == &(view.irMatrixBox.newInChannelsButton))
    {
        processor.inChannelStatus = Mcfx_convolverAudioProcessor::InChannelStatus::requested;
        processor.ReloadConfiguration();
    }
    else if (buttonThatWasClicked == &(view.inputChannelDialog.OKButton))
    {
//        if (view.inputChannelDialog.diagonalToggle.getToggleState())
//        {
//            processor.tempInputChannels = -1;
//            view.inputChannelDialog.resetState(true);
//            processor.inputChannelRequired = false;
//            processor.notify();
//        }
//        else if ((getInputChannelFromDialog() > 0)
//            && getInputChannelFromDialog() <= processor.getTotalNumInputChannels())
//        {
//            processor.tempInputChannels = getInputChannelFromDialog();
//            view.inputChannelDialog.resetState();
//            processor.inputChannelRequired = false;
//            processor.notify();
//        }
//        else
//        {
//            view.inputChannelDialog.invalidState(processor.getTotalNumInputChannels());
//        }
        if (view.inputChannelDialog.diagonalToggle.getToggleState())
        {
            processor.tempInputChannels = -1;
            ///reset input dialog with toggle uncheck
            view.inputChannelDialog.resetState(true);
//            processor.inputChannelRequired = false;
        }
        else
        {
            processor.tempInputChannels = getInputChannelFromDialog();
            view.inputChannelDialog.resetState();
        }
        processor.notify();
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
        String extension = "*.conf";
        if (demoComponent->processor.presetType ==  Mcfx_convolverAudioProcessor::PresetType::wav)
            extension = "*.wav";
        
        FileChooser myChooser ("Please select the preset file to load...",
                               demoComponent->processor.lastSearchDir, //old version: ourProcessor->lastSearchDir,
                               extension);
        if (myChooser.browseForFileToOpen())
        {
            File mooseFile (myChooser.getResult());
//            demoComponent->processor.LoadConfigurationAsync(mooseFile);
            demoComponent->processor.LoadSetupFromFile(mooseFile);
            demoComponent->processor.lastSearchDir = mooseFile.getParentDirectory();
        }
    }
    else if (result == -2)
    {
        FileChooser myChooser("Save the loaded preset as .zip file...",
            demoComponent->processor.lastSearchDir.getChildFile(demoComponent->processor.activePresetName),"*.zip");
        if (myChooser.browseForFileToSave(true))
        {
            File mooseFile(myChooser.getResult());
            demoComponent->processor.SaveConfiguration(mooseFile);

            demoComponent->processor.lastSearchDir = mooseFile.getParentDirectory();
        }
    }
    else // load preset
    {
//        demoComponent->processor.LoadPreset(result - 1);
        File empty;
        demoComponent->processor.LoadPresetFromMenu(result - 1);
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
    if (!view.inputChannelDialog.textEditor.isEmpty())
    {
        String temp =  view.inputChannelDialog.textEditor.getText();
        int value = temp.getIntValue();
        return value;
    }
    else
    {
        return -2;
    }
    
}
