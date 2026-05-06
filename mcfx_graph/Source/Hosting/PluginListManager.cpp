#include "PluginListManager.h"
#include <juce_audio_processors/format/juce_AudioPluginFormatManagerHelpers.h>

PluginListManager::PluginListManager()
{
    addDefaultFormatsToManager (formatManager_);

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

    scanner_ = std::make_unique<ParallelPluginScanner> (formatManager_,
                                                        knownPluginList_,
                                                        scannerExe_);
    if (progressCb)
        scanner_->setProgressCallback (std::move (progressCb));

    scanner_->startThread();
}

void PluginListManager::cancelRescan()
{
    if (scanner_ != nullptr)
    {
        scanner_->signalThreadShouldExit();
        scanner_->stopThread (5000);
        scanner_.reset();
    }
}
