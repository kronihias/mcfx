/*
  ==============================================================================

   PluginListManager — owns the AudioPluginFormatManager + KnownPluginList
   used by mcfx_graph, with persistent caching at
   ~/Library/Application Support/mcfx_graph/pluginList.xml (and the
   equivalent on Windows / Linux), plus the ParallelPluginScanner driver.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginHost/OutOfProcessPluginScanner.h"
#include "PluginHost/findScannerExecutable.h"
#include <memory>

class PluginListManager : private juce::ChangeListener
{
public:
    PluginListManager();
    ~PluginListManager() override;

    juce::AudioPluginFormatManager& getFormatManager() { return formatManager_; }
    juce::KnownPluginList&          getKnownPluginList() { return knownPluginList_; }
    const juce::File&               getScannerExecutable() const { return scannerExe_; }

    /** Trigger a parallel rescan of every plugin format. Progress is reported
        via the supplied callback (called on the message thread). */
    void startRescan (std::function<void (float progress, juce::String currentName, int found)> progressCb);
    void cancelRescan();
    bool isScanning() const { return scanner_ != nullptr && scanner_->isThreadRunning(); }

    static juce::File getCacheFile();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void saveCache();
    void loadCache();

    juce::AudioPluginFormatManager formatManager_;
    juce::KnownPluginList          knownPluginList_;
    juce::File                     scannerExe_;
    std::unique_ptr<ParallelPluginScanner> scanner_;
};
