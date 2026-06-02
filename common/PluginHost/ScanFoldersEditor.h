/*
  ==============================================================================

   ScanFoldersEditor — shared dialog content (mcfx_anything, mcfx_graph, ...)

   Lets the user add / remove / reorder the folders scanned for each plug-in
   format. Persists into a PropertiesFile under the same
   "lastPluginScanPath_<FormatName>" keys that PluginListComponent and the
   ParallelPluginScanner read, so edits take effect on the next scan. Only
   formats that actually use search folders are listed (e.g. AudioUnit, which is
   found via the system, is not).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class ScanFoldersEditor : public juce::Component
{
public:
    ScanFoldersEditor (juce::AudioPluginFormatManager& formatManager,
                       juce::PropertiesFile& properties)
        : properties_ (properties)
    {
        for (auto* f : formatManager.getFormats())
            if (f->getDefaultLocationsToSearch().getNumPaths() > 0)
                formats_.add (f);

        intro_.setText ("Folders scanned for each plug-in format. Changes are saved "
                        "automatically and used the next time you scan.",
                        juce::dontSendNotification);
        intro_.setJustificationType (juce::Justification::topLeft);
        intro_.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible (intro_);

        formatLabel_.setText ("Format:", juce::dontSendNotification);
        addAndMakeVisible (formatLabel_);

        for (int i = 0; i < formats_.size(); ++i)
            formatBox_.addItem (formats_[i]->getName(), i + 1);
        formatBox_.onChange = [this] { showFormat (formatBox_.getSelectedId() - 1); };
        addAndMakeVisible (formatBox_);

        addAndMakeVisible (pathList_);

        if (! formats_.isEmpty())
            formatBox_.setSelectedId (1, juce::sendNotificationSync); // -> showFormat(0)

        setSize (520, 360);
    }

    ~ScanFoldersEditor() override { saveCurrent(); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);
        intro_.setBounds (area.removeFromTop (44));
        area.removeFromTop (4);
        auto row = area.removeFromTop (26);
        formatLabel_.setBounds (row.removeFromLeft (60));
        formatBox_.setBounds (row.removeFromLeft (220));
        area.removeFromTop (8);
        pathList_.setBounds (area);
    }

private:
    void showFormat (int index)
    {
        saveCurrent();            // persist edits to the format we're leaving
        currentIndex_ = index;

        if (auto* f = formats_[index])
            pathList_.setPath (juce::PluginListComponent::getLastSearchPath (properties_, *f));
    }

    void saveCurrent()
    {
        if (auto* f = formats_[currentIndex_])   // operator[] yields nullptr when out of range
        {
            juce::PluginListComponent::setLastSearchPath (properties_, *f, pathList_.getPath());
            properties_.saveIfNeeded();
        }
    }

    juce::PropertiesFile& properties_;
    juce::Array<juce::AudioPluginFormat*> formats_;
    int currentIndex_ = -1;

    juce::Label    intro_, formatLabel_;
    juce::ComboBox formatBox_;
    juce::FileSearchPathListComponent pathList_;
};
