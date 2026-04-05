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

//==============================================================================
Mcfx_anythingAudioProcessor::Mcfx_anythingAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::discreteChannels(NUM_CHANNELS), true)
                      .withOutput ("Output", juce::AudioChannelSet::discreteChannels(NUM_CHANNELS), true))
{
    addDefaultFormatsToManager (_formatManager);

    // Load cached plugin list from disk so we don't need to rescan
    loadPluginListFromCache();

    // Auto-save plugin list whenever it changes (e.g. after scanning)
    _knownPluginList.addChangeListener (&_pluginListChangeListener);
}

Mcfx_anythingAudioProcessor::~Mcfx_anythingAudioProcessor()
{
    _paramSyncTimer.stopTimer();
    _knownPluginList.removeChangeListener (&_pluginListChangeListener);
    unloadPlugin();
}

//==============================================================================

const String Mcfx_anythingAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int Mcfx_anythingAudioProcessor::getNumParameters()
{
    return totalNumParams;
}

float Mcfx_anythingAudioProcessor::getParameter (int index)
{
    return 0.0f;
}

void Mcfx_anythingAudioProcessor::setParameter (int index, float newValue)
{
}

const String Mcfx_anythingAudioProcessor::getParameterName (int index)
{
    return String();
}

const String Mcfx_anythingAudioProcessor::getParameterText (int index)
{
    return String();
}

const String Mcfx_anythingAudioProcessor::getInputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

const String Mcfx_anythingAudioProcessor::getOutputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

bool Mcfx_anythingAudioProcessor::isInputChannelStereoPair (int index) const   { return true; }
bool Mcfx_anythingAudioProcessor::isOutputChannelStereoPair (int index) const  { return true; }

bool Mcfx_anythingAudioProcessor::acceptsMidi() const    { return false; }
bool Mcfx_anythingAudioProcessor::producesMidi() const   { return false; }
bool Mcfx_anythingAudioProcessor::silenceInProducesSilenceOut() const { return false; }

double Mcfx_anythingAudioProcessor::getTailLengthSeconds() const
{
    if (_currentPluginDesc != nullptr && _pluginInstances.size() > 0)
        return _pluginInstances.getUnchecked(0)->getTailLengthSeconds();
    return 0.0;
}

int Mcfx_anythingAudioProcessor::getNumPrograms()    { return 0; }
int Mcfx_anythingAudioProcessor::getCurrentProgram() { return 0; }
void Mcfx_anythingAudioProcessor::setCurrentProgram (int index) {}
const String Mcfx_anythingAudioProcessor::getProgramName (int index) { return String(); }
void Mcfx_anythingAudioProcessor::changeProgramName (int index, const String& newName) {}

//==============================================================================

bool Mcfx_anythingAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::discreteChannels(NUM_CHANNELS))
        return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::discreteChannels(NUM_CHANNELS))
        return false;
    return true;
}

void Mcfx_anythingAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    _currentSampleRate = sampleRate;
    _currentBlockSize = samplesPerBlock;

    // Prepare all loaded instances
    const ScopedLock lock (_pluginLock);
    for (auto* instance : _pluginInstances)
        instance->prepareToPlay (sampleRate, samplesPerBlock);

    // Resize scratch buffer for new block size
    int scratchCh = _totalInstanceChannels - _channelsPerInstance;
    if (scratchCh > 0)
        _scratchBuffer.setSize (scratchCh, samplesPerBlock, false, true);
}

void Mcfx_anythingAudioProcessor::releaseResources()
{
    const ScopedLock lock (_pluginLock);
    for (auto* instance : _pluginInstances)
        instance->releaseResources();
}

void Mcfx_anythingAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    if (! _pluginReady.load())
    {
        buffer.clear();
        return;
    }

    const ScopedTryLock lock (_pluginLock);
    if (! lock.isLocked())
    {
        buffer.clear();
        return;
    }

    const int totalChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const int mainBusCh = _channelsPerInstance;       // stride through our buffer
    const int pluginTotalCh = _totalInstanceChannels; // what the plugin expects
    const int scSourceCh = _sidechainSourceChannel;   // sidechain source (-1 = off)
    const int scNumCh = _sidechainNumChannels;

    MidiBuffer emptyMidi;

    // Track how many channels are actually processed by plugin instances
    int channelsProcessed = 0;

    for (int i = 0; i < _numInstances; ++i)
    {
        auto* instance = _pluginInstances.getUnchecked (i);
        const int startChannel = i * mainBusCh;

        if (startChannel + mainBusCh > totalChannels)
            break;

        // Build a sub-buffer for processBlock:
        // - channels 0..mainBusCh-1 reference our main multichannel buffer
        // - channels mainBusCh..pluginTotalCh-1 reference scratch (sidechain/aux)
        float* channelPtrs[256];
        jassert (pluginTotalCh <= 256);

        for (int ch = 0; ch < mainBusCh; ++ch)
            channelPtrs[ch] = buffer.getWritePointer (startChannel + ch);

        // Clear scratch buffer for this instance (sidechain + any aux channels)
        if (_scratchBuffer.getNumChannels() > 0)
            _scratchBuffer.clear (0, numSamples);

        for (int ch = mainBusCh; ch < pluginTotalCh; ++ch)
            channelPtrs[ch] = _scratchBuffer.getWritePointer (ch - mainBusCh);

        // Route sidechain: copy source channel data into sidechain channels
        if (_hasSidechain && scSourceCh >= 0 && scSourceCh < totalChannels && scNumCh > 0)
        {
            const float* src = buffer.getReadPointer (scSourceCh);
            for (int ch = 0; ch < scNumCh; ++ch)
                FloatVectorOperations::copy (channelPtrs[mainBusCh + ch], src, numSamples);
        }

        AudioBuffer<float> subBuffer (channelPtrs, pluginTotalCh, numSamples);
        instance->processBlock (subBuffer, emptyMidi);

        channelsProcessed = startChannel + mainBusCh;
    }

    // Silence any remaining channels not covered by plugin instances
    for (int ch = channelsProcessed; ch < totalChannels; ++ch)
        buffer.clear (ch, 0, numSamples);
}

//==============================================================================

AudioProcessorEditor* Mcfx_anythingAudioProcessor::createEditor()
{
    return new Mcfx_anythingAudioProcessorEditor (this);
}

bool Mcfx_anythingAudioProcessor::hasEditor() const
{
    return true;
}

//==============================================================================

void Mcfx_anythingAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    auto xml = std::make_unique<XmlElement> ("MCFX_ANYTHING_STATE");

    // Save known plugin list
    auto knownPluginsXml = _knownPluginList.createXml();
    if (knownPluginsXml)
        xml->addChildElement (knownPluginsXml.release());

    // Save current plugin description
    if (_currentPluginDesc)
    {
        auto descXml = _currentPluginDesc->createXml();
        if (descXml)
        {
            descXml->setTagName ("CURRENT_PLUGIN");
            xml->addChildElement (descXml.release());
        }
    }

    // Save sidechain routing
    if (_hasSidechain && _sidechainSourceChannel >= 0)
        xml->setAttribute ("sidechainSourceChannel", _sidechainSourceChannel);

    // Save hosted plugin state (from master instance)
    if (_currentPluginDesc != nullptr && _pluginInstances.size() > 0)
    {
        MemoryBlock pluginState;
        _pluginInstances.getUnchecked(0)->getStateInformation (pluginState);
        xml->createNewChildElement ("PLUGIN_STATE")
            ->addTextElement (pluginState.toBase64Encoding());
    }

    copyXmlToBinary (*xml, destData);
}

void Mcfx_anythingAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (! xml)
        return;

    // Restore known plugin list
    if (auto* knownPluginsXml = xml->getChildByName ("KNOWNPLUGINS"))
        _knownPluginList.recreateFromXml (*knownPluginsXml);

    // Restore sidechain routing (applied after plugin loads)
    if (xml->hasAttribute ("sidechainSourceChannel"))
        _sidechainSourceChannel = xml->getIntAttribute ("sidechainSourceChannel", -1);

    // Restore current plugin description and schedule deferred loading
    if (auto* descXml = xml->getChildByName ("CURRENT_PLUGIN"))
    {
        _pendingPluginDesc = std::make_unique<PluginDescription>();
        if (_pendingPluginDesc->loadFromXml (*descXml))
        {
            // Store the hosted plugin state for applying after load
            if (auto* stateXml = xml->getChildByName ("PLUGIN_STATE"))
            {
                _pendingPluginState.reset();
                _pendingPluginState.fromBase64Encoding (stateXml->getAllSubText());
            }

            _deferredLoader.triggerAsyncUpdate();
        }
        else
        {
            _pendingPluginDesc.reset();
        }
    }
}

//==============================================================================
// Persistent plugin list cache

File Mcfx_anythingAudioProcessor::getPluginListCacheFile()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
               .getChildFile ("mcfx_anything")
               .getChildFile ("pluginList.xml");
}

void Mcfx_anythingAudioProcessor::savePluginListToCache()
{
    auto cacheFile = getPluginListCacheFile();
    cacheFile.getParentDirectory().createDirectory();

    if (auto xml = _knownPluginList.createXml())
        xml->writeTo (cacheFile);
}

void Mcfx_anythingAudioProcessor::loadPluginListFromCache()
{
    auto cacheFile = getPluginListCacheFile();
    if (! cacheFile.existsAsFile())
        return;

    if (auto xml = parseXML (cacheFile))
        _knownPluginList.recreateFromXml (*xml);
}

//==============================================================================
// Plugin hosting

void Mcfx_anythingAudioProcessor::disableNonMainBuses (AudioPluginInstance& instance)
{
    // Method 1: Try setting a complete layout with non-main buses disabled,
    // but keep the sidechain input bus (bus 1) enabled.
    {
        auto layout = instance.getBusesLayout();
        bool needsChange = false;

        for (int b = 1; b < layout.inputBuses.size(); ++b)
        {
            // Keep input bus 1 (sidechain) enabled
            if (b == 1)
                continue;

            if (! layout.inputBuses.getReference (b).isDisabled())
            {
                layout.inputBuses.getReference (b) = AudioChannelSet::disabled();
                needsChange = true;
            }
        }
        for (int b = 1; b < layout.outputBuses.size(); ++b)
        {
            if (! layout.outputBuses.getReference (b).isDisabled())
            {
                layout.outputBuses.getReference (b) = AudioChannelSet::disabled();
                needsChange = true;
            }
        }

        if (needsChange)
        {
            if (instance.setBusesLayout (layout))
            {
                fprintf (stderr, "  Successfully disabled auxiliary buses via setBusesLayout (kept sidechain)\n");
                return;
            }
            fprintf (stderr, "  setBusesLayout failed, trying individual bus disable\n");
        }
    }

    // Method 2: Fallback — disable individual buses, skip input bus 1 (sidechain)
    for (int b = instance.getBusCount (true) - 1; b >= 1; --b)
    {
        // Keep input bus 1 (sidechain) enabled
        if (b == 1)
            continue;

        if (auto* bus = instance.getBus (true, b))
        {
            bool ok = bus->enable (false);
            fprintf (stderr, "  Disabling input bus %d (%s, %dch): %s\n",
                     b, bus->getName().toRawUTF8(), bus->getNumberOfChannels(),
                     ok ? "OK" : "FAILED");
        }
    }

    for (int b = instance.getBusCount (false) - 1; b >= 1; --b)
    {
        if (auto* bus = instance.getBus (false, b))
        {
            bool ok = bus->enable (false);
            fprintf (stderr, "  Disabling output bus %d (%s, %dch): %s\n",
                     b, bus->getName().toRawUTF8(), bus->getNumberOfChannels(),
                     ok ? "OK" : "FAILED");
        }
    }
}

int Mcfx_anythingAudioProcessor::probeReducedMainBus (AudioPluginInstance& instance, int fullChannels)
{
    // Try progressively smaller main-bus sizes and see if the plugin accepts
    // them. We try stereo first, then mono — these are the sizes Waves
    // "packed sidechain" plugins typically expose as their non-sidechain
    // layout.
    for (int target : { 2, 1 })
    {
        if (target >= fullChannels)
            continue;

        auto layout = instance.getBusesLayout();
        if (layout.inputBuses.size() > 0)
            layout.inputBuses.getReference (0) = AudioChannelSet::canonicalChannelSet (target);
        if (layout.outputBuses.size() > 0)
            layout.outputBuses.getReference (0) = AudioChannelSet::canonicalChannelSet (target);

        if (instance.checkBusesLayoutSupported (layout))
            return target;
    }
    return 0;
}

bool Mcfx_anythingAudioProcessor::applyReducedMainBus (AudioPluginInstance& instance, int targetChannels)
{
    instance.releaseResources();
    auto layout = instance.getBusesLayout();
    if (layout.inputBuses.size() > 0)
        layout.inputBuses.getReference (0) = AudioChannelSet::canonicalChannelSet (targetChannels);
    if (layout.outputBuses.size() > 0)
        layout.outputBuses.getReference (0) = AudioChannelSet::canonicalChannelSet (targetChannels);
    return instance.setBusesLayout (layout);
}

void Mcfx_anythingAudioProcessor::loadPlugin (const PluginDescription& desc)
{
    jassert (MessageManager::getInstance()->isThisTheMessageThread());

    String errorMessage;

    // Create the first instance to determine channel configuration
    auto firstInstance = _formatManager.createPluginInstance (desc, _currentSampleRate, _currentBlockSize, errorMessage);
    if (! firstInstance)
    {
        DBG ("Failed to load plugin: " + errorMessage);
        return;
    }

    // Log bus layout BEFORE any modifications
    {
        fprintf (stderr, "=== Plugin bus layout BEFORE disabling non-main buses ===\n");
        fprintf (stderr, "  Input buses: %d\n", firstInstance->getBusCount (true));
        for (int b = 0; b < firstInstance->getBusCount (true); ++b)
            if (auto* bus = firstInstance->getBus (true, b))
                fprintf (stderr, "    Input bus %d: \"%s\" %dch [%s] layout: %s\n",
                         b, bus->getName().toRawUTF8(), bus->getNumberOfChannels(),
                         bus->isEnabled() ? "enabled" : "disabled",
                         bus->getCurrentLayout().getDescription().toRawUTF8());
        fprintf (stderr, "  Output buses: %d\n", firstInstance->getBusCount (false));
        for (int b = 0; b < firstInstance->getBusCount (false); ++b)
            if (auto* bus = firstInstance->getBus (false, b))
                fprintf (stderr, "    Output bus %d: \"%s\" %dch [%s] layout: %s\n",
                         b, bus->getName().toRawUTF8(), bus->getNumberOfChannels(),
                         bus->isEnabled() ? "enabled" : "disabled",
                         bus->getCurrentLayout().getDescription().toRawUTF8());
        fprintf (stderr, "  Main bus: %din/%dout\n",
                 firstInstance->getMainBusNumInputChannels(),
                 firstInstance->getMainBusNumOutputChannels());
        fprintf (stderr, "  Total: %din/%dout\n",
                 firstInstance->getTotalNumInputChannels(),
                 firstInstance->getTotalNumOutputChannels());
    }

    // Try to disable non-main buses (sidechain, aux) so the plugin only
    // processes the main bus channels.  Some plugins (e.g. Waves VST2)
    // don't support disabling sidechain buses, so we handle both cases.
    disableNonMainBuses (*firstInstance);
    firstInstance->prepareToPlay (_currentSampleRate, _currentBlockSize);

    // Log bus layout AFTER modifications
    {
        fprintf (stderr, "=== Plugin bus layout AFTER disabling non-main buses + prepareToPlay ===\n");
        for (int b = 0; b < firstInstance->getBusCount (true); ++b)
            if (auto* bus = firstInstance->getBus (true, b))
                fprintf (stderr, "    Input bus %d: \"%s\" %dch [%s] layout: %s\n",
                         b, bus->getName().toRawUTF8(), bus->getNumberOfChannels(),
                         bus->isEnabled() ? "enabled" : "disabled",
                         bus->getCurrentLayout().getDescription().toRawUTF8());
        for (int b = 0; b < firstInstance->getBusCount (false); ++b)
            if (auto* bus = firstInstance->getBus (false, b))
                fprintf (stderr, "    Output bus %d: \"%s\" %dch [%s] layout: %s\n",
                         b, bus->getName().toRawUTF8(), bus->getNumberOfChannels(),
                         bus->isEnabled() ? "enabled" : "disabled",
                         bus->getCurrentLayout().getDescription().toRawUTF8());
        fprintf (stderr, "  Main bus: %din/%dout\n",
                 firstInstance->getMainBusNumInputChannels(),
                 firstInstance->getMainBusNumOutputChannels());
        fprintf (stderr, "  Total: %din/%dout\n",
                 firstInstance->getTotalNumInputChannels(),
                 firstInstance->getTotalNumOutputChannels());
    }

    // Main bus channels = our stride through the multichannel buffer.
    // This is the "useful" channel count per instance.
    int pluginInputCh  = firstInstance->getMainBusNumInputChannels();
    int pluginOutputCh = firstInstance->getMainBusNumOutputChannels();
    int channelsPerInstance = jmax (pluginInputCh, pluginOutputCh);

    // Total channels the plugin expects in processBlock (may include
    // sidechain/aux buses that couldn't be disabled).
    int totalInputCh  = firstInstance->getTotalNumInputChannels();
    int totalOutputCh = firstInstance->getTotalNumOutputChannels();
    int totalInstanceChannels = jmax (totalInputCh, totalOutputCh);

    // Detect sidechain: input bus 1 that is still enabled
    bool hasSidechain = false;
    int sidechainNumChannels = 0;
    if (firstInstance->getBusCount (true) > 1)
    {
        if (auto* scBus = firstInstance->getBus (true, 1))
        {
            if (scBus->isEnabled() && scBus->getNumberOfChannels() > 0)
            {
                hasSidechain = true;
                sidechainNumChannels = scBus->getNumberOfChannels();
                fprintf (stderr, "  Sidechain detected: %d channels (%s)\n",
                         sidechainNumChannels, scBus->getName().toRawUTF8());
            }
        }
    }

    // Packed-sidechain detection: plugins like Waves expose e.g. a 4ch main
    // bus whose extra channels are an internal sidechain, and also support a
    // reduced (e.g. 2ch) main-bus layout for plain operation. If no regular
    // aux-bus sidechain was found and the main bus can be shrunk, treat the
    // size delta as a packed sidechain.
    bool packedCapable = false;
    int  packedReduced = 0;
    int  packedFull    = channelsPerInstance;
    if (! hasSidechain && channelsPerInstance > 1)
    {
        packedReduced = probeReducedMainBus (*firstInstance, channelsPerInstance);
        if (packedReduced > 0 && packedReduced < channelsPerInstance)
        {
            packedCapable        = true;
            hasSidechain         = true;
            sidechainNumChannels = packedFull - packedReduced;
            fprintf (stderr, "  Packed sidechain detected: full=%dch, reduced=%dch, sc=%dch\n",
                     packedFull, packedReduced, sidechainNumChannels);
        }
    }

    // For packed plugins, decide the operating layout based on whether a
    // sidechain source is currently selected. With no source we apply the
    // reduced layout so the plugin runs as a plain N-channel effect.
    const bool useReducedLayout = packedCapable && _sidechainSourceChannel < 0;
    if (useReducedLayout)
    {
        if (applyReducedMainBus (*firstInstance, packedReduced))
        {
            firstInstance->prepareToPlay (_currentSampleRate, _currentBlockSize);
            fprintf (stderr, "  Applied reduced main-bus layout (%dch, sidechain off)\n", packedReduced);

            pluginInputCh  = firstInstance->getMainBusNumInputChannels();
            pluginOutputCh = firstInstance->getMainBusNumOutputChannels();
            totalInputCh   = firstInstance->getTotalNumInputChannels();
            totalOutputCh  = firstInstance->getTotalNumOutputChannels();
            totalInstanceChannels = jmax (totalInputCh, totalOutputCh);
        }
        else
        {
            fprintf (stderr, "  WARNING: failed to apply reduced main-bus layout\n");
            firstInstance->prepareToPlay (_currentSampleRate, _currentBlockSize);
        }
    }

    // For packed plugins the stride through the multichannel buffer is the
    // *reduced* main-bus size regardless of the current layout. When the full
    // layout is active, the extra channels are scratch/sidechain slots that
    // the processBlock loop fills from the selected sidechain source.
    if (packedCapable)
        channelsPerInstance = packedReduced;

    fprintf (stderr, "=== Final configuration ===\n");
    fprintf (stderr, "  Stride (channelsPerInstance): %d\n", channelsPerInstance);
    fprintf (stderr, "  ProcessBlock buffer (totalInstanceChannels): %d\n", totalInstanceChannels);
    fprintf (stderr, "  Sidechain: %s (%d ch)\n", hasSidechain ? "YES" : "no", sidechainNumChannels);
    fprintf (stderr, "  NUM_CHANNELS: %d\n", NUM_CHANNELS);

    if (channelsPerInstance == 0)
        channelsPerInstance = 2; // fallback
    if (totalInstanceChannels < channelsPerInstance)
        totalInstanceChannels = channelsPerInstance;

    int numNeeded = NUM_CHANNELS / channelsPerInstance;
    if (numNeeded == 0)
        numNeeded = 1;

    fprintf (stderr, "  Instances needed: %d (stride %d x %d = %d of %d channels)\n",
             numNeeded, numNeeded, channelsPerInstance,
             numNeeded * channelsPerInstance, NUM_CHANNELS);

    OwnedArray<AudioPluginInstance> newInstances;
    newInstances.add (firstInstance.release());

    // Create additional instances
    for (int i = 1; i < numNeeded; ++i)
    {
        auto instance = _formatManager.createPluginInstance (desc, _currentSampleRate, _currentBlockSize, errorMessage);
        if (! instance)
        {
            DBG ("Failed to create plugin instance " + String(i) + ": " + errorMessage);
            return;
        }
        disableNonMainBuses (*instance);
        if (useReducedLayout)
            applyReducedMainBus (*instance, packedReduced);
        instance->prepareToPlay (_currentSampleRate, _currentBlockSize);
        newInstances.add (instance.release());
    }

    // Keep old instances alive until the editor has released its references.
    // The editor holds a hosted plugin GUI that references the old plugin
    // instance; destroying instances before the editor releases its GUI
    // causes a pure virtual call in VSTPluginWindow::closePluginWindow().
    OwnedArray<AudioPluginInstance> oldInstances;

    // Swap instances atomically
    {
        const ScopedLock lock (_pluginLock);
        _pluginReady.store (false);
        _paramSyncTimer.stopTimer();

        // Move old instances out — they stay alive until after the editor updates
        while (_pluginInstances.size() > 0)
            oldInstances.add (_pluginInstances.removeAndReturn (0));

        while (newInstances.size() > 0)
            _pluginInstances.add (newInstances.removeAndReturn (0));

        _pluginNumInputChannels  = pluginInputCh;
        _pluginNumOutputChannels = pluginOutputCh;
        _channelsPerInstance     = channelsPerInstance;
        _totalInstanceChannels   = totalInstanceChannels;
        _numInstances            = _pluginInstances.size();
        _currentPluginDesc       = std::make_unique<PluginDescription> (desc);
        _hasSidechain            = hasSidechain;
        _sidechainNumChannels    = sidechainNumChannels;
        _packedSidechainCapable  = packedCapable;
        _packedReducedChannels   = packedReduced;
        _packedFullChannels      = packedFull;
        // Keep existing sidechain source if valid, otherwise reset
        if (! hasSidechain)
            _sidechainSourceChannel = -1;

        // Allocate scratch buffer for sidechain/aux channels that the plugin
        // expects in processBlock but don't map to our multichannel I/O
        int scratchChannels = totalInstanceChannels - channelsPerInstance;
        if (scratchChannels > 0)
            _scratchBuffer.setSize (scratchChannels, _currentBlockSize, false, true);
        else
            _scratchBuffer.setSize (0, 0);

        // Snapshot initial parameter values for change detection
        auto masterParams = _pluginInstances.getFirst()->getParameters();
        _lastParameterValues.resize ((size_t) masterParams.size());
        for (int p = 0; p < masterParams.size(); ++p)
            _lastParameterValues[(size_t) p] = masterParams[p]->getValue();

        // NOTE: _pluginReady remains false here — the audio thread must NOT
        // process through the new instances until the editor has finished
        // creating the hosted plugin GUI (some plugins, e.g. Waves AU,
        // are not safe to process audio while their view is being created).
    }

    // Synchronous notification — the editor destroys its old hosted GUI and
    // creates the new one here, while oldInstances are still alive.
    sendSynchronousChangeMessage();

    // Now that the editor has finished creating the plugin GUI, it is safe
    // for the audio thread to start processing through the new instances.
    _pluginReady.store (true);

    // Start polling parameters for sync (50ms interval)
    _paramSyncTimer.startTimer (50);

    // oldInstances destroyed here safely — no editor references them anymore.
}

void Mcfx_anythingAudioProcessor::unloadPlugin()
{
    jassert (MessageManager::getInstance()->isThisTheMessageThread());

    // Step 1: Stop audio processing through the plugin instances BEFORE
    // destroying any editors. processBlock will now silence the output.
    {
        const ScopedLock lock (_pluginLock);
        _pluginReady.store (false);
        _paramSyncTimer.stopTimer();
    }

    // Step 2: Destroy hosted plugin editor(s) directly while instances are
    // still fully alive and present in _pluginInstances. This avoids the
    // deep recursive stack that sendSynchronousChangeMessage would create,
    // which some plugins (e.g. TDR Kotelnikov) cannot handle safely during
    // ~VST3PluginWindow -> IPlugView::removed().
    if (auto* ed = dynamic_cast<Mcfx_anythingAudioProcessorEditor*> (getActiveEditor()))
        ed->destroyHostedEditorsForUnload();

    // Step 3: Now tear down the plugin instances. They are moved to a local
    // OwnedArray so we can hold the processor lock only briefly.
    OwnedArray<AudioPluginInstance> oldInstances;
    {
        const ScopedLock lock (_pluginLock);

        while (_pluginInstances.size() > 0)
            oldInstances.add (_pluginInstances.removeAndReturn (0));

        _currentPluginDesc.reset();
        _channelsPerInstance = 0;
        _totalInstanceChannels = 0;
        _numInstances = 0;
        _pluginNumInputChannels = 0;
        _pluginNumOutputChannels = 0;
        _hasSidechain = false;
        _sidechainNumChannels = 0;
        _sidechainSourceChannel = -1;
        _packedSidechainCapable = false;
        _packedReducedChannels = 0;
        _packedFullChannels = 0;
        _scratchBuffer.setSize (0, 0);
    }

    // Step 4: Notify editor to refresh its UI ("No plugin loaded" state).
    // Hosted editor is already gone, so this is just a repaint trigger.
    sendChangeMessage();

    // oldInstances destroyed here
}

AudioPluginInstance* Mcfx_anythingAudioProcessor::getMasterInstance() const
{
    if (_currentPluginDesc != nullptr && _pluginInstances.size() > 0)
        return _pluginInstances.getUnchecked (0);
    return nullptr;
}

AudioPluginInstance* Mcfx_anythingAudioProcessor::getInstance (int index) const
{
    if (_currentPluginDesc != nullptr && index >= 0 && index < _pluginInstances.size())
        return _pluginInstances.getUnchecked (index);
    return nullptr;
}

String Mcfx_anythingAudioProcessor::getLoadedPluginName() const
{
    if (_currentPluginDesc)
        return _currentPluginDesc->name;
    return String();
}

void Mcfx_anythingAudioProcessor::setSidechainSourceChannel (int channel)
{
    const int newValue = (channel >= 0 && channel < NUM_CHANNELS) ? channel : -1;
    if (newValue == _sidechainSourceChannel)
        return;

    const bool wasOff   = _sidechainSourceChannel < 0;
    const bool willBeOff = newValue < 0;
    const bool crossing = (wasOff != willBeOff);

    _sidechainSourceChannel = newValue;

    // For packed-sidechain plugins, crossing the on/off boundary requires
    // reloading the plugin with a different main-bus layout (reduced vs
    // full). We save and restore the plugin state across the reload so the
    // user doesn't lose parameter settings.
    if (_packedSidechainCapable && crossing && _currentPluginDesc != nullptr)
    {
        MemoryBlock savedState;
        if (_pluginInstances.size() > 0)
            _pluginInstances.getUnchecked (0)->getStateInformation (savedState);

        PluginDescription descCopy = *_currentPluginDesc;
        loadPlugin (descCopy);

        if (savedState.getSize() > 0)
        {
            const ScopedLock lock (_pluginLock);
            for (auto* instance : _pluginInstances)
                instance->setStateInformation (savedState.getData(),
                                               (int) savedState.getSize());
        }
    }
}

void Mcfx_anythingAudioProcessor::handleDeferredLoad()
{
    if (_pendingPluginDesc)
    {
        loadPlugin (*_pendingPluginDesc);

        // Apply saved plugin state to all instances
        if (_currentPluginDesc != nullptr && _pendingPluginState.getSize() > 0)
        {
            const ScopedLock lock (_pluginLock);
            for (auto* instance : _pluginInstances)
                instance->setStateInformation (_pendingPluginState.getData(),
                                               (int) _pendingPluginState.getSize());
        }

        _pendingPluginDesc.reset();
        _pendingPluginState.reset();
    }
}

//==============================================================================
// Parameter sync (timer-based polling)

void Mcfx_anythingAudioProcessor::syncParametersFromMaster()
{
    if (! _pluginReady.load() || _pluginInstances.size() <= 1)
        return;

    auto* master = _pluginInstances.getUnchecked (0);
    auto masterParams = master->getParameters();
    const int numParams = masterParams.size();

    if ((int) _lastParameterValues.size() != numParams)
    {
        _lastParameterValues.resize ((size_t) numParams);
        for (int p = 0; p < numParams; ++p)
            _lastParameterValues[(size_t) p] = masterParams[p]->getValue();
        return;
    }

    // Check each parameter for changes and sync to slaves
    for (int p = 0; p < numParams; ++p)
    {
        float currentValue = masterParams[p]->getValue();
        if (currentValue != _lastParameterValues[(size_t) p])
        {
            _lastParameterValues[(size_t) p] = currentValue;

            // Push to all slave instances
            for (int i = 1; i < _pluginInstances.size(); ++i)
            {
                auto* slave = _pluginInstances.getUnchecked (i);
                auto slaveParams = slave->getParameters();
                if (p < slaveParams.size())
                    slaveParams[p]->setValue (currentValue);
            }
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Mcfx_anythingAudioProcessor();
}
