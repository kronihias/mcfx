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
        TextButton newInChannelsButton;
        
        TextButton confModeButton;
        TextButton wavModeButton;
        
        enum  modeState {conf, wav};
        modeState lastState = conf;
        
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
        ToggleButton diagonalToggle;
        TextButton OKButton;
        
        InputChannelDialog();
        
        enum InvalidType {notFeasible, notMultiple};
        void invalidState(InvalidType type, int maxInput=NUM_CHANNELS);
        void resetState(bool toggleChecked=false);
    private:
        void paint (Graphics& g);
        void resized();
    };
    
    class StatusLed : public Component
    {
    public:
        enum State {green, yellow, red};
        
        void setStatus(State newState);
        
        StatusLed();
        
    private:
        DrawablePath drawable;
        Path shape;
        
        State state;
        
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
    
    StatusLed statusLed;
    TextEditor statusText;
    
    Label versionLabel;

    //constructor
    View();

private:
    void paint (Graphics& g);
    void resized();
};


#endif  // PLUGINVIEW_H_INCLUDED
