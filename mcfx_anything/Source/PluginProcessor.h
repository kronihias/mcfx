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

class Mcfx_anythingAudioProcessor;

//==============================================================================
// Forwarding parameter: one slot in a fixed-size pool of proxy parameters
// exposed to the DAW. Each slot may be bound to an inner-plugin parameter of
// the master AudioPluginInstance. Host-driven writes (automation) are
// deposited lock-free and applied to ALL instances at the same block
// boundary by processBlock. GUI-driven changes on the master plugin flow
// through the sync timer and are mirrored back to the host via
// setValueNotifyingHost on the proxy.
class ForwardingParameter : public HostedAudioProcessorParameter
{
public:
    explicit ForwardingParameter (int index);

    // Message-thread only: bind/unbind this slot to a master-instance parameter.
    void bind (AudioProcessorParameter* innerParam);
    void unbind();

    // Message-thread: called from the sync timer when the master plugin's GUI
    // moves its own parameter. Updates the cache and notifies the host so the
    // DAW can record automation. Does NOT set the dirty flag (would feed the
    // value back into the master instance in the next processBlock).
    void updateFromInner (float newValue);

    // Audio thread: if a host write is pending, writes the value to `out` and
    // clears the dirty flag, returning true. Otherwise returns false.
    bool consumePending (float& out) noexcept;

    bool isBound() const noexcept { return _innerParam != nullptr; }

    // HostedAudioProcessorParameter
    String getParameterID() const override { return _stableId; }

    // AudioProcessorParameter
    float  getValue() const override                        { return _cachedValue.load (std::memory_order_relaxed); }
    void   setValue (float newValue) override;
    float  getDefaultValue() const override;
    String getName (int maximumStringLength) const override;
    String getLabel() const override;
    String getText (float value, int maximumStringLength) const override;
    float  getValueForText (const String& text) const override;
    int    getNumSteps() const override;
    bool   isDiscrete() const override;
    bool   isBoolean() const override;
    bool   isAutomatable() const override                   { return true; }

private:
    const int _index;
    const String _stableId;   // "p000" .. "pNNN" — STABLE for lifetime of processor

    // Current value visible to the host (updated by both directions).
    std::atomic<float> _cachedValue   { 0.0f };
    // Pending host-driven write to broadcast in the next processBlock.
    std::atomic<float> _pendingValue  { 0.0f };
    std::atomic<bool>  _pendingDirty  { false };

    // Bound on the message thread from loadPlugin / unloadPlugin. Only read
    // from the message thread (sync timer + metadata getters). Never touched
    // from the audio thread — the audio thread talks to the instances
    // directly via their own parameter arrays.
    AudioProcessorParameter* _innerParam = nullptr;
};

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

    // Note: legacy getNumParameters / getParameter / setParameter / getParameterName
    // / getParameterText overrides were REMOVED because they shadowed the modern
    // AudioProcessorParameter pool (getNumParameters returning 0 would hide every
    // forwarding parameter from the host). The base class implementations route
    // correctly through the parameter list we populate via addParameter().

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

    // Audio-thread lock (taken briefly on the main thread when destroying
    // or creating plugin editors, to avoid races with processBlock on the
    // audio thread — some plugins crash if their editor is torn down while
    // their processAudio is concurrently running).
    CriticalSection& getPluginLock() { return _pluginLock; }
    void setPluginReady (bool ready) { _pluginReady.store (ready); }
    bool isPluginReady() const { return _pluginReady.load(); }

    // Sidechain routing
    bool hasSidechain() const { return _hasSidechain; }
    int getSidechainNumChannels() const { return _sidechainNumChannels; }
    int getSidechainSourceChannel() const { return _sidechainSourceChannel; }  // -1 = off
    void setSidechainSourceChannel (int channel);  // -1 = off, 0-based channel index

    // Deferred plugin loading (called from setStateInformation)
    void handleDeferredLoad();

    // Persistent plugin list cache (shared across all instances)
    void savePluginListToCache();
    void loadPluginListFromCache();
    static File getPluginListCacheFile();

    // Fixed-size pool of forwarding parameters exposed to the DAW. Each slot
    // can be dynamically bound to a parameter of the currently loaded plugin.
    // Declared at construction time; never resized (adding/removing
    // parameters after construction crashes many hosts).
    static constexpr int kForwardingParameterCount = 256;
    int  getForwardingParameterCount() const noexcept { return kForwardingParameterCount; }
    ForwardingParameter* getForwardingParameter (int index) const noexcept
    {
        return (index >= 0 && index < _forwardingParameters.size())
                 ? _forwardingParameters.getUnchecked (index) : nullptr;
    }

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

    // Fixed pool of forwarding parameters. Owned by AudioProcessor (via
    // addParameter) — the OwnedArray here only provides fast indexed access;
    // we do NOT delete through it.
    Array<ForwardingParameter*> _forwardingParameters;

    // Sidechain routing: route one of our input channels to the sidechain bus
    bool _hasSidechain = false;
    int _sidechainNumChannels = 0;       // number of channels in the sidechain bus
    int _sidechainSourceChannel = -1;    // -1 = off, 0-based index into our multichannel buffer

    // "Packed" sidechain support for plugins (e.g. Waves) that expose a larger
    // main bus where the extra channels are actually an internal sidechain,
    // and also support a reduced main bus for plain operation. When capable,
    // we load the plugin with the reduced layout when no sidechain source is
    // selected (so the plugin functions as a plain N-channel effect), and
    // with the full layout when a sidechain source is selected.
    bool _packedSidechainCapable = false;
    int _packedReducedChannels = 0;     // e.g. 2 for Waves 4ch→2ch
    int _packedFullChannels = 0;        // e.g. 4

    // Probe whether the plugin's main bus can be reduced to a smaller size;
    // returns the reduced channel count, or 0 if not reducible.
    static int probeReducedMainBus (AudioPluginInstance& instance, int fullChannels);
    // Apply a reduced main-bus layout (both input and output bus 0).
    static bool applyReducedMainBus (AudioPluginInstance& instance, int targetChannels);

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
