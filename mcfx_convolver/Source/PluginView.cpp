
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

#include "PluginView.h"

#define VAL(str) #str
#define TOSTRING(str) VAL(str)

//==============================================================================
//=============================== Main View ====================================

View::View()
{
    title.setFont(Font (17.2000f, Font::bold));
    title.setColour(Label::textColourId, Colours::white);
    title.setJustificationType (Justification::centred);
    title.setText("MCFX-CONVOLVER",dontSendNotification);
    addAndMakeVisible(title);

    subtitle.setFont(Font (12.4000f, Font::plain));
    subtitle.setColour(Label::textColourId, Colours::white);
    subtitle.setJustificationType (Justification::centredTop);
    subtitle.setText("multichannel non-equal partioned convolution matrix", dontSendNotification);
    addAndMakeVisible(subtitle);
    
    debugText.setFont(Font(10,1));
    debugText.setMultiLine (true);
    debugText.setReturnKeyStartsNewLine (false);
    debugText.setReadOnly (true);
    debugText.setScrollbarsShown (true);
    debugText.setCaretVisible (false);
    debugText.setPopupMenuEnabled (true);
    addAndMakeVisible(debugText);
    
    debugWinLabel.setFont (Font (10.0000f, Font::plain));
    debugWinLabel.setJustificationType (Justification::left);
    debugWinLabel.setColour (Label::textColourId, Colours::white);
    debugWinLabel.attachToComponent(&debugText, false);
    debugWinLabel.setText ("debug window",dontSendNotification);
    addAndMakeVisible(debugWinLabel);
    
    skippedCyclesLabel.setFont(Font(10.0000f, Font::plain));
    skippedCyclesLabel.setJustificationType(Justification::right);
    skippedCyclesLabel.setColour(Label::textColourId, Colours::yellow);
    skippedCyclesLabel.setText ("skipped cycles: ", dontSendNotification);
    addAndMakeVisible(skippedCyclesLabel);
    
    addAndMakeVisible(presetManagingBox);
    addAndMakeVisible(irMatrixBox);
    addAndMakeVisible(oscManagingBox);
    addAndMakeVisible(ioDetailBox);
    addAndMakeVisible(convManagingBox);
        
    statusLed.setStatus(StatusLed::State::red);
    addAndMakeVisible(statusLed);
    
    statusText.setFont(Font(12,1));
    statusText.setJustification(Justification::left);
    statusText.setReadOnly (true);
    statusText.setCaretVisible (false);
    statusText.setPopupMenuEnabled(false);
    statusText.setColour(TextEditor::backgroundColourId, Colours::lightgrey);
    addAndMakeVisible(statusText);
    
    String version_string;
    version_string << "v" << TOSTRING(VERSION);
    versionLabel.setFont(Font (10.00f, Font::plain));
    versionLabel.setColour(Label::textColourId, Colours::white);
    versionLabel.setJustificationType(Justification::right);
    versionLabel.setText(version_string, dontSendNotification);
    addAndMakeVisible(versionLabel);
    
    addAndMakeVisible(inputChannelDialog);
    inputChannelDialog.setVisible(false);
}

void View::paint(Graphics& g)
{
    //background
    g.fillAll (Colours::white);
    
    //color gradient background
    Colour darkgrey = Colour::fromRGBA(100, 100, 100, 255);//grey
    ColourGradient backgoundGradient = ColourGradient(darkgrey,
                                        proportionOfWidth (0.65f),  proportionOfHeight (0.70f),
                                        Colours::black,
                                        proportionOfWidth (0.11f),  proportionOfHeight (0.08f),
                                        true);
    g.setGradientFill(backgoundGradient);
    g.fillRect (0, 0, getWidth(), getHeight());
    
    //border
    g.setColour (Colours::darkgrey);
    g.drawRect (0, 0, getWidth(), getHeight(), 1);
}

void View::resized()
{
    auto area = getLocalBounds();
    int border = 15;
    int separator = 6;
    
    int titleHeight = 25;
    
    int presetBoxHeight = 84;//24*3 + 6*2
    
    int IRBoxHeight = 36;
    
    int OSCheight = 24;
    
    int IOBoxWidtdh = 190;
    int IOBoxHeight = 77;
    
    int convBoxHeight = 40;
    
    int debugWinHeight = 100;

    int shapesize = 25;
    int smallLabelHeight = 16;
    
    inputChannelDialog.setBounds(getBounds());
    versionLabel.setBounds(area.removeFromBottom(smallLabelHeight));
    area.removeFromLeft(border);
    area.removeFromRight(border);
//    area.reduce(1,1); //white border
    
    title.setBounds(area.removeFromTop(titleHeight)); //title
    subtitle.setBounds(area.removeFromTop(titleHeight)); //subtitle
    
    presetManagingBox.setBounds(area.removeFromTop(presetBoxHeight));
    area.removeFromTop(separator);
    
    irMatrixBox.setBounds(area.removeFromTop(IRBoxHeight));
    area.removeFromTop(separator);
    
    oscManagingBox.setBounds(area.removeFromTop(OSCheight));
    area.removeFromTop(separator);
    
    auto areabis = area.removeFromTop(IOBoxHeight); //temporary, waiting for fill the space on the right
    ioDetailBox.setBounds(areabis.removeFromLeft(IOBoxWidtdh));
    area.removeFromTop(separator);
    
    convManagingBox.setBounds(area.removeFromTop(convBoxHeight));
    area.removeFromTop(separator);
    
    area.removeFromTop(smallLabelHeight);
    debugText.setBounds(area.removeFromTop(debugWinHeight));
    skippedCyclesLabel.setBounds(area.removeFromTop(smallLabelHeight));
    
//    std::cout << "global area: " << area.toString() << std::endl;
    
    auto sectionArea = area.removeFromBottom(20);
    statusLed.setBounds(sectionArea.removeFromLeft(20));
    statusText.setBounds(sectionArea);
}

//==============================================================================
//==============================================================================
//   Nested GUI classes

View::PresetManagingBox::PresetManagingBox()
{
    pathLabel.setFont (Font (15.0000f, Font::plain));
    pathLabel.setColour (Label::textColourId, Colours::white);
    pathLabel.setText("Path:", dontSendNotification);
    addAndMakeVisible (pathLabel);
    
    pathText.setFont (Font (10.0f, Font::plain));
    pathText.setJustification(Justification::left);
    pathText.setReadOnly(true);
    pathText.setPopupMenuEnabled(false);
    addAndMakeVisible (pathText);
    
    textEditor.setReadOnly(true);
    textEditor.setPopupMenuEnabled(false);
    addAndMakeVisible (textEditor);

    textLabel.setFont (Font (15.0000f, Font::plain));
    textLabel.setColour (Label::textColourId, Colours::white);
    textLabel.setText("Preset:",dontSendNotification);
    addAndMakeVisible (textLabel);

    chooseButton.setTooltip ("browse presets or open from file");
    chooseButton.setColour (TextButton::buttonColourId, Colours::white);
    chooseButton.setColour (TextButton::buttonOnColourId, Colours::blue);
    chooseButton.setButtonText ("choose");
    addAndMakeVisible(chooseButton);

    saveToggle.setButtonText(TRANS("Save preset within project"));
    saveToggle.setTooltip(TRANS(  "this will save the preset and data within the project and reload it from there; CAUTION: may slow down saving/opening your project and increases project file size!"));
    saveToggle.setToggleState(true, dontSendNotification);
    saveToggle.setColour(ToggleButton::textColourId, Colours::white);
    addAndMakeVisible(saveToggle);

    selectFolderButton.setTooltip ("choose another preset folder");
    selectFolderButton.setButtonText ("preset folder");
    selectFolderButton.setColour (TextButton::buttonColourId, Colours::white);
    selectFolderButton.setColour (TextButton::buttonOnColourId, Colours::blue);
    addAndMakeVisible(selectFolderButton);
}

void View::PresetManagingBox::paint(Graphics& g)
{}

void View::PresetManagingBox::resized()
{
    auto labelButtonWidth = 46;
    auto rowHeight = 24;
    auto buttonWidth = 94;
    auto separatorHeight = 6;
    
    //width based on font size and specific string
    int pathLabelWidth = pathLabel.getFont().getStringWidth(pathLabel.getText());
    pathLabelWidth = pathLabelWidth + 4*2; //add internal borders
    
    //flex row 0 ------------------------------------------------------
    FlexBox pathBox;
    pathBox.justifyContent = FlexBox::JustifyContent::flexStart;

    FlexItem labelElement   (pathLabelWidth,          rowHeight, pathLabel);
    FlexItem editorElement  (proportionOfWidth(0.80f),  rowHeight, pathText);

    pathBox.items.addArray ( { labelElement, editorElement } );

    //flex row 1 ------------------------------------------------------
    FlexBox SelectionBox;
    SelectionBox.justifyContent = FlexBox::JustifyContent::spaceBetween;

    FlexItem label      (labelButtonWidth,          rowHeight, textLabel);
    FlexItem text       (proportionOfWidth(0.65f),  rowHeight, textEditor);
    FlexItem choiceButton     (labelButtonWidth,    rowHeight, chooseButton);

    SelectionBox.items.addArray ( { label, text, choiceButton } );
    
    //flex row 2 ------------------------------------------------------
    FlexBox ModifierBox;
    ModifierBox.justifyContent= FlexBox::JustifyContent::spaceBetween;

    FlexItem toggle         (proportionOfWidth(0.55f),  rowHeight, saveToggle);
    FlexItem folderButton   (buttonWidth,               rowHeight, selectFolderButton);

    ModifierBox.items.addArray ( { toggle, folderButton } );

    //main flex --------------------------------------------------------
    FlexBox MainBox;
    MainBox.flexDirection = FlexBox::Direction::column;
    MainBox.justifyContent = FlexBox::JustifyContent::spaceBetween;

    FlexItem path       (getWidth(), rowHeight, pathBox);
    FlexItem selection  (getWidth(), rowHeight, SelectionBox);
    FlexItem modifier   (getWidth(),  rowHeight, ModifierBox);

    MainBox.items.addArray( { path, selection, modifier} );
    MainBox.performLayout (getLocalBounds().toFloat());
}

//==============================================================================
View::IRMatrixBox::IRMatrixBox()
{
    boxLabel.setFont (Font (15.0000f, Font::plain));
    boxLabel.setColour (Label::textColourId, Colours::white);
    boxLabel.setJustificationType(Justification::left);
    boxLabel.setText("IR Filter Matrix:", dontSendNotification);
    addAndMakeVisible (boxLabel);

//    loadUnloadButton.setTooltip ("load directly an Impulse Response filter matrix");
//    loadUnloadButton.setColour (TextButton::buttonColourId, Colours::white);
//    loadUnloadButton.setColour (TextButton::buttonOnColourId, Colours::blue);
//    loadUnloadButton.setButtonText ("load");
//    addAndMakeVisible(loadUnloadButton);
    
    confModeButton.setClickingTogglesState (true);
    confModeButton.setRadioGroupId (34567);
    confModeButton.setColour (TextButton::textColourOffId,  Colours::black);
    confModeButton.setColour (TextButton::textColourOnId,   Colours::white);
    confModeButton.setColour (TextButton::buttonColourId,   Colours::white);
    confModeButton.setColour (TextButton::buttonOnColourId, Colours::blueviolet.brighter());
    confModeButton.setConnectedEdges (Button::ConnectedOnRight);
    confModeButton.setToggleState (true, dontSendNotification);
    confModeButton.setButtonText(".conf");
    addAndMakeVisible(confModeButton);
    
    wavModeButton.setClickingTogglesState (true);
    wavModeButton.setRadioGroupId (34567);
    wavModeButton.setColour (TextButton::textColourOffId,  Colours::black);
    wavModeButton.setColour (TextButton::textColourOnId,   Colours::white);
    wavModeButton.setColour (TextButton::buttonColourId,   Colours::white);
    wavModeButton.setColour (TextButton::buttonOnColourId, Colours::blueviolet.brighter());
    wavModeButton.setConnectedEdges (Button::ConnectedOnLeft);
    wavModeButton.setToggleState (false, dontSendNotification);
    wavModeButton.setButtonText(".wav");
    addAndMakeVisible(wavModeButton);
}

void View::IRMatrixBox::paint(Graphics& g)
{
    g.setColour (Colours::darkgrey);
    g.drawRect (0, 0, getWidth(), getHeight(), 1);
}

void View::IRMatrixBox::resized()
{
    int editorWidth = proportionOfWidth(0.5f);
    int buttonWidth = 50;
    int labelWidth = 100;
    
    int height = 24;
    
    FlexBox toggleButtons;
    toggleButtons.justifyContent = FlexBox::JustifyContent::center;
    toggleButtons.alignItems = FlexBox::AlignItems::center;
    
    FlexItem conf (buttonWidth, height, confModeButton);
    conf.alignSelf = FlexItem::AlignSelf::autoAlign;
    
    FlexItem wav (buttonWidth, height, wavModeButton);
    wav.alignSelf = FlexItem::AlignSelf::autoAlign;
    
    toggleButtons.items.addArray({ conf, wav });
    
    //--------------------------------------------------------------------
    FlexBox mainFlex;
    mainFlex.justifyContent = FlexBox::JustifyContent::spaceAround;
    mainFlex.alignItems = FlexBox::AlignItems::center;
    
    FlexItem label  (labelWidth,  height, boxLabel);
    label.alignSelf = FlexItem::AlignSelf::autoAlign;
    
//    FlexItem editor (editorWidth, height, pathText);
    FlexItem editor (editorWidth, height, toggleButtons);
    editor.alignSelf = FlexItem::AlignSelf::autoAlign;
    
    FlexItem button (buttonWidth, height, loadUnloadButton);
    button.alignSelf = FlexItem::AlignSelf::autoAlign;
    
    mainFlex.items.addArray({ label, editor, button });
    mainFlex.performLayout(getLocalBounds().toFloat());
}

//==============================================================================
View::OSCManagingBox::OSCManagingBox()
{
    activeReceiveToggle.setTooltip(TRANS("enable OSC receive, supported messages: /reload, /load <preset.conf> (preset needs to be within the search path)"));
    activeReceiveToggle.setToggleState(true, dontSendNotification);
    activeReceiveToggle.setColour(ToggleButton::textColourId, Colours::white);
    activeReceiveToggle.setButtonText(TRANS("OSC receive port: "));
    addAndMakeVisible(activeReceiveToggle);
    
    receivePortText.setColour(TextEditor::textColourId, Colours::black);
    receivePortText.setJustification(Justification::centredLeft);
    receivePortText.setTooltip(TRANS("OSC receive port"));
    receivePortText.setCaretVisible(false);
    receivePortText.setReadOnly(false);
    receivePortText.setPopupMenuEnabled(true);
    receivePortText.setText(TRANS("7200"));
    addAndMakeVisible(receivePortText);
}

void View::OSCManagingBox::paint(Graphics& g)
{}

void View::OSCManagingBox::resized()
{
    int boxHeight = 24;
    int editorWidth = 40;
    int editorHeight = 20;
    
    auto area = getLocalBounds();
    area.removeFromRight(proportionOfWidth(0.45f));
    auto toggleWidth = (area.getWidth())*0.80;
    
    FlexBox flexBox;
    flexBox.justifyContent = FlexBox::JustifyContent::spaceBetween;
    flexBox.alignItems = FlexBox::AlignItems::center;
//    flexBox.alignContent = FlexBox::AlignContent::center;

    FlexItem toggle (toggleWidth, boxHeight,activeReceiveToggle);
    toggle.alignSelf=FlexItem::AlignSelf::autoAlign;
    FlexItem editor (editorWidth,editorHeight, receivePortText);
    editor.alignSelf=FlexItem::AlignSelf::autoAlign;

    flexBox.items.addArray( { toggle, editor} );
    flexBox.performLayout (area.toFloat());
}

//==============================================================================

View::IODetailBox::IODetailBox()
{
    inputLabel.setFont (Font (15.0000f, Font::plain));
    inputLabel.setJustificationType (Justification::centredRight);
    inputLabel.setColour (Label::textColourId, Colours::white);
    inputLabel.setText("Input channels:", dontSendNotification);
    addAndMakeVisible (inputLabel);
    
    inputNumber.setFont (Font (15.0000f, Font::plain));
    inputNumber.setJustificationType (Justification::centred);
    inputNumber.setColour (Label::textColourId, Colours::white);
    inputNumber.setText("0", dontSendNotification);
    addAndMakeVisible (inputNumber);
    
    outputLabel.setFont (Font (15.0000f, Font::plain));
    outputLabel.setJustificationType (Justification::centredRight);
    outputLabel.setColour (Label::textColourId, Colours::white);
    outputLabel.setText("Output channels:", dontSendNotification);
    addAndMakeVisible (outputLabel);
    
    outputNumber.setFont (Font (15.0000f, Font::plain));
    outputNumber.setJustificationType (Justification::centred);
    outputNumber.setColour (Label::textColourId, Colours::white);
    outputNumber.setText("0", dontSendNotification);
    addAndMakeVisible (outputNumber);
    
    IRLabel.setFont (Font (15.0000f, Font::plain));
    IRLabel.setJustificationType (Justification::centredRight);
    IRLabel.setColour (Label::textColourId, Colours::white);
    IRLabel.setText("Impulse responses:", dontSendNotification);
    addAndMakeVisible (IRLabel);
    
    IRNumber.setFont (Font (15.0000f, Font::plain));
    IRNumber.setJustificationType (Justification::centred);
    IRNumber.setColour (Label::textColourId, Colours::white);
    IRNumber.setText("0", dontSendNotification);
    addAndMakeVisible (IRNumber);
}

void View::IODetailBox::paint(Graphics& g)
{
//    g.setColour (Colour (0x410000ff));
    
    auto boxColor = Colours::blue;
    g.setColour(boxColor.withAlpha(0.26f)); //transparency 26%
    
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 10.0f);
}

void View::IODetailBox::resized()
{
   Grid grid;

   using Track = Grid::TrackInfo;

   grid.templateRows    = { Track (1_fr), Track (1_fr), Track (1_fr) };
   grid.templateColumns = { Track (3_fr), Track (1_fr)};

   grid.items = {  GridItem (inputLabel),   GridItem (inputNumber),
                   GridItem (outputLabel),  GridItem (outputNumber),
                   GridItem (IRLabel),      GridItem (IRNumber)
               };

   grid.performLayout (getLocalBounds());
}

//==============================================================================
View::ConvManagingBox::ConvManagingBox()
{
    bufferCombobox.setTooltip("set higher buffer size to optimize CPU performance but increased latency");
    bufferCombobox.setEditableText (false);
    bufferCombobox.setJustificationType (Justification::centredLeft);
    addAndMakeVisible(bufferCombobox);
    
    maxPartCombobox.setTooltip("set maximum partition size for CPU load optimizations, leave it at 8192 if you don't know what you are doing!");
    maxPartCombobox.setEditableText (false);
    maxPartCombobox.setJustificationType (Justification::centredLeft);
    maxPartCombobox.setColour(ComboBox::backgroundColourId, Colour (Colours::grey));
    addAndMakeVisible(maxPartCombobox);
    
    maxPartLabel.setFont (Font (12.4000f, Font::plain));
    maxPartLabel.setColour (Label::textColourId, Colours::white);
    maxPartLabel.setJustificationType(Justification::left);
    maxPartLabel.setText("Maximum Partition Size", dontSendNotification);
    maxPartLabel.attachToComponent(&maxPartCombobox, false);
    addAndMakeVisible (maxPartLabel);
    
    bufferLabel.setFont (Font (12.4000f, Font::plain));
    bufferLabel.setColour (Label::textColourId, Colours::white);
    bufferLabel.setJustificationType(Justification::left);
    bufferLabel.setText("First Partition size", dontSendNotification);
    bufferLabel.attachToComponent(&bufferCombobox, false);
    addAndMakeVisible (bufferLabel);
}

void View::ConvManagingBox::paint(Graphics& g)
{}

void View::ConvManagingBox::resized()
{
    int height = 20;
    int width = proportionOfWidth(0.40f);
    
    FlexBox flexBox;
    flexBox.justifyContent = FlexBox::JustifyContent::spaceAround;
    flexBox.alignItems = FlexBox::AlignItems::flexEnd;
    
    FlexItem bufferCombo (width, height, bufferCombobox);
    bufferCombo.alignSelf = FlexItem::AlignSelf::autoAlign;
    FlexItem maxPartCombo (width, height, maxPartCombobox);
    maxPartCombo.alignSelf = FlexItem::AlignSelf::autoAlign;
    
    flexBox.items.addArray({ bufferCombo, maxPartCombo });
    flexBox.performLayout(getLocalBounds().toFloat());
}

//==============================================================================
View::StatusLed::StatusLed() :
state(StatusLed::State::red)
{
    drawable.setStrokeFill (Colours::black);
    drawable.setStrokeThickness (1.0f);
    addAndMakeVisible(drawable);
}

void View::StatusLed::paint (Graphics& g)
{
//    g.setColour (Colours::white);
//    g.drawRect (0, 0, getWidth(), getHeight(), 1);
    ColourGradient ledColor;
    
    switch (state) {
        case State::green :
            ledColor = ColourGradient ( Colours::green,
                                        proportionOfWidth (0.70f),  proportionOfHeight (0.70f),
                                        Colours::lawngreen,
                                        proportionOfWidth (0.15f),  proportionOfHeight (0.15f),
                                        false);
            break;
            
        case State::yellow :
            ledColor = ColourGradient ( Colours::darkgoldenrod,
                                        proportionOfWidth (0.70f),  proportionOfHeight (0.70f),
                                        Colours::yellow,
                                        proportionOfWidth (0.10f),  proportionOfHeight (0.10f),
                                        false);
            break;
            
        case State::red :
            ledColor = ColourGradient ( Colours::red,
                                        proportionOfWidth (0.70f),  proportionOfHeight (0.70f),
                                        Colours::lightsalmon,
                                        proportionOfWidth (0.15f),  proportionOfHeight (0.15f),
                                        false);
            break;
        default:
            break;
    };
    
    drawable.setFill(ledColor);
}

void View::StatusLed::resized()
{
    shape.clear();
    shape.addEllipse(getLocalBounds().toFloat());
    
    drawable.setPath (shape);
}

void View::StatusLed::setStatus(State newState)
{
    state = newState;
    repaint();
}

//==============================================================================
View::InputChannelDialog::InputChannelDialog() :
rectSize (260,175)
{
    title.setFont(Font (17.2000f, Font::bold));
    title.setColour(Label::textColourId, Colours::white);
    title.setJustificationType (Justification::centred);
    title.setText("A value is missing...",dontSendNotification);
    addAndMakeVisible(title);
    
    message.setFont(Font (12.4000f, Font::plain));
    message.setColour(Label::textColourId, Colours::white);
    message.setJustificationType (Justification::bottomLeft);
    message.setText("Number of input channels is missing, \n"
                    "please enter it manually:", dontSendNotification);
    addAndMakeVisible(message);
    
    invalidMessage.setFont(Font (12.4000f, Font::plain));
    invalidMessage.setColour(Label::textColourId, Colours::lightsalmon);
    invalidMessage.setJustificationType (Justification::bottomLeft);
    addAndMakeVisible(invalidMessage);
    invalidMessage.setVisible(false);
    
    textEditor.setColour(TextEditor::textColourId, Colours::black);
    textEditor.setJustification(Justification::left);
    textEditor.setCaretVisible(true);
    textEditor.setTextToShowWhenEmpty("value", Colours::darkgrey);
    textEditor.setInputRestrictions(3,"1234567890");
    textEditor.onReturnKey = [&] {OKButton.triggerClick();};
    textEditor.setSelectAllWhenFocused(true);
    addAndMakeVisible(textEditor);
    
    diagonalToggle.setButtonText(TRANS("Diagonal filter matrix"));
    diagonalToggle.setTooltip(TRANS("Check if the multipack filter matrix is diagonal"));
    diagonalToggle.setToggleState(false, dontSendNotification);
    diagonalToggle.setColour(ToggleButton::textColourId, Colours::white);
    diagonalToggle.onStateChange = [&]{
        if ( diagonalToggle.getToggleState() )
        {
            textEditor.setEnabled(false);
            textEditor.setText("");
        }
        else
            textEditor.setEnabled(true);
            };
    addAndMakeVisible(diagonalToggle);
    
    OKButton.setColour (TextButton::buttonColourId, Colours::white);
    OKButton.setColour (TextButton::buttonOnColourId, Colours::blue);
    OKButton.addShortcut(KeyPress(KeyPress::returnKey));
    
    OKButton.setButtonText ("OK");
    addAndMakeVisible(OKButton);
}

void View::InputChannelDialog::paint(Graphics& g)
{
    //color gradient
    Colour darkgrey = Colour::fromRGBA(100, 100, 100, 255);//grey

    //background
    Colour opac_grey = darkgrey.withAlpha(0.5f);
    g.fillAll (opac_grey);
    
    //-------------------------------------------------------------------------------
    
   ColourGradient boxGradient = ColourGradient(darkgrey,
                                       rectSize.proportionOfWidth (0.65f)+rectSize.getX(),  rectSize.proportionOfHeight (0.70f)+rectSize.getY(),
                                       Colours::black,
                                       rectSize.proportionOfWidth (0.11f)+rectSize.getX(),  rectSize.proportionOfHeight (0.08f)+rectSize.getY(),
                                       true);
   g.setGradientFill(boxGradient);
   g.fillRect (rectSize);
   
   //border
   g.setColour (Colours::dimgrey);
   g.drawRect (rectSize, 1);
}

void View::InputChannelDialog::resized()
{
    int height = 24;
    int editorHeight = 18;
    int separator = 6;
    
    rectSize.setX((getWidth()/2)-rectSize.getWidth()/2);
    rectSize.setY((getHeight()/2)-rectSize.getHeight()/2);
    
    Rectangle<int> areaToDraw = rectSize;
    areaToDraw.reduce(12, 4); // border
    
    title.setBounds(areaToDraw.removeFromTop(height));
        
    Rectangle<int> messageArea = areaToDraw.removeFromTop(height*2);
    messageArea.reduce(messageArea.proportionOfWidth(0.07f), 0);
    message.setBounds(messageArea);
    invalidMessage.setBounds(messageArea);
    areaToDraw.removeFromTop(separator);
    
    int shrinkValue = areaToDraw.proportionOfWidth(0.20f);
    areaToDraw.reduce(shrinkValue, 0);
//    areaToDraw.removeFromLeft(areaToDraw.proportionOfWidth(0.20f));
//    areaToDraw.removeFromRight(areaToDraw.proportionOfWidth(0.20f));
    
    textEditor.setBounds(areaToDraw.removeFromTop(editorHeight));
    areaToDraw.removeFromTop(separator);
    
    diagonalToggle.setBounds(areaToDraw.removeFromTop(editorHeight));
    
    areaToDraw.reduce(16, 0);
    OKButton.setSize(areaToDraw.getWidth(), height);
    OKButton.setTopLeftPosition(areaToDraw.getX(), areaToDraw.getY()+areaToDraw.getHeight()/2-height/2);
}

void View::InputChannelDialog::invalidState(InvalidType type, int maxInput)
{
    String  text;
    switch (type) {
        case InvalidType::notFeasible:
            text << "Value must be between 1 and " << maxInput << "\n(based on current max plugin inputs)\n";
            text << "please enter it again:";
            break;
        
        case InvalidType::notMultiple:
            text << "Value must be a valid divider for the given wavefile length \n";
            text << "please enter it again:";
            break;
            
        default:
            break;
    }
    invalidMessage.setText(text, dontSendNotification);
    message.setVisible(false);
    invalidMessage.setVisible(true);
}

void View::InputChannelDialog::resetState(bool toggleChecked)
{
    message.setVisible(true);
    invalidMessage.setVisible(false);
    if(toggleChecked)
        diagonalToggle.triggerClick();
    
    setVisible(false);
}
