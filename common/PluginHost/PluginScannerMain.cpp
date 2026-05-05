/*
  ==============================================================================

   Out-of-process plugin scanner entry point.

   Each mcfx plugin that hosts other plugins (mcfx_anything, mcfx_graph, ...)
   builds its own copy of this scanner as a separate executable embedded in its
   bundle. The exe name is set per-plugin in CMake via juce_add_gui_app(name).

   Usage:  <scanner_exe> <formatName> <fileOrIdentifier>

   Scans a single plugin file using the specified format, writes the resulting
   PluginDescription(s) as XML to stdout, and exits.

   Exit codes:
     0  — success (XML written)
     1  — error (format not found, no results, bad args)
     >1 / signal — plugin crashed during scan

  ==============================================================================
*/

#include <JuceHeader.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <thread>

#if JUCE_MODULE_AVAILABLE_juce_audio_processors
 #include <juce_audio_processors/format/juce_AudioPluginFormatManagerHelpers.h>
#endif

#if JUCE_WINDOWS
 #include <windows.h>
 #include <tlhelp32.h>
#elif JUCE_MAC || JUCE_LINUX
 #include <unistd.h>
#endif

using namespace juce;

class PluginScannerApp : public JUCEApplication
{
public:
    const String getApplicationName()    override { return "mcfx_plugin_scanner"; }
    const String getApplicationVersion() override { return "1.0"; }
    bool moreThanOneInstanceAllowed()    override { return true; }

    void initialise (const String&) override
    {
        auto args = getCommandLineParameterArray();

        if (args.size() < 2)
        {
            std::cerr << "Usage: " << getApplicationName().toStdString()
                      << " <formatName> <fileOrIdentifier>" << std::endl;
            setApplicationReturnValue (1);
            quit();
            return;
        }

        formatName_       = args[0];
        fileOrIdentifier_ = args[1];

       #if JUCE_WINDOWS
        parentProcessHandle_ = openParentProcess();
       #endif

        startWatchdog();

        // Dispatch the actual scan to run after the message loop has started.
        // This is required for AU scanning on macOS, which needs the event
        // loop to be pumping.
        MessageManager::callAsync ([this] { performScan(); });
    }

    void performScan()
    {
        addDefaultFormatsToManager (formatManager_);

        AudioPluginFormat* format = nullptr;

        for (auto* f : formatManager_.getFormats())
        {
            if (f->getName() == formatName_)
            {
                format = f;
                break;
            }
        }

        if (format == nullptr)
        {
            std::cerr << "Unknown format: " << formatName_.toStdString() << std::endl;
            setApplicationReturnValue (1);
            quit();
            return;
        }

        OwnedArray<PluginDescription> results;
        format->findAllTypesForFile (results, fileOrIdentifier_);

        if (results.isEmpty())
        {
            setApplicationReturnValue (1);
            quit();
            return;
        }

        auto root = std::make_unique<XmlElement> ("PLUGINS");

        for (auto* desc : results)
            if (auto xml = desc->createXml())
                root->addChildElement (xml.release());

        std::cout << root->toString().toStdString() << std::flush;

        setApplicationReturnValue (0);
        quit();
    }

    void shutdown() override
    {
        stopWatchdog();
       #if JUCE_WINDOWS
        if (parentProcessHandle_ != nullptr)
        {
            CloseHandle (parentProcessHandle_);
            parentProcessHandle_ = nullptr;
        }
       #endif
    }

private:
    // Runs on a dedicated thread so it fires even if a misbehaving plugin
    // wedges the message thread inside findAllTypesForFile().
    void startWatchdog()
    {
        watchdogThread_ = std::thread ([this]
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (120);
            std::unique_lock<std::mutex> lock (watchdogMutex_);

            while (! watchdogStop_.load())
            {
                if (watchdogCv_.wait_for (lock, std::chrono::seconds (1),
                                          [this] { return watchdogStop_.load(); }))
                    return;

                if (std::chrono::steady_clock::now() > deadline)
                    std::_Exit (2);

               #if JUCE_MAC || JUCE_LINUX
                // After the parent dies, the kernel reparents us to PID 1
                // (launchd/init). _Exit skips destructors — fine here, the
                // parent is gone and there's no one reading our stdout.
                if (::getppid() == 1)
                    std::_Exit (2);
               #elif JUCE_WINDOWS
                if (parentProcessHandle_ != nullptr
                    && WaitForSingleObject (parentProcessHandle_, 0) == WAIT_OBJECT_0)
                    std::_Exit (2);
               #endif
            }
        });
    }

    void stopWatchdog()
    {
        {
            std::lock_guard<std::mutex> lock (watchdogMutex_);
            watchdogStop_.store (true);
        }
        watchdogCv_.notify_all();

        if (watchdogThread_.joinable())
            watchdogThread_.join();
    }

   #if JUCE_WINDOWS
    static HANDLE openParentProcess()
    {
        const DWORD myPid = GetCurrentProcessId();
        DWORD parentPid = 0;
        HANDLE snap = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);

        if (snap == INVALID_HANDLE_VALUE)
            return nullptr;

        PROCESSENTRY32 pe { sizeof (pe) };

        if (Process32First (snap, &pe))
        {
            do
            {
                if (pe.th32ProcessID == myPid)
                {
                    parentPid = pe.th32ParentProcessID;
                    break;
                }
            } while (Process32Next (snap, &pe));
        }

        CloseHandle (snap);

        if (parentPid == 0)
            return nullptr;

        return OpenProcess (SYNCHRONIZE, FALSE, parentPid);
    }

    HANDLE parentProcessHandle_ { nullptr };
   #endif

    String formatName_;
    String fileOrIdentifier_;
    AudioPluginFormatManager formatManager_;

    std::thread watchdogThread_;
    std::mutex watchdogMutex_;
    std::condition_variable watchdogCv_;
    std::atomic<bool> watchdogStop_ { false };
};

START_JUCE_APPLICATION (PluginScannerApp)
