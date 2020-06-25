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
    
    class StatusLed : public Component
    {
    public:
        DrawablePath drawable;
        Path shape;
        
        StatusLed();
    private:
        void paint (Graphics& g)
        {
            g.setColour (Colours::white);
            g.drawRect (0, 0, getWidth(), getHeight(), 1);
        }
        void resized()
        {
            float shapeRay;
            Point<float> center (getWidth()/2,getHeight()/2);
            if (getWidth() >= getHeight())
                shapeRay = getHeight()/2;
            else
                shapeRay = getWidth()/2;
            
            shape.clear();
            shape.addStar (center, 5, shapeRay*0.5, shapeRay, -0.2f);
            
            drawable.setPath (shape);
            std::cout << center.toString() << std::endl;
        }
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

    
    Label versionLabel;

    //constructor
    View();

private:
    void paint (Graphics& g);
    void resized();
};


#endif  // PLUGINVIEW_H_INCLUDED
