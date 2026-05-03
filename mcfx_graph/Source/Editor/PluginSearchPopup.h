/*
  ==============================================================================

   PluginSearchPopup — searchable plugin selector. Text field on top filters
   the list as you type, format-toggle buttons (VST / VST3 / AudioUnit / ...)
   restrict by plugin format. Mirrors mcfx_anything's selector UI so users
   moving between the two plugins see the same affordance.

   Use:
       auto content = std::make_unique<PluginSearchPopup> (descriptions,
           [] (int idx) { ... });
       juce::CallOutBox::launchAsynchronously (std::move (content),
                                                buttonArea, nullptr);

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <functional>

class PluginSearchPopup : public juce::Component,
                          private juce::TextEditor::Listener,
                          private juce::ListBoxModel
{
public:
    PluginSearchPopup (juce::Array<juce::PluginDescription> allTypes,
                       std::function<void (int)> onPicked);

    void resized() override;
    void paint (juce::Graphics& g) override;
    void visibilityChanged() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    // TextEditor::Listener
    void textEditorTextChanged       (juce::TextEditor&) override;
    void textEditorReturnKeyPressed  (juce::TextEditor&) override;
    void textEditorEscapeKeyPressed  (juce::TextEditor&) override;

    // ListBoxModel
    int  getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;

    void pickRow (int row);
    void rebuildFiltered();

    juce::Array<juce::PluginDescription> all_;
    juce::Array<int> filtered_;
    juce::TextEditor search_;
    juce::OwnedArray<juce::TextButton> formatButtons_;
    juce::ListBox list_;
    std::function<void (int)> onPicked_;
};
