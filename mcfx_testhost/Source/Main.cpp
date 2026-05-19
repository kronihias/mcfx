/*
  mcfx_testhost — offline CLI test harness for mcfx VST3 plugins.

  Loads a VST3 (or AU) plugin, sets parameters from a JSON file,
  processes a WAV input file, and writes the result to a WAV file.

  Usage:
    mcfx_testhost --plugin path/to/plugin.vst3
                  --params path/to/params.json
                  --input  path/to/in.wav
                  --output path/to/out.wav
                  [--channels 36]
                  [--blocksize 512]
                  [--samplerate 48000]

  params.json:
    {
      "Peak 1 Freq": 0.5,
      "Peak 1 Q":    0.27,
      "Peak 1 Gain": 0.75
    }
  Keys are exact parameter names as returned by AudioProcessorParameter::getName().
  Values are normalized [0, 1] floats.

  Exit codes:
    0 — success
    1 — argument / file error
    2 — plugin load failure
*/

#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void die(const String& msg, int code = 1)
{
    std::cerr << "mcfx_testhost error: " << msg << "\n";
    std::exit(code);
}

static String readFile(const File& f)
{
    if (! f.existsAsFile())
        die("File not found: " + f.getFullPathName());
    return f.loadFileAsString();
}

// ---------------------------------------------------------------------------
// Application
// ---------------------------------------------------------------------------

class TestHostApp final : public JUCEApplicationBase
{
public:
    const String getApplicationName()    override { return "mcfx_testhost"; }
    const String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed()    override { return true; }

    void initialise(const String& /*commandLine*/) override
    {
        // getCommandLineParameterArray() returns a temporary — copy it.
        const StringArray args = getCommandLineParameterArray();

        // ---- Parse arguments -------------------------------------------
        String pluginPath, paramsPath, inputPath, outputPath;
        String loadStatePath, saveStatePath, loadInnerStatePath;
        String describePluginPath;
        int    numChannels = 2;
        int    blockSize   = 512;
        double sampleRate  = 48000.0;
        bool   noOutput    = false;

        for (int i = 0; i < args.size(); ++i)
        {
            const auto& a = args[i];
            if      (a == "--plugin"     && i + 1 < args.size()) pluginPath  = args[++i];
            else if (a == "--params"     && i + 1 < args.size()) paramsPath  = args[++i];
            else if (a == "--input"      && i + 1 < args.size()) inputPath   = args[++i];
            else if (a == "--output"     && i + 1 < args.size()) outputPath  = args[++i];
            else if (a == "--channels"   && i + 1 < args.size()) numChannels = args[++i].getIntValue();
            else if (a == "--blocksize"  && i + 1 < args.size()) blockSize   = args[++i].getIntValue();
            else if (a == "--samplerate" && i + 1 < args.size()) sampleRate  = args[++i].getDoubleValue();
            else if (a == "--load-state"       && i + 1 < args.size()) loadStatePath      = args[++i];
            else if (a == "--save-state"       && i + 1 < args.size()) saveStatePath      = args[++i];
            else if (a == "--load-inner-state" && i + 1 < args.size()) loadInnerStatePath = args[++i];
            else if (a == "--describe-plugin"  && i + 1 < args.size()) describePluginPath = args[++i];
            else if (a == "--no-output")                          noOutput    = true;
        }

        // ---- One-shot mode: print PluginDescription XML and exit -------
        // Used by tests that need to construct a plugin's description blob
        // (e.g. mcfx_anything's CURRENT_PLUGIN child) without re-implementing
        // JUCE's scanner in Python.
        if (describePluginPath.isNotEmpty())
        {
            AudioPluginFormatManager fm;
#if JUCE_PLUGINHOST_VST3
            fm.addFormat(new VST3PluginFormat());
#endif
#if JUCE_PLUGINHOST_AU
            fm.addFormat(new AudioUnitPluginFormat());
#endif
            OwnedArray<PluginDescription> found;
            KnownPluginList kpl;
            for (auto* fmt : fm.getFormats())
            {
                kpl.scanAndAddFile(describePluginPath, false, found, *fmt);
                if (! found.isEmpty())
                    break;
            }
            if (found.isEmpty())
                die("Could not scan plugin: " + describePluginPath, 2);

            std::unique_ptr<XmlElement> descXml(found[0]->createXml());
            std::cout << descXml->toString().toRawUTF8();
            quit();
            return;
        }

        if (pluginPath.isEmpty() || inputPath.isEmpty() || (outputPath.isEmpty() && ! noOutput))
        {
            std::cerr <<
                "Usage: mcfx_testhost --plugin <path.vst3>\n"
                "                     --input  <in.wav>\n"
                "                     --output <out.wav>\n"
                "                     [--params <params.json>]\n"
                "                     [--channels 2]\n"
                "                     [--blocksize 512]\n"
                "                     [--samplerate 48000]\n"
                "                     [--load-state <state.bin>]        (call setStateInformation with raw bytes)\n"
                "                     [--save-state <state.bin>]        (write getStateInformation bytes after processing)\n"
                "                     [--load-inner-state <state.bin>]  (wrap raw bytes in VST3PluginState envelope, then load)\n"
                "                     [--describe-plugin <path.vst3>]   (one-shot: print PluginDescription XML and exit)\n"
                "                     [--no-output]  (skip writing output WAV; for benchmarking)\n";
            std::exit(1);
        }

        // ---- Load plugin ------------------------------------------------
        AudioPluginFormatManager formatManager;
        // addDefaultFormats() is deleted in the headless module — add explicitly.
#if JUCE_PLUGINHOST_VST3
        formatManager.addFormat(new VST3PluginFormat());
#endif
#if JUCE_PLUGINHOST_AU
        formatManager.addFormat(new AudioUnitPluginFormat());
#endif

        OwnedArray<PluginDescription> found;
        KnownPluginList kpl;

        for (auto* fmt : formatManager.getFormats())
        {
            kpl.scanAndAddFile(pluginPath, false, found, *fmt);
            if (! found.isEmpty())
                break;
        }

        if (found.isEmpty())
            die("Could not scan plugin: " + pluginPath, 2);

        String loadErr;
        auto plugin = formatManager.createPluginInstance(
            *found[0], sampleRate, blockSize, loadErr);

        if (plugin == nullptr)
            die("Could not load plugin: " + loadErr, 2);

        // ---- Set bus layout --------------------------------------------
        {
            auto layout = AudioProcessor::BusesLayout();
            layout.inputBuses .add(AudioChannelSet::discreteChannels(numChannels));
            layout.outputBuses.add(AudioChannelSet::discreteChannels(numChannels));

            if (! plugin->checkBusesLayoutSupported(layout))
            {
                std::cerr << "mcfx_testhost: " << numChannels
                          << " channels not supported — using default layout\n";
                numChannels = plugin->getTotalNumInputChannels();
            }
            else
            {
                plugin->setBusesLayout(layout);
            }
        }

        // ---- Pre-initialise sample rate / block size ---------------------
        // Some plugins (e.g. mcfx_convolver) use _SampleRate / _BufferSize
        // inside setStateInformation / LoadConfiguration.  Calling prepareToPlay
        // here ensures those fields are populated before config loading starts,
        // so the subsequent prepareToPlay call (after WAV reading) will see the
        // same values and won't trigger a second async reload.
        plugin->prepareToPlay(sampleRate, blockSize);

        // ---- Load raw state blob if requested --------------------------
        if (loadStatePath.isNotEmpty())
        {
            File stateFile(loadStatePath);
            if (! stateFile.existsAsFile())
                die("State file not found: " + loadStatePath);
            MemoryBlock stateBytes;
            if (! stateFile.loadFileAsData(stateBytes))
                die("Could not read state file: " + loadStatePath);
            plugin->setStateInformation(stateBytes.getData(),
                                        (int) stateBytes.getSize());
            std::cerr << "mcfx_testhost: loaded state (" << stateBytes.getSize()
                      << " bytes) from " << loadStatePath << "\n";
        }

        // ---- Load *inner* state, wrapping in the VST3 envelope ---------
        // The file contains the JUCE-binary form of the plugin's own state
        // (what its setStateInformation expects). VST3PluginInstance::
        // setStateInformation requires the outer VST3PluginState/IComponent
        // wrapper, so we add it here — same pattern as the Config File path.
        if (loadInnerStatePath.isNotEmpty())
        {
            File innerFile(loadInnerStatePath);
            if (! innerFile.existsAsFile())
                die("Inner-state file not found: " + loadInnerStatePath);
            MemoryBlock innerBin;
            if (! innerFile.loadFileAsData(innerBin))
                die("Could not read inner-state file: " + loadInnerStatePath);

            XmlElement outerXml("VST3PluginState");
            outerXml.createNewChildElement("IComponent")
                    ->addTextElement(innerBin.toBase64Encoding());
            MemoryBlock outerBin;
            juce::AudioProcessor::copyXmlToBinary(outerXml, outerBin);
            plugin->setStateInformation(outerBin.getData(),
                                        (int) outerBin.getSize());
            std::cerr << "mcfx_testhost: loaded inner state (" << innerBin.getSize()
                      << " bytes) from " << loadInnerStatePath << "\n";
            // Give async loaders (mcfx_anything deferred plugin load) time.
            Thread::sleep(3000);
        }

        // ---- Apply parameters from JSON --------------------------------
        if (paramsPath.isNotEmpty())
        {
            auto json   = readFile(File(paramsPath));
            auto parsed = JSON::parse(json);

            if (auto* obj = parsed.getDynamicObject())
            {
                // Special case: "Config File" is a string path that sets the
                // plugin state (e.g. mcfx_convolver config file).  We build a
                // minimal MYPLUGINSETTINGS XML block and call setStateInformation
                // so the plugin can locate and load the file.  After that we
                // wait for any background loading thread to finish.
                if (obj->hasProperty("Config File"))
                {
                    const String cfgPath = obj->getProperty("Config File").toString();
                    std::cerr << "mcfx_testhost: loading Config File: " << cfgPath << "\n";
                    File cfgFile(cfgPath);
                    if (cfgFile.existsAsFile())
                    {
                        std::cerr << "mcfx_testhost: Config File found, calling setStateInformation\n";

                        // Build the inner MYPLUGINSETTINGS XML (what the plugin's
                        // setStateInformation expects) and encode it as a JUCE binary.
                        XmlElement innerXml("MYPLUGINSETTINGS");
                        innerXml.setAttribute("presetDir",   cfgFile.getParentDirectory().getFullPathName());
                        innerXml.setAttribute("activePreset", cfgFile.getFileName());
                        innerXml.setAttribute("ConvBufferSize", (int)blockSize);
                        innerXml.setAttribute("MaxPartSize",    8192);
                        innerXml.setAttribute("storeConfigDataInProject", 0);

                        MemoryBlock innerBin;
                        juce::AudioProcessor::copyXmlToBinary(innerXml, innerBin);

                        // JUCE's VST3PluginInstance::setStateInformation expects the data
                        // to decode to a VST3PluginState XML whose IComponent child holds
                        // the component state as a base64-encoded binary blob.
                        XmlElement outerXml("VST3PluginState");
                        outerXml.createNewChildElement("IComponent")
                                ->addTextElement(innerBin.toBase64Encoding());

                        MemoryBlock outerBin;
                        juce::AudioProcessor::copyXmlToBinary(outerXml, outerBin);
                        plugin->setStateInformation(outerBin.getData(), (int)outerBin.getSize());

                        // The convolver loads its IR on a background thread.
                        // The plugin is wrapped by JUCE's VST3PluginInstance so
                        // we can't cast it to Thread*.  Sleep to allow loading.
                        std::cerr << "mcfx_testhost: waiting 3 s for IR loading\n";
                        Thread::sleep(3000);
                    }
                    else
                    {
                        std::cerr << "mcfx_testhost: Config File not found: "
                                  << cfgPath << "\n";
                    }
                }

                // Special case: "Mimoeq Config File" is a JSON file path whose raw
                // bytes are passed directly to setStateInformation (mcfx_mimoeq
                // reads JSON in its setStateInformation, no XML wrapper needed).
                if (obj->hasProperty("Mimoeq Config File"))
                {
                    const String cfgPath = obj->getProperty("Mimoeq Config File").toString();
                    std::cerr << "mcfx_testhost: loading Mimoeq Config File: " << cfgPath << "\n";
                    File cfgFile(cfgPath);
                    if (cfgFile.existsAsFile())
                    {
                        // Read the JSON as raw bytes — mimoeq's setStateInformation
                        // calls String::createStringFromData and then JSON::parse.
                        String jsonContent = cfgFile.loadFileAsString();
                        MemoryBlock innerBin(jsonContent.toRawUTF8(),
                                            jsonContent.getNumBytesAsUTF8());

                        // JUCE's VST3PluginInstance::setStateInformation unwraps the
                        // outer VST3PluginState/IComponent envelope and passes the
                        // decoded bytes to the plugin's setStateInformation.
                        XmlElement outerXml("VST3PluginState");
                        outerXml.createNewChildElement("IComponent")
                                ->addTextElement(innerBin.toBase64Encoding());

                        MemoryBlock outerBin;
                        juce::AudioProcessor::copyXmlToBinary(outerXml, outerBin);
                        plugin->setStateInformation(outerBin.getData(), (int)outerBin.getSize());
                        std::cerr << "mcfx_testhost: Mimoeq Config File loaded OK\n";
                    }
                    else
                    {
                        std::cerr << "mcfx_testhost: Mimoeq Config File not found: "
                                  << cfgPath << "\n";
                    }
                }

                // Use the modern AudioProcessorParameter API (non-deprecated).
                const auto& params = plugin->getParameters();
                for (auto* p : params)
                {
                    const String pname = p->getName(512);
                    if (obj->hasProperty(pname))
                    {
                        const float val = static_cast<float>(
                            static_cast<double>(obj->getProperty(pname)));
                        p->setValue(val);
                    }
                }
            }
        }

        // ---- Read input WAV --------------------------------------------
        AudioFormatManager fmtMgr;
        fmtMgr.registerBasicFormats();

        std::unique_ptr<AudioFormatReader> reader(
            fmtMgr.createReaderFor(File(inputPath)));
        if (reader == nullptr)
            die("Cannot open input WAV: " + inputPath);

        const int64 totalSamples = reader->lengthInSamples;
        AudioBuffer<float> inputBuf(numChannels, (int)totalSamples);
        inputBuf.clear();

        {
            const int readerCh = jmin((int)reader->numChannels, numChannels);
            AudioBuffer<float> tmp((int)reader->numChannels, (int)totalSamples);
            reader->read(&tmp, 0, (int)totalSamples, 0, true, true);
            for (int ch = 0; ch < readerCh; ++ch)
                inputBuf.copyFrom(ch, 0, tmp, ch, 0, (int)totalSamples);
        }
        reader.reset();

        // ---- Prepare and process ---------------------------------------
        plugin->prepareToPlay(sampleRate, blockSize);

        AudioBuffer<float> outputBuf(numChannels, (int)totalSamples);
        outputBuf.clear();

        MidiBuffer midi;
        int pos = 0;

        while (pos < (int)totalSamples)
        {
            const int thisBlock = jmin(blockSize, (int)totalSamples - pos);
            AudioBuffer<float> block(numChannels, thisBlock);

            for (int ch = 0; ch < numChannels; ++ch)
                block.copyFrom(ch, 0, inputBuf, ch, pos, thisBlock);

            midi.clear();
            plugin->processBlock(block, midi);

            for (int ch = 0; ch < numChannels; ++ch)
                outputBuf.copyFrom(ch, pos, block, ch, 0, thisBlock);

            pos += thisBlock;
        }

        plugin->releaseResources();

        // ---- Save raw state blob if requested --------------------------
        if (saveStatePath.isNotEmpty())
        {
            MemoryBlock stateBytes;
            plugin->getStateInformation(stateBytes);
            File outFile(saveStatePath);
            outFile.deleteFile();
            if (! outFile.replaceWithData(stateBytes.getData(), stateBytes.getSize()))
                die("Could not write state file: " + saveStatePath);
            std::cerr << "mcfx_testhost: wrote state (" << stateBytes.getSize()
                      << " bytes) to " << saveStatePath << "\n";
        }

        // ---- Write output WAV -----------------------------------------
        if (noOutput)
        {
            std::cout << "mcfx_testhost: processed " << totalSamples << " samples ("
                      << numChannels << " ch), output discarded (--no-output)\n";
        }
        else
        {
            File outFile(outputPath);
            outFile.deleteFile();

            WavAudioFormat wavFmt;
            // Use the AudioChannelSet overload (non-deprecated).
            std::unique_ptr<AudioFormatWriter> writer(
                wavFmt.createWriterFor(
                    outFile.createOutputStream().release(),
                    sampleRate,
                    AudioChannelSet::discreteChannels(numChannels),
                    32,   // 32-bit float
                    {},
                    0));

            if (writer == nullptr)
                die("Cannot create output WAV: " + outputPath);

            writer->writeFromAudioSampleBuffer(outputBuf, 0, (int)totalSamples);
            writer->flush();
            writer.reset();

            std::cout << "mcfx_testhost: wrote " << totalSamples << " samples ("
                      << numChannels << " ch) to " << outputPath << "\n";
        }

        quit();
    }

    void shutdown()    override {}
    void anotherInstanceStarted(const String&) override {}
    void systemRequestedQuit()  override { quit(); }
    void suspended()   override {}
    void resumed()     override {}
    void unhandledException(const std::exception* e,
                            const String& file, int line) override
    {
        std::cerr << "mcfx_testhost unhandled exception at "
                  << file << ":" << line;
        if (e) std::cerr << ": " << e->what();
        std::cerr << "\n";
        std::exit(1);
    }
};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

START_JUCE_APPLICATION(TestHostApp)
