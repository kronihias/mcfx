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

#include "../JuceLibraryCode/JuceHeader.h"

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
    int getNumPrograms();
    int getCurrentProgram();
    void setCurrentProgram (int index);
    const String getProgramName (int index);
    void changeProgramName (int index, const String& newName);

    //==============================================================================
    void getStateInformation (MemoryBlock& destData);
    void setStateInformation (const void* data, int sizeInBytes);

    // use a thread to load a configuration
    void run();
    
    // do the loading in a background thread
    void LoadConfigurationAsync(File configFile);

    void LoadConfiguration(File configFile); // do the loading
    
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
    void oscMessageReceived(const OSCMessage& message);

    Atomic<int> _storeConfigDataInProject;

    File presetDir; // where to search for presets
    File lastDir; // for open file dialog...
    
    String activePreset; // store filename
    
    Array<File> _presetFiles;
    
    String box_preset_str;
    
    int _min_in_ch;
    int _min_out_ch;
    
    
    int _num_conv;
    
    File _configFile;
    

private:

    void DeleteTemporaryFiles();

    void DebugPrint(String debugText, bool reset=false);

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
