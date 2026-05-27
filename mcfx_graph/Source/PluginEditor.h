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
#include "PresetManager.h"

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

    void onPresetsClicked();
    void onScanClicked();
    void startPluginScan();
    void onUndoClicked();
    void onRedoClicked();
    void onHelpClicked();

    // Shared by the file-picker menu items and the named-preset list.
    void loadPresetFile (const juce::File& file);
    void savePresetFile (const juce::File& file);

    // Named-preset prompts (all async, drive juce::AlertWindow).
    void promptSaveAsNamedPreset();
    void promptRenamePreset (const juce::File& file);
    void confirmDeletePreset (const juce::File& file);

    Mcfx_graphAudioProcessor& processor_;
    PresetManager presets_ { "mcfx_graph" };

    juce::TextButton presetsButton_ { "Presets" };
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
    // Reused across the named-preset prompts (save/rename/delete/overwrite).
    // Held as a member so the async modal callback can clean it up safely.
    std::unique_ptr<juce::AlertWindow> alertWindow_;
};
