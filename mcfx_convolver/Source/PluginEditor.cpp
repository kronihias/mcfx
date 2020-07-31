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
    
//    view.FilterManagingBox.chooseButton.addListener(this);
//    view.FilterManagingBox.saveToggle.addListener(this);
    view.changePathBox.pathButton.addListener(this);
    view.FilterManagingBox.filterSelector.addListener(this);
    view.FilterManagingBox.reloadButton.addListener(this);
    
//    view.irMatrixBox.confModeButton.addListener(this);
//    view.irMatrixBox.wavModeButton.addListener(this);
    
    view.oscManagingBox.activeReceiveToggle.addListener(this);
    view.oscManagingBox.receivePortText.addListener(this);
    
    view.convManagingBox.bufferCombobox.addListener(this);
    view.convManagingBox.maxPartCombobox.addListener(this);
    
    view.inputChannelDialog.OKButton.addListener(this);
    view.inputChannelDialog.saveIntoMetaToggle.addListener(this);
    
    addAndMakeVisible(view);

    setSize (420, 420); //originally 350, 330
    
    UpdateText();
    
    UpdateFiltersMenu();
    
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
    view.convManagingBox.skippedCyclesValue.setText(String(processor.getSkippedCyclesCount()), sendNotification);
    
    view.debugText.setText(processor.getDebugString(), true);   //notify for what purpose?
    view.debugText.moveCaretToEnd();
}

void Mcfx_convolverAudioProcessorEditor::changeListenerCallback (ChangeBroadcaster *source)
{
    UpdateFiltersMenu();
    UpdateText();
    repaint();
}

/// update the overall plugin text based on the processor data and the stored one
void Mcfx_convolverAudioProcessorEditor::UpdateText()
{
    
    view.FilterManagingBox.filterSelector.setText(processor.filterNameToShow,dontSendNotification);
    view.FilterManagingBox.filterSelector.setTooltip(view.FilterManagingBox.filterSelector.getText()); //to see all the string
    
//    view.FilterManagingBox.saveToggle.setToggleState(processor._storeConfigDataInProject.get(), dontSendNotification);
    view.changePathBox.pathText.setText(processor.defaultFilterDir.getFullPathName(), dontSendNotification);
    
    view.ioDetailBox.inputNumber.setText(String(processor._min_in_ch), dontSendNotification);
    view.ioDetailBox.outputNumber.setText(String(processor._min_out_ch), dontSendNotification);
    view.ioDetailBox.IRNumber.setText(String(processor._num_conv), dontSendNotification);
    
    view.ioDetailBox.sampleRateNumber.setText(String(processor.getSamplerate()), dontSendNotification);
    view.ioDetailBox.hostBufferNumber.setText(String(processor.getBufferSize()), dontSendNotification);
    
    view.ioDetailBox.filterLengthInSeconds.setText(String(processor._filter_len_secs, 2), dontSendNotification);
    view.ioDetailBox.filterLengthInSamples.setText(String(processor._filter_len_smpls), dontSendNotification);
    view.ioDetailBox.resampledLabel.setVisible(processor.filterHasBeenResampled.get());
    
    view.convManagingBox.latencyValue.setText(String(processor.getLatencySamples()), sendNotification);
    
    view.oscManagingBox.activeReceiveToggle.setToggleState(processor.getOscIn(), dontSendNotification);
    view.oscManagingBox.receivePortText.setText(String(processor.getOscInPort()), dontSendNotification);
    
//    if (processor.presetType == Mcfx_convolverAudioProcessor::PresetType::conf)
//    {
//        view.irMatrixBox.confModeButton.setToggleState(true, dontSendNotification);
//        view.irMatrixBox.lastState = View::IRMatrixBox::modeState::conf;
//        view.irMatrixBox.newInChannelsButton.setEnabled(false);
//    }
//    else
//    {
//        view.irMatrixBox.wavModeButton.setToggleState(true, dontSendNotification);
//        view.irMatrixBox.lastState = View::IRMatrixBox::modeState::wav;
//        if (processor.filterNameToShow.isNotEmpty())
//            view.irMatrixBox.newInChannelsButton.setEnabled(true);
//        view.inputChannelDialog.saveIntoMetaToggle.setToggleState(processor.storeInChannelIntoWav.get(), dontSendNotification);
//    }
    
    if (processor.getConvolverStatus() == Mcfx_convolverAudioProcessor::ConvolverStatus::Loaded)
        view.FilterManagingBox.reloadButton.setEnabled(true);
    else
        view.FilterManagingBox.reloadButton.setEnabled(false);
    
    view.inputChannelDialog.saveIntoMetaToggle.setToggleState(processor.storeInChannelIntoWav.get(), dontSendNotification);
    
    if(processor.restoredConfiguration.get())
        view.FilterManagingBox.infoLabel.setVisible(true);
    else
        view.FilterManagingBox.infoLabel.setVisible(false);
    
    switch (processor.getConvolverStatus())
    {
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
    
    switch (processor.inChannelStatus)
    {
        case Mcfx_convolverAudioProcessor::InChannelStatus::missing:
            view.inputChannelDialog.setVisible(true);
            view.inputChannelDialog.textEditor.grabKeyboardFocus();
            break;
        case Mcfx_convolverAudioProcessor::InChannelStatus::notFeasible:
            view.inputChannelDialog.setVisible(true);
            view.inputChannelDialog.invalidState(View::InputChannelDialog::InvalidType::notFeasible) ;
            view.inputChannelDialog.textEditor.grabKeyboardFocus();
            break;
        case Mcfx_convolverAudioProcessor::InChannelStatus::notMultiple:
            view.inputChannelDialog.setVisible(true);
            view.inputChannelDialog.invalidState(View::InputChannelDialog::InvalidType::notMultiple) ;
            view.inputChannelDialog.textEditor.grabKeyboardFocus();
            break;
        case Mcfx_convolverAudioProcessor::InChannelStatus::requested:
            view.inputChannelDialog.setVisible(true);
            if(processor.tempInputChannels == -1)
                view.inputChannelDialog.diagonalToggle.setToggleState(true, sendNotification);
            else
                view.inputChannelDialog.textEditor.setText((String)processor.tempInputChannels);
                
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

/// Update the filters  popup menu  based on a predefined folder
void Mcfx_convolverAudioProcessorEditor::UpdateFiltersMenu()
{
    view.FilterManagingBox.filterSelector.clear(dontSendNotification);
    filterSubmenus.clear(); // contains submenus
    
    StringArray subDirectories; // hold name of subdirectories, they will be the just the first parent directory name
    String lastSubdirectory;
    int j = 1;
    int tickedFolder = -1;
    int tickedItem = -1;
    
    for (int i=0; i < processor.filterFilesList.size(); i++)
    {
        // add separator for new subfolder
        String currentFilterDir = processor.filterFilesList.getUnchecked(i).getParentDirectory().getFileNameWithoutExtension();
        
        //check if the current filter file has the same parent directory as the precedent one
        if (!lastSubdirectory.equalsIgnoreCase(currentFilterDir))
        {
            filterSubmenus.add(new PopupMenu());
            subDirectories.add(currentFilterDir);
            
            j++;
            lastSubdirectory = currentFilterDir;
        }
        
        // add item to submenu
        filterSubmenus.getLast()->addItem(i+1, processor.filterFilesList.getUnchecked(i).getFileNameWithoutExtension());
        
        // save indexes of current item and folder if it's the target one for loading
        if (processor.getTargetFilter() == processor.filterFilesList.getUnchecked(i))
        {
            tickedFolder = subDirectories.size()-1;
            tickedItem = i;
        }
        
    }
    
    // add all subdirs to main menu
    for (int i=0; i < filterSubmenus.size(); i++)
    {
        if (i == tickedFolder)
            view.FilterManagingBox.filterSelector.getRootMenu()->addSubMenu(subDirectories.getReference(i), *filterSubmenus.getUnchecked(i), true, nullptr, true);
        else
            view.FilterManagingBox.filterSelector.getRootMenu()->addSubMenu(subDirectories.getReference(i), *filterSubmenus.getUnchecked(i));
    }
    
//    if (processor.filterNameForStoring.isNotEmpty())
//    {
//        view.FilterManagingBox.filterSelector.addSeparator();
//        view.FilterManagingBox.filterSelector.getRootMenu()->addItem(-2, String("save filter to .zip file..."), processor._readyToSaveConfiguration.get());
//    }

    view.FilterManagingBox.filterSelector.addSeparator();
    view.FilterManagingBox.filterSelector.getRootMenu()->addItem(-1, String("open filter from file..."));
    
    //tick the selected item (if present)
    if (tickedItem != -1)
        view.FilterManagingBox.filterSelector.setSelectedItemIndex(tickedItem,dontSendNotification);
}

void Mcfx_convolverAudioProcessorEditor::buttonClicked (Button* buttonThatWasClicked)
{
    /*
    if (buttonThatWasClicked == &(view.FilterManagingBox.chooseButton))
    {
        filterMenu.showMenuAsync(PopupMenu::Options().withTargetComponent (view.FilterManagingBox.chooseButton), ModalCallbackFunction::forComponent (menuItemChosenCallback, this));
    }
    else*/ if (buttonThatWasClicked == &(view.changePathBox.pathButton))
    {
        FileChooser myChooser ("Please select the new filter folder...",
                               processor.defaultFilterDir,
                               "");
        
        if (myChooser.browseForDirectory())
        {
            
            File mooseFile (myChooser.getResult());
            processor.defaultFilterDir = mooseFile;
            
            processor.SearchFilters(mooseFile);
            
            processor.lastSearchDir = mooseFile.getParentDirectory();
            processor.sendChangeMessage();
        }
    }
    else if (buttonThatWasClicked == &view.oscManagingBox.activeReceiveToggle)
    {
        processor.setOscIn(view.oscManagingBox.activeReceiveToggle.getToggleState());
    }
//    else if (buttonThatWasClicked == &(view.FilterManagingBox.saveToggle))
//    {
//        processor._storeConfigDataInProject = view.FilterManagingBox.saveToggle.getToggleState();
//    }
    /*
    else if (buttonThatWasClicked == &(view.irMatrixBox.confModeButton))
    {
        if (view.irMatrixBox.confModeButton.getToggleState())
        {
            if (view.irMatrixBox.lastState != View::IRMatrixBox::conf)
            {
                processor.changePresetType(Mcfx_convolverAudioProcessor::PresetType::conf);
                view.irMatrixBox.lastState = View::IRMatrixBox::conf;
//            std::cout << "conf was clicked" << std::endl;
            }
        }
        else
        {
            if (view.irMatrixBox.lastState != View::IRMatrixBox::wav)
            {
                processor.changePresetType(Mcfx_convolverAudioProcessor::PresetType::wav);
                view.irMatrixBox.lastState = View::IRMatrixBox::wav;
//            std::cout << "conf was clicked" << std::endl;
            }
        }
    }*/
    else if (buttonThatWasClicked == &(view.FilterManagingBox.reloadButton))
    {
        processor.inChannelStatus = Mcfx_convolverAudioProcessor::InChannelStatus::requested;
        processor.ReloadConfiguration();
    }
    else if (buttonThatWasClicked == &(view.inputChannelDialog.OKButton))
    {
        if (view.inputChannelDialog.diagonalToggle.getToggleState())
        {
            processor.tempInputChannels = -1;
            ///reset input dialog with toggle uncheck
            view.inputChannelDialog.resetState(true);
        }
        else
        {
            processor.tempInputChannels = getInputChannelFromDialog();
            view.inputChannelDialog.resetState();
        }
        processor.notify();
    }
    else if (buttonThatWasClicked == &(view.inputChannelDialog.saveIntoMetaToggle))
    {
        if(view.inputChannelDialog.saveIntoMetaToggle.getToggleState())
            processor.storeInChannelIntoWav.set(true);
        else
            processor.storeInChannelIntoWav.set(false);
    }
}

void Mcfx_convolverAudioProcessorEditor::menuItemChosenCallback (int result, Mcfx_convolverAudioProcessorEditor* demoComponent)
{
    // file chooser....
    if (result == 0)
    {
        // do nothing
    }
    else if (result == -1)
    {
        String extension = "*.wav";
        
        FileChooser myChooser ("Please select the filter file to load...",
                               demoComponent->processor.lastSearchDir, //old version: ourProcessor->lastSearchDir,
                               extension);
        if (myChooser.browseForFileToOpen())
        {
            File mooseFile (myChooser.getResult());
            demoComponent->processor.LoadFilterFromFile(mooseFile);
            demoComponent->processor.lastSearchDir = mooseFile.getParentDirectory();
        }
    }
    else if (result == -2)
    {
//        FileChooser myChooser("Save the loaded filter as .zip file...",
//            demoComponent->processor.lastSearchDir.getChildFile(demoComponent->processor.filterNameForStoring),"*.zip");
//        if (myChooser.browseForFileToSave(true))
//        {
//            File mooseFile(myChooser.getResult());
//            demoComponent->processor.SaveConfiguration(mooseFile);
//
//            demoComponent->processor.lastSearchDir = mooseFile.getParentDirectory();
//        }
    }
    else // load filter from menu based on chosen index
    {
        File empty;
        demoComponent->processor.LoadFilterFromMenu(result - 1);
    }
}

void Mcfx_convolverAudioProcessorEditor::comboBoxChanged (ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &view.FilterManagingBox.filterSelector)
    {
//        MessageManager::callAsync([&] () {
//            menuItemChosenCallback(view.FilterManagingBox.filterSelector.getSelectedId(), this);
//        });
        menuItemChosenCallback(view.FilterManagingBox.filterSelector.getSelectedId(), this);
    }
    else if (comboBoxThatHasChanged == &view.convManagingBox.bufferCombobox)
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
