
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
    title.setText("X-MCFX-CONVOLVER",dontSendNotification);
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
    
    addAndMakeVisible(FilterManagingBox);
//    addAndMakeVisible(irMatrixBox);
    addAndMakeVisible(oscManagingBox);
    addAndMakeVisible(ioDetailBox);
    addAndMakeVisible(convManagingBox);
    addAndMakeVisible(changePathBox);
        
    statusLed.setStatus(StatusLed::State::red);
    addAndMakeVisible(statusLed);
    
    statusText.setFont(Font(12, 1));
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
    int separator = 12;
    
    int titleHeight = 25;
    
    int presetBoxHeight = 48;//24 + 6*2
    
    int IRBoxHeight = 36;
    
    int OSCheight = 24;
    
    int IOBoxWidtdh = 190;
    int IOBoxHeight = 120;
    
    int convBoxHeight = 60;
    
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
    
    FilterManagingBox.setBounds(area.removeFromTop(presetBoxHeight));
    area.removeFromTop(separator);
    
//    irMatrixBox.setBounds(area.removeFromTop(IRBoxHeight));
//    area.removeFromTop(separator);
    
    ioDetailBox.setBounds(area.removeFromTop(IOBoxHeight));
    area.removeFromTop(separator);
    
    convManagingBox.setBounds(area.removeFromTop(convBoxHeight));
    area.removeFromTop(separator);
    
    changePathBox.setBounds(area.removeFromTop(presetBoxHeight));
    area.removeFromTop(separator);
    
//    oscManagingBox.setBounds(area.removeFromTop(OSCheight));
//    area.removeFromTop(separator);
//
//    area.removeFromTop(smallLabelHeight);
//    debugText.setBounds(area.removeFromTop(debugWinHeight));
//    skippedCyclesLabel.setBounds(area.removeFromTop(smallLabelHeight));
    
    auto sectionArea = area.removeFromBottom(20);
    statusLed.setBounds(sectionArea.removeFromLeft(20));
    sectionArea.removeFromLeft(5);
    statusText.setBounds(sectionArea);
}

//==============================================================================
//==============================================================================
//   Nested GUI classes

View::FilterManagingBox::FilterManagingBox()
{
    filterLabel.setFont (Font (15.0f, Font::plain));
    filterLabel.setColour (Label::textColourId, Colours::white);
    filterLabel.setText("Filter", dontSendNotification);
    addAndMakeVisible (filterLabel);
    
    addAndMakeVisible (filterSelector);

//    chooseButton.setTooltip ("browse filters or open from file");
//    chooseButton.setColour (TextButton::buttonColourId, Colours::white);
//    chooseButton.setColour (TextButton::buttonOnColourId, Colours::blue);
//    chooseButton.setButtonText ("choose");
//    addAndMakeVisible(chooseButton);
    
    reloadButton.setTooltip ("reload the current matrix specifying a new input channels number");
    reloadButton.setColour (TextButton::buttonColourId, Colours::white);
    reloadButton.setColour (TextButton::buttonOnColourId, Colours::blue);
    reloadButton.setEnabled(false);
    reloadButton.setButtonText ("reload");
    addAndMakeVisible(reloadButton);
    
    infoLabel.setFont (Font (11.0f, Font::plain));
    infoLabel.setJustificationType(Justification::left);
    infoLabel.setColour (Label::textColourId, Colours::white);
    infoLabel.setText("(saved within the project)", dontSendNotification);
    addAndMakeVisible (infoLabel);
    infoLabel.setVisible(false);
}

void View::FilterManagingBox::paint(Graphics& g)
{}

void View::FilterManagingBox::resized()
{
    int rowHeight = 24;
    
    int labelWidth = 40;
    int buttonWitdh = 50;
    
    Grid grid;
    using Track = Grid::TrackInfo;
//    grid.justifyItems = Grid::JustifyItems::center;
    grid.alignContent = Grid::AlignContent::spaceBetween;
    grid.justifyContent = Grid::JustifyContent::spaceBetween;
    grid.alignItems = Grid::AlignItems::center;
    grid.justifyItems = Grid::JustifyItems::center;
    

    grid.templateRows    = { //Track (3_fr),
                                Track (3_fr), Track (1_fr) };
    grid.templateColumns = {Track (45_px), Track (8_fr), Track (2_fr)};
    
//    GridItem TextPath (pathText);
//    TextPath = TextPath.withHeight(rowHeight);
//
//    GridItem pathSelection (pathButton);
//    pathSelection = pathSelection.withSize(labelWidth, rowHeight);
//    pathSelection = pathSelection.withJustifySelf(GridItem::JustifySelf::start);
//    pathSelection = pathSelection.withMargin(GridItem::Margin(0,0,0,10));
    
    GridItem TextEditor (filterSelector);
    TextEditor = TextEditor.withHeight(rowHeight);
    
    GridItem ReloadButton (reloadButton);
    ReloadButton = ReloadButton.withSize(buttonWitdh, rowHeight);
    ReloadButton = ReloadButton.withJustifySelf(GridItem::JustifySelf::start);
    ReloadButton = ReloadButton.withMargin(GridItem::Margin(0,0,0,10));
    
    GridItem infoText (infoLabel);
    infoText = infoText.withAlignSelf(GridItem::AlignSelf::start);
    
    grid.items = {  //GridItem (pathLabel) ,  TextPath,    pathSelection,
                    GridItem (filterLabel), TextEditor,  ReloadButton,
                    GridItem(),             infoText,    GridItem()
                };

    grid.performLayout (getLocalBounds());
}

//==============================================================================

View::ChangePathBox::ChangePathBox()
{
    pathLabel.setFont (Font (15.0f, Font::plain));
    pathLabel.setColour (Label::textColourId, Colours::white);
    pathLabel.setJustificationType(Justification::right);
    pathLabel.setText("Library path", dontSendNotification);
    addAndMakeVisible (pathLabel);
    
    pathText.setFont (Font (11.0f, Font::plain));
    pathText.setColour(TextEditor::backgroundColourId, Colour (Colours::lightgrey));
    pathText.setReadOnly(true);
    pathText.setPopupMenuEnabled(false);
    addAndMakeVisible (pathText);
    
    pathButton.setTooltip ("choose another filter libray folder");
    pathButton.setButtonText ("...");
    pathButton.setColour (TextButton::buttonColourId, Colours::white);
    pathButton.setColour (TextButton::buttonOnColourId, Colours::blue);
    addAndMakeVisible(pathButton);
}

void View::ChangePathBox::paint(Graphics& g)
{
//    g.setColour (Colours::darkgrey);
//    g.drawRect (0, 0, getWidth(), getHeight(), 1);
}

void View::ChangePathBox::resized()
{
    int editorWidth = proportionOfWidth(0.75f);
    int buttonWidth = 30;
   
    int labelWidth = 50;
    
    int height = 24;
    int labelHeight = 54;
    
    FlexBox mainFlex;
    mainFlex.justifyContent = FlexBox::JustifyContent::spaceAround;
    mainFlex.alignItems = FlexBox::AlignItems::center;
    
    FlexItem label  (labelWidth,  labelHeight, pathLabel);
    label.alignSelf = FlexItem::AlignSelf::autoAlign;
    
//    FlexItem editor (editorWidth, height, pathText);
    FlexItem editor (editorWidth, height, pathText);
    editor.alignSelf = FlexItem::AlignSelf::autoAlign;
    
    FlexItem button (buttonWidth, height, pathButton);
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
    inputLabel.setJustificationType (Justification::right);
    inputLabel.setColour (Label::textColourId, Colours::white);
    inputLabel.setText("Input channels:", dontSendNotification);
    addAndMakeVisible (inputLabel);
    
    inputNumber.setFont (Font (15.0000f, Font::plain));
    inputNumber.setJustificationType (Justification::right);
    inputNumber.setColour (Label::textColourId, Colours::white);
    inputNumber.setText("0", dontSendNotification);
    addAndMakeVisible (inputNumber);
    
    outputLabel.setFont (Font (15.0000f, Font::plain));
    outputLabel.setJustificationType (Justification::right);
    outputLabel.setColour (Label::textColourId, Colours::white);
    outputLabel.setText("Output channels:", dontSendNotification);
    addAndMakeVisible (outputLabel);
    
    outputNumber.setFont (Font (15.0000f, Font::plain));
    outputNumber.setJustificationType (Justification::right);
    outputNumber.setColour (Label::textColourId, Colours::white);
    outputNumber.setText("0", dontSendNotification);
    addAndMakeVisible (outputNumber);
    
    IRLabel.setFont (Font (15.0000f, Font::plain));
    IRLabel.setJustificationType (Justification::centredRight);
    IRLabel.setColour (Label::textColourId, Colours::white);
    IRLabel.setText("Impulse responses:", dontSendNotification);
    addAndMakeVisible (IRLabel);
    
    IRNumber.setFont (Font (15.0000f, Font::plain));
    IRNumber.setJustificationType (Justification::right);
    IRNumber.setColour (Label::textColourId, Colours::white);
    IRNumber.setText("0", dontSendNotification);
    addAndMakeVisible (IRNumber);
    
    sampleRateLabel.setFont (Font (15.0000f, Font::plain));
    sampleRateLabel.setJustificationType (Justification::right);
    sampleRateLabel.setColour (Label::textColourId, Colours::white);
    sampleRateLabel.setText("Samplerate:", dontSendNotification);
    addAndMakeVisible (sampleRateLabel);
    
    sampleRateNumber.setFont (Font (15.0000f, Font::plain));
    sampleRateNumber.setJustificationType (Justification::right);
    sampleRateNumber.setColour (Label::textColourId, Colours::white);
    sampleRateNumber.setText("0", dontSendNotification);
    addAndMakeVisible (sampleRateNumber);
    
    hostBufferLabel.setFont (Font (15.0000f, Font::plain));
    hostBufferLabel.setJustificationType (Justification::right);
    hostBufferLabel.setColour (Label::textColourId, Colours::white);
    hostBufferLabel.setText("Host Buffer Size:", dontSendNotification);
    addAndMakeVisible (hostBufferLabel);
    
    hostBufferNumber.setFont (Font (15.0000f, Font::plain));
    hostBufferNumber.setJustificationType (Justification::right);
    hostBufferNumber.setColour (Label::textColourId, Colours::white);
    hostBufferNumber.setText("0", dontSendNotification);
    addAndMakeVisible (hostBufferNumber);
    
    filterLengthLabel.setFont (Font (15.0000f, Font::plain));
    filterLengthLabel.setJustificationType (Justification::right);
    filterLengthLabel.setColour (Label::textColourId, Colours::white);
    filterLengthLabel.setText("Filter Length:", dontSendNotification);
    addAndMakeVisible (filterLengthLabel);
    
    filterLengthInSeconds.setFont (Font (15.0000f, Font::plain));
    filterLengthInSeconds.setJustificationType (Justification::right);
    filterLengthInSeconds.setColour (Label::textColourId, Colours::white);
    filterLengthInSeconds.setText("0", dontSendNotification);
    addAndMakeVisible (filterLengthInSeconds);
    
    filterLengthInSamples.setFont (Font (15.0000f, Font::plain));
    filterLengthInSamples.setJustificationType (Justification::topRight);
    filterLengthInSamples.setColour (Label::textColourId, Colours::white);
    filterLengthInSamples.setText("0", dontSendNotification);
    addAndMakeVisible (filterLengthInSamples);
    
    secondsLabel.setFont (Font (15.0000f, Font::plain));
    secondsLabel.setJustificationType (Justification::left);
    secondsLabel.setColour (Label::textColourId, Colours::white);
    secondsLabel.setText("s", dontSendNotification);
    addAndMakeVisible (secondsLabel);
    
    samplesLabel.setFont (Font (15.0000f, Font::plain));
    samplesLabel.setJustificationType (Justification::topLeft);
    samplesLabel.setColour (Label::textColourId, Colours::white);
    samplesLabel.setText("smpls", dontSendNotification);
    addAndMakeVisible (samplesLabel);
    
    resampledLabel.setFont (Font (12.0000f, Font::plain));
    resampledLabel.setJustificationType (Justification::bottomRight);
    resampledLabel.setColour (Label::textColourId, Colours::yellow);
    resampledLabel.setText("[resampled wavefile]", dontSendNotification);
    resampledLabel.setTooltip(TRANS("wavefile resampled to match host samplerate"));
    addAndMakeVisible (resampledLabel);
    resampledLabel.setVisible(false);
}

void View::IODetailBox::paint(Graphics& g)
{
    float chafmer = 10.0f;
    auto boxColor = Colours::blue;
    g.setColour(boxColor.withAlpha(0.26f)); //transparency 26%

    
    g.fillRoundedRectangle(getLocalBounds().toFloat(), chafmer);
    
    g.setColour (Colours::white);
    g.drawLine(proportionOfWidth(0.45f), chafmer/2, proportionOfWidth(0.45f), getHeight()-chafmer/2, 0.75f);
}

void View::IODetailBox::resized()
{
    auto localArea = getLocalBounds();
    localArea.reduce(0, 8.0f);
//    localArea.removeFromTop(16);
    
    Grid gridLeft;
    using TrackL = Grid::TrackInfo;

    gridLeft.templateRows    = { TrackL (1_fr), TrackL (1_fr),  TrackL (1_fr), TrackL (1_fr) };
    gridLeft.templateColumns = { TrackL (1_fr), TrackL (50_px)};

    GridItem inCh (inputNumber);
    inCh = inCh.withMargin(GridItem::Margin(0,10,0,0));
    
    GridItem outCh (outputNumber);
    outCh = outCh.withMargin(GridItem::Margin(0,10,0,0));
    
    GridItem irNum (IRNumber);
    irNum = irNum.withMargin(GridItem::Margin(0,10,0,0));
    
    gridLeft.items = {  GridItem(inputLabel),   inCh,
                        GridItem(outputLabel),  outCh,
                        GridItem(IRLabel),      irNum,
                        GridItem(resampledLabel),             GridItem()
                    };

    gridLeft.performLayout (localArea.removeFromLeft(localArea.proportionOfWidth(0.45f)));
    
    //--------------------------------------------------------------------------
    
    Grid gridRight;
    using TrackR = Grid::TrackInfo;

    gridRight.templateRows    = { TrackR (1_fr), TrackR (1_fr), TrackR (1_fr), TrackR (1_fr) };
    gridRight.templateColumns = { TrackR (1_fr), TrackR (52_px), TrackR (42_px)};
    
    GridItem smprt (sampleRateNumber);
//    smprt = smprt.withMargin(GridItem::Margin(0,8,0,0));
    
    GridItem buff (hostBufferNumber);
//    buff = buff.withMargin(GridItem::Margin(0,8,0,0));
    
    GridItem firlenSecs (filterLengthInSeconds);
//    firlenSecs = firlenSecs.withMargin(GridItem::Margin(0,8,0,0));
    
    GridItem firlenSmpls (filterLengthInSamples);
//    firlenSmpls = firlenSmpls.withMargin(GridItem::Margin(0,8,0,0));

    gridRight.items = { GridItem (sampleRateLabel),     smprt,      GridItem(),
                        GridItem (hostBufferLabel),     buff,       GridItem(),
                        GridItem (filterLengthLabel),   firlenSecs, GridItem(secondsLabel),
                        GridItem(),       firlenSmpls,GridItem(samplesLabel)
               };

    gridRight.performLayout (localArea);
}

//==============================================================================
View::ConvManagingBox::ConvManagingBox()
{
    maxPartLabel.setFont (Font (12.4000f, Font::plain));
    maxPartLabel.setColour (Label::textColourId, Colours::white);
    maxPartLabel.setJustificationType(Justification::left);
    maxPartLabel.setText("Maximum Partition Size", dontSendNotification);
    maxPartLabel.attachToComponent(&maxPartCombobox, true);
    addAndMakeVisible (maxPartLabel);
    
    bufferLabel.setFont (Font (12.4000f, Font::plain));
    bufferLabel.setColour (Label::textColourId, Colours::white);
    bufferLabel.setJustificationType(Justification::left);
    bufferLabel.setText("First Partition size", dontSendNotification);
    bufferLabel.attachToComponent(&bufferCombobox, true);
    addAndMakeVisible (bufferLabel);
    
    bufferCombobox.setTooltip("set higher buffer size to optimize CPU performance but increased latency");
    bufferCombobox.setEditableText (false);
    bufferCombobox.setJustificationType (Justification::centredLeft);
    addAndMakeVisible(bufferCombobox);
    
    maxPartCombobox.setTooltip("set maximum partition size for CPU load optimizations");
    maxPartCombobox.setEditableText (false);
    maxPartCombobox.setJustificationType (Justification::centredLeft);
    addAndMakeVisible(maxPartCombobox);
    
    latencyLabel.setFont(Font(12.4000f, Font::plain));
    latencyLabel.setJustificationType(Justification::right);
    latencyLabel.setColour(Label::textColourId, Colours::white);
    latencyLabel.setText ("Plugin latency: ", dontSendNotification);
    addAndMakeVisible(latencyLabel);
    
    latencyValue.setFont(Font(12.4000f, Font::plain));
    latencyValue.setJustificationType(Justification::right);
    latencyValue.setColour(Label::textColourId, Colours::white);
    latencyValue.onTextChange = [&] {
        if (latencyValue.getText().getIntValue()!=0)
        {
            latencyLabel.setColour(Label::textColourId, Colours::lightsalmon);
            latencyValue.setColour(Label::textColourId, Colours::lightsalmon);
            samplesLabel.setColour (Label::textColourId, Colours::lightsalmon);
        }
        else
        {
            latencyLabel.setColour(Label::textColourId, Colours::white);
            latencyValue.setColour(Label::textColourId, Colours::white);
            samplesLabel.setColour (Label::textColourId, Colours::white);
        }
    };
    latencyValue.setText ("0", dontSendNotification);
    addAndMakeVisible(latencyValue);
    
    skippedCyclesLabel.setFont(Font(12.4000f, Font::plain));
    skippedCyclesLabel.setJustificationType(Justification::right);
    skippedCyclesLabel.setColour(Label::textColourId, Colours::white);
    skippedCyclesLabel.setText ("Skipped cycles: ", dontSendNotification);
    addAndMakeVisible(skippedCyclesLabel);
    
    skippedCyclesValue.setFont(Font(12.4000f, Font::plain));
    skippedCyclesValue.setJustificationType(Justification::right);
    skippedCyclesValue.setColour(Label::textColourId, Colours::white);
    skippedCyclesValue.onTextChange = [&] {
        if (skippedCyclesValue.getText().getIntValue()!=0)
        {
            skippedCyclesLabel.setColour(Label::textColourId, Colours::yellow);
            skippedCyclesValue.setColour(Label::textColourId, Colours::yellow);
        }
        else
        {
            skippedCyclesLabel.setColour(Label::textColourId, Colours::white);
            skippedCyclesValue.setColour(Label::textColourId, Colours::white);
        }
    };
    skippedCyclesValue.setText ("1000", dontSendNotification);
    addAndMakeVisible(skippedCyclesValue);
    
    samplesLabel.setFont (Font (12.4000f, Font::plain));
    samplesLabel.setJustificationType (Justification::left);
    samplesLabel.setColour (Label::textColourId, Colours::white);
    samplesLabel.setText("smpls", dontSendNotification);
    addAndMakeVisible (samplesLabel);
}

void View::ConvManagingBox::paint(Graphics& g)
{}

void View::ConvManagingBox::resized()
{
    auto localArea = getLocalBounds();
    localArea.removeFromLeft(130);
    
    int height = 20;
    int width = 70;
    
    Grid grid;
    using Track = Grid::TrackInfo;

    grid.templateRows    = { Track (1_fr), Track (1_fr) };
    grid.templateColumns = { Track (1_fr), Track (80_px), Track (35_px), Track (35_px) };
    
    GridItem bufferCombo (bufferCombobox);
    bufferCombo = bufferCombo.withSize(width, height);
    bufferCombo = bufferCombo.withAlignSelf(GridItem::AlignSelf::center);
//    bufferCombo = bufferCombo.withMargin(GridItem::Margin(0,0,0,10));
    
    GridItem bufferPart (maxPartCombobox);
    bufferPart = bufferPart.withSize(width, height);
    bufferPart = bufferPart.withAlignSelf(GridItem::AlignSelf::center);
    
    GridItem measureUnit (samplesLabel);
    
    bufferPart = bufferPart.withAlignSelf(GridItem::AlignSelf::center);
    

    grid.items = { bufferCombo,  GridItem(latencyLabel),          GridItem(latencyValue),       measureUnit,
                   bufferPart,   GridItem(skippedCyclesLabel),    GridItem(skippedCyclesValue), GridItem()
               };

    grid.performLayout (localArea);
    
    /*
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
     */
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
rectSize (260,200)
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
    
    saveIntoMetaToggle.setButtonText(TRANS("Save value into wavefile metadata"));
    saveIntoMetaToggle.setTooltip(TRANS("The value will be stored within the wavefile"));
    saveIntoMetaToggle.setToggleState(false, dontSendNotification);
    saveIntoMetaToggle.setColour(ToggleButton::textColourId, Colours::white);
    addAndMakeVisible(saveIntoMetaToggle);
    
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
    auto editorArea = areaToDraw.reduced(shrinkValue, 0);
    
    textEditor.setBounds(editorArea.removeFromTop(editorHeight));
    editorArea.removeFromTop(separator);
    
    diagonalToggle.setBounds(editorArea.removeFromTop(editorHeight));
    areaToDraw.removeFromTop(editorHeight*2+separator*3);
    
    shrinkValue = areaToDraw.proportionOfWidth(0.12f);
    areaToDraw.reduce(shrinkValue, 0);
    
    saveIntoMetaToggle.setBounds(areaToDraw.removeFromTop(editorHeight));
    
    areaToDraw.reduce(40, 0);
    OKButton.setSize(areaToDraw.getWidth(), height);
    OKButton.setTopLeftPosition(areaToDraw.getX(), areaToDraw.getY()+areaToDraw.getHeight()/2-height/2);
}

void View::InputChannelDialog::invalidState(InvalidType type)
{
    String  text;
    switch (type) {
        case InvalidType::notFeasible:
            text << "Value must not be 0 or null! \n";
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
    textEditor.setText("");
    
    if(toggleChecked)
//        diagonalToggle.triggerClick();
        diagonalToggle.setToggleState(false, sendNotification);
    
    textEditor.grabKeyboardFocus();
    
    setVisible(false);
}
