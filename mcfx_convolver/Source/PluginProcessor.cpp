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

// #define VAL(str) #str
// #define TOSTRING(str) VAL(str)

//==============================================================================
Mcfx_convolverAudioProcessor::Mcfx_convolverAudioProcessor() :
AudioProcessor (BusesProperties()   .withInput  ("MainInput",  juce::AudioChannelSet::discreteChannels(NUM_CHANNELS), true )
                                    .withOutput ("MainOutput", juce::AudioChannelSet::discreteChannels(NUM_CHANNELS), true )
                                    ),
Thread("mtx_convolver_master"),
_readyToSaveConfiguration(false),
//_storeConfigDataInProject(1),

_min_in_ch(0),
_min_out_ch(0),
_num_conv(0),
_MaxPartSize(MAX_PART_SIZE),

_isProcessing(false),

convolverReady(false),
convolverStatus(ConvolverStatus::Unloaded),
isAReload(false),

inputChannelRequired(false),
inChannelStatus(InChannelStatus::agreed),
tempInputChannels(0),
storeInChannelIntoWav(false),
storedInChannels(0),

_paramReload(false),
_skippedCycles(0),
safemode_(false),

_osc_in_port(7200),
_osc_in(false),

newStatusText(false)
{
    // check these values... they are all zeros in this point
//    _SampleRate = getSampleRate();
//    _BufferSize = getBlockSize();
//    _ConvBufferSize = getBlockSize();
    
    defaultFilterDir = defaultFilterDir.getSpecialLocation(File::userApplicationDataDirectory).getChildFile("mcfx/convolver_presets");
    
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
        stopThread(10);
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
}

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

bool Mcfx_convolverAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet()  == juce::AudioChannelSet::disabled()
     || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::disabled())
        return false;
    
    return (layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet());
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
    
    isAReload = false;
    sendChangeMessage();
}

void Mcfx_convolverAudioProcessor::LoadConfigurationAsync(File fileToLoad, bool reload)
{
    DebugPrint("Loading Filter...\n\n");
    if(reload)
        isAReload=true;
    
    //store for the thread
    setTargetFilter(fileToLoad);
    
    if(isThreadRunning())
        stopThread(100);
    
    startThread(6); // medium priority
}


void Mcfx_convolverAudioProcessor::ReloadConfiguration()
{
    if (convolverReady || filterNameForStoring.isNotEmpty())
    {
        String debug = "reloading for host new samplerate or buffer size \n";
        DebugPrint(debug);
        if (inChannelStatus == InChannelStatus::requested)
            LoadConfigurationAsync(getTargetFilter());
        else
            LoadConfigurationAsync(getTargetFilter(),true);
    }
}

//-------------------------------------------------------------------------

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

void Mcfx_convolverAudioProcessor::LoadIRMatrixFilter(File filterFile)
{
    String debug;
    
    //existance check (in case configuration loading starts from a state saving)
    if (!filterFile.existsAsFile())
    {
        debug.clear();
        debug << "Filter file does not exist!";
        addNewStatus(debug);
        
        DebugPrint(debug << " in: " << filterFile.getFullPathName() << "\n\n");
        return;
    }
    
    _ConvBufferSize = nextPowerOfTwo(_ConvBufferSize);
    
    //---------------------------------------------------------------------------------------------
    debug.clear();
    debug << "trying to load filter...";
    addNewStatus(debug);
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
    AudioSampleBuffer TempAudioBuffer(1,256); //1 channel, 256 samples
    conv_data.setSampleRate(_SampleRate);
    
    float gain = 1.f;
    int delay = 0;
    int offset = 0;
    int length = 0;
    int inChannels = 0;

    double src_samplerate;
    
    // funtion for get input channels included in this one
    addNewStatus("Reading file...");
    if (loadIr(&TempAudioBuffer, filterFile, -1, src_samplerate, gain, 0, 0)) // offset/length has to be done while processing individual irs
    {
        ///kill the thread if requested
        if (tempInputChannels == -2)
        {
            ///on restoring, a value will be still needed
            tempInputChannels = 0;
            return;
        }
        bool isDiagonal;
        if(tempInputChannels == -1)
            isDiagonal = true;
        else
            isDiagonal = false;
        
        int outChannels = TempAudioBuffer.getNumChannels();
        int irLength;
        
        if (isDiagonal)
        {
            irLength = TempAudioBuffer.getNumSamples();
            inChannels = outChannels;
        }
        else
        {
            irLength = TempAudioBuffer.getNumSamples()/tempInputChannels;
            inChannels = tempInputChannels;
        }
         
        /*
        if ((inChannels > getTotalNumInputChannels()) || outChannels > getTotalNumOutputChannels())
        {
            debug.clear();
            debug << "Input/Output channel assignement not feasible." << "\n";
            debug << "Not enough input/output channels of the plugin. Need Input: " << inChannels << " Output: " << outChannels << "\n\n";
            DebugPrint( debug );
            
            String status;
            status << "ERROR: In/Out plugin channels not feasible. " << "Need " << inChannels << " ins, " << outChannels << " outs";
            addNewStatus(status);
            return;
        }*/
        
        if (length <= 0)
            length = irLength-offset;
        
        // std::cout << "TotalLength: " <<  TempAudioBuffer.getNumSamples() << " IRLength: " <<  irLength << " Used Length: " << length << " Channels: " << TempAudioBuffer.getNumChannels() << std::endl;
        for (int i=0; i < outChannels; i++)
        {
            for (int j=0; j < inChannels; j++)
            {
                // add IR to my convolution data - offset and length are already done while reading file
                if (isDiagonal)
                {
                    if (i==j)
                        conv_data.addIR(j, i, offset, delay, length, &TempAudioBuffer, i, src_samplerate);
                }
                else
                {
                    int individualOffset = j*irLength;
                    // CHECK IF LENGTH/OFFSET E.G. IS OK!!
                    conv_data.addIR(j, i, individualOffset + offset, delay, length, &TempAudioBuffer, i, src_samplerate);
                }
                // std::cout << "AddIR: IN: " << j << " OUT: " << i << " individualOffset: " << individualOffset << std::endl;
            }
        }
        debug.clear();
        debug << "Loaded " << conv_data.getNumIRs() << " filters" << "\n";
        debug << "\t\t" << "length " << length << "\n";
        debug << "\t\t" << "input channels: " << inChannels << "\n";
        debug << "\t\t" << "output channels: " << outChannels << "\n";
        debug << "\t\t" << "filename: " << filterFile.getFileName() << "\n\n";
        DebugPrint( debug );
    }
    else
    {
        debug.clear();
        debug << "ERROR: not loaded: " << filterFile.getFullPathName();
        DebugPrint(debug << "\n");
        
        return;
    }
     
    // initiate convolver with the current found parameters and filters
    addNewStatus("Loading matrix into the convolver...");
    loadConvolver();
    
//    filterFileLoaded = filterFile;

    debug.clear();
    debug << "IR filter matrix loaded" << "\n";
    debug << "Maximum filter length: " << String(conv_data.getMaxLengthInSeconds(), 2) << "[s], " << conv_data.getMaxLength() << "[smpls] \n";
    debug << "Plugin Latency: " << getLatencySamples() << "[smpls] \n";
    DebugPrint(debug << "\n\n");
    
    String status = "Convolver ready";
    addNewStatus(status);
    sendChangeMessage(); // notify editor
    
    //BLOCK save config/wave as zip file
    /*
    _tempConfigZipFile = _tempConfigZipFile.createTempFile(".zip");
    _cleanUpFilesOnExit.add(_tempConfigZipFile);
    
    ZipFile::Builder compressedFileToStore;
//    auto test = filterFile.getParentDirectory().getFileName();
    
    compressedFileToStore.addFile(filterFile, 5);
    
    FileOutputStream outputStream(_tempConfigZipFile);
//    std::cout << "temp file path: " << _tempConfigZipFile.getFullPathName() << std::endl;
    
    if (outputStream.openedOk())
    {
        outputStream.setPosition(0); // overwrite file if already exists
        outputStream.truncate();

        double progress = 0.;
        compressedFileToStore.writeToStream(outputStream, &progress);

    _readyToSaveConfiguration.set(true);
    }
    
    sendChangeMessage(); // notify editor again. Ready to save configuration to zip (wavefile)
    */
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
    
    _min_in_ch = conv_data.getNumInputChannels();
    _min_out_ch = conv_data.getNumOutputChannels();
    _num_conv = conv_data.getNumIRs();
    
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
    
    // delete configuration
    
    _conv_in.clear();
    _conv_out.clear();
    
    _min_in_ch = 0;
    _min_out_ch = 0;
    _num_conv = 0;
    
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
    inChannelStatus = InChannelStatus::missing;
    while (inChannelStatus != InChannelStatus::agreed)
    {
        sendChangeMessage();
        wait(-1);
        /// thread kill
        if (threadShouldExit())
        {
            tempInputChannels = -2;
            return;
        }
        ///value returned check
        if (tempInputChannels == -1)
        {
            inChannelStatus = InChannelStatus::agreed;
            return;
        }
        if (tempInputChannels > 0 )
        {
            int irLength = waveFileLength/tempInputChannels;
            if (irLength*tempInputChannels != waveFileLength)
            {
                inChannelStatus = InChannelStatus::notMultiple;
            }
            else
            {
                inChannelStatus = InChannelStatus::agreed;
                return;
            }
        }
        else
        {
            inChannelStatus = InChannelStatus::notFeasible;
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
    
    // search for the input channels number into the wavefile metadata tags
    if (storedInChannels != 0)
    {
        tempInputChannels = storedInChannels;
        storedInChannels = 0;
        delete reader;
        return true;
    }
    if (isAReload)
    {
        delete reader;
        return true;
    }
    
    /// try to read input channels in the metadata tag. if tag was empty, put 0 for inChannels
    String metaTagValue =  reader->metadataValues.getValue(WavAudioFormat::riffInfoKeywords, "0");
    int inChannels = metaTagValue.getIntValue();
    
    if(inChannels != 0 && inChannelStatus != InChannelStatus::requested)
    {
        tempInputChannels = inChannels;
        delete reader;
        return true;
    }
    else
    {
        /// request new input channels number to the user
        getInChannels(IRBuffer->getNumSamples());

        
        /// write the new value in metadata tag
        if (storeInChannelIntoWav.get())
        {
            reader->metadataValues.set(WavAudioFormat::riffInfoKeywords, (String)tempInputChannels);
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
        }
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

void Mcfx_convolverAudioProcessor::LoadFilterFromMenu(unsigned int filterIndex)
{
    //check if the ID of the loading filter is coherent with the filter list
    if (filterIndex < (unsigned int)filterFilesList.size())
    {
        DeleteTemporaryFiles();
        LoadConfigurationAsync(filterFilesList.getUnchecked(filterIndex));
        filterNameToShow = filterFilesList.getUnchecked(filterIndex).getFileNameWithoutExtension();
    }
}

void Mcfx_convolverAudioProcessor::LoadFilterFromFile(File filterToLoad)
{
    DeleteTemporaryFiles();
    LoadConfigurationAsync(filterToLoad);
    filterNameToShow.clear();
    filterNameToShow << filterToLoad.getFileNameWithoutExtension() << " (picked outside)";
}

///DEPRECATED
//void Mcfx_convolverAudioProcessor::LoadPresetByName(String filterNameToShow)
//{
//    Array <File> files;
//    defaultPresetDir.findChildFiles(files, File::findFiles, true, filterNameToShow);
//
//    if (files.size())
//    {
//        DeleteTemporaryFiles();
//        LoadConfigurationAsync(files.getUnchecked(0)); // Load first result
//        filterNameToShow = files.getUnchecked(0).getFileName();
//    }
//    else
//    { // preset not found -> post!
//        String debug_msg;
//        debug_msg << "ERROR loading preset: " << filterNameToShow << ", Preset not found in search folder!\n\n";
//        DebugPrint(debug_msg);
//    }
//}

// ================= Editor set/get functions =================
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

//======================== Open sound control ========================
void Mcfx_convolverAudioProcessor::setOscIn(bool arg)
{
    if (arg)
    {
        if (oscReceiver.connect(_osc_in_port))
        {
            oscReceiver.addListener(this, "/reload");
            oscReceiver.addListener(this, "/load");
            _osc_in = true;
        }
        else
        {
            DebugPrint("Could not open UDP port, try a different port.\n");
        }
    }
    else
    {
        // turn off osc
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
    DebugPrint("Received Reload via OSC (/reload).\n");
    ReloadConfiguration();
  }
  else if (message.getAddressPattern() == "/load")
  {

    DebugPrint("Received Load via OSC (/load).\n");

    if (message.size() < 1)
      return;

//    if (message[0].isString())
//      LoadPresetByName(message[0].getString());
  }
}

//======================== Utility functions ========================
void Mcfx_convolverAudioProcessor::DeleteTemporaryFiles()
{
    _readyToSaveConfiguration.set(false);

    for (int i = 0; i < _cleanUpFilesOnExit.size(); i++)
    {
        _cleanUpFilesOnExit.getUnchecked(i).deleteRecursively();
    }
    _cleanUpFilesOnExit.clear();
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
void Mcfx_convolverAudioProcessor::timerCallback()
{
    if (!statusTextList.isEmpty())
    {
        newStatusText = true;
        sendChangeMessage();
        startTimer(250);
    }
    else
        Timer::stopTimer();
}

String Mcfx_convolverAudioProcessor::getStatusText()
{
    ScopedLock lock(statusTextMutex);
    return statusTextList.removeAndReturn(0);
}

void Mcfx_convolverAudioProcessor::addNewStatus(String newStatus)
{
    ScopedLock lock(statusTextMutex);
    statusTextList.add(newStatus);
    
    if (!Timer::isTimerRunning())
    {
        newStatusText = true;
        sendChangeMessage();
        startTimer(250);
    }
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

//======================== Save to zipfile ========================
bool Mcfx_convolverAudioProcessor::SaveConfiguration(File zipFile)
{
    if (_readyToSaveConfiguration.get())
        return _tempConfigZipFile.copyFileTo(zipFile);
    else
        return false;
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
    xml.setAttribute("SampleRate", (int)_SampleRate);
    xml.setAttribute("BufferSize", (int)_BufferSize);
    xml.setAttribute("ConvBufferSize", (int)_ConvBufferSize);
    xml.setAttribute("MaxPartSize", (int)_MaxPartSize);
    xml.setAttribute("oscIn", _osc_in);
    xml.setAttribute("oscInPort", _osc_in_port);
    
    xml.setAttribute ("defaultFilterDir", defaultFilterDir.getFullPathName());
    
    xml.setAttribute("storeConfigDataInProject", _storeConfigDataInProject.get());
    
    // if filter has been loaded...
    if(convolverReady && _storeConfigDataInProject.get())
    {
        if (_tempConfigZipFile.existsAsFile())
        {
            MemoryBlock tempFileBlock;
            if (_tempConfigZipFile.loadFileAsData(tempFileBlock))
                xml.setAttribute("configData", tempFileBlock.toBase64Encoding());
        }
        
        xml.setAttribute("filterFileName", filterNameForStoring);
        xml.setAttribute("inputChannelsNumber",(int)tempInputChannels);
        
        /// save also fullpath of the config files, in case it is not possible to restored from saved data
        Array <File> files;
        defaultFilterDir.findChildFiles(files, File::findFiles, true, filterNameForStoring);
        if (files.size())
            xml.setAttribute ("targetWasInMenu", (bool)true);
        
        xml.setAttribute("filterFullPathName", targetFilter.getFullPathName());
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
            _osc_in_port        = xmlState->getIntAttribute("oscInPort", _osc_in_port);
            setOscIn(xmlState->getBoolAttribute("oscIn", false));
            
            String newFilterDir = xmlState->getStringAttribute("defaultFilterDir", defaultFilterDir.getFullPathName());
            
            File tempDir(newFilterDir);
            if (tempDir.exists())
            {
                defaultFilterDir = tempDir;
                SearchFilters(defaultFilterDir);
            }
            
            // -> default: don't store convolver data for existing projects --->> ??????
            _storeConfigDataInProject.set(xmlState->getIntAttribute("storeConfigDataInProject", 0));

            if (xmlState->hasAttribute("configData") && _storeConfigDataInProject.get())
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
                    filterNameToShow << " (saved within project)";
                    return;
                }
                
                /// if restoring from memory got troubles try to restore from filter menu
                targetFilter = File(xmlState->getStringAttribute("filterFullPathName", "/IDONTEXIST")) ;
                if(xmlState->getBoolAttribute("targetWasInMenu",false))
                {
                    /// find the filter index from the rebuilt list of filter files
                    DefaultElementComparator<File> comparator;
                    unsigned int filterIndex = filterFilesList.indexOfSorted(comparator, targetFilter);
                    if (filterIndex != -1)
                    {
                        storedInChannels = xmlState->getIntAttribute("inputChannelsNumber", 0);
                        LoadFilterFromMenu(filterIndex);
                        filterNameToShow << " (saved within project)";
                        return;
                    }
                    filterNameToShow = targetFilter.getFileNameWithoutExtension();
                    addNewStatus("ERROR: filter not found in the library folder!");
                    
                    DebugPrint("ERROR: filter not found in the library folder!\n\n");
                    return;
                }
                
                /// or original forlder if filter file was taken outside the menu
                if(targetFilter.existsAsFile())
                {
                    storedInChannels = xmlState->getIntAttribute("inputChannelsNumber", 0);
                    LoadFilterFromFile(targetFilter);
                    filterNameToShow << " (saved within project)";
                    return;
                }

                String debug;
                debug << "ERROR: filter not found in the original folder!";
                filterNameToShow = targetFilter.getFileNameWithoutExtension();
                addNewStatus(debug);
                
                DebugPrint(debug << "\n\n");
            }
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


