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

#if JUCE_MODULE_AVAILABLE_juce_audio_processors
 #include <juce_audio_processors/format/juce_AudioPluginFormatManagerHelpers.h>
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

    void shutdown() override {}

private:
    String formatName_;
    String fileOrIdentifier_;
    AudioPluginFormatManager formatManager_;
};

START_JUCE_APPLICATION (PluginScannerApp)
