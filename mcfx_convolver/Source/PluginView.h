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

#ifndef PLUGINVIEW_H_INCLUDED
#define PLUGINVIEW_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
//#include "PluginEditor.h"

//=========================== VIEW COMPONENTS ==================================


/*
class SceneComponent    : public Component
{
public:
    SceneComponent(){
        setPaintingIsUnclipped(true);

        inputChannelLabel.setText("input channels: ", dontSendNotification);
        inputChannelLabel.setFont (Font (15.0000f, Font::plain));
        inputChannelLabel.setJustificationType (Justification::right);
        inputChannelLabel.setEditable (false, false, false);
        inputChannelLabel.setColour (Label::textColourId, Colours::white);
        inputChannelLabel.setColour (TextEditor::textColourId, Colours::black);
        inputChannelLabel.setColour (TextEditor::backgroundColourId, Colour (0x0));
        addAndMakeVisible(inputChannelLabel);

        inputChannelNumber.setText("0", dontSendNotification);
        inputChannelNumber.setFont (Font (15.0000f, Font::plain));
        inputChannelNumber.setJustificationType (Justification::centred);
        inputChannelNumber.setEditable (false, false, false);
        inputChannelNumber.setColour (Label::textColourId, Colours::white);
        inputChannelNumber.setColour (TextEditor::textColourId, Colours::black);
        inputChannelNumber.setColour (TextEditor::backgroundColourId, Colour (0x0));
        addAndMakeVisible(inputChannelNumber);

        addAndMakeVisible(outputChannelLabel);
        addAndMakeVisible(outputChannelNumber);

        addAndMakeVisible(impulseResposeLabel);
        addAndMakeVisible(impulseResposneNumber);
    }

    void resized() override
    {
        // auto area = getLocalBounds();
        // area.reduce(getHeight()*0.05,getWidth()*0.05);

        // auto LabelHeight = 24;
        // inputChannelLabel.setBounds(area.removeFromTop(LabelHeight));

        Grid grid;
 
        using Track = Grid::TrackInfo;
    
        grid.templateRows    = { Track (1_fr), Track (1_fr), Track (1_fr) };
        grid.templateColumns = { Track (3_fr), Track (1_fr)};
    
        grid.items = {  GridItem (inputChannelLabel),   GridItem (inputChannelNumber), 
                        GridItem (outputChannelLabel),   GridItem (outputChannelNumber),
                        GridItem (impulseResposeLabel),   GridItem (impulseResposneNumber) 
                    };
    
        grid.performLayout (getLocalBounds());
    }

    void inputNumberValue(int newValue)
    {
        inputChannelNumber.setText(String(newValue), dontSendNotification);
        //resized();
    }

    void paint (Graphics& g) override
    {
        g.setColour (Colour (0x410000ff));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 15.0000f); 
    }
private:
    Label inputChannelLabel;
    Label outputChannelLabel;
    Label impulseResposeLabel;

    Label inputChannelNumber;
    Label outputChannelNumber;
    Label impulseResposneNumber;

    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SceneComponent)
    
};
*/

//==============================================================================
//========================== MAIN VIEW =========================================



class View  : public Component
{
public:
    // GUI nested classes
    //preset managing box
    class PresetManagingBox : public Component
    {
    public:
        Label pathLabel;
        TextEditor pathText;
        
        Label textLabel;
        TextEditor textEditor;
        TextButton chooseButton;
        TextButton selectFolderButton;
        ToggleButton saveToggle;

        PresetManagingBox();
    private:
        void paint (Graphics& g);
        void resized();
    };
    //IR Matrix Box
    class IRMatrixBox : public Component
    {
    public:
        Label boxLabel;
        TextButton loadUnloadButton;
        
        TextButton confModeButton;
        TextButton wavModeButton;
        
        IRMatrixBox();
    private:
        void paint(Graphics& g);
        void resized();
    };
    //OSC managing box
    class OSCManagingBox : public Component
    {
    public:
        ToggleButton activeReceiveToggle;
        TextEditor receivePortText;

        OSCManagingBox();
    private:
        void paint (Graphics& g);
        void resized();
    };
    //input-output information box
    class IODetailBox : public Component
    {
    public:
        Label inputLabel;
        Label inputNumber;
        Label outputLabel;
        Label outputNumber;
        Label IRLabel;
        Label IRNumber;

        IODetailBox();
    private:
        void paint (Graphics& g);
        void resized();
    };
    
    class ConvManagingBox : public Component
    {
    public:
        Label bufferLabel;
        Label maxPartLabel;
        ComboBox bufferCombobox;
        ComboBox maxPartCombobox;

        ConvManagingBox();
    private:
        void paint (Graphics& g);
        void resized();
    };
    
    class InputChannelDialog : public Component
    {
    public:
        Rectangle<int> rectSize;
        
        Label title;
        Label message;
        Label invalidMessage;
        TextEditor textEditor;
        TextButton OKButton;
        
        InputChannelDialog();
        
        void invalidState();
        void resetState();
    private:
        void paint (Graphics& g);
        void resized();
    };

    //Instanciation
    Label title;
    Label subtitle;
    PresetManagingBox presetManagingBox;
    IRMatrixBox irMatrixBox;
    OSCManagingBox oscManagingBox;
    IODetailBox ioDetailBox;
    ConvManagingBox convManagingBox;
    
    InputChannelDialog inputChannelDialog;
    
    Label debugWinLabel;
    TextEditor debugText;
    Label skippedCyclesLabel;
    
    Label versionLabel;

    //constructor
    View();

private:
    void paint (Graphics& g);
    void resized();
};


#endif  // PLUGINVIEW_H_INCLUDED
