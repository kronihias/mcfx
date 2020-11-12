
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
    LookAndFeel::setDefaultLookAndFeel(&MyLookAndFeel);
    
    addAndMakeVisible(titleBox);
    
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
    
    addAndMakeVisible(filterManagingBox);
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
    versionLabel.setJustificationType(Justification::bottomRight);
    versionLabel.setText(version_string, dontSendNotification);
    addAndMakeVisible(versionLabel);
    
    addAndMakeVisible(inputChannelDialog);
    inputChannelDialog.setVisible(false);
}

void View::LockSensibleElements()
{
    filterManagingBox.LockSensibleElements();
    convManagingBox.LockSensibleElements();
}

void View::UnlockSensibleElements()
{
    filterManagingBox.UnlockSensibleElements();
    convManagingBox.UnlockSensibleElements();
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
    auto area = getBounds();
    int border = 15;
    int separator = 12;
    
    int titleBoxHeight = 50;
    
    int filterBoxHeight = 48;//24 + 6*2
    
    int IRBoxHeight = 36;
    
    int IOBoxHeight = 132;
    
    int convBoxHeight = 78;
    
    int changePathHeight = 24;
    
    int debugWinHeight = 100;

    int shapesize = 20;
    int smallLabelHeight = 24;
    
    inputChannelDialog.setBounds(getBounds());
    
    versionLabel.setBounds(area.removeFromBottom(smallLabelHeight));
    
    area.removeFromLeft(border);
    area.removeFromRight(border);
    int mainWidth = area.getWidth();
    
    //---------------------------------------------------------------------
    FlexBox statusBox;
    statusBox.justifyContent = FlexBox::JustifyContent::spaceBetween;
    statusBox.alignItems = FlexBox::AlignItems::center;
    
    FlexItem ledItem  (shapesize,  shapesize, statusLed);
    ledItem.alignSelf = FlexItem::AlignSelf::autoAlign;
    ledItem.margin = FlexItem::Margin(0,10,0,0);
    
    FlexItem textItem  (statusText);
    textItem.alignSelf = FlexItem::AlignSelf::autoAlign;
    textItem.minHeight = shapesize;
    textItem.minWidth = 1;
    textItem.flexGrow = 1;
    
    statusBox.items.addArray({ ledItem, textItem });
    
    //---------------------------------------------------------------------
    FlexBox mainFlex;
    mainFlex.flexDirection = FlexBox::Direction::column;
    mainFlex.justifyContent = FlexBox::JustifyContent::spaceBetween;
    mainFlex.alignItems = FlexBox::AlignItems::center;
    
    FlexItem titleItem  (mainWidth,  titleBoxHeight, titleBox);
    titleItem.alignSelf = FlexItem::AlignSelf::autoAlign;
    
    FlexItem filterMangItem (mainWidth, filterBoxHeight, filterManagingBox);
    filterMangItem.alignSelf = FlexItem::AlignSelf::autoAlign;
    
    FlexItem ioBoxItem (mainWidth, IOBoxHeight, ioDetailBox);
    ioBoxItem.alignSelf = FlexItem::AlignSelf::autoAlign;
    
    FlexItem convBoxItem (mainWidth, convBoxHeight, convManagingBox);
    convBoxItem.alignSelf = FlexItem::AlignSelf::autoAlign;
    
    FlexItem changePathItem (mainWidth, changePathHeight, changePathBox);
    changePathItem.alignSelf = FlexItem::AlignSelf::autoAlign;
    
    FlexItem statusItem (mainWidth, shapesize, statusBox);
    statusItem.alignSelf = FlexItem::AlignSelf::autoAlign;
    
    mainFlex.items.addArray({   titleItem,
                                changePathItem,
                                filterMangItem,
                                ioBoxItem,
                                convBoxItem,
                                statusItem
                            });
    
    mainFlex.performLayout(area.toFloat());
}

//==============================================================================
//==============================================================================
//   Nested GUI classes
View::TitleBox::TitleBox()
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
}

void View::TitleBox::resized()
{
    auto area = getLocalBounds();
    int titleHeight = 25;
    
    title.setBounds(area.removeFromTop(titleHeight)); //title
    subtitle.setBounds(area.removeFromTop(titleHeight)); //subtitle
}

//==============================================================================
View::FilterManagingBox::FilterManagingBox()
{
    filterLabel.setFont (Font (15.0f, Font::plain));
    filterLabel.setColour (Label::textColourId, Colours::white);
    filterLabel.setJustificationType(Justification::right);
    filterLabel.setText("Filter", dontSendNotification);
    addAndMakeVisible (filterLabel);
    
    addAndMakeVisible (filterSelector);
    
    reloadButton.setTooltip ("reload the current matrix specifying a new input channels number");
    reloadButton.setColour (TextButton::buttonColourId, Colours::white);
    reloadButton.setColour (TextButton::buttonOnColourId, Colours::blue);
    reloadButton.setEnabled(false);
    reloadButton.setButtonText ("reload");
    addAndMakeVisible(reloadButton);
    
    restoredSettingsLabel.setFont (Font (11.0f, Font::plain));
    restoredSettingsLabel.setJustificationType(Justification::topLeft);
    restoredSettingsLabel.setColour (Label::textColourId, Colours::white);
    restoredSettingsLabel.setText("(saved within the project)", dontSendNotification);
    addAndMakeVisible (restoredSettingsLabel);
    restoredSettingsLabel.setVisible(false);
}

void View::FilterManagingBox::LockSensibleElements()
{
    filterSelector.setEnabled(false);
    reloadButton.setEnabled(false);
}
void View::FilterManagingBox::UnlockSensibleElements()
{
    filterSelector.setEnabled(true);
    reloadButton.setEnabled(true);
}

void View::FilterManagingBox::paint(Graphics& g)
{
//    g.setColour(Colours::white);
//    g.drawRect(getLocalBounds());
}

void View::FilterManagingBox::resized()
{
    int rowHeight = 24;
    
    int labelHeight = 55;
    
    int labelWidth = 45;
//    int pathLabelWidth = 45;
    int pathButtonWidth = 30;
    int reloadButtonWitdh = 50;
    
    auto area = getLocalBounds();
    
    Grid grid;
    using Track = Grid::TrackInfo;
    grid.alignContent = Grid::AlignContent::spaceBetween;
    grid.justifyContent = Grid::JustifyContent::spaceBetween;
    grid.alignItems = Grid::AlignItems::center;
    grid.justifyItems = Grid::JustifyItems::center;
//    grid.rowGap = {20_px};
    
    grid.templateRows    = {Track (3_fr), Track (1_fr) };
    grid.templateColumns = {Track (45_px), Track (1_fr), Track (60_px)};
    
    GridItem TextEditor (filterSelector);
    TextEditor = TextEditor.withHeight(rowHeight);
    
    GridItem ReloadButton (reloadButton);
    ReloadButton = ReloadButton.withSize(reloadButtonWitdh, rowHeight);
    ReloadButton = ReloadButton.withMargin(GridItem::Margin(0,0,0,10));
    
    GridItem infoText (restoredSettingsLabel);
    infoText = infoText.withAlignSelf(GridItem::AlignSelf::start);
    
    grid.items = {  GridItem (filterLabel), TextEditor, ReloadButton,
                    GridItem(),             infoText,   GridItem()
                };

    grid.performLayout (area);
}

//==============================================================================
View::ChangePathBox::ChangePathBox()
{
    pathLabel.setFont (Font (15.0f, Font::plain));
    pathLabel.setColour (Label::textColourId, Colours::white);
    pathLabel.setJustificationType(Justification::right);
    pathLabel.setText("Path", dontSendNotification);
    addAndMakeVisible (pathLabel);
    
    pathText.setFont (Font (11.0f, Font::plain));
    pathText.setColour(Label::outlineColourId, Colour(0xff8e989b)); //from slider LookAndFeel(_V3?) outline textbox colour
    pathText.setColour(Label::textColourId, Colours::white);
    addAndMakeVisible (pathText);
    
    pathButton.setTooltip ("choose another filter libray folder\n(Right click to change generic path)");
    pathButton.setButtonText ("...");
    pathButton.setColour (TextButton::buttonColourId, Colours::white);
    pathButton.setColour (TextButton::buttonOnColourId, Colours::blue);
    addAndMakeVisible(pathButton);
}

void View::ChangePathBox::paint(Graphics& g)
{
//    g.setColour(Colours::white);
//    g.drawRect(getLocalBounds());
}

void View::ChangePathBox::resized()
{
    int buttonWidth = 30;
   
    int labelWidth = 45;
    
    int height = 24;
//    int labelHeight = 48;
    
    FlexBox mainFlex;
    mainFlex.justifyContent = FlexBox::JustifyContent::flexStart;
    mainFlex.alignItems = FlexBox::AlignItems::center;
    
    FlexItem label  (labelWidth,  height, pathLabel);
    label.alignSelf = FlexItem::AlignSelf::autoAlign;
//    label.margin = FlexItem::Margin(0,10,0,0);
    
    FlexItem editor (pathText);
    editor.alignSelf = FlexItem::AlignSelf::autoAlign;
    editor.flexGrow = 1;
    editor.minWidth = labelWidth;
    editor.minHeight = height;
    
    FlexItem button (buttonWidth, height, pathButton);
    button.alignSelf = FlexItem::AlignSelf::autoAlign;
    button.margin = FlexItem::Margin(0,0,0,10);
    
    mainFlex.items.addArray({ label, editor, button });
    
    mainFlex.performLayout(getLocalBounds().toFloat());
}

//==============================================================================
View::IODetailBox::IODetailBox()
{
    matrixConfigLabel.setText("Matrix dimensions:", dontSendNotification);
    addAndMakeVisible (matrixConfigLabel);

    inputValue.setText("0 ins", dontSendNotification);
    addAndMakeVisible (inputValue);
    
    outputValue.setJustificationType (Justification::left);
    outputValue.setText("0 outs", dontSendNotification);
    addAndMakeVisible (outputValue);
    
    IRLabel.setJustificationType (Justification::centredRight);
    IRLabel.setText("Impulse responses:", dontSendNotification);
    addAndMakeVisible (IRLabel);
    
    IRValue.setText("0", dontSendNotification);
    addAndMakeVisible (IRValue);
    
    diagonalValue.setFont (Font (13.0000f, Font::plain));
    diagonalValue.setJustificationType (Justification::centredLeft);
    diagonalValue.setText("(diagonal)", dontSendNotification);
    addAndMakeVisible (diagonalValue);
    
    sampleRateLabel.setText("Samplerate:", dontSendNotification);
    addAndMakeVisible (sampleRateLabel);
    
    sampleRateNumber.setText("0", dontSendNotification);
    addAndMakeVisible (sampleRateNumber);
    
    hostBufferLabel.setText("Host Buffer Size:", dontSendNotification);
    addAndMakeVisible (hostBufferLabel);
    
    hostBufferNumber.setText("0", dontSendNotification);
    addAndMakeVisible (hostBufferNumber);
    
    filterLengthLabel.setText("Filter Length:", dontSendNotification);
    addAndMakeVisible (filterLengthLabel);
    
    filterLengthInSeconds.setText("0", dontSendNotification);
    addAndMakeVisible (filterLengthInSeconds);
    
    filterLengthInSamples.setJustificationType (Justification::topRight);
    filterLengthInSamples.setText("0", dontSendNotification);
    addAndMakeVisible (filterLengthInSamples);
    
    secondsLabel.setJustificationType (Justification::left);
    secondsLabel.setText("s", dontSendNotification);
    addAndMakeVisible (secondsLabel);
    
    samplesLabel.setJustificationType (Justification::topLeft);
    samplesLabel.setText("smpls", dontSendNotification);
    addAndMakeVisible (samplesLabel);
    
    resampledLabel.setFont (Font (12.0000f, Font::plain));
    resampledLabel.setJustificationType (Justification::bottomRight);
    resampledLabel.setColour (Label::textColourId, Colours::yellow);
    resampledLabel.setText("(resampled wavefile)", dontSendNotification);
    resampledLabel.setTooltip(TRANS("wavefile resampled to match host samplerate"));
    addAndMakeVisible (resampledLabel);
    resampledLabel.setVisible(false);
    
    gainLabel.setJustificationType (Justification::centred);
    gainLabel.setText("Master gain", dontSendNotification);
    addAndMakeVisible (gainLabel);
    
    gainKnob.setSliderStyle (Slider::RotaryVerticalDrag);
//    gainKnob.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, false);
    gainKnob.setRange(-60, 12);
    gainKnob.setValue(0);
    gainKnob.setDoubleClickReturnValue(true, 0);
    gainKnob.setColour(Slider::rotarySliderFillColourId, Colours::white);
    gainKnob.setTextBoxStyle (Slider::TextBoxRight, true, 70, 20);
    gainKnob.setNumDecimalPlacesToDisplay(1);
    gainKnob.setTextValueSuffix (" dB");
    gainKnob.setMouseDragSensitivity(125);
//    auto color = gainKnob.getLookAndFeel().findColour(Slider::textBoxOutlineColourId);
    addAndMakeVisible(gainKnob);
    
    
//    getParentComponent()->getLookAndFeel().setColour(Slider::rotarySliderFillColourId, Colours::white);
}

void View::IODetailBox::paint(Graphics& g)
{
    auto localArea = getLocalBounds();
    int separator = 6;
    float chafmer = 10.0f;
    
//    g.setColour(Colours::white);
//    g.drawRect(getLocalBounds());
    
    auto boxColor = Colours::blue;
    g.setColour(boxColor.withAlpha(0.26f)); //transparency 26%
    g.fillRoundedRectangle(localArea.removeFromLeft(proportionOfWidth(0.59f)).toFloat(), chafmer);
    localArea.removeFromLeft(separator);
    
    boxColor = Colours::violet;
    g.setColour(boxColor.withAlpha(0.26f)); //transparency 26%
    g.fillRoundedRectangle(localArea.removeFromTop(proportionOfHeight(0.41f)).toFloat(), chafmer);
    
//    g.setColour (Colours::white);
//    g.drawLine(proportionOfWidth(0.45f), chafmer/2, proportionOfWidth(0.45f), getHeight()-chafmer/2, 0.75f);
}

void View::IODetailBox::resized()
{
    auto localArea = getLocalBounds();
    int separator = 6;
    
    Grid matrixGrid;
    using TrackR = Grid::TrackInfo;

    matrixGrid.templateRows       = { TrackR (1_fr), TrackR (1_fr), TrackR (1_fr), TrackR (1_fr), TrackR(15_px) };
    matrixGrid.templateColumns    = { TrackR (1_fr), TrackR (60_px) , TrackR (60_px)};
    matrixGrid.columnGap          = {0_px};
    
    GridItem resample (resampledLabel);
    resample = resample.withArea(5, 1, 5, 4);
    
    GridItem firlenSecs (filterLengthInSeconds);
//    firlenSecs = firlenSecs.withMargin(GridItem::Margin(0,8,0,0));
    
    GridItem firlenSmpls (filterLengthInSamples);
//    firlenSmpls = firlenSmpls.withMargin(GridItem::Margin(0,8,0,0));

    matrixGrid.items = {GridItem (matrixConfigLabel),   GridItem(inputValue),    GridItem(outputValue),
                        GridItem (IRLabel),             GridItem(IRValue),       GridItem(diagonalValue),
                        GridItem (filterLengthLabel),   firlenSecs,              GridItem(secondsLabel),
                        GridItem(),                     firlenSmpls,             GridItem(samplesLabel),
                        resample
               };
    
    auto gridArea = localArea.removeFromLeft(proportionOfWidth(0.59f));
    gridArea.reduce(0, 8);
    matrixGrid.performLayout (gridArea);
    localArea.removeFromLeft(separator);
    
    //--------------------------------------------------------------------------
    Grid DSPgrid;
    using TrackL = Grid::TrackInfo;

    DSPgrid.templateRows    = { TrackL (1_fr), TrackL (1_fr) };
    DSPgrid.templateColumns = { TrackL (1_fr), TrackL (50_px) };

//    GridItem inCh (inputNumber);
//    inCh = inCh.withMargin(GridItem::Margin(0,10,0,0));
//
//    GridItem outCh (outputNumber);
//    outCh = outCh.withMargin(GridItem::Margin(0,10,0,0));
//
//    GridItem irNum (IRNumber);
//    irNum = irNum.withMargin(GridItem::Margin(0,10,0,0));
    
    DSPgrid.items = {   GridItem(sampleRateLabel),      GridItem(sampleRateNumber),
                        GridItem(hostBufferLabel),      GridItem(hostBufferNumber)
                    };
    
    gridArea = localArea.removeFromTop(proportionOfHeight(0.41f));
    gridArea.reduce(0, 8);
    DSPgrid.performLayout(gridArea);
    
    localArea.removeFromTop(8);
    gainLabel.setBounds(localArea.removeFromBottom(15));
    gainKnob.setBounds(localArea);
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

void View::ConvManagingBox::LockSensibleElements()
{
    bufferCombobox.setEnabled(false);
    maxPartCombobox.setEnabled(false);
}
void View::ConvManagingBox::UnlockSensibleElements()
{
    bufferCombobox.setEnabled(true);
    maxPartCombobox.setEnabled(true);
}

void View::ConvManagingBox::paint(Graphics& g)
{
//    g.setColour(Colours::white);
//    g.drawRect(getLocalBounds());
}

void View::ConvManagingBox::resized()
{
    auto localArea = getLocalBounds();
    localArea.removeFromLeft(140);
    
    int height = 20;
    int width = 70;
    
    Grid grid;
    using Track = Grid::TrackInfo;

    grid.templateRows    = { Track (1_fr), Track (1_fr) };
    grid.templateColumns = { Track (70_px), Track (100_px), Track (35_px), Track (1_fr) };
    
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
    diagonalToggle.onClick = [&]{
        if ( diagonalToggle.getToggleState() )
        {
            textEditor.setEnabled(false);
            textEditor.setText("");
            diagonalToggle.grabKeyboardFocus();
        }
        else
        {
            textEditor.setEnabled(true);
            textEditor.grabKeyboardFocus();
        }
            };
    addAndMakeVisible(diagonalToggle);
    
    OKButton.setColour (TextButton::buttonColourId, Colours::white);
    OKButton.setColour (TextButton::buttonOnColourId, Colours::blue);
    OKButton.addShortcut(KeyPress(KeyPress::returnKey));
//    OKButton.onClick = [&] {OKButton.grabKeyboardFocus();};
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
    
    int centerHeight = 120;
    
    rectSize.setX((getWidth()/2)-rectSize.getWidth()/2);
    rectSize.setY((getHeight()/2)-rectSize.getHeight()/2);
    
    Rectangle<int> areaToDraw = rectSize;
    areaToDraw.reduce(12, 4); // border
    
    title.setBounds(areaToDraw.removeFromTop(height));
        
    FlexBox flexBox;
    flexBox.flexDirection = FlexBox::Direction::column;
    flexBox.justifyContent = FlexBox::JustifyContent::spaceAround;
    flexBox.alignItems = FlexBox::AlignItems::center;
    
    FlexItem text (areaToDraw.proportionOfWidth(0.90f) , height*2, message);
    text.alignSelf = FlexItem::AlignSelf::autoAlign;
    
    FlexItem editor (areaToDraw.proportionOfWidth(0.60f), editorHeight, textEditor);
    editor.alignSelf = FlexItem::AlignSelf::autoAlign;
    
    FlexItem toggle (areaToDraw.proportionOfWidth(0.60f), editorHeight, diagonalToggle);
    toggle.alignSelf = FlexItem::AlignSelf::autoAlign;
    
    flexBox.items.addArray({ text, editor, toggle });
    flexBox.performLayout(areaToDraw.removeFromTop(centerHeight).toFloat());
    
    /*
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
    */
    int shrinkValue = areaToDraw.proportionOfWidth(0.12f);
    areaToDraw.reduce(shrinkValue, 0);
    
//    saveIntoMetaToggle.setBounds(areaToDraw.removeFromTop(editorHeight));
    
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
    message.setText(text, dontSendNotification);
    message.setColour(Label::textColourId, Colours::lightsalmon);
}

void View::InputChannelDialog::resetState(bool editorFocus)
{
    setVisible(true);
    
    message.setText("Number of input channels is missing, \nplease enter it manually:", dontSendNotification);
    message.setColour(Label::textColourId, Colours::white);
    
    textEditor.setText("",dontSendNotification);

    if(diagonalToggle.getToggleState())
        diagonalToggle.setToggleState(false, sendNotification);
    else if (editorFocus)
        textEditor.grabKeyboardFocus();

}
