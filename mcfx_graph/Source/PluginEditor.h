/*
  ==============================================================================

   Top-level editor for mcfx_graph: toolbar (save/load JSON, scan plugins)
   above a GraphEditorComponent canvas.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Editor/GraphEditorComponent.h"
#include "Editor/NodePropertiesPanel.h"
#include "Editor/BreadcrumbBar.h"

class Mcfx_graphAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::FileDragAndDropTarget,
                                       private juce::ChangeListener
{
public:
    explicit Mcfx_graphAudioProcessorEditor (Mcfx_graphAudioProcessor&);
    ~Mcfx_graphAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool keyPressed (const juce::KeyPress&) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void onSaveClicked();
    void onLoadClicked();
    void onScanClicked();
    void onUndoClicked();
    void onRedoClicked();
    void onHelpClicked();

    Mcfx_graphAudioProcessor& processor_;

    juce::TextButton saveButton_   { "Save JSON" };
    juce::TextButton loadButton_   { "Load JSON" };
    juce::TextButton scanButton_   { "Scan plugins" };
    juce::TextButton undoButton_   { "Undo" };
    juce::TextButton redoButton_   { "Redo" };
    juce::TextButton zoomOutButton_ { "-" };
    juce::TextButton resetZoomButton_ { "1:1" };
    juce::TextButton zoomInButton_  { "+" };
    juce::TextButton helpButton_    { "?" };
    juce::Label      statusLabel_;
    juce::Label      titleLabel_;        // "mcfx_graph"
    juce::Label      versionLabel_;      // "v0.7.2"

    juce::TooltipWindow tooltipWindow_ { this, 700 };

    GraphEditorComponent canvas_;
    juce::Viewport       canvasViewport_;
    BreadcrumbBar        breadcrumbBar_;
    NodePropertiesPanel  propertiesPanel_;

    // Drag-to-resize divider between canvas and properties panel.
    juce::StretchableLayoutManager   horizontalLayout_;
    std::unique_ptr<juce::StretchableLayoutResizerBar> resizerBar_;

    std::unique_ptr<juce::FileChooser> chooser_;
};
