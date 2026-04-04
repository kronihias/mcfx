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

//==============================================================================
class Mcfx_anythingAudioProcessor  : public AudioProcessor,
                                       public ChangeBroadcaster
{
public:
    //==============================================================================
    Mcfx_anythingAudioProcessor();
    ~Mcfx_anythingAudioProcessor();

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

    int getNumParameters() override;

    float getParameter (int index) override;
    void setParameter (int index, float newValue) override;

    const String getParameterName (int index) override;
    const String getParameterText (int index) override;

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

    // JUCE 8 removed AudioProcessor::setParameterNotifyingHost.
    void setParameterNotifyingHost(int parameterIndex, float newValue)
    {
        setParameter(parameterIndex, newValue);
    }

    enum Parameters
    {
        totalNumParams
    };

    //==============================================================================
    // Plugin hosting interface
    void loadPlugin (const PluginDescription& desc);
    void unloadPlugin();
    bool isPluginLoaded() const { return _currentPluginDesc != nullptr; }

    AudioPluginFormatManager& getFormatManager() { return _formatManager; }
    KnownPluginList& getKnownPluginList() { return _knownPluginList; }

    AudioPluginInstance* getMasterInstance() const;
    AudioPluginInstance* getInstance (int index) const;
    String getLoadedPluginName() const;
    int getNumInstances() const { return _numInstances; }
    int getChannelsPerInstance() const { return _channelsPerInstance; }
    int getTotalInstanceChannels() const { return _totalInstanceChannels; }

    // Deferred plugin loading (called from setStateInformation)
    void handleDeferredLoad();

    // Persistent plugin list cache (shared across all instances)
    void savePluginListToCache();
    void loadPluginListFromCache();
    static File getPluginListCacheFile();

private:
    //==============================================================================
    // Plugin hosting
    AudioPluginFormatManager _formatManager;
    KnownPluginList _knownPluginList;

    // Listener to auto-save plugin list when it changes (after scan)
    class PluginListChangeListener : public ChangeListener
    {
    public:
        PluginListChangeListener (Mcfx_anythingAudioProcessor& owner) : _owner (owner) {}
        void changeListenerCallback (ChangeBroadcaster*) override { _owner.savePluginListToCache(); }
    private:
        Mcfx_anythingAudioProcessor& _owner;
    };
    PluginListChangeListener _pluginListChangeListener { *this };

    // Loaded plugin instances
    OwnedArray<AudioPluginInstance> _pluginInstances;
    std::unique_ptr<PluginDescription> _currentPluginDesc;
    int _pluginNumInputChannels = 0;
    int _pluginNumOutputChannels = 0;
    int _channelsPerInstance = 0;       // main bus channels = stride through our multichannel buffer
    int _totalInstanceChannels = 0;     // total channels the plugin expects in processBlock (includes sidechain/aux)
    int _numInstances = 0;
    std::atomic<bool> _pluginReady { false };
    CriticalSection _pluginLock;

    // Scratch buffer for sidechain/aux bus channels that the plugin expects
    // but that don't map to our multichannel I/O
    AudioBuffer<float> _scratchBuffer;

    // Timer-based parameter sync (polls master and pushes to slaves)
    // This works with all plugin types including legacy setParameter() plugins.
    class ParameterSyncTimer : public Timer
    {
    public:
        ParameterSyncTimer (Mcfx_anythingAudioProcessor& owner) : _owner (owner) {}
        void timerCallback() override { _owner.syncParametersFromMaster(); }
    private:
        Mcfx_anythingAudioProcessor& _owner;
    };
    ParameterSyncTimer _paramSyncTimer { *this };
    void syncParametersFromMaster();
    std::vector<float> _lastParameterValues;

    // Deferred load support (for state restoration)
    class DeferredLoader : public AsyncUpdater
    {
    public:
        DeferredLoader (Mcfx_anythingAudioProcessor& owner) : _owner (owner) {}
        void handleAsyncUpdate() override { _owner.handleDeferredLoad(); }
    private:
        Mcfx_anythingAudioProcessor& _owner;
    };
    DeferredLoader _deferredLoader { *this };
    std::unique_ptr<PluginDescription> _pendingPluginDesc;
    MemoryBlock _pendingPluginState;

    // Helper: disable sidechain and auxiliary buses on a plugin instance
    static void disableNonMainBuses (AudioPluginInstance& instance);

    // Playback state
    double _currentSampleRate = 44100.0;
    int _currentBlockSize = 512;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Mcfx_anythingAudioProcessor)
};

#endif  // PLUGINPROCESSOR_H_INCLUDED
