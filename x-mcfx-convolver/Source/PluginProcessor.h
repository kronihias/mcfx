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
class Mcfx_convolverAudioProcessor  :   public AudioProcessor
                                        , public ChangeBroadcaster
                                        , public Thread
                                        , private AudioProcessorValueTreeState::Listener
//                                        , private juce::OSCReceiver
//                                        , private juce::OSCReceiver::ListenerWithOSCAddress<juce::OSCReceiver::MessageLoopCallback>
{
public:
    //==============================================================================
    Mcfx_convolverAudioProcessor();
    ~Mcfx_convolverAudioProcessor();
    
    /** Manage plugin channels
            
     it is currently not in used because the channels setup is performed in pre-building phase by cmake settings
     */
    //    bool isBusesLayoutSupported (const BusesLayout& layouts) const;

    //==============================================================================
    /// Block processing functions
    void    prepareToPlay (double sampleRate, int samplesPerBlock);
    void    releaseResources();

    void    processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages);

    //==============================================================================
    AudioProcessorEditor*   createEditor();
    bool                    hasEditor() const;
    const String            getName() const;

    //==============================================================================
    // Deprecated host passthrough parameters
    /*
    int             getNumParameters();

    float           getParameter (int index);
    void            setParameter (int index, float newValue);

    const String    getParameterName (int index);
    const String    getParameterText (int index);
    */

    //==============================================================================
    // ARE THESE FUNCTIONS REALLY NECESSARY?
    const String    getInputChannelName (int channelIndex) const;
    const String    getOutputChannelName (int channelIndex) const;
    bool            isInputChannelStereoPair (int index) const;
    bool            isOutputChannelStereoPair (int index) const;

    bool            acceptsMidi() const;
    bool            producesMidi() const;
    bool            silenceInProducesSilenceOut() const;
    double          getTailLengthSeconds() const;

    //==============================================================================
    // ARE THESE FUNCTIONS REALLY NECESSARY?
    int             getNumPrograms();
    int             getCurrentProgram();
    void            setCurrentProgram (int index);
    const           String getProgramName (int index);
    void            changeProgramName (int index, const String& newName);

    //==============================================================================
    /// Storing/restoring settings from memory (host project)
    void            getStateInformation (MemoryBlock& destData);
    void            setStateInformation (const void* data, int sizeInBytes);

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
    void            exportWavefileAsync(File newAudioFile);
    
    // Audio processing parameters  --------------------------------------------------------
    double          getSamplerate();
    unsigned int    getBufferSize();
    
    unsigned int    getConvBufferSize();
    void            setConvBufferSize(unsigned int bufsize);
    
    unsigned int    getMaxPartitionSize();
    void            setMaxPartitionSize(unsigned int maxsize);
    
    int             getSkippedCyclesCount();
    
    
    // =========================================================================
    /** Matrix filter file interfaces for GUI */
    
    /// Path to the directory to use for the filter menu build
    File            defaultFilterDir;
    
    /// Store the location of the last directory been questioned for a filter file outside menu
    File            lastSearchDir;
    
    /// It maintains the list of file of the menu of filter
    Array<File>     filterFilesList;
    
    /// Return the target filter file
    File            getTargetFilter();
    
    /// If target is oustide menu we need this for the correct storing of the name in the GUI
    Atomic<bool>    targetOutMenu;
    
    /// This label will be applied to the GUI combobox when a restore setting occurr or the filter is take oustide
    String          filterNameToShow;
    
    /// Employed in GUI when a filter has been restored from a save
    Atomic<bool>    restoredSettings;
    
    // =========================================================================
    /** Convolver setup variables */
    
    int             activeNumInputs;
    int             activeNumOutputs;
    int             activeNumIRs;
    double          filterLenghtInSecs;
    int             filterLenghtInSmpls;
    Atomic<bool>    wavefileHasBeenResampled;
    
    /// Number of input channels to adopt in the convolver loading
    int             numInputChannels;
    
    /// Triggered by a request of reload from GUI that need a different input channels number
    bool            changeNumInputChannels;
    
    /// Enumerator fot the number of input channels check when coming from GUI
    enum            NumInputsStatus {agreed, missing, notMultiple, notFeasible, requested};
    NumInputsStatus numInputsStatus;
    
    /// Represents the flag check og the GUI requesting to save the number of input channel into the metadata of wavefile
    Atomic<bool>    storeNumInputsIntoWav;
    
    // =========================================================================
    /**  Plug-in parameters  */
    AudioProcessorValueTreeState valueTreeState;
    AudioProcessorValueTreeState::ParameterLayout createParameters();
    
    void parameterChanged(const String& parameterID, float newValue);
    
    /// Represents raw value of parameters
    std::atomic<float>* masterGain = nullptr;
    std::atomic<float>* minPartSize = nullptr;
    std::atomic<float>* maxPartSize = nullptr;
    
    
    float previousGain;
    
    /// this is a safe chack to not trigger multiple action when restoring parameters from save
    Atomic<bool>    restoringFromMemory;
    
    //----------------------------------------------------------------------------
    /*
    void setOscIn(bool arg); // activate osc in
    bool getOscIn();
    void setOscInPort(int port);
    int getOscInPort();
    void oscMessageReceived(const OSCMessage& message);
     */

private:
    // old debug status windows TO BE REMOVED in future releases
    String              _DebugText;
    CriticalSection     _DebugTextMutex;
    void                DebugPrint(String debugText, bool reset = false);
    
    // new status bar text handling
    String              statusText;
    void                addNewStatus(String newStatus);
    
    // ===========================================================================
    /** FILTER LOADING  */
    
    /// Convolver data to configure to emply the convolver library
    ConvolverData       conv_data;
    Array<int>          _conv_in;    // list with input routing
    Array<int>          _conv_out;   // list with output routing
    
    /// this will add some latency for hosts that might send partial blocks,  done automatically based on host type
    bool                safemode_;
    
    /// It manages the target filter to load path
    File                targetFilter;    //config file copy for thread
    CriticalSection     targetFilterMutex;
    void                setTargetFilter(File newTargetFile);
    
    /// Starts the thread that handles the matrix of filters loading
    void                LoadConfigurationAsync(File fileToLoad);
    void                run();
    
    /// Manage the actual loading process.  It configures the convolver data from the IRs file
    void                LoadIRMatrixFilter(File filterFile);
    
    /// It manages the IR file load to the buffer, subdividing the wavefile channels into the right amount of samples per each filter
    bool                loadIr(AudioSampleBuffer*   IRBuffer,
                               const File&          audioFile,
                               int                  channel,
                               double               &samplerate,
                               float                gain=1.f,
                               int                  offset=0,
                               int                  length=0
                               );
    
    /// Employed in LoadIr for handling the IR file
    AudioSampleBuffer   bufferRead;
    
    /// This variable is activated during a reload that demand a different number of input channels (action triggered only from GUI)
    bool                holdNumInputChannel;
    
    /// Number of input channels is also stored in the host settings for those filter matrix files which do not embody the value by itself
    int                 storedInChannels;
    
    /// Apply controls on the input channles value coming from GUI
    void                getInChannels(int waveFileLength);
    
    /// Report and set the convolver library status to the GUI
    bool                convolverReady; //substitute for _configLoaded ande filterLoaded
    ConvolverStatus     convolverStatus;
    CriticalSection     convStatusMutex;
    void                setConvolverStatus(ConvolverStatus status);
    
    /// Manage the convolver library
    void                loadConvolver();
    void                unloadConvolver();
    
    /// Reload convolver for different parameters occurring
    void                ReloadConfiguration();
    
    // ===========================================================================
    /** AUDIO PROCESSING   */
    
    double              _SampleRate;
    unsigned int        _BufferSize;        // size of the processing Block
//    unsigned int        _ConvBufferSize;    // size of the head convolution block (possibility to make it larger in order to reduce CPU load)
//    unsigned int        _MaxPartSize;       // maximum size of the partition
    
    /// Convoling process (ext. library)
#ifdef USE_ZITA_CONVOLVER
    Convproc            zita_conv; /* zita-convolver engine class instances */
#else
    MtxConvMaster       mtxconv_;
#endif
    
    Atomic<int>         _skippedCycles; // the number of skipped cycles do to unfinished partitions
    bool                _isProcessing;
    
//	bool                _paramReload; // vst parameter to allow triggering reload of configuration
    
    // -----------------------------------------------------------------------------
    // OSC objects
    /*
    OSCReceiver oscReceiver;
    int osc_in_port;
    bool osc_in;
     */
     
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Mcfx_convolverAudioProcessor)
};

#endif  // PLUGINPROCESSOR_H_INCLUDED
