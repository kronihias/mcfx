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

#ifndef PLUGINEDITOR_H_INCLUDED
#define PLUGINEDITOR_H_INCLUDED

#include "JuceHeader.h"
#include "PluginProcessor.h"


//==============================================================================
/**
*/
class Mcfx_convolverAudioProcessorEditor  : public AudioProcessorEditor,
                                            public Button::Listener,
                                            public ChangeListener,
                                            public ComboBox::Listener,
                                            public Timer,
                                            public TextEditor::Listener,
                                            public FileDragAndDropTarget
{
    Mcfx_convolverAudioProcessor& processor; //<! Reference to the processor object
public:
    Mcfx_convolverAudioProcessorEditor (Mcfx_convolverAudioProcessor& ownerFilter);
    ~Mcfx_convolverAudioProcessorEditor();

    //==============================================================================
    // This is just a standard Juce paint method...
    void paint (Graphics& g) override;

    void resized() override;
    void buttonClicked (Button* buttonThatWasClicked) override;

    void changeListenerCallback (ChangeBroadcaster *source) override;

    void comboBoxChanged (ComboBox* comboBoxThatHasChanged) override;

    void textEditorFocusLost(TextEditor& ed) override;

    void textEditorReturnKeyPressed(TextEditor& ed) override;

    static void menuItemChosenCallback (int result, Mcfx_convolverAudioProcessorEditor* demoComponent);

    void UpdatePresets();

    void UpdateText();

    void timerCallback() override;

    bool isInterestedInFileDrag(StringArray const& files) override;
    void filesDropped(StringArray const& files, int x, int y) override;

private:
    Mcfx_convolverAudioProcessor* getProcessor() const
    {
        return static_cast <Mcfx_convolverAudioProcessor*> (getAudioProcessor());
    }

    void loadWavFile(File wavFile);

    /**
     * @brief Calback function for when the AlertWindow with manual inChannels entry is closed
     * If the user loads a wav file with no inputChannel number in the metadata, an alert window is shown,
     * promping the user to enter the number of input channels manually, the nature of the matrix (diagonal or dense) and whether the channel count
     * should be stored in the wav file.
     * This callback function is called when the alert window is closed, and it gets the user choices and sets the flags accordingly.
     * 
     * If save is chosen, save flags are set to true and the number of input channels is temporarily stored in a member variable of the processor.
     * The wav overwrite is performed after a completed load, and only if successful.
     * 
     * @param result        Code for the button pressed in the alert window
     * @param dropped_file  Wav file that did not contain metadata for automatic load
     * @param ourEditor     Pointer to the editor
     */
    static void InputChannelCallback(int result, File dropped_file, Mcfx_convolverAudioProcessorEditor* ourEditor)
    {
        auto& aw = *ourEditor->asyncAlertWindow;

        aw.exitModalState (result);
        aw.setVisible (false);

        if (result == 0)
        {
            // user pressed cancel
            return;
        }

        auto isDiagonal = aw.getComboBoxComponent ("isDiagonal")->getSelectedItemIndex();
        auto inputChannelsTextField = aw.getTextEditorContents ("numInChannels").getIntValue();
        auto doSaveMetadata = aw.getComboBoxComponent ("saveMetadata")->getSelectedItemIndex();
        int numInChannels = (isDiagonal == 1) ? -1 : inputChannelsTextField;

        auto ourProcessor = ourEditor->getProcessor();
        // ourProcessor->DebugPrint("InputChannelCallback: isDiagonal: " + String(isDiagonal) + ", inputChannelsTextField: " + String(inputChannelsTextField) + ", doSaveMetadata: " + String(doSaveMetadata));

        // Set the flag to store the number of input channels into the wav file
        // The channel count will only be stored if the load is successful
        // If not (e.g. the input channel is not valid, or not a multiple the length) the flag will be ignored and an error message will be displayed
        ourProcessor->storeNumInputsIntoWav.set(doSaveMetadata==1); // Set the flag to store the number of input channels into the wav file
        ourProcessor->numInputChannels = numInChannels;
        // Load the Wav file containing the impulse responses
        ourProcessor->LoadWavFile(dropped_file, numInChannels);
    }

    TooltipWindow tooltipWindow;

    Label label;
    TextEditor txt_preset;
    PopupMenu popup_presets;
    OwnedArray<PopupMenu> popup_submenu;
    Label label5;
    TextEditor txt_debug;
    TextButton btn_open;
    Label label2;
    Label label3;
    TextButton btn_clear_debug;
    Label lbl_skippedcycles;
    Label num_ch;
    Label num_spk;
    Label num_hrtf;
    TextButton btn_preset_folder;
    ComboBox box_conv_buffer;
    ComboBox box_maxpart;
    TextEditor txt_rcv_port;
    ToggleButton tgl_rcv_active;
    ToggleButton tgl_save_preset;

    struct SnappingSlider  : public Slider
    {
        double snapValue (double attemptedValue, DragMode dragMode)
        {
            if (attemptedValue > -1.5 && attemptedValue < 1.5)
                return 0.0;
            return attemptedValue;
        }
    };

    SnappingSlider sld_master_gain;
    Label lbl_master_gain;

    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> atc_master_gain;

    int window_width = 450,
        window_height = 330;  // Current window height, to support resizing of debug window
    // Lambdas to simplify measuring from the right and bottom of the plugin window
    std::function<int(int)> fromRight = [this](int c) { jassert(window_width>=0); return window_width-c; };
    std::function<int(int)> fromBottom = [this](int c) { jassert(window_height>=0); return window_height-c; };
    
    std::unique_ptr<AlertWindow> asyncAlertWindow;

    LookAndFeel_V3 MyLookAndFeel;
};


#endif  // PLUGINEDITOR_H_INCLUDED
