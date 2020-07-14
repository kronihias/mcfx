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

// #include "../JuceLibraryCode/JuceHeader.h"
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
                                        public Timer,
                                        private OSCReceiver::ListenerWithOSCAddress<OSCReceiver::RealtimeCallback>
{
public:
    //==============================================================================
    Mcfx_convolverAudioProcessor();
    ~Mcfx_convolverAudioProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock);
    void releaseResources();

    void processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages);

    //==============================================================================
    AudioProcessorEditor* createEditor();
    bool hasEditor() const;

    //==============================================================================
    const String getName() const;

    int getNumParameters();

    float getParameter (int index);
    void setParameter (int index, float newValue);

    const String getParameterName (int index);
    const String getParameterText (int index);

    const String getInputChannelName (int channelIndex) const;
    const String getOutputChannelName (int channelIndex) const;
    bool isInputChannelStereoPair (int index) const;
    bool isOutputChannelStereoPair (int index) const;

    bool acceptsMidi() const;
    bool producesMidi() const;
    bool silenceInProducesSilenceOut() const;
    double getTailLengthSeconds() const;

    //==============================================================================
    int     getNumPrograms();
    int     getCurrentProgram();
    void    setCurrentProgram (int index);
    const   String getProgramName (int index);
    void    changeProgramName (int index, const String& newName);

    //==============================================================================
    void getStateInformation (MemoryBlock& destData);
    void setStateInformation (const void* data, int sizeInBytes);
    
    bool isBusesLayoutSupported (const BusesLayout& layouts) const;

    // use a thread to load a configuration
    void run();
    
    // do the loading in a background thread
    void LoadConfigurationAsync(File presetFile, bool reload=false);
    void LoadConfiguration(File configFile); // do the loading
    void ReloadConfiguration(); //just reload convolver? nope
    
    void changePresetTypeAsync();
    
    void LoadIRMatrixFilter(File filterFile);
//    void LoadIRMatrixFilterAsync(File filterFile);
    
    void loadConvolver();
    void unloadConvolver();
    

    // for gui --------------------------------------------------------------------
    enum PresetType {conf, wav};
    PresetType presetType;
    
    String  getDebugString();
    
    bool newStatusText;
    void timerCallback();
    String  getStatusText();
    
    bool    SaveConfiguration(File zipFile);
    void    SearchPresets(File SearchFolder);
//    void    LoadPreset(unsigned int preset);
    void    LoadPresetFromMenu(unsigned int preset);
    void    LoadSetupFromFile(File settings);
    void    LoadPresetByName(String presetName);
    
    int     FindPresetIndex(File activePresetName);
    void    changePresetType(PresetType mode);
    
    //returning parameter for gui
    unsigned int    getBufferSize();
    unsigned int    getConvBufferSize();
    unsigned int    getMaxPartitionSize();
    int             getSkippedCyclesCount();

    void    setConvBufferSize(unsigned int bufsize);
    void    setMaxPartitionSize(unsigned int maxsize);

    
    //return the status of the convolver configuration
    enum ConvolverStatus {Loaded,Loading,Unloaded};
    int getConvolverStatus();
    
    // OSC functions --------------------------------------------------------------
    void    setOscIn(bool arg); // activate osc in
    bool    getOscIn();
    void    setOscInPort(int port);
    int     getOscInPort();
    void    oscMessageReceived(const OSCMessage& message);
    
    // VARIABLES ==================================================================
    int _min_in_ch;
    int _min_out_ch;
    int _num_conv;
    
    bool    inputChannelRequired; //going to deprecated
    enum    InChannelStatus {agreed, missing, unkwnown, notMultiple, notFeasible, requested};
    InChannelStatus inChannelStatus;
    int     tempInputChannels;
    bool    matrixIsDiagonal;
    
    //----------------------------------------------------------------------------
    File defaultPresetDir; // where to search for presets
    File lastSearchDir; // for open file dialog...
    
    Array<File> presetFilesList;
//    File configFileLoaded;
    File getTargetPreset();
    
    String activePresetName; // store filename (is it real necessary?)
    String presetName; // string for gui
    
    //----------------------------------------------------------------------------
    File IRlastDirectory;
//    File filterFileLoaded;
    
    //----------------------------------------------------------------------------
    Atomic<int> _readyToSaveConfiguration;
    Atomic<int> _storeConfigDataInProject;

private:
    String _DebugText;
    CriticalSection _DebugTextMutex;
    void DebugPrint(String debugText, bool reset = false);
    
    CriticalSection statusTextMutex;
    Array<String> statusTextList;
    void addNewStatus(String newStatus);
    
#ifdef USE_ZITA_CONVOLVER
    Convproc        zita_conv; /* zita-convolver engine class instances */
#else
    MtxConvMaster   mtxconv_;
#endif
    
    ConvolverData   conv_data;
    
    Array<int>      _conv_in;    // list with input routing
    Array<int>      _conv_out;   // list with output routing
    
    File            targetPreset;    //config file copy for thread
    CriticalSection targetPresetMutex;
    void            setTargetPreset(File newTargetPreset);
    bool            isAReload;
    
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
    
    // IR Filter Matrix -----------------------------------------------------------

    File            filterFileToLoad;
    
//    bool filterLoaded;
    
    // ----------------------------------------------------------------------------
    
    bool loadIr(AudioSampleBuffer* IRBuffer, const File& audioFile, int channel, double &samplerate, float gain=1.f, int offset=0, int length=0);
    
    // OSC ------------------------------------------------------------------------
    OSCReceiver     oscReceiver;
    int             _osc_in_port;
    bool            _osc_in;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Mcfx_convolverAudioProcessor)
};

#endif  // PLUGINPROCESSOR_H_INCLUDED
