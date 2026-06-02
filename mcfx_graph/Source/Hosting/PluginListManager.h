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
#include <atomic>
#include <memory>

class PluginListManager : private juce::ChangeListener
{
public:
    PluginListManager();
    ~PluginListManager() override;

    juce::AudioPluginFormatManager& getFormatManager() { return formatManager_; }
    juce::KnownPluginList&          getKnownPluginList() { return knownPluginList_; }
    const juce::File&               getScannerExecutable() const { return scannerExe_; }

    // Persistent settings (user-customized plugin search folders, etc.). May be
    // null if the settings file can't be created. Used by both the parallel
    // scanner and the "Edit scan folders" UI so they agree on which folders to
    // scan, via the "lastPluginScanPath_<FormatName>" key scheme.
    juce::PropertiesFile*           getSettings() { return settings_.get(); }

    /** Trigger a parallel rescan of every plugin format. Progress is reported
        via the supplied callback (called on the message thread). */
    void startRescan (std::function<void (float progress, juce::String currentName, int found)> progressCb);
    void cancelRescan();
    bool isScanning() const { return scanner_ != nullptr && scanner_->isThreadRunning(); }

    static juce::File getCacheFile();
    static juce::File getSettingsFile();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void saveCache();
    void loadCache();

    juce::AudioPluginFormatManager formatManager_;
    juce::KnownPluginList          knownPluginList_;
    juce::File                     scannerExe_;
    std::unique_ptr<juce::PropertiesFile>  settings_;
    std::unique_ptr<ParallelPluginScanner> scanner_;

    // Bumped on every start/cancel so late progress callbacks queued via
    // callAsync — which capture the callback by value at fire time — can
    // notice they belong to a stale scan and silently drop.
    std::atomic<int> scanGeneration_ { 0 };
};
