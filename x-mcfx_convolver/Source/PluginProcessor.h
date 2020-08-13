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

#ifndef PLUGINPROCESSOR_H_INCLUDED
#define PLUGINPROCESSOR_H_INCLUDED

#include <JuceHeader.h>

#include "ConvolverData.h"

#ifdef USE_ZITA_CONVOLVER
    // in case you have problems with the other conv. engine
    #include <zita-convolver.h>
#else
    #include "MtxConv.h"
#endif


//==============================================================================
/**
*/
class Mcfx_convolverAudioProcessor  :   public AudioProcessor,
                                        public ChangeBroadcaster,
                                        public Thread,
                                        public Timer
{
public:
    //==============================================================================
    Mcfx_convolverAudioProcessor();
    ~Mcfx_convolverAudioProcessor();

    //==============================================================================
    void    prepareToPlay (double sampleRate, int samplesPerBlock);
    void    releaseResources();

    void    processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages);

    //==============================================================================
    AudioProcessorEditor*   createEditor();
    bool                    hasEditor() const;

    //==============================================================================
    const String    getName() const;

    int             getNumParameters();

    float           getParameter (int index);
    void            setParameter (int index, float newValue);

    const String    getParameterName (int index);
    const String    getParameterText (int index);

    const String    getInputChannelName (int channelIndex) const;
    const String    getOutputChannelName (int channelIndex) const;
    bool            isInputChannelStereoPair (int index) const;
    bool            isOutputChannelStereoPair (int index) const;

    bool            acceptsMidi() const;
    bool            producesMidi() const;
    bool            silenceInProducesSilenceOut() const;
    double          getTailLengthSeconds() const;

    //==============================================================================
    int     getNumPrograms();
    int     getCurrentProgram();
    void    setCurrentProgram (int index);
    const   String getProgramName (int index);
    void    changeProgramName (int index, const String& newName);

    //==============================================================================
    void    getStateInformation (MemoryBlock& destData);
    void    setStateInformation (const void* data, int sizeInBytes);
    
//    bool isBusesLayoutSupported (const BusesLayout& layouts) const;
    
    void            ReloadConfiguration(); //reload convolver the whole config

    // functions for gui control =======================================================
    // debug status text
    String          getDebugString();
    
    bool            newStatusText;
    String          getStatusText();
    
    //return the status of the convolver configuration -----------------------------------
    enum            ConvolverStatus {Loaded,Loading,Unloaded};
    int             getConvolverStatus();
    
    // Filter loading --------------------------------------------------------------------
    void            SearchFilters(File SearchFolder);
    void            setNewGlobalFilterFolder(File newGloablFolder);
    void            LoadFilterFromMenu(unsigned int filterIndex, bool restored = false);
    void            LoadFilterFromFile(File filterToLoad, bool restored = false);
    
    //returning parameter for gui --------------------------------------------------------
    double          getSamplerate();
    unsigned int    getBufferSize();
    unsigned int    getConvBufferSize();
    unsigned int    getMaxPartitionSize();
    int             getSkippedCyclesCount();
    
    //setting up parameter from gui ------------------------------------------------------
    void            setConvBufferSize(unsigned int bufsize);
    void            setMaxPartitionSize(unsigned int maxsize);
    
    // VARIABLES =========================================================================
    int             activeNumInputs;
    int             activeNumOutputs;
    int             activeNumIRs;
    double          filterLenghtInSecs;
    int             filterLenghtInSmpls;
    Atomic<bool>    wavefileHasBeenResampled;
    
    bool            changeNumInputChannels; //request from gui a reload with new input channels number
    enum            NumInputsStatus {agreed, missing, notMultiple, notFeasible, requested};
    NumInputsStatus numInputsStatus;
    int             tempNumInputs;
    Atomic<bool>    storeNumInputsIntoWav;
    
    //----------------------------------------------------------------------------
    File            defaultFilterDir; // where to search for presets
    File            lastSearchDir; // for open file dialog...
    
    Array<File>     filterFilesList;
    File            getTargetFilter();
    
    String          filterNameForStoring; // store filename
    String          filterNameToShow; // string for gui
    Atomic<bool>    restoredSettings;
    
    //----------------------------------------------------------------------------
    Atomic<int>     _readyToSaveConfiguration;
    Atomic<int>     _storeConfigDataInProject;
    
    Atomic<float>   masterGain;

private:
    String          _DebugText;
    CriticalSection _DebugTextMutex;
    void            DebugPrint(String debugText, bool reset = false);
    
    CriticalSection statusTextMutex;
    Array<String>   statusTextList;
    void            timerCallback();
    void            addNewStatus(String newStatus);
    
    void            LoadConfigurationAsync(File fileToLoad);
    void            run();
    void            LoadIRMatrixFilter(File filterFile);
    
    void            loadConvolver();
    void            unloadConvolver();
    
#ifdef USE_ZITA_CONVOLVER
    Convproc        zita_conv; /* zita-convolver engine class instances */
#else
    MtxConvMaster   mtxconv_;
#endif
    
    ConvolverData   conv_data;
    
    Array<int>      _conv_in;    // list with input routing
    Array<int>      _conv_out;   // list with output routing
    
    File            targetFilter;    //config file copy for thread
    CriticalSection targetFilterMutex;
    void            setTargetFilter(File newTargetFile);
    bool            holdNumInputChannel;
    
    File            _tempConfigZipFile;
    Array<File>     _cleanUpFilesOnExit;
    void            DeleteTemporaryFiles();
    
    double          _SampleRate;
    unsigned int    _BufferSize;        // size of the processing Block
    unsigned int    _ConvBufferSize;    // size of the head convolution block (possibility to make it larger in order to reduce CPU load)
    unsigned int    _MaxPartSize;       // maximum size of the partition
    
    int             storedInChannels;
    void            getInChannels(int waveFileLength);
    
    bool            changingPresetType;
    bool            convolverReady; //substitute for _configLoaded ande filterLoaded
    ConvolverStatus convolverStatus;
    CriticalSection convStatusMutex;
    void            setConvolverStatus(ConvolverStatus status);
    
	bool            _paramReload; // vst parameter to allow triggering reload of configuration
    Atomic<int>     _skippedCycles; // the number of skipped cycles do to unfinished partitions
    
    bool            _isProcessing;
    
    bool            safemode_; // this will add some latency for hosts that might send partial blocks, done automatically based on host type
    
    // ----------------------------------------------------------------------------
    bool            loadIr(AudioSampleBuffer*   IRBuffer,
                           const File&          audioFile,
                           int                  channel,
                           double               &samplerate,
                           float                gain=1.f,
                           int                  offset=0,
                           int                  length=0
                           );
    
    float*          storedGain;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Mcfx_convolverAudioProcessor)
};

#endif  // PLUGINPROCESSOR_H_INCLUDED
