/*
  ==============================================================================

   BreadcrumbBar — horizontal bar showing the current subgraph navigation path
   (Root > Subgraph A > Subgraph B). Each entry is a clickable button that
   jumps the canvas back to that level.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class GraphEditorComponent;

class BreadcrumbBar : public juce::Component
{
public:
    explicit BreadcrumbBar (GraphEditorComponent& editor);

    void rebuild();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    GraphEditorComponent& editor_;
    juce::OwnedArray<juce::TextButton> buttons_;
    juce::OwnedArray<juce::Label>      separators_;
};
