#include "PluginListManager.h"
#include <juce_audio_processors/format/juce_AudioPluginFormatManagerHelpers.h>

PluginListManager::PluginListManager()
{
    addDefaultFormatsToManager (formatManager_);

    {
        auto settingsFile = getSettingsFile();
        settingsFile.getParentDirectory().createDirectory();

        juce::PropertiesFile::Options options;
        options.commonToAllUsers = false;
        settings_ = std::make_unique<juce::PropertiesFile> (settingsFile, options);
    }

    loadCache();

    scannerExe_ = findScannerExecutable ("mcfx_graph_plugin_scanner");
    if (scannerExe_.existsAsFile())
        knownPluginList_.setCustomScanner (
            std::make_unique<OutOfProcessPluginScanner> (scannerExe_));

    knownPluginList_.addChangeListener (this);
}

PluginListManager::~PluginListManager()
{
    knownPluginList_.removeChangeListener (this);
    cancelRescan();
}

juce::File PluginListManager::getCacheFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("mcfx_graph")
               .getChildFile ("pluginList.xml");
}

juce::File PluginListManager::getSettingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("mcfx_graph")
               .getChildFile ("pluginSettings.settings");
}

void PluginListManager::loadCache()
{
    auto f = getCacheFile();
    if (! f.existsAsFile()) return;
    if (auto xml = juce::parseXML (f))
        knownPluginList_.recreateFromXml (*xml);
}

void PluginListManager::saveCache()
{
    auto f = getCacheFile();
    f.getParentDirectory().createDirectory();
    if (auto xml = knownPluginList_.createXml())
        xml->writeTo (f);
}

void PluginListManager::changeListenerCallback (juce::ChangeBroadcaster*)
{
    saveCache();
}

void PluginListManager::startRescan (std::function<void (float, juce::String, int)> progressCb)
{
    cancelRescan();

    if (! scannerExe_.existsAsFile())
    {
        // Fire the progress callback so the editor can surface the failure
        // instead of getting stuck on "Scanning…". Deferred via callAsync so
        // the caller can still set its own pre-scan UI state immediately
        // after this returns.
        if (progressCb)
            juce::MessageManager::callAsync ([cb = std::move (progressCb)]()
            {
                cb (1.0f, "Scanner helper not found in plugin bundle", 0);
            });

        return;
    }

    const int gen = ++scanGeneration_;

    scanner_ = std::make_unique<ParallelPluginScanner> (formatManager_,
                                                        knownPluginList_,
                                                        scannerExe_,
                                                        settings_.get());
    if (progressCb)
        scanner_->setProgressCallback (
            [this, gen, cb = std::move (progressCb)]
            (float p, juce::String n, int f)
            {
                if (scanGeneration_.load() == gen)
                    cb (p, n, f);
            });

    scanner_->startThread();
}

void PluginListManager::cancelRescan()
{
    // Bump first so already-queued progress callbacks (carrying the old
    // generation) become no-ops by the time they fire on the message thread.
    ++scanGeneration_;

    if (scanner_ != nullptr)
    {
        scanner_->signalThreadShouldExit();
        scanner_->stopThread (5000);
        scanner_.reset();
    }
}
