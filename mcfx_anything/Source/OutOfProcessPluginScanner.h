/*
  ==============================================================================

   Out-of-process plugin scanner for mcfx_anything.

   OutOfProcessPluginScanner  — KnownPluginList::CustomScanner that spawns a
                                 child process per plugin (crash isolation).

   ParallelPluginScanner      — Thread that scans all plugin files in parallel
                                 using multiple child processes.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <functional>

//==============================================================================
/** Parses XML output from the scanner child process and adds PluginDescriptions
    to the result array.  Returns true if at least one plugin was parsed. */
inline bool parsePluginScannerOutput (const juce::String& xmlOutput,
                                       juce::OwnedArray<juce::PluginDescription>& result)
{
    if (auto xml = juce::parseXML (xmlOutput))
    {
        if (xml->getTagName() == "PLUGINS")
        {
            bool found = false;

            for (auto* child : xml->getChildIterator())
            {
                auto desc = std::make_unique<juce::PluginDescription>();

                if (desc->loadFromXml (*child))
                {
                    result.add (desc.release());
                    found = true;
                }
            }

            return found;
        }
    }

    return false;
}

//==============================================================================
/** Runs the scanner child process for a single plugin file, reading stdout
    incrementally to avoid pipe-buffer deadlocks.

    Returns true on success (exit code 0 and valid XML output).
    On timeout or crash, returns false. If shouldExitFn returns true,
    the process is killed and the function returns true (cancelled). */
inline bool runScannerProcess (const juce::File& scannerExe,
                                const juce::String& formatName,
                                const juce::String& fileOrIdentifier,
                                int timeoutMs,
                                juce::OwnedArray<juce::PluginDescription>& result,
                                std::function<bool()> shouldExitFn = nullptr)
{
    juce::ChildProcess proc;
    juce::StringArray args;
    args.add (scannerExe.getFullPathName());
    args.add (formatName);
    args.add (fileOrIdentifier);

    if (! proc.start (args, juce::ChildProcess::wantStdOut))
        return false;

    juce::MemoryOutputStream output;
    char buffer[4096];
    int elapsedMs = 0;
    constexpr int pollIntervalMs = 50;

    while (proc.isRunning())
    {
        if (shouldExitFn && shouldExitFn())
        {
            proc.kill();
            return true; // cancelled — not an error
        }

        if (elapsedMs > timeoutMs)
        {
            proc.kill();
            return false; // timed out
        }

        int bytesRead = proc.readProcessOutput (buffer, sizeof (buffer));

        if (bytesRead > 0)
            output.write (buffer, (size_t) bytesRead);
        else
            juce::Thread::sleep (pollIntervalMs);

        elapsedMs += pollIntervalMs;
    }

    // Drain remaining output after process exits
    for (;;)
    {
        int bytesRead = proc.readProcessOutput (buffer, sizeof (buffer));
        if (bytesRead <= 0) break;
        output.write (buffer, (size_t) bytesRead);
    }

    if (proc.getExitCode() != 0)
        return false;

    return parsePluginScannerOutput (output.toString(), result);
}

//==============================================================================
/** CustomScanner that runs each plugin scan in a child process.
    Used as a fallback when PluginListComponent's built-in scan buttons are
    clicked (serial, but crash-isolated). */
class OutOfProcessPluginScanner : public juce::KnownPluginList::CustomScanner
{
public:
    explicit OutOfProcessPluginScanner (const juce::File& scannerExe, int timeoutMs = 30000)
        : scannerExe_ (scannerExe), timeoutMs_ (timeoutMs) {}

    bool findPluginTypesFor (juce::AudioPluginFormat& format,
                             juce::OwnedArray<juce::PluginDescription>& result,
                             const juce::String& fileOrIdentifier) override
    {
        return runScannerProcess (scannerExe_, format.getName(), fileOrIdentifier,
                                  timeoutMs_, result,
                                  [this] { return shouldExit(); });
    }

private:
    juce::File scannerExe_;
    int timeoutMs_;
};

//==============================================================================
/** Scans all plugin files in parallel using multiple child processes.
    Progress is reported via a callback on the message thread. */
class ParallelPluginScanner : public juce::Thread
{
public:
    ParallelPluginScanner (juce::AudioPluginFormatManager& formatManager,
                           juce::KnownPluginList& knownPlugins,
                           const juce::File& scannerExe,
                           int maxConcurrent = 0,
                           int timeoutPerPluginMs = 30000)
        : Thread ("ParallelPluginScanner"),
          formatManager_ (formatManager),
          knownPlugins_ (knownPlugins),
          scannerExe_ (scannerExe),
          maxConcurrent_ (maxConcurrent > 0 ? maxConcurrent
                              : juce::jlimit (2, 8, (int) std::thread::hardware_concurrency())),
          timeoutMs_ (timeoutPerPluginMs)
    {
    }

    /** Set a callback for progress updates. Called on the message thread.
        Parameters: (progress 0..1, current plugin name, plugins found so far). */
    void setProgressCallback (std::function<void (float, juce::String, int)> cb)
    {
        progressCallback_ = std::move (cb);
    }

    void run() override
    {
        // Gather all plugin files to scan
        struct ScanJob
        {
            juce::String formatName;
            juce::String fileOrIdentifier;
        };

        std::vector<ScanJob> jobs;

        for (auto* format : formatManager_.getFormats())
        {
            auto paths = format->getDefaultLocationsToSearch();
            auto files = format->searchPathsForPlugins (paths, true, false);

            for (auto& file : files)
            {
                if (knownPlugins_.isListingUpToDate (file, *format))
                    continue;

                jobs.push_back ({ format->getName(), file });
            }
        }

        if (jobs.empty())
        {
            reportProgress (1.0f, "No new plugins found", 0);
            return;
        }

        totalJobs_ = (int) jobs.size();
        completedJobs_ = 0;
        pluginsFound_ = 0;

        // Process jobs with a pool of concurrent child processes
        std::atomic<size_t> nextJob { 0 };
        std::atomic<int> completed { 0 };
        std::atomic<int> found { 0 };
        std::mutex mutex;

        auto worker = [&]()
        {
            while (! threadShouldExit())
            {
                size_t idx = nextJob.fetch_add (1);

                if (idx >= jobs.size())
                    break;

                auto& job = jobs[idx];

                // Extract a short name for progress display
                juce::String shortName = juce::File (job.fileOrIdentifier).getFileNameWithoutExtension();
                if (shortName.isEmpty())
                    shortName = job.fileOrIdentifier;

                juce::OwnedArray<juce::PluginDescription> results;
                bool ok = runScannerProcess (scannerExe_, job.formatName,
                                              job.fileOrIdentifier, timeoutMs_,
                                              results,
                                              [this] { return threadShouldExit(); });

                if (ok && ! results.isEmpty())
                {
                    int newFound = 0;

                    juce::MessageManager::callAsync ([this, results = std::move (results)]() mutable
                    {
                        for (auto* desc : results)
                            knownPlugins_.addType (*desc);
                    });

                    newFound = results.size();
                    found.fetch_add (newFound);
                }

                int c = completed.fetch_add (1) + 1;
                float progress = (float) c / (float) jobs.size();
                reportProgress (progress, shortName, found.load());
            }
        };

        // Launch worker threads
        std::vector<std::thread> workers;
        int numWorkers = juce::jmin (maxConcurrent_, (int) jobs.size());

        for (int i = 0; i < numWorkers; ++i)
            workers.emplace_back (worker);

        for (auto& w : workers)
            w.join();

        if (! threadShouldExit())
            reportProgress (1.0f, "Scan complete", found.load());
    }

    int getTotalJobs()     const { return totalJobs_; }
    int getCompletedJobs() const { return completedJobs_; }
    int getPluginsFound()  const { return pluginsFound_; }

private:
    void reportProgress (float progress, const juce::String& name, int found)
    {
        completedJobs_ = (int) (progress * totalJobs_);
        pluginsFound_  = found;

        if (progressCallback_)
        {
            juce::MessageManager::callAsync ([cb = progressCallback_, progress, name, found]()
            {
                cb (progress, name, found);
            });
        }
    }

    juce::AudioPluginFormatManager& formatManager_;
    juce::KnownPluginList& knownPlugins_;
    juce::File scannerExe_;
    int maxConcurrent_;
    int timeoutMs_;

    std::function<void (float, juce::String, int)> progressCallback_;
    std::atomic<int> totalJobs_ { 0 };
    std::atomic<int> completedJobs_ { 0 };
    std::atomic<int> pluginsFound_ { 0 };
};
