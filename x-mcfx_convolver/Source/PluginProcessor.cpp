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

#define MAX_PART_SIZE 65536

//==============================================================================
Mcfx_convolverAudioProcessor::Mcfx_convolverAudioProcessor() : /*
AudioProcessor (BusesProperties()   .withInput  ("MainInput",  juce::AudioChannelSet::discreteChannels(NUM_CHANNELS), true )
                                    .withOutput ("MainOutput", juce::AudioChannelSet::discreteChannels(NUM_CHANNELS), true )
                                    ), */
Thread("mtx_convolver_master"),
threadTask(ThreadTask::loading),
restoredSettings(false),

activeNumInputs(0),
activeNumOutputs(0),
activeNumIRs(0),
_MaxPartSize(MAX_PART_SIZE),
filterLenghtInSecs(0),
filterLenghtInSmpls(0),
wavefileHasBeenResampled(false),

_isProcessing(false),

convolverReady(false),
convolverStatus(ConvolverStatus::Unloaded),
holdNumInputChannel(false),

changeNumInputChannels(false),
numInputsStatus(NumInputsStatus::agreed),
tempNumInputs(0),
//storeNumInputsIntoWav(false),
storedInChannels(0),

readyToExportWavefile(false),

masterGain(0),
storedGain(nullptr),

_paramReload(false),
_skippedCycles(0),
safemode_(false),

newStatusText(false)
{
    // check these values... they are all zeros in this point
    _SampleRate = getSampleRate();
    _BufferSize = getBlockSize();
    _ConvBufferSize = getBlockSize();
    
    if (_MaxPartSize != 8192)
        _MaxPartSize = 8192;
#if JUCE_MAC
    File globalPluginOptions = globalPluginOptions.getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Application Support/x-mcfx/options.ini");
#else
    File globalPluginOptions = globalPluginOptions.getSpecialLocation(File::userApplicationDataDirectory).getChildFile("x-mcfx/options.ini");
#endif
    StringArray readLines;
    if (globalPluginOptions.existsAsFile())
    {
        globalPluginOptions.readLines(readLines);
        if (readLines.size() == 1)
        {
            defaultFilterDir = File(readLines.getReference(0));
        }
    }
    else
        defaultFilterDir = defaultFilterDir.getSpecialLocation(File::userApplicationDataDirectory).getChildFile("x-mcfx/filter_library");
//    defaultFilterDir = defaultFilterDir.getSpecialLocation(File::userApplicationDataDirectory).getChildFile("mcfx/convolver_presets");
    
	String debug;
    debug << "Filter directory: " << defaultFilterDir.getFullPathName();
    DebugPrint(debug << "\n\n");
    
    SearchFilters(defaultFilterDir);
    
    // this is for the open dialog of the gui
    lastSearchDir = lastSearchDir.getSpecialLocation(File::userHomeDirectory);
}

Mcfx_convolverAudioProcessor::~Mcfx_convolverAudioProcessor()
{
    if(isThreadRunning())
        stopThread(500);
#ifdef USE_ZITA_CONVOLVER
    zita_conv.stop_process();
    zita_conv.cleanup();
#else
    mtxconv_.StopProc();
    mtxconv_.Cleanup();
#endif

//    DeleteTemporaryFiles();
}

//==============================================================================
const String Mcfx_convolverAudioProcessor::getName() const                      { return JucePlugin_Name; }

int Mcfx_convolverAudioProcessor::getNumParameters()                            { return 1;}

float Mcfx_convolverAudioProcessor::getParameter (int index)                    { return (float)_paramReload;}

void Mcfx_convolverAudioProcessor::setParameter (int index, float newValue)
{
    bool newParamReload = (newValue <= 0.5f) ? false : true;
    if (newParamReload && !_paramReload)
        ReloadConfiguration();

    _paramReload = newParamReload;
}

const String Mcfx_convolverAudioProcessor::getParameterName (int index)         { return "ReloadConfig"; }

const String Mcfx_convolverAudioProcessor::getParameterText (int index)
{
    if (_paramReload)
        return "Reload";
    else
        return "";
}

const String Mcfx_convolverAudioProcessor::getInputChannelName (int channelIndex) const     { return String (channelIndex + 1); }

const String Mcfx_convolverAudioProcessor::getOutputChannelName (int channelIndex) const    { return String (channelIndex + 1); }

bool Mcfx_convolverAudioProcessor::isInputChannelStereoPair (int index) const               { return true; }

bool Mcfx_convolverAudioProcessor::isOutputChannelStereoPair (int index) const              { return true; }

/*
bool Mcfx_convolverAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet()  == juce::AudioChannelSet::disabled()
     || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::disabled())
        return false;
    
    if (layouts.getMainInputChannelSet()    != juce::AudioChannelSet::discreteChannels(12)
     && layouts.getMainOutputChannelSet()   != juce::AudioChannelSet::discreteChannels(19))
        return false;
    
    return true;
    //(layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet());
}*/

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

bool Mcfx_convolverAudioProcessor::silenceInProducesSilenceOut() const { return false; }

double Mcfx_convolverAudioProcessor::getTailLengthSeconds() const
{
    if (convolverReady)
    {
        // TODO: return length...
        return 0.0;
    }
    else
    {
        return 0.0;
    }
}

int Mcfx_convolverAudioProcessor::getNumPrograms()                                          { return 0; }

int Mcfx_convolverAudioProcessor::getCurrentProgram()                                       { return 0; }

void Mcfx_convolverAudioProcessor::setCurrentProgram (int index)                            { }

const String Mcfx_convolverAudioProcessor::getProgramName (int index)                       { return String(); }

void Mcfx_convolverAudioProcessor::changeProgramName (int index, const String& newName)     { }

//==============================================================================
void Mcfx_convolverAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    //line-up in case host samplerate/buffer changes
    
    // buffer size line-up ------------------------------------------------------------------------
    bool change = false;
    
    if (_SampleRate != sampleRate)
    {
        _SampleRate = sampleRate;
        change = true;
    }
    if (_BufferSize != samplesPerBlock)
    {
        _BufferSize = samplesPerBlock;
        change = true;
    }
    if (_ConvBufferSize < _BufferSize)
    {
        _ConvBufferSize = _BufferSize;
        change = true;
    }
    
    if (convolverReady && change)
        ReloadConfiguration();

    change = false;
    
    if (convolverReady)
    {
        // mtxconv_.Reset();
    }
    
}

void Mcfx_convolverAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void Mcfx_convolverAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    // std::cout << "in: " << getTotalNumInputChannels() << " out: " << getTotalNumOutputChannels() << std::endl;
    if ( convolverReady )
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
        buffer.applyGain(juce::Decibels::decibelsToGain(masterGain.get()));
        _skippedCycles.set(mtxconv_.getSkipCount());
        
#endif
        _isProcessing = false;
    }
    else
    { // config loaded
        // clear output in case no config is loaded!
        buffer.clear();
    }
}

void Mcfx_convolverAudioProcessor::run()
{
    if (threadTask == ThreadTask::loading)
    {
        DebugPrint("Loading Filter...\n\n");

        setConvolverStatus(ConvolverStatus::Loading);
        sendChangeMessage();
        
        //unload first....
        if (convolverReady)
            unloadConvolver();
        
         LoadIRMatrixFilter(getTargetFilter());
        
        if (!convolverReady)
            setConvolverStatus(ConvolverStatus::Unloaded);
        else
            setConvolverStatus(ConvolverStatus::Loaded);
        
        holdNumInputChannel = false;
        changeNumInputChannels = false;
        
        sendChangeMessage();
    }
    else
        exportWavefileWithMetadata(targetExport);
}

void Mcfx_convolverAudioProcessor::LoadConfigurationAsync(File fileToLoad)
{
    //store for the thread
    setTargetFilter(fileToLoad);
    readyToExportWavefile.set(false);
    
    if(isThreadRunning())
        stopThread(100);
    
    threadTask = ThreadTask::loading;
    startThread(6); // medium priority
}


void Mcfx_convolverAudioProcessor::ReloadConfiguration()
{
//    if (convolverReady || filterNameForStoring.isNotEmpty())
    if (convolverReady || getTargetFilter().existsAsFile())
    {
        String debug = "reloading for host new samplerate or buffer size \n";
        DebugPrint(debug);
        
        if (!changeNumInputChannels)
            holdNumInputChannel = true;
        
        LoadConfigurationAsync(getTargetFilter());
    }
    sendChangeMessage();
}

void Mcfx_convolverAudioProcessor::exportWavefileAsync(File newAudioFile)
{
    targetExport = newAudioFile;
    
    threadTask = ThreadTask::exporting;
    startThread(6); // medium priority
}
//-------------------------------------------------------------------------

void Mcfx_convolverAudioProcessor::LoadIRMatrixFilter(File filterFile)
{
    
    String debug;
    //existance check (in case configuration loading starts from a state saving)
    if (!filterFile.existsAsFile())
    {
        debug.clear();
        debug << "ERROR: filter file does not exist in: " ;
        debug << filterFile.getFullPathName() << "\n\n";
        DebugPrint(debug);
        
        addNewStatus("ERROR: filter file does not exist!");
        return;
    }
    
    _ConvBufferSize = nextPowerOfTwo(_ConvBufferSize);
    
    //---------------------------------------------------------------------------------------------
    debug.clear();
    debug << "trying to load filter...";
    DebugPrint(debug << filterFile.getFullPathName() << "\n\n");
    
    // debug print samplerate and buffer size
    debug.clear();
    debug << "Samplerate: "             << _SampleRate << "\n";
    debug << "Host Buffer Size: "       << (int)_BufferSize << "\n";
    debug << "Internal Buffer Size: "   << (int)_ConvBufferSize << "\n\n";
    DebugPrint(debug);
    
    // store filename only, on restart search filters folder for it!
    filterNameForStoring = filterFile.getFileName();

    // global settings
    /// temporary audio buffer backward compatible with old 32bit audio buffer
    AudioSampleBuffer tempAudioBuffer(1,256); //1 channel, 256 samples
    conv_data.setSampleRate(_SampleRate);
    
    double src_samplerate;
    
    float gain = 1.f;
    int delay = 0;
    int offset = 0;
    int length = 0;
    int inChannels = 0;
    
    // funtion for get input channels included in this one
    if (!loadIr(&tempAudioBuffer, filterFile, -1, src_samplerate, gain, 0, 0)) // offset/length has to be done while processing individual irs
    {
        debug.clear();
        debug << "ERROR: not loaded: " << filterFile.getFullPathName();
        DebugPrint(debug << "\n");
        
        addNewStatus("ERROR: selected wavefile not loaded");
        return;
    }
        
    ///kill the thread if requested
    if (tempNumInputs == -2)
    {
        ///on restoring, a value will be still needed
        tempNumInputs = 0;
        return;
    }
    readyToExportWavefile.set(true);
    
    bool isDiagonal;
    if(tempNumInputs == -1)
        isDiagonal = true;
    else
        isDiagonal = false;
    
    int outChannels = tempAudioBuffer.getNumChannels();
    int irLength;
    
    if (isDiagonal)
    {
        irLength = tempAudioBuffer.getNumSamples();
        inChannels = outChannels;
    }
    else
    {
        irLength = tempAudioBuffer.getNumSamples()/tempNumInputs;
        inChannels = tempNumInputs;
    }
     
    
    if ((inChannels > getTotalNumInputChannels()) || outChannels > getTotalNumOutputChannels())
    {
        debug.clear();
        debug << "Input/Output channel assignement not feasible." << "\n";
        debug << "Not enough input/output channels of the plugin. Need Input: " << inChannels << " Output: " << outChannels << "\n\n";
        DebugPrint( debug );
        
        String status;
        status << "ERROR: In/Out plugin channels not feasible. Need at least " << inChannels << " ins, " << outChannels << " outs";
        addNewStatus(status);
        return;
    }
    
    if (src_samplerate != _SampleRate)
        wavefileHasBeenResampled.set(true);
    else
        wavefileHasBeenResampled.set(false);
    
    if (length <= 0)
        length = irLength-offset;
    
    // std::cout << "TotalLength: " <<  TempAudioBuffer.getNumSamples() << " IRLength: " <<  irLength << " Used Length: " << length << " Channels: " << TempAudioBuffer.getNumChannels() << std::endl;
    
    addNewStatus("Loading filter matrix into the convolver data...");
    for (int i=0; i < outChannels; i++)
    {
        for (int j=0; j < inChannels; j++)
        {
            // add IR to my convolution data - offset and length are already done while reading file
            if (isDiagonal)
            {
                if (i==j)
                    conv_data.addIR(j, i, offset, delay, length, &tempAudioBuffer, i, src_samplerate);
            }
            else
            {
                int individualOffset = j*irLength;
                // CHECK IF LENGTH/OFFSET E.G. IS OK!!
                conv_data.addIR(j, i, individualOffset + offset, delay, length, &tempAudioBuffer, i, src_samplerate);
            }
            // std::cout << "AddIR: IN: " << j << " OUT: " << i << " individualOffset: " << individualOffset << std::endl;
        }
    }
    
    debug.clear();
    debug  << "Loaded " << conv_data.getNumIRs() << " filters with:" << "\n";
    debug  << "length " << length << "\n";
    debug  << "samplerate " << src_samplerate << "\n";
    debug  << "input channels: " << inChannels << "\n";
    debug  << "output channels: " << outChannels << "\n";
    debug  << "filename: " << filterFile.getFileName() << "\n\n";
    DebugPrint( debug );
     
    // initiate convolver with the current found parameters and filters
    loadConvolver();

    debug.clear();
    debug << "Filter matrix loaded" << "\n";
    debug << "Maximum filter length: " << String(conv_data.getMaxLengthInSeconds(), 2) << "[s], " << conv_data.getMaxLength() << "[smpls] \n";
    debug << "Plugin Latency: " << getLatencySamples() << "[smpls] \n";
    DebugPrint(debug << "\n\n");
    
    // search for the input channels number into the wavefile metadata tags
    if (storedGain != nullptr)
    {
        masterGain.set(*storedGain);
        storedGain = nullptr;
    }
    
    addNewStatus("Convolver ready");
    
    sendChangeMessage(); // notify editor

}

void Mcfx_convolverAudioProcessor::loadConvolver()
{
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
    addNewStatus("Configuring convolver settings...");
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
    
    if (safemode_)
        setLatencySamples(_ConvBufferSize);
    else
        setLatencySamples(_ConvBufferSize-_BufferSize);
    
    _skippedCycles.set(0);
    
    activeNumInputs = conv_data.getNumInputChannels();
    activeNumOutputs = conv_data.getNumOutputChannels();
    activeNumIRs = conv_data.getNumIRs();
    filterLenghtInSecs = conv_data.getMaxLengthInSeconds();
    filterLenghtInSmpls = conv_data.getMaxLength();
    
    convolverReady = true;
}

void Mcfx_convolverAudioProcessor::unloadConvolver()
{
    String debug;
    
    while (_isProcessing)
    {
       Sleep(1);
    }
    
    debug << "Unloading Convolver..." << "\n";
    DebugPrint(debug);
    
    addNewStatus("Unloading Convolver...");
    
    // delete configuration
    _conv_in.clear();
    _conv_out.clear();
    
    activeNumInputs = 0;
    activeNumOutputs = 0;
    activeNumIRs = 0;
    filterLenghtInSecs = 0;
    filterLenghtInSmpls = 0;
    
#ifdef USE_ZITA_CONVOLVER
    
    zita_conv.stop_process();
    zita_conv.cleanup();

#else
    
    mtxconv_.StopProc();
    mtxconv_.Cleanup();
    
#endif
    
    conv_data.clear();
    
    convolverReady = false;
    
    debug.clear();
    debug << "Convolver settings unloaded" << "\n\n";
    DebugPrint(debug); // clear debug window
}

void Mcfx_convolverAudioProcessor::getInChannels(int waveFileLength)
{
    if (changeNumInputChannels)
        numInputsStatus = NumInputsStatus::requested;
    else
        numInputsStatus = NumInputsStatus::missing;
    
    while (numInputsStatus != NumInputsStatus::agreed)
    {
        sendChangeMessage();
        wait(-1);
        /// thread kill
        if (threadShouldExit())
        {
            tempNumInputs = -2;
            return;
        }
        ///value returned check
        if (tempNumInputs == -1)
        {
            numInputsStatus = NumInputsStatus::agreed;
            return;
        }
        if (tempNumInputs > 0 )
        {
            int irLength = waveFileLength/tempNumInputs;
            if (irLength*tempNumInputs != waveFileLength)
            {
                numInputsStatus = NumInputsStatus::notMultiple;
            }
            else
            {
                numInputsStatus = NumInputsStatus::agreed;
                return;
            }
        }
        else
        {
            numInputsStatus = NumInputsStatus::notFeasible;
        }
    }
}

bool Mcfx_convolverAudioProcessor::loadIr(AudioSampleBuffer* IRBuffer, const File& audioFile, int channel, double &samplerate, float gain, int offset, int length)
{
    if (!audioFile.existsAsFile())
    {
        String debug;
        debug << "ERROR: wavefile " << audioFile.getFileName() << " does not exist!";
        addNewStatus(debug);
        
        DebugPrint(debug << "\n");
        return false;
    }
    
    AudioFormatManager formatManager;
    
    // this can read .wav and .aiff
    formatManager.registerBasicFormats();
    
    AudioFormatReader* reader = formatManager.createReaderFor(audioFile);
    
    if (!reader)
    {
        String debug;
        debug << "ERROR: " << audioFile.getFileName() << " could not be read!";
        addNewStatus(debug);
        
        DebugPrint(debug << "\n");
        return false;
    }
    
	if (offset < 0)
        offset = 0;
    
    int64 wav_length = (int)reader->lengthInSamples;
    
    if (length <= 0)
        length = wav_length - offset;
    
    if (wav_length <= 0)
    {
        String debug;
        debug << "ERROR: " << audioFile.getFileName() << " contains no samples!";
        addNewStatus(debug);
        
        DebugPrint(debug << "\n");
        return false;
    }
    if (wav_length-offset < length)
    {
        length = wav_length-offset;
        
        String debug;
        debug << "Warning: not enough samples in one IR, loading " << String(length) << " samples";
        DebugPrint(debug << "\n");
    }
    if ((int)reader->numChannels <= channel)
    {
        String debug;
        debug << "ERROR: wavefile doesn't have enough channels: " << String(reader->numChannels);
        addNewStatus(debug);
        
        DebugPrint(debug << "\n");
        return false;
    }
    
//    AudioSampleBuffer ReadBuffer(reader->numChannels, length); // create buffer
    bufferRead.clear();
    bufferRead = AudioSampleBuffer(reader->numChannels, length);
    
    addNewStatus("Reading file...");
    reader->read(&bufferRead, 0, length, offset, true, true);
    
    // set the samplerate -> maybe we have to resample later...
    samplerate = reader->sampleRate;
    
    // copy the wanted channel into our IR Buffer
    
    if (channel >= 0) // only one channel wanted...
    {
        IRBuffer->setSize(1, length);
        IRBuffer->copyFrom(0, 0, bufferRead, channel, 0, length);
    }
    else // copy all channels from the wav file if channel == -1 -> used for packedmatrix
    {
        IRBuffer->setSize(reader->numChannels, length);
        
        for (unsigned int i=0; i<reader->numChannels; i++) {
            IRBuffer->copyFrom(i, 0, bufferRead, i, 0, length);
        }
    }
   
    // scale ir with gain
    if (gain != 1.f)
        IRBuffer->applyGain(gain);
    
    // std::cout << "ReadRMS: " << IRBuffer->getRMSLevel(0, 0, ir_length) << std::endl;
    
    // search for the input channels number into the wavefile metadata tags
    if (storedInChannels != 0)
    {
        tempNumInputs = storedInChannels;
        storedInChannels = 0;
        delete reader;
        return true;
    }
    if (holdNumInputChannel)
    {
        delete reader;
        return true;
    }
    
    /// try to read input channels in the metadata tag. if tag was empty, put 0 for inChannels
    String metaTagValue =  reader->metadataValues.getValue(WavAudioFormat::riffInfoKeywords, "0");
    int inChannels = metaTagValue.getIntValue();
    
    if(inChannels != 0 && !changeNumInputChannels)
    {
        tempNumInputs = inChannels;
        delete reader;
        return true;
    }
    else
    {
        /// request new input channels number to the user
        addNewStatus("Waiting for user input...");
        getInChannels(IRBuffer->getNumSamples());
        
        //DEPRECATED
        /// write the new value in metadata tag
        /*
        if (storeNumInputsIntoWav.get())
        {
            addNewStatus("Storing number of input channel into wavefile metadata...");
            reader->metadataValues.set(WavAudioFormat::riffInfoKeywords, (String)tempNumInputs);
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
            
            writer->writeFromAudioSampleBuffer(bufferRead, 0, bufferRead.getNumSamples());
            delete writer;
        }
         */
    }
    
    delete reader;
    return true;
}

//=============================================================================
///Search recursively all the filter file in the searchFolder directory and subdirectories
void Mcfx_convolverAudioProcessor::SearchFilters(File SearchFolder)
{
    filterFilesList.clear();
    
    String extension = "*.wav";
    
    SearchFolder.findChildFiles(filterFilesList, File::findFiles, true, extension);
    filterFilesList.sort();
    
    String debug;
    debug << "Found " << filterFilesList.size() << " wav filters in the selected directory";
    std::cout << debug << "\n";
    addNewStatus(debug);
}

void Mcfx_convolverAudioProcessor::setNewGlobalFilterFolder(File newGloablFolder)
{
    File globalPluginOptions;
#if JUCE_MAC
    globalPluginOptions = globalPluginOptions.getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Application Support/x-mcfx");
#else
    globalPluginOptions = globalPluginOptions.getSpecialLocation(File::userApplicationDataDirectory).getChildFile("x-mcfx");
#endif
    globalPluginOptions.createDirectory();
    globalPluginOptions = globalPluginOptions.getChildFile("options.ini");
    globalPluginOptions.create();
    
    bool executed = globalPluginOptions.replaceWithText(newGloablFolder.getFullPathName());
}

void Mcfx_convolverAudioProcessor::LoadFilterFromMenu(unsigned int filterIndex, bool restored)
{
    //check if the ID of the loading filter is coherent with the filter list
    if (filterIndex < (unsigned int)filterFilesList.size())
    {
//        DeleteTemporaryFiles();
        LoadConfigurationAsync(filterFilesList.getUnchecked(filterIndex));
        filterNameToShow = filterFilesList.getUnchecked(filterIndex).getFileNameWithoutExtension();
    }
    if (restored)
        restoredSettings.set(true);
    else
        restoredSettings.set(false);
}

void Mcfx_convolverAudioProcessor::LoadFilterFromFile(File filterToLoad, bool restored)
{
//    DeleteTemporaryFiles();
    LoadConfigurationAsync(filterToLoad);
    filterNameToShow.clear();
    filterNameToShow << filterToLoad.getFileNameWithoutExtension() << " (outside library)";
    
    if (restored)
        restoredSettings.set(true);
    else
        restoredSettings.set(false);
}

// ================= Editor set/get functions =================
double Mcfx_convolverAudioProcessor::getSamplerate()
{
    return _SampleRate;
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

int Mcfx_convolverAudioProcessor::getSkippedCyclesCount()
{
    return _skippedCycles.get();
}

//======================== Utility functions ========================
/*
void Mcfx_convolverAudioProcessor::DeleteTemporaryFiles()
{
    _readyToSaveConfiguration.set(false);

    for (int i = 0; i < _cleanUpFilesOnExit.size(); i++)
    {
        _cleanUpFilesOnExit.getUnchecked(i).deleteRecursively();
    }
    _cleanUpFilesOnExit.clear();
}*/

void Mcfx_convolverAudioProcessor::exportWavefileWithMetadata(File newAudioFile)
{
    addNewStatus("Configuring new wavefile for export...");
    AudioFormatManager formatManager;
    // this can read .wav and .aiff
    formatManager.registerBasicFormats();
    
    AudioFormatReader* reader = formatManager.createReaderFor(getTargetFilter());
    
    reader->metadataValues.set(WavAudioFormat::riffInfoKeywords, (String)tempNumInputs);
    FileOutputStream* outStream = new FileOutputStream(newAudioFile);
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
    delete reader;
    delete wave;
    addNewStatus("Exporting wavefile with metadata info...");
    bool executed = writer->writeFromAudioSampleBuffer(bufferRead, 0, bufferRead.getNumSamples());
    if (executed)
        addNewStatus("Done.");
    else
        addNewStatus("An error occurred during the export phase...");
    
    delete writer;
}

void Mcfx_convolverAudioProcessor::DebugPrint(String debugText, bool reset)
{
    ///avoiding multiple calling of variable modification from threads
    //_DebugText can be called by processor thread and editor thread
    ScopedLock lock(_DebugTextMutex);
    String temp;
    
    temp << _DebugText;
    
    if (!reset)
        temp << debugText;
    
    // std::cout << debugText << std::endl;
    
    _DebugText = temp;
}

String Mcfx_convolverAudioProcessor::getDebugString()
{
  ScopedLock lock(_DebugTextMutex);
  return _DebugText;
}

//-----------------------------------------------------------------
String Mcfx_convolverAudioProcessor::getStatusText()
{
    return statusText;
}

void Mcfx_convolverAudioProcessor::addNewStatus(String newStatus)
{
    statusText.clear();
    statusText = newStatus;
    
    newStatusText = true;
    sendChangeMessage();
    wait(125);
}
//-----------------------------------------------------------------
File Mcfx_convolverAudioProcessor::getTargetFilter()
{
    ScopedLock lock(targetFilterMutex);
    return targetFilter;
}

void Mcfx_convolverAudioProcessor::setTargetFilter(File newTargetFile)
{
    ScopedLock lock(targetFilterMutex);
    targetFilter = newTargetFile;
}

//-----------------------------------------------------------------
int Mcfx_convolverAudioProcessor::getConvolverStatus()
{
    ScopedLock lock(convStatusMutex);
    return convolverStatus;
}

void Mcfx_convolverAudioProcessor::setConvolverStatus(ConvolverStatus status)
{
    ScopedLock lock(convStatusMutex);
    convolverStatus = status;
}

//-----------------------------------------------------------------
void Mcfx_convolverAudioProcessor::setTargetExport(File targetAudioFile)
{
    targetExport = targetAudioFile;
}

//======================== Save to zipfile ========================
//bool Mcfx_convolverAudioProcessor::SaveConfiguration(File zipFile)
//{
//    if (_readyToSaveConfiguration.get())
//        return _tempConfigZipFile.copyFileTo(zipFile);
//    else
//        return false;
//}

//==============================================================================
void Mcfx_convolverAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    
    // Create an outer XML element..
    
    XmlElement xml ("MYPLUGINSETTINGS");
    
    // add some attributes to it..
    xml.setAttribute("SampleRate", (int)_SampleRate);
    xml.setAttribute("BufferSize", (int)_BufferSize);
    xml.setAttribute("ConvBufferSize", (int)_ConvBufferSize);
    xml.setAttribute("MaxPartSize", (int)_MaxPartSize);
    
    xml.setAttribute ("defaultFilterDir", defaultFilterDir.getFullPathName());
    
    // if filter has been loaded...
    if(convolverReady )
    {
        ///DEPRECATED
        /*
        if (_tempConfigZipFile.existsAsFile())
        {
            MemoryBlock tempFileBlock;
            if (_tempConfigZipFile.loadFileAsData(tempFileBlock))
                xml.setAttribute("configData", tempFileBlock.toBase64Encoding());
        }*/
        
        xml.setAttribute("filterFileName", filterNameForStoring);
        xml.setAttribute("inputChannelsNumber",(int)tempNumInputs);
        
        /// save also fullpath of the config files, in case it is not possible to restored from saved data
        Array <File> files;
        defaultFilterDir.findChildFiles(files, File::findFiles, true, filterNameForStoring);
        if (files.size())
            xml.setAttribute ("targetWasInMenu", (bool)true);
        
        xml.setAttribute("filterFullPathName", targetFilter.getFullPathName());
        auto test = masterGain.get();
        xml.setAttribute("masterGain", masterGain.get());
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
        // make sure that it's actually our type of XML object..
        if (xmlState->hasTagName ("MYPLUGINSETTINGS"))
        {
            // ok, now pull out our parameters..
            _SampleRate         = xmlState->getIntAttribute("SampleRate", 44100);
            _BufferSize         = xmlState->getIntAttribute("BufferSize", 512);
            _ConvBufferSize     = xmlState->getIntAttribute("ConvBufferSize", _ConvBufferSize);
            _MaxPartSize        = xmlState->getIntAttribute("MaxPartSize", _MaxPartSize);
            
            String newFilterDir = xmlState->getStringAttribute("defaultFilterDir", defaultFilterDir.getFullPathName());
            
            File tempDir(newFilterDir);
            if (tempDir.exists())
            {
                defaultFilterDir = tempDir;
                SearchFilters(defaultFilterDir);
            }
            
            ///DEPRECATED
            //Restoring from whole wavefile save into xml
            /*
            if (xmlState->hasAttribute("configData"))
            {
                filterNameForStoring = xmlState->getStringAttribute("filterFileName");
                
                DebugPrint("Load configuration from saved project data\n");
                addNewStatus("Loading restored configuration from saved data...");
                
                MemoryBlock tempMem;
                bool decodingOK = tempMem.fromBase64Encoding(xmlState->getStringAttribute("configData"));
                
                MemoryInputStream tempInStream(tempMem, false);
                ZipFile dataZip(tempInStream);
                
                File tempConfigFolder =  File::createTempFile("");
                Result unzip (dataZip.uncompressTo(tempConfigFolder, true));
                
                _cleanUpFilesOnExit.add(tempConfigFolder);

                Array <File> configfiles;
                tempConfigFolder.findChildFiles(configfiles, File::findFiles, false, filterNameForStoring);
                
                // should be exactly one wav o conf file
                if (decodingOK && unzip.wasOk() && configfiles.size() == 1)
                {
                    storedInChannels = xmlState->getIntAttribute("inputChannelsNumber", 0);
                    LoadConfigurationAsync(configfiles.getUnchecked(0));
                    filterNameToShow.clear();
                    filterNameToShow = configfiles.getUnchecked(0).getFileNameWithoutExtension();
                    restoredSettings.set(true);
                    return;
                }
            }
             */
            /// if restoring from memory got troubles try to restore from filter menu
            //Restoring from saved locations
            targetFilter = File(xmlState->getStringAttribute("filterFullPathName", "/IDONTEXIST")) ;
            if(xmlState->getBoolAttribute("targetWasInMenu",false))
            {
                /// find the filter index from the rebuilt list of filter files
                DefaultElementComparator<File> comparator;
                unsigned int filterIndex = filterFilesList.indexOfSorted(comparator, targetFilter);
                if (filterIndex != -1)
                {
                    storedInChannels = xmlState->getIntAttribute("inputChannelsNumber", 0);
                    storedGain = new float((float)xmlState->getDoubleAttribute("masterGain",9999));
                    if (*storedGain == 9999)
                        storedGain = nullptr;
                    
                    LoadFilterFromMenu(filterIndex, true);
                    return;
                }
                filterNameToShow = targetFilter.getFileNameWithoutExtension();
                restoredSettings.set(true);
                
                addNewStatus("ERROR: filter not found in the library folder!");
                
                DebugPrint("ERROR: filter not found in the library folder!\n\n");
                return;
            }
            
            /// or original forlder if filter file was taken outside the menu
            if(targetFilter.existsAsFile())
            {
                storedInChannels = xmlState->getIntAttribute("inputChannelsNumber", 0);
                storedGain = new float((float)xmlState->getDoubleAttribute("masterGain",9999));
                if (*storedGain == 9999)
                    storedGain = nullptr;
                
                LoadFilterFromFile(targetFilter, true);
                return;
            }

            String debug;
            debug << "ERROR: filter not found in the original folder!";
            filterNameToShow = targetFilter.getFileNameWithoutExtension();
            filterNameToShow << " (outside library)";
            restoredSettings.set(true);
            addNewStatus(debug);
            
            DebugPrint(debug << "\n\n");
        }
    }
}

//==============================================================================
bool Mcfx_convolverAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* Mcfx_convolverAudioProcessor::createEditor()
{
    //new way Controller-View editor type
    return new Mcfx_convolverAudioProcessorEditor (*this);


    //old way pointer creation
    // return new Mcfx_convolverAudioProcessorEditor (this);
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Mcfx_convolverAudioProcessor();
}


