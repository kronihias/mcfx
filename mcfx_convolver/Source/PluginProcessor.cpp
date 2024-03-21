/*
 ==============================================================================

 This file is part of the mcfx (Multichannel Effects) plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com

 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)

 Details of these licenses can be found at: www.gnu.org/licenses

 mcfx is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"

#ifdef _WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#define Sleep(x) usleep((x)*1000)
#endif

#define CONVPROC_SCHEDULER_PRIORITY 0
#define CONVPROC_SCHEDULER_CLASS SCHED_FIFO
#define THREAD_SYNC_MODE true

#define MAX_PART_SIZE 8192
//==============================================================================
Mcfx_convolverAudioProcessor::Mcfx_convolverAudioProcessor() :
    AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::discreteChannels(NUM_CHANNELS), true)
        .withOutput ("Output", juce::AudioChannelSet::discreteChannels(NUM_CHANNELS), true)
    ),
    Thread("mtx_convolver_master"),
    _readyToSaveConfiguration(false),
    _storeConfigDataInProject(1),
    _min_in_ch(0),
    _min_out_ch(0),
    _num_conv(0),
    _MaxPartSize(MAX_PART_SIZE),
    _isProcessing(false),
    _configLoaded(false),
    _paramReload(false),
    _skippedCycles(0),
    safemode_(false),
    _osc_in_port(7200),
    _osc_in(false),
    valueTreeState(*this, nullptr, "PARAMETERS", createParameters())
{

    _master_gain_raw = valueTreeState.getRawParameterValue ("MASTERGAIN"); // get raw parameter value for master gain
    valueTreeState.addParameterListener("RELOAD", this);                    

    _SampleRate = getSampleRate();
    _BufferSize = getBlockSize();
    _ConvBufferSize = getBlockSize();

    presetDir = presetDir.getSpecialLocation(File::userApplicationDataDirectory).getChildFile("mcfx/convolver_presets");
    std::cout << "Search dir:" << presetDir.getFullPathName() << std::endl;

	String debug;
    debug << "Search dir: " << presetDir.getFullPathName();

    DebugPrint(debug);

    SearchPresets(presetDir);


    // this is for the open dialog of the gui
    lastDir = lastDir.getSpecialLocation(File::userHomeDirectory);


}

Mcfx_convolverAudioProcessor::~Mcfx_convolverAudioProcessor()
{
#ifdef USE_ZITA_CONVOLVER
    zita_conv.stop_process();
    zita_conv.cleanup();
#else
    mtxconv_.StopProc();
    mtxconv_.Cleanup();
#endif

    DeleteTemporaryFiles();
}

//==============================================================================
const String Mcfx_convolverAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

/* deprecated host passthrough parameters

int Mcfx_convolverAudioProcessor::getNumParameters()
{
    return 1;
}

float Mcfx_convolverAudioProcessor::getParameter (int index)
{
    return (float)_paramReload;
}

void Mcfx_convolverAudioProcessor::setParameter (int index, float newValue)
{
    bool newParamReload = (newValue <= 0.5f) ? false : true;
    if (newParamReload && !_paramReload)
        ReloadConfiguration();

    _paramReload = newParamReload;
}

const String Mcfx_convolverAudioProcessor::getParameterName (int index)
{
    return "ReloadConfig";
}

const String Mcfx_convolverAudioProcessor::getParameterText (int index)
{
    if (_paramReload)
        return "Reload";
    else
        return "";
}*/

const String Mcfx_convolverAudioProcessor::getInputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

const String Mcfx_convolverAudioProcessor::getOutputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

bool Mcfx_convolverAudioProcessor::isInputChannelStereoPair (int index) const
{
    return true;
}

bool Mcfx_convolverAudioProcessor::isOutputChannelStereoPair (int index) const
{
    return true;
}

bool Mcfx_convolverAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool Mcfx_convolverAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool Mcfx_convolverAudioProcessor::silenceInProducesSilenceOut() const
{
    return false;
}

double Mcfx_convolverAudioProcessor::getTailLengthSeconds() const
{
    if (_configLoaded)
    {
        // TODO: return length...
        return 0.0;
    } else {
        return 0.0;
    }
}

int Mcfx_convolverAudioProcessor::getNumPrograms()
{
    return 0;
}

int Mcfx_convolverAudioProcessor::getCurrentProgram()
{
    return 0;
}

void Mcfx_convolverAudioProcessor::setCurrentProgram (int index)
{
}

const String Mcfx_convolverAudioProcessor::getProgramName (int index)
{
    return String();
}

void Mcfx_convolverAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void Mcfx_convolverAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{

    if (_SampleRate != sampleRate || _BufferSize != samplesPerBlock)
    {
        _SampleRate = sampleRate;
        _BufferSize = samplesPerBlock;

        ReloadConfiguration();
    }

    if (_configLoaded)
    {
        // mtxconv_.Reset();
    }

    _previous_master_gain = *_master_gain_raw; // update previous master gain for interpolation
}

void Mcfx_convolverAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool Mcfx_convolverAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return ((layouts.getMainOutputChannelSet().size() == NUM_CHANNELS) &&
            (layouts.getMainInputChannelSet().size() == NUM_CHANNELS));
}

void Mcfx_convolverAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{

    // std::cout << "in: " << getTotalNumInputChannels() << " out: " << getTotalNumOutputChannels() << std::endl;

    if (_configLoaded)
    {

        _isProcessing = true;

        int NumSamples = buffer.getNumSamples();

#ifdef USE_ZITA_CONVOLVER

        for (int i=0; i < jmin(conv_data.getNumInputChannels(), getTotalNumInputChannels()) ; i++)
        {
            float* indata = zita_conv.inpdata(i);
            memcpy(indata, buffer.getReadPointer(i), NumSamples*sizeof(float));
        }

            zita_conv.process(THREAD_SYNC_MODE);

        for (int i=0; i < jmin(conv_data.getNumOutputChannels(), getNumOutputChannels()) ; i++)
        {
            float* outdata = zita_conv.outdata(i)+_ConvBufferPos;
            memcpy(buffer.getWritePointer(i), outdata, NumSamples*sizeof(float));
        }

#else
        //mtxconv_.processBlock(buffer, buffer, isNonRealtime()); // if isNotRealtime always set to true!
        mtxconv_.processBlock(buffer, buffer, NumSamples, true); // try to always wait except - add a special flag to deactivate waiting...

        _skippedCycles.set(mtxconv_.getSkipCount());
#endif

        float currentGain = *_master_gain_raw;
        if (currentGain == _previous_master_gain) // If gain was not changed, apply gain directly to the entire buffer
            buffer.applyGain (juce::Decibels::decibelsToGain(currentGain));
        else    // If gain was changed, apply gain ramp interpolation
        {
            buffer.applyGainRamp (0, buffer.getNumSamples(),  juce::Decibels::decibelsToGain(_previous_master_gain), juce::Decibels::decibelsToGain(currentGain));
            _previous_master_gain = currentGain;
        }

        _isProcessing = false;

    } else { // config loaded

        // clear output in case no config is loaded!
        buffer.clear();
    }


}

void Mcfx_convolverAudioProcessor::run()
{
    LoadConfiguration(_desConfigFile);
    valueTreeState.getParameter("RELOAD")->setValueNotifyingHost(false); // "Recoil" action on the reload parameter, so that it goes back to 0 after the configuration is loaded, and it can then be used again to reload the configuration.
}

void Mcfx_convolverAudioProcessor::LoadConfigurationAsync(File configFile)
{
    DebugPrint("Loading preset...");
    _desConfigFile = configFile;
    startThread(Thread::Priority::normal);
}

void Mcfx_convolverAudioProcessor::ReloadConfiguration()
{
    if (_configLoaded || activePreset.isNotEmpty())
        LoadConfigurationAsync(_configFile);
}

void getIntFromLine(int &ret, String &line)
{
    if (line.isEmpty())
        return;
    ret = line.getIntValue();
    line = line.fromFirstOccurrenceOf(" ", false, true).trim();
}

void getFloatFromLine(float &ret, String &line)
{
    if (line.isEmpty())
        return;
    ret = line.getFloatValue();
    line = line.fromFirstOccurrenceOf(" ", false, true).trim();
}

int Mcfx_convolverAudioProcessor::getWavInputChannelMetadata(File wavFile)
{
    // check audio file, maybe there is Info metadata specifying the number of input channels (x-mcfx does this, MATLAB can write it as well)
    AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    AudioFormatReader* reader = formatManager.createReaderFor(wavFile);
    // get metadata
    String metaTagValue =  reader->metadataValues.getValue(WavAudioFormat::riffInfoKeywords, "0");

    return metaTagValue.getIntValue();
}

void Mcfx_convolverAudioProcessor::LoadWavFile(File wavFile, int numPassedInputChannels)
{
    if (wavFile.getFileExtension() != ".wav") {
        DebugPrint("Error: not a .wav file");
        return;
    }

    this->presetWavFilename = wavFile.getFullPathName();
    // conf file should be located in the same folder
    File confFile(wavFile.getParentDirectory().getChildFile(wavFile.getFileNameWithoutExtension() + ".conf"));
    // check for existing .conf file and try to load if we don't have a specific numPassedInputChannels
    if (confFile.existsAsFile() && !numPassedInputChannels) {
        LoadConfigurationAsync(confFile);
        return;
    }

    // use either the metadata or the passed value
    int inChannels = numPassedInputChannels ? numPassedInputChannels : getWavInputChannelMetadata(wavFile);

    // confText header
    String confText = "# mcfx_convolver configuration\n";
    confText << "# ------------------------\n";
    confText << "/cd \n";
    confText << "#\n#\n#\n";

    if (!confFile.hasWriteAccess()) {
        DebugPrint("Error: can not write config file");
        return;
    }

    if (inChannels == 0) {
        DebugPrint("Error: Invalid input channel count!");
        return;
    }

    if (inChannels > 0) {
        // dense matrix

        confText << "#               #inchannels  gain  delay  offset  length        filename\n";
        confText << "# ------------------------------------------------------------------------------\n#\n";
        confText << "/impulse/packedmatrix " << String(inChannels) << " 1.0 0 0 0 " << wavFile.getFileName() << "\n";

    } else if (inChannels == -1) {
        // diagonal matrix

        confText << "#/impulse/read   <input> <output> <gain> <delay> <offset> <length> <channel> <file>\n";

        AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        AudioFormatReader* reader = formatManager.createReaderFor(wavFile);
        int numChannels = reader->getChannelLayout().size();

        for (int i = 1; i <= numChannels; i++) {
            confText << "/impulse/read " << i << " " << i << " 1.0 0 0 0 " << i << " " << wavFile.getFileName() << "\n";
        }

    }

    confFile.replaceWithText(confText);

    LoadConfigurationAsync(confFile);
}

void Mcfx_convolverAudioProcessor::LoadConfiguration(File configFile)
{
    if (!configFile.existsAsFile())
    {

        String debug;
        debug << "Configuration file does not exist:\n" << configFile.getFullPathName();

        //std::cout << debug << std::endl;
        DebugPrint(debug);

        return;
    }

    // unload first....
    if (_configLoaded) {

        while (_isProcessing) {
            Sleep(1);
        }

        std::cout << "Unloading Config..." << std::endl;
        UnloadConfiguration();
        // DebugClear(); // clear debug window
        DebugPrint("Config Unloaded...");
        std::cout << "Config Unloaded..." << std::endl;
    }

    if (_ConvBufferSize < _BufferSize)
        _ConvBufferSize = _BufferSize;

    _ConvBufferSize = nextPowerOfTwo(_ConvBufferSize);

    String debug;
    debug << "trying to load " << configFile.getFullPathName();

    DebugPrint(debug);

    // debug print samplerate and buffer size
    debug = "Samplerate: ";
    debug << _SampleRate;
    debug << " Host Buffer Size: ";
    debug << (int)_BufferSize;
    debug << " Internal Buffer Size: ";
    debug << (int)_ConvBufferSize;
    DebugPrint(debug);

    activePreset = configFile.getFileName(); // store filename only, on restart search preset folder for it!
    box_preset_str = configFile.getFileNameWithoutExtension();

    Array<File> configFileAndDataFiles;
    configFileAndDataFiles.add(configFile);

    StringArray myLines;

    configFile.readLines(myLines);

    // global settings

    String directory("");

    AudioBuffer<float> TempAudioBuffer(1,256);

    conv_data.setSampleRate(_SampleRate);

    // iterate over all lines
    for (int currentLine = 0; currentLine < myLines.size(); currentLine++)
    {
        // get the line and remove spaces from start and end
        String line (myLines[currentLine].trim());

        if (line.startsWith("#"))
        {

            // ignore these lines

        } else if (line.contains("/cd")) {

            line = line.trimCharactersAtStart("/cd").trim();
            directory = line;

            std::cout << "Dir: " << directory << std::endl;

        } else if (line.contains("/convolver/new")) {
            int t_in_ch = 0;
            int t_out_ch = 0;

            line = line.trimCharactersAtStart("/convolver/new").trim();

            getIntFromLine(t_in_ch, line);
            getIntFromLine(t_out_ch, line);

        } else if (line.contains("/impulse/read"))
        {

            if (threadShouldExit())
                return;

            int in_ch = 0;
            int out_ch = 0;
            float gain = 1.f;
            int delay = 0;
            int offset = 0;
            int length = 0;
            int channel = 0;
            String filename;

            line = line.trimCharactersAtStart("/impulse/read").trim();

            getIntFromLine(in_ch, line);
            getIntFromLine(out_ch, line);
            getFloatFromLine(gain, line);
            getIntFromLine(delay, line);
            getIntFromLine(offset, line);
            getIntFromLine(length, line);
            getIntFromLine(channel, line);

            if (line.length() > 0) // the rest is filename
            {
                filename = line;
            }
            // printf("load ir: %i %i %f %i %i %i %i %s \n", in_ch, out_ch, gain, delay, offset, length, channel, filename);

            File IrFilename;


            // check if /cd is defined in config
            if (directory.isEmpty()) {
                IrFilename = configFile.getParentDirectory().getChildFile(filename);

            } else { // /cd is defined
                if (File::isAbsolutePath(directory))
                {
                    // absolute path is defined
                    File path(directory);

                    IrFilename = path.getChildFile(filename);
                } else {

                    // relative path to the config file is defined
                    IrFilename = configFile.getParentDirectory().getChildFile(directory).getChildFile(filename);
                }
            }

            configFileAndDataFiles.addIfNotAlreadyThere(IrFilename);

            if ( ( in_ch < 1 ) || ( in_ch > NUM_CHANNELS ) || ( out_ch < 1 ) || ( out_ch > NUM_CHANNELS ) )
            {

                this->_presetLoadState = PresetLoadState::Failed;
                this->_presetLoadErrorMessage = PresetLoadError::CHANNEL_ASSIGNMENT_NOT_FEASIBLE;
                String debug;
                debug << "ERROR: channel assignment not feasible: In: " << in_ch << " Out: " << out_ch;
                DebugPrint(debug);


            } else {

                double src_samplerate;
                if (loadIr(&TempAudioBuffer, IrFilename, channel-1, src_samplerate, gain, offset, length))
                {
                    // std::cout << "Length: " <<  TempAudioBuffer.getNumSamples() << " Channels: " << TempAudioBuffer.getNumChannels() << " MaxLevel: " << TempAudioBuffer.getRMSLevel(0, 0, 2048) << std::endl;

                    // add IR to my convolution data - offset and length are already done while reading file, samplerate conversion is done during addIR
                    conv_data.addIR(in_ch-1, out_ch-1, 0, delay, 0, TempAudioBuffer, 0, src_samplerate);

                    String debug;
                    debug << "conv # " << conv_data.getNumIRs() << " " << IrFilename.getFullPathName() << " (" << TempAudioBuffer.getNumSamples() << " smpls) loaded";
                    if (src_samplerate != _SampleRate)
                      debug << " (resampled to " << conv_data.getLength(conv_data.getNumIRs()-1) <<  " smpls)";
                    DebugPrint(debug);
                    this->_presetLoadState = PresetLoadState::Loaded;

                } else {
                    this->_presetLoadState = PresetLoadState::Failed;
                    this->_presetLoadErrorMessage = PresetLoadError::NOT_LOADED;
                    String debug;
                    debug << "ERROR: not loaded: " << IrFilename.getFullPathName();
                    DebugPrint(debug);

                }


            } // end check channel assignment


        } // end "/impulse/read" line


        /*
        packed matrix in one .wav file as used in X-Volver by Angelo Farina

        one channel in the .wav file represents one output channel of the convolver matrix
        in one channel there are sequentially aligned input IRs

        the length of one impulse response is (length .wav) / #in_channels

        ir_#in_#out

        wav ch #1:  ir_1_1  ir_2_1  ir_3_1  ... ir_m_1
        wav ch #2:  ir_1_2  ir_2_2  ir_3_2  ... ir_m_2
        ...
        wav ch #3:  ir_1_n  ir_2_n  ir_3_n  ... ir_m_n


        by defining the number of input channels the plugin can recognize the length of each impulse response
        */
        else if (line.contains("/impulse/packedmatrix"))
        {
            int inchannels = 0;
            float gain = 1.f;
            int delay = 0;
            int offset = 0;
            int length = 0;
            String filename("");

            line = line.trimCharactersAtStart("/impulse/packedmatrix").trim();

            getIntFromLine(inchannels, line);
            getFloatFromLine(gain, line);
            getIntFromLine(delay, line);
            getIntFromLine(offset, line);
            getIntFromLine(length, line);

            if (line.length() > 0)
                filename = line;


            File IrFilename;

            // check if /cd is defined in config
            if (directory.isEmpty()) {
                IrFilename = configFile.getParentDirectory().getChildFile(filename);

            } else { // /cd is defined
                if (File::isAbsolutePath(directory))
                {
                    // absolute path is defined
                    File path(directory);

                    IrFilename = path.getChildFile(filename);
                } else {

                    // relative path to the config file is defined
                    IrFilename = configFile.getParentDirectory().getChildFile(directory).getChildFile(filename);
                }
            }

            configFileAndDataFiles.addIfNotAlreadyThere(IrFilename);

            if (inchannels < 1)
            {
                this->_presetLoadState = PresetLoadState::Failed;
                this->_presetLoadErrorMessage = PresetLoadError::NUMBER_INPUT_CHANNELS_NOT_FEASIBLE;
                String debug;
                debug << "ERROR: Number of input channels not feasible: " << inchannels;
                DebugPrint(debug);

                return;
            }


            double src_samplerate;
            if (loadIr(&TempAudioBuffer, IrFilename, -1, src_samplerate, gain, 0, 0)) // offset/length has to be done while processing individual irs
            {


                int numOutChannels = TempAudioBuffer.getNumChannels();
                int irLength = TempAudioBuffer.getNumSamples()/inchannels;

                if (irLength*inchannels != TempAudioBuffer.getNumSamples())
                {
                    String debug;
                    debug << "ERROR: length of wav file is not multiple of irLength*numinchannels!" << IrFilename.getFullPathName();
                    this->_presetLoadState = PresetLoadState::Failed;
                    this->_presetLoadErrorMessage = PresetLoadError::WAV_LENGTH_NOT_MULTIPLE_INCHANNELS_IRLEN;
                    DebugPrint(debug);

                    return;
                }

                if ((inchannels > getTotalNumInputChannels()) || numOutChannels > getTotalNumOutputChannels())
                {
                    String debug;
                    debug << "Input/Output channel assignement not feasible. Not enough input/output channels of the plugin. Need Input: " << inchannels << " Output: " << numOutChannels;
                    DebugPrint(debug);

                    return;
                }

                if (length <= 0)
                    length = irLength-offset;

                // std::cout << "TotalLength: " <<  TempAudioBuffer.getNumSamples() << " IRLength: " <<  irLength << " Used Length: " << length << " Channels: " << TempAudioBuffer.getNumChannels() << std::endl;


                for (int i=0; i < numOutChannels; i++) {
                    for (int j=0; j < inchannels; j++) {
                        // add IR to my convolution data - offset and length are already done while reading file
                        int individualOffset = j*irLength;

                        // std::cout << "AddIR: IN: " << j << " OUT: " << i << " individualOffset: " << individualOffset << std::endl;

                        // CHECK IF LENGTH/OFFSET E.G. IS OK!!
                        conv_data.addIR(j, i, individualOffset + offset, delay, length, TempAudioBuffer, i, src_samplerate);

                    }
                }

                String debug;
                debug << "loaded " << conv_data.getNumIRs() << " filters with: length " << length << ", input channels: " << inchannels << ", output channels: " << numOutChannels << ", filename: " << IrFilename.getFileName();
                DebugPrint(debug);
                this->_presetLoadState = PresetLoadState::Loaded;

            } else {
                this->_presetLoadState = PresetLoadState::Failed;
                this->_presetLoadErrorMessage = PresetLoadError::NOT_LOADED;
                String debug;
                debug << "ERROR: not loaded: " << IrFilename.getFullPathName();
                DebugPrint(debug);

                return;
            }

        } // end "/impulse/packedmatrix" line


    } // iterate over lines

    // initiate convolution

#ifdef USE_ZITA_CONVOLVER
    int err=0;

    unsigned int   options = 0;

    options |= Convproc::OPT_FFTW_MEASURE;
    options |= Convproc::OPT_VECTOR_MODE;

    zita_conv.set_options (options);
    zita_conv.set_density(0.5);

    printf("max length: %lli \n", conv_data.getMaxLength());

    err = zita_conv.configure(conv_data.getNumInputChannels(), conv_data.getNumOutputChannels(), (unsigned int)conv_data.getMaxLength(), _BufferSize, _ConvBufferSize, Convproc::MAXPART);

    for (int i=0; i < conv_data.getNumIRs(); i++)
    {

        err = zita_conv.impdata_create(conv_data.getInCh(i), conv_data.getOutCh(i), 1, (float *)conv_data.getIR(i)->getReadPointer(0), 0, (unsigned int)conv_data.getLength(i));

    }

    zita_conv.print();
    zita_conv.start_process(CONVPROC_SCHEDULER_PRIORITY, CONVPROC_SCHEDULER_CLASS);

#else
    _MaxPartSize = jmin(MAX_PART_SIZE, nextPowerOfTwo(_MaxPartSize));

    // try autodetecting host and deciding whether we need safemode (to avoid having to add another user parameter - let's see how this works for testers)
    PluginHostType me;
    safemode_ = me.isAdobeAudition() || me.isPremiere() || me.isSteinberg(); // probably an incomplete list

    mtxconv_.Configure(conv_data.getNumInputChannels(), conv_data.getNumOutputChannels(), _BufferSize, conv_data.getMaxLength(), _ConvBufferSize, _MaxPartSize, safemode_);

    for (int i=0; i < conv_data.getNumIRs(); i++)
    {
        if (threadShouldExit())
            return;

        mtxconv_.AddFilter(conv_data.getInCh(i), conv_data.getOutCh(i), *conv_data.getIR(i));
        // no delay and length yet!

    }

    mtxconv_.StartProc();

#endif

    _configLoaded = true;

    if (safemode_)
        setLatencySamples(_ConvBufferSize);
    else
        setLatencySamples(_ConvBufferSize-_BufferSize);

    _skippedCycles.set(0);

    _min_in_ch = conv_data.getNumInputChannels();
    _min_out_ch = conv_data.getNumOutputChannels();
    _num_conv = conv_data.getNumIRs();

    _configFile = configFile;


    this->_presetLoadState = PresetLoadState::Loaded;
    DebugPrint("Configuration loaded, maximum filter length: " + String(conv_data.getMaxLengthInSeconds(), 2) + "[s], " + String(conv_data.getMaxLength()) + " [smpls]");
    DebugPrint("Plugin Latency: " + String(getLatencySamples()) + " [smpls]");
    sendChangeMessage(); // notify editor


    /* save preset files as temporary .zip file, save this zip file later in the chunk if user wants to store preset within project */

    _tempConfigZipFile = _tempConfigZipFile.createTempFile(".zip");

    _cleanUpFilesOnExit.add(_tempConfigZipFile); // delete the file when we exit the plugin

    ZipFile::Builder compressedConfigFileAndDataFiles;
    for (int i = 0; i < configFileAndDataFiles.size(); i++)
    {
      String storedPath = ""; // add relative path
      if (i > 0)
        storedPath = configFileAndDataFiles.getUnchecked(i).getRelativePathFrom(configFileAndDataFiles.getUnchecked(0));

        compressedConfigFileAndDataFiles.addFile(configFileAndDataFiles.getUnchecked(i), 5, storedPath);
    }

    FileOutputStream outputStream(_tempConfigZipFile);
    if (outputStream.openedOk())
    {
      outputStream.setPosition(0); // overwrite file if already exists
      outputStream.truncate();

      double progress = 0.;
      compressedConfigFileAndDataFiles.writeToStream(outputStream, &progress);

      _readyToSaveConfiguration.set(true);
      /*
          String debugText = "Configuration File and Data Stored to: ";
          debugText << _tempConfigZipFile.getFullPathName() << "\n";
          DebugPrint(debugText);
      */
    }

    setParameterNotifyingHost(0, getParameter(0)); // this is a hack to make some hosts save the plugin state!
    sendChangeMessage(); // notify editor again

    // ====================================================================================================

    // if presetWavFilename is not empty we loaded this preset from a .wav file
    // If the user also manually entered the in channel number and asked to store the number in the wavfile metadata, we do it here
    if (storeNumInputsIntoWav.get() && numInputChannels > 0 && presetWavFilename.isNotEmpty() && _presetLoadState != PresetLoadState::Failed)
    {
        DebugPrint("Storing number of input channels ("+String(numInputChannels)+") into the wavefile ("+presetWavFilename+") metadata...");

        // We: (1) read the wav file, (2) write the number of input channels into the metadata, (3) write the wav file again, same filepath, overwriting the original
                
        AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        AudioFormatReader* reader = nullptr;
        WavAudioFormat* wave = nullptr;
        AudioFormatWriter* writer = nullptr;
        try
        {
            File wavFile = File(presetWavFilename);
            reader = formatManager.createReaderFor(wavFile);
            
            AudioSampleBuffer ReadBuffer(reader->numChannels, reader->lengthInSamples); // create buffer
            reader->read(&ReadBuffer, 0, reader->lengthInSamples, 0, true, true);            

            reader->metadataValues.set(WavAudioFormat::riffInfoKeywords, (String)this->numInputChannels);
            FileOutputStream* outStream = new FileOutputStream(wavFile);
            if (outStream->openedOk())
            {
                outStream->setPosition (0);
                outStream->truncate();

            }
            wave = new WavAudioFormat();
            writer = wave->createWriterFor(outStream,
                                           reader->sampleRate,
                                           reader->getChannelLayout(),
                                           reader->bitsPerSample,
                                           reader->metadataValues,
                                           0
                                           );
            delete wave; wave = nullptr;
            writer->writeFromAudioSampleBuffer(ReadBuffer, 0, ReadBuffer.getNumSamples());
            delete writer; writer = nullptr;
            delete reader; reader = nullptr;

            DebugPrint("Successfully stored number of input channels into the wavefile metadata ("+String(wavFile.getFullPathName())+").");
            storeNumInputsIntoWav = false; // reset the flag
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            DebugPrint("Error: "+String(e.what()));
            if (reader) delete reader;
            if (writer) delete writer;
            if (wave)   delete wave;
        }
    }
}

bool Mcfx_convolverAudioProcessor::loadIr(AudioSampleBuffer* IRBuffer, const File& audioFile, int channel, double &samplerate, float gain, int offset, int length)
{
    if (!audioFile.existsAsFile())
    {
        this->_presetLoadState = PresetLoadState::Failed;
        this->_presetLoadErrorMessage = PresetLoadError::FILE_DOES_NOT_EXIST;
        String debug;
        debug << "ERROR: file does not exist!";
        DebugPrint(debug);
        return false;
    }

    AudioFormatManager formatManager;

    // this can read .wav and .aiff
    formatManager.registerBasicFormats();

    AudioFormatReader* reader = formatManager.createReaderFor(audioFile);

    if (!reader) {
        this->_presetLoadState = PresetLoadState::Failed;
        this->_presetLoadErrorMessage = PresetLoadError::CANT_READ_IR;
        String debug;
        debug << "ERROR: could not read impulse response file!";
        DebugPrint(debug);
        return false;
    }

	if (offset < 0)
        offset = 0;

    int64 wav_length = (int)reader->lengthInSamples;

    if (length <= 0)
        length = wav_length - offset;

    if (wav_length <= 0) {
        this->_presetLoadState = PresetLoadState::Failed;
        this->_presetLoadErrorMessage = PresetLoadError::ZERO_SAMPLES;
        String debug;
        debug << "ERROR: zero samples in impulse response file!";
        DebugPrint(debug);
        return false;
    }

    if (wav_length-offset < length) {
        length = wav_length-offset;

        String debug;
        debug << "Warning: not enough samples in impulse response file, reading maximum number of samples: " << String(length);
        DebugPrint(debug);
    }

    if ((int)reader->numChannels <= channel) {
        this->_presetLoadState = PresetLoadState::Failed;        
        this->_presetLoadErrorMessage = PresetLoadError::NOT_ENOUGH_CHANNELS;
        String debug;
        debug << "ERROR: wav file doesn't have enough channels: " << String(reader->numChannels);
        DebugPrint(debug);
        return false;
    }


    AudioSampleBuffer ReadBuffer(reader->numChannels, length); // create buffer


    reader->read(&ReadBuffer, 0, length, offset, true, true);

    // set the samplerate -> maybe we have to resample later...
    samplerate = reader->sampleRate;

    // copy the wanted channel into our IR Buffer

    if (channel >= 0) // only one channel wanted...
    {
        IRBuffer->setSize(1, length);
        IRBuffer->copyFrom(0, 0, ReadBuffer, channel, 0, length);
    }
    else // copy all channels from the wav file if channel == -1 -> used for packedmatrix
    {
        IRBuffer->setSize(reader->numChannels, length);

        for (unsigned int i=0; i<reader->numChannels; i++) {
            IRBuffer->copyFrom(i, 0, ReadBuffer, i, 0, length);
        }
    }


    // scale ir with gain
    if (gain != 1.f)
        IRBuffer->applyGain(gain);

    // std::cout << "ReadRMS: " << IRBuffer->getRMSLevel(0, 0, ir_length) << std::endl;

    // #define RESET_METADATA_INCHANNELS // Enable this to reset the metadata in the input channels of the wav file (useful for testing, but not for production)
    #ifdef RESET_METADATA_INCHANNELS
        reader->metadataValues.set(WavAudioFormat::riffInfoKeywords, "");
        FileOutputStream* outStream = new FileOutputStream(audioFile);
        if (outStream->openedOk())
        {
            outStream->setPosition (0);
            outStream->truncate();
        }
        WavAudioFormat* wave = new WavAudioFormat();
        AudioFormatWriter* writer = wave->createWriterFor(outStream,
                                                            reader->sampleRate,
                                                            reader->getChannelLayout(),
                                                            reader->bitsPerSample,
                                                            reader->metadataValues,
                                                            0
                                                            );
        delete wave;
        writer->writeFromAudioSampleBuffer(ReadBuffer, 0, ReadBuffer.getNumSamples());
        delete writer;
        DebugPrint("-------------> Metadata resetted");
    #endif

    delete reader;
    return true;
}

unsigned int Mcfx_convolverAudioProcessor::getBufferSize()
{
    return _BufferSize;
}

unsigned int Mcfx_convolverAudioProcessor::getConvBufferSize()
{
    return _ConvBufferSize;
}

unsigned int Mcfx_convolverAudioProcessor::getMaxPartitionSize()
{
  return _MaxPartSize;
}

void Mcfx_convolverAudioProcessor::setConvBufferSize(unsigned int bufsize)
{
    if (nextPowerOfTwo(bufsize) != _ConvBufferSize)
    {
        _ConvBufferSize = nextPowerOfTwo(bufsize);
        ReloadConfiguration();
    }
}

void Mcfx_convolverAudioProcessor::setMaxPartitionSize(unsigned int maxsize)
{
    if (maxsize > MAX_PART_SIZE)
      return;

    if (nextPowerOfTwo(maxsize) != _MaxPartSize)
    {
      _MaxPartSize = nextPowerOfTwo(maxsize);
      ReloadConfiguration();
    }
}

void Mcfx_convolverAudioProcessor::setOscIn(bool arg)
{
    
String debug;
debug << "OSC receiver " << (arg?"enabled":"disabled");
DebugPrint(debug);

  if (arg) {

    if (oscReceiver.connect(_osc_in_port))
    {
      oscReceiver.addListener(this, "/reload");
      oscReceiver.addListener(this, "/load");
      _osc_in = true;
    }
    else
    {
      DebugPrint("Could not open UDP port, try a different port.");
    }


  } else { // turn off osc

    _osc_in = false;

    oscReceiver.removeListener(this);

    oscReceiver.disconnect();

  }
}

bool Mcfx_convolverAudioProcessor::getOscIn()
{
  return _osc_in;
}

void Mcfx_convolverAudioProcessor::setOscInPort(int port)
{

    String debug;
    debug << "Set OSCreceiver port to " << juce::String(port);
    DebugPrint(debug);

  _osc_in_port = port;

  if (_osc_in)
  {
    setOscIn(false);
    setOscIn(true);
  }
}

int Mcfx_convolverAudioProcessor::getOscInPort()
{
  return _osc_in_port;
}

void Mcfx_convolverAudioProcessor::oscMessageReceived(const OSCMessage & message)
{
  if (message.getAddressPattern() == "/reload")
  {
    DebugPrint("Received Reload via OSC (/reload).");
    ReloadConfiguration();
  }
  else if (message.getAddressPattern() == "/load")
  {

    DebugPrint("Received Load via OSC (/load).");

    if (message.size() < 1)
      return;

    if (message[0].isString())
      LoadPresetByName(message[0].getString());
  }
  else {
    DebugPrint("Received unknown OSC message");
  }
}

void Mcfx_convolverAudioProcessor::UnloadConfiguration()
{
    // delete configuration
    _configLoaded = false;

    _conv_in.clear();
    _conv_out.clear();

    _min_in_ch = 0;
    _min_out_ch = 0;

#ifdef USE_ZITA_CONVOLVER

    zita_conv.stop_process();
    zita_conv.cleanup();

#else

    mtxconv_.StopProc();
    mtxconv_.Cleanup();

#endif

    conv_data.clear();

    std::cout << "Unloaded Convolution..." << std::endl;

}

void Mcfx_convolverAudioProcessor::DeleteTemporaryFiles()
{
    _readyToSaveConfiguration.set(false);

    for (int i = 0; i < _cleanUpFilesOnExit.size(); i++)
    {
        _cleanUpFilesOnExit.getUnchecked(i).deleteRecursively();
    }
    _cleanUpFilesOnExit.clear();
}

/**
 * @brief Clear the debug text
 * Clear the debug text that is displayed in the Debug window
 */
void Mcfx_convolverAudioProcessor::DebugClear()
{
    ScopedLock lock(_DebugTextMutex);
    String temp;
    temp << "";

    _DebugText = temp;
}


/**
 * @brief Print a string to the debug window and add a newline
 * @param debugText The string to print to the debug window
 */
void Mcfx_convolverAudioProcessor::DebugPrint(const String& debugText)
{
    
    ScopedLock lock(_DebugTextMutex);
    String temp;

    juce::String currentTime = Time::getCurrentTime().formatted("%H:%M:%S"); // Log current time for better debugging

    temp << currentTime << " | "<< debugText << "\n";
    temp << _DebugText;

    // std::cout << debugText << std::endl;

    _DebugText = temp;
}

bool Mcfx_convolverAudioProcessor::SaveConfiguration(File zipFile)
{
    if (_readyToSaveConfiguration.get())
        return _tempConfigZipFile.copyFileTo(zipFile);
    else
        return false;
}

String Mcfx_convolverAudioProcessor::getDebugString()
{
  ScopedLock lock(_DebugTextMutex);

  return _DebugText;
}

void Mcfx_convolverAudioProcessor::SearchPresets(File SearchFolder)
{
    _presetFiles.clear();

    SearchFolder.findChildFiles(_presetFiles, File::findFiles, true, "*.conf");
    _presetFiles.sort();
    std::cout << "Found preset files: " << _presetFiles.size() << std::endl;

}

void Mcfx_convolverAudioProcessor::LoadPreset(unsigned int preset)
{
    if (preset < (unsigned int)_presetFiles.size())
    {
        DeleteTemporaryFiles();
        LoadConfigurationAsync(_presetFiles.getUnchecked(preset));
        box_preset_str = _presetFiles.getUnchecked(preset).getFileNameWithoutExtension();
    }
}

void Mcfx_convolverAudioProcessor::LoadPresetByName(String presetName)
{
    Array <File> files;
    presetDir.findChildFiles(files, File::findFiles, true, presetName);

    if (files.size())
    {
        DeleteTemporaryFiles();
        LoadConfigurationAsync(files.getUnchecked(0)); // Load first result
        box_preset_str = files.getUnchecked(0).getFileNameWithoutExtension();
    }
    else
    { // preset not found -> post!
        String debug_msg;
        debug_msg << "ERROR loading preset: " << presetName << ", Preset not found in search folder!";
        DebugPrint(debug_msg);
    }

}

//==============================================================================
bool Mcfx_convolverAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* Mcfx_convolverAudioProcessor::createEditor()
{
    return new Mcfx_convolverAudioProcessorEditor (*this);
}

//==============================================================================
void Mcfx_convolverAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.

    // Create an outer XML element..
    XmlElement xml ("MYPLUGINSETTINGS");

    // add some attributes to it..
    xml.setAttribute ("activePreset", activePreset);
    xml.setAttribute ("presetDir", presetDir.getFullPathName());
    xml.setAttribute("ConvBufferSize", (int)_ConvBufferSize);
    xml.setAttribute("MaxPartSize", (int)_MaxPartSize);
    xml.setAttribute("oscIn", _osc_in);
    xml.setAttribute("oscInPort", _osc_in_port);
    xml.setAttribute("storeConfigDataInProject", _storeConfigDataInProject.get());

    // add .zip configuration as base64 dump
    if (_tempConfigZipFile.existsAsFile() && _storeConfigDataInProject.get())
    {
      MemoryBlock tempFileBlock;
      if (_tempConfigZipFile.loadFileAsData(tempFileBlock))
        xml.setAttribute("configData", tempFileBlock.toBase64Encoding());
    }

    // then use this helper function to stuff it into the binary blob and return it..
    copyXmlToBinary (xml, destData);
}

void Mcfx_convolverAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.

    // probably we should switch to value tree which can include binary data more efficiently and gzip the data...!

    std::unique_ptr<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr)
    {
        String newPresetDir;

        // make sure that it's actually our type of XML object..
        if (xmlState->hasTagName ("MYPLUGINSETTINGS"))
        {
            // ok, now pull out our parameters..
            activePreset  = xmlState->getStringAttribute("activePreset", "");

            newPresetDir = xmlState->getStringAttribute("presetDir", presetDir.getFullPathName());

            _ConvBufferSize = xmlState->getIntAttribute("ConvBufferSize", _ConvBufferSize);
            _MaxPartSize = xmlState->getIntAttribute("MaxPartSize", _MaxPartSize);

            _osc_in_port = xmlState->getIntAttribute("oscInPort", _osc_in_port);
            if (xmlState->getBoolAttribute("oscIn"))
              setOscIn(true);

            _storeConfigDataInProject.set(xmlState->getIntAttribute("storeConfigDataInProject", 0)); // -> default: don't store convolver data for existing projects
        }

        File tempDir(newPresetDir);
        if (tempDir.exists()) {
            presetDir = tempDir;
            SearchPresets(presetDir);
        }

        // load config from chunk data
        if (xmlState->hasAttribute("configData") && _storeConfigDataInProject.get())
        {
          DebugPrint("Load configuration from saved project data");
          // todo....!
          MemoryBlock tempMem;
          tempMem.fromBase64Encoding(xmlState->getStringAttribute("configData"));
          MemoryInputStream tempInStream(tempMem, false);
          ZipFile dataZip(tempInStream);
          File tempConfigFolder =  File::createTempFile("");
          dataZip.uncompressTo(tempConfigFolder, true); // we should later delete those files!!

          _cleanUpFilesOnExit.add(tempConfigFolder);

          Array <File> configfiles; // should be exactly one...
          tempConfigFolder.findChildFiles(configfiles, File::findFiles, false, activePreset);

          if (configfiles.size() == 1)
          {
              LoadConfigurationAsync(configfiles.getUnchecked(0));
              box_preset_str = configfiles.getUnchecked(0).getFileNameWithoutExtension();
              box_preset_str << " (saved within project)";
          }

          return;
        }

        // load preset from file in case it was not stored
        if (activePreset.isNotEmpty()) {
            LoadPresetByName(activePreset);
        }


    }
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Mcfx_convolverAudioProcessor();
}


//==============================================================================
//
AudioProcessorValueTreeState::ParameterLayout Mcfx_convolverAudioProcessor::createParameters()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterBool>    (   "RELOAD", "ReloadConfig", false));
    layout.add (std::make_unique<AudioParameterFloat>   (   "MASTERGAIN"
                                                         ,  "Master gain"
                                                         ,  NormalisableRange<float> ( -100.0f,   // minimum value
                                                                                      40.0f,   // maximum value
                                                                                      0.1f)
                                                         ,  0.0f
                                                         ));
          
    return layout;
}

void Mcfx_convolverAudioProcessor::parameterChanged (const String& parameterID, float newValue)
{
    if ( parameterID == "RELOAD" )
    {
        if (newValue == true)
            ReloadConfiguration();
    }
}

//==============================================================================