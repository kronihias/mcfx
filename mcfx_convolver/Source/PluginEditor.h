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

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginProcessor.h"


//==============================================================================
/**
*/
class Mcfx_convolverAudioProcessorEditor  : public AudioProcessorEditor,
                                            public Button::Listener,
                                            public ChangeListener,
                                            public ComboBox::Listener,
                                            public Timer,
                                            public TextEditor::Listener
{
public:
    Mcfx_convolverAudioProcessorEditor (Mcfx_convolverAudioProcessor* ownerFilter);
    ~Mcfx_convolverAudioProcessorEditor();

    //==============================================================================
    // This is just a standard Juce paint method...
    void paint (Graphics& g);

    void resized();
    void buttonClicked (Button* buttonThatWasClicked);

    void changeListenerCallback (ChangeBroadcaster *source);

    void comboBoxChanged (ComboBox* comboBoxThatHasChanged);

    void textEditorFocusLost(TextEditor& ed);

    void textEditorReturnKeyPressed(TextEditor& ed);

    static void menuItemChosenCallback (int result, Mcfx_convolverAudioProcessorEditor* demoComponent);

    void UpdatePresets();

    void UpdateText();

    void timerCallback();

private:
    Mcfx_convolverAudioProcessor* getProcessor() const
    {
        return static_cast <Mcfx_convolverAudioProcessor*> (getAudioProcessor());
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
    Label label4;
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

    LookAndFeel_V3 MyLookAndFeel;
};


#endif  // PLUGINEDITOR_H_INCLUDED
