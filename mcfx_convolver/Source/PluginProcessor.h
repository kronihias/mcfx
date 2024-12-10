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

#include "JuceHeader.h"

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
class Mcfx_convolverAudioProcessor  : public AudioProcessor,
                                      public ChangeBroadcaster,
                                      public Thread,
                                      private OSCReceiver::ListenerWithOSCAddress<OSCReceiver::RealtimeCallback>,
                                      private AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    Mcfx_convolverAudioProcessor();
    ~Mcfx_convolverAudioProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override;

    //==============================================================================
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const String getName() const override;

    //==============================================================================
    // Deprecated host passthrough parameters
    /*
    int getNumParameters() override;

    float getParameter (int index) override;
    void setParameter (int index, float newValue) override;

    const String getParameterName (int index) override;
    const String getParameterText (int index) override;
    */

    // =========================================================================
    /**  Plug-in parameters  */
    AudioProcessorValueTreeState valueTreeState; //!< Value tree state containing the plug-in parameters
    AudioProcessorValueTreeState::ParameterLayout createParameters();   //!< Create the parameters of the plug-in
    
    /// Callback for AudioProcessorValueTreeState::Listener
    void parameterChanged(const String& parameterID, float newValue) override;
    float _previous_master_gain;                    //!< Previous RAW value of the master gain, used for interpolation
    std::atomic<float>* _master_gain_raw = nullptr; //!< Pointer to the RAW master gain parameter value
    // =========================================================================

    const String getInputChannelName (int channelIndex) const override;
    const String getOutputChannelName (int channelIndex) const override;
    bool isInputChannelStereoPair (int index) const override;
    bool isOutputChannelStereoPair (int index) const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool silenceInProducesSilenceOut() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // use a thread to load a configuration
    void run() override;

    // do the loading in a background thread
    void LoadConfigurationAsync(File configFile);

    void LoadConfiguration(File configFile); // do the loading

    int getWavInputChannelMetadata(File wavFile);

    void LoadWavFile(File wavFile, int numInputChannels = 0); // this will first create a .conf file, then load it async

    void UnloadConfiguration();

    void ReloadConfiguration();

    // for gui
    bool SaveConfiguration(File zipFile);
    Atomic<int> _readyToSaveConfiguration;

    String getDebugString();

    void SearchPresets(File SearchFolder);

    void LoadPreset(unsigned int preset);

    void LoadPresetByName(String presetName);

    unsigned int getBufferSize();
    unsigned int getConvBufferSize();
    unsigned int getMaxPartitionSize();
    void setConvBufferSize(unsigned int bufsize);
    void setMaxPartitionSize(unsigned int maxsize);

    int getSkippedCyclesCount()
    {
        return _skippedCycles.get();
    }

    void setOscIn(bool arg); // activate osc in
    bool getOscIn();
    void setOscInPort(int port);
    int getOscInPort();
    void oscMessageReceived(const OSCMessage& message) override;

    Atomic<int> _storeConfigDataInProject;

    File presetDir; // where to search for presets
    File lastDir; // for open file dialog...

    String activePreset; // store filename

    Array<File> _presetFiles;

    String box_preset_str;

    int _min_in_ch;
    int _min_out_ch;

    /// State of the preset load, used to communicate with the GUI. After "Loaded" is consumed by the gui. It will be set to "None"
    enum class PresetLoadState
    {
        None = -1,
        Failed = 0,
        Loaded = 1
    };
    std::atomic<PresetLoadState> _presetLoadState {PresetLoadState::None}; //!< State of the preset load, used to communicate with the GUI. After "Loaded" is consumed by the gui. It will be set to "None"
    /// Error message of the preset load, used to communicate with the GUI in the case that presetLoadState is "Failed"_
    enum class PresetLoadError
    {
        None = 0,                                   //!< No error
        CHANNEL_ASSIGNMENT_NOT_FEASIBLE,            //!< The channel assignment is not feasible
        NOT_LOADED,                                 //!< Generic error, from original plugin debug messages
        NUMBER_INPUT_CHANNELS_NOT_FEASIBLE,         //!< The number of input channels is not feasible
        WAV_LENGTH_NOT_MULTIPLE_INCHANNELS_IRLEN,   //!< The length of the wav file is not a multiple of the number of input channels specified manually
        FILE_DOES_NOT_EXIST,                        //!< The file does not exist
        CANT_READ_IR,                               //!< The IR file can't be read
        ZERO_SAMPLES,                               //!< The IR file has zero samples
        NOT_ENOUGH_CHANNELS                         //!< The Wav file has less channels than required

    };
    std::atomic<PresetLoadError> _presetLoadErrorMessage{ PresetLoadError::None};
    String presetWavFilename = "";

    int _num_conv;

    File _configFile;


    //==============================================================================
    // Saving the manually specified input channels into wav metadata
    Atomic<bool>    storeNumInputsIntoWav;
    int numInputChannels;
    //==============================================================================

    void DebugClear();
    void DebugPrint(const String& debugText);

private:
    void DeleteTemporaryFiles();


    String _DebugText;
    CriticalSection _DebugTextMutex;


    File _desConfigFile;

    File _tempConfigZipFile;
    Array<File> _cleanUpFilesOnExit;

    ConvolverData conv_data;

    Array<int> _conv_in; // list with input routing
    Array<int> _conv_out; // list with output routing


    double _SampleRate;
    unsigned int _BufferSize; // size of the processing Block
    unsigned int _ConvBufferSize; // size of the head convolution block (possibility to make it larger in order to reduce CPU load)
    unsigned int _MaxPartSize; // maximum size of the partition

    bool _isProcessing;

    bool _configLoaded; // is a configuration successfully loaded?

	bool _paramReload; // vst parameter to allow triggering reload of configuration

    Atomic<int> _skippedCycles; // the number of skipped cycles do to unfinished partitions

    bool loadIr(AudioSampleBuffer* IRBuffer, const File& audioFile, int channel, double &samplerate, float gain=1.f, int offset=0, int length=0);

#ifdef USE_ZITA_CONVOLVER
	Convproc zita_conv; /* zita-convolver engine class instances */
#else
    MtxConvMaster mtxconv_;
#endif

    bool safemode_; // this will add some latency for hosts that might send partial blocks, done automatically based on host type

    OSCReceiver oscReceiver;
    int _osc_in_port;
    bool _osc_in;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Mcfx_convolverAudioProcessor)
};

#endif  // PLUGINPROCESSOR_H_INCLUDED
