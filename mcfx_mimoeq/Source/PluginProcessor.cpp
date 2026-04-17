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
#include "mcfx_buses.h"

Mcfx_mimoeqAudioProcessor::Mcfx_mimoeqAudioProcessor()
    : AudioProcessor (
#if MCFX_MULTICHANNEL_BUILD
          MCFX_MULTICHANEL_BUSES
#else
          BusesProperties()
              .withInput  ("Input",  juce::AudioChannelSet::discreteChannels(NUM_CHANNELS), true)
              .withOutput ("Output", juce::AudioChannelSet::discreteChannels(NUM_CHANNELS), true)
#endif
      ),
      apvts(*this, nullptr, "PARAMETERS", createParameters())
{
    // Register APVTS parameter listeners for host automation
    for (int i = 1; i <= kMaxAutomatedBands; ++i)
    {
        String prefix = "B" + String(i).paddedLeft('0', 2) + "_";
        apvts.addParameterListener(prefix + "Freq", this);
        apvts.addParameterListener(prefix + "Q", this);
        apvts.addParameterListener(prefix + "Gain", this);
        apvts.addParameterListener(prefix + "Enable", this);
    }

    // Default 6 bands like mcfx_filter: HP, low shelf, peak1, peak2, high shelf, LP
    // All neutral settings, HP/LP disabled by default

    // Band 1: High Pass (low cut)
    auto* hp = diagonalChain_.addBand();
    hp->setType(EqBandType::IIR);
    hp->setIIRSubType(IIRSubType::HighPass);
    hp->setFrequency(48.f);
    hp->setQ(0.707f);
    hp->setGainDB(0.f);
    hp->setDiagonal(true);
    hp->setEnabled(false);

    // Band 2: Low Shelf
    auto* ls = diagonalChain_.addBand();
    ls->setType(EqBandType::IIR);
    ls->setIIRSubType(IIRSubType::LowShelf);
    ls->setFrequency(94.f);
    ls->setQ(0.707f);
    ls->setGainDB(0.f);
    ls->setDiagonal(true);

    // Band 3: Peak 1
    auto* pk1 = diagonalChain_.addBand();
    pk1->setType(EqBandType::IIR);
    pk1->setIIRSubType(IIRSubType::Peak);
    pk1->setFrequency(186.f);
    pk1->setQ(0.707f);
    pk1->setGainDB(0.f);
    pk1->setDiagonal(true);

    // Band 4: Peak 2
    auto* pk2 = diagonalChain_.addBand();
    pk2->setType(EqBandType::IIR);
    pk2->setIIRSubType(IIRSubType::Peak);
    pk2->setFrequency(1428.f);
    pk2->setQ(0.707f);
    pk2->setGainDB(0.f);
    pk2->setDiagonal(true);

    // Band 5: High Shelf
    auto* hs = diagonalChain_.addBand();
    hs->setType(EqBandType::IIR);
    hs->setIIRSubType(IIRSubType::HighShelf);
    hs->setFrequency(2817.f);
    hs->setQ(0.707f);
    hs->setGainDB(0.f);
    hs->setDiagonal(true);

    // Band 6: Low Pass (high cut)
    auto* lp = diagonalChain_.addBand();
    lp->setType(EqBandType::IIR);
    lp->setIIRSubType(IIRSubType::LowPass);
    lp->setFrequency(10960.f);
    lp->setQ(0.707f);
    lp->setGainDB(0.f);
    lp->setDiagonal(true);
    lp->setEnabled(false);

    syncModelToAPVTS();
}

Mcfx_mimoeqAudioProcessor::~Mcfx_mimoeqAudioProcessor()
{
    delete activeState_;
    delete pendingState_.load(std::memory_order_acquire);
    delete garbageState_.load(std::memory_order_acquire);
}

void Mcfx_mimoeqAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate_ = sampleRate;
    currentBlockSize_ = samplesPerBlock;
    workBuffer_.setSize(getTotalNumInputChannels(), samplesPerBlock);
    inputSnapshot_.setSize(getTotalNumInputChannels(), samplesPerBlock);
    tempBuffer_.setSize(1, samplesPerBlock);
    mimoOutputMask_.assign(getTotalNumInputChannels(), false);

    int numCh = getTotalNumInputChannels();
    inputAnalyzer_.prepare(sampleRate, numCh);
    outputAnalyzer_.prepare(sampleRate, numCh);

    rebuildProcessingChains();
}

void Mcfx_mimoeqAudioProcessor::releaseResources()
{
}

bool Mcfx_mimoeqAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if MCFX_MULTICHANNEL_BUILD
    return mcfx::isMultichannelLayoutSupported (layouts, NUM_CHANNELS, wrapperType);
#else
    return layouts.getMainInputChannelSet().size()  == NUM_CHANNELS
        && layouts.getMainOutputChannelSet().size() == NUM_CHANNELS;
#endif
}

void Mcfx_mimoeqAudioProcessor::rebuildProcessingChains()
{
    // Clean up garbage from previous swap (safe: called on GUI/host thread)
    delete garbageState_.exchange(nullptr, std::memory_order_acquire);

    int numCh = getTotalNumInputChannels();
    auto* newState = new ProcessingState();
    newState->diagChannelMask = diagChannelMask_;

    // Build per-channel diagonal copies
    if (diagonalChain_.getNumBands() > 0)
    {
        var diagJson = diagonalChain_.toJson();
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* chChain = new EqChain();
            chChain->fromJson(diagJson, currentSampleRate_);
            chChain->prepare(currentSampleRate_, currentBlockSize_);
            newState->diagChannelChains.add(chChain);
        }
    }

    // Build per-path chain processing copies (including empty paths for passthrough)
    for (auto& kv : pathChains_)
    {
        auto* srcChain = kv.second;
        auto* copy = new EqChain();
        if (srcChain->getNumBands() > 0)
        {
            var pathJson = srcChain->toJson();
            copy->fromJson(pathJson, currentSampleRate_);
        }
        copy->prepare(currentSampleRate_, currentBlockSize_);
        newState->ownedPathChains.add(copy);
        newState->pathChains[kv.first] = copy;
    }

    // Compute and report maximum convolver latency across all chains
    int maxLatency = 0;
    for (auto* ch : newState->diagChannelChains)
        maxLatency = jmax(maxLatency, ch->getConvolverLatency());
    for (auto* ch : newState->ownedPathChains)
        maxLatency = jmax(maxLatency, ch->getConvolverLatency());
    setLatencySamples(maxLatency);

    // Publish lock-free — audio thread will pick up on next buffer
    auto* oldPending = pendingState_.exchange(newState, std::memory_order_release);
    delete oldPending;  // discard unconsumed pending state (if any)
}

EqChain* Mcfx_mimoeqAudioProcessor::getChainForPath(int inCh, int outCh)
{
    auto it = pathChains_.find({inCh, outCh});
    if (it != pathChains_.end())
        return it->second;
    return nullptr;
}

EqChain* Mcfx_mimoeqAudioProcessor::getOrCreateChainForPath(int inCh, int outCh)
{
    auto it = pathChains_.find({inCh, outCh});
    if (it != pathChains_.end())
        return it->second;

    auto* chain = new EqChain();
    chain->prepare(currentSampleRate_, currentBlockSize_);
    ownedPathChains_.add(chain);
    pathChains_[{inCh, outCh}] = chain;
    return chain;
}

bool Mcfx_mimoeqAudioProcessor::removePathChain(int inCh, int outCh)
{
    auto it = pathChains_.find({inCh, outCh});
    if (it == pathChains_.end())
        return false;

    EqChain* chain = it->second;
    pathChains_.erase(it);
    ownedPathChains_.removeObject(chain, true);
    return true;
}

std::vector<PathKey> Mcfx_mimoeqAudioProcessor::getPathKeys() const
{
    std::vector<PathKey> keys;
    keys.reserve(pathChains_.size());
    for (auto& kv : pathChains_)
        keys.push_back(kv.first);
    return keys;
}

void Mcfx_mimoeqAudioProcessor::doParamSyncIfNeeded()
{
    if (needsParamSync_.exchange(false, std::memory_order_acquire))
    {
        if (activeState_ != nullptr)
        {
            // Sync diagonal chain parameters to per-channel processing copies.
            for (int ch = 0; ch < activeState_->diagChannelChains.size(); ++ch)
                activeState_->diagChannelChains[ch]->syncParametersFrom(diagonalChain_);

            // Sync MIMO path chain parameters to per-path processing copies.
            for (auto& kv : activeState_->pathChains)
            {
                auto* modelChain = getChainForPath(kv.first.first, kv.first.second);
                if (modelChain != nullptr)
                    kv.second->syncParametersFrom(*modelChain);
            }
        }
    }
}

void Mcfx_mimoeqAudioProcessor::processBlock(AudioSampleBuffer& buffer, MidiBuffer&)
{
    // Lock-free state swap: pick up new processing state if available
    auto* pending = pendingState_.exchange(nullptr, std::memory_order_acquire);
    if (pending != nullptr)
    {
        // Stash old state for GUI thread to delete (can't allocate/free on audio thread)
        garbageState_.store(activeState_, std::memory_order_release);
        activeState_ = pending;
        needsParamSync_.store(false, std::memory_order_relaxed); // rebuild supersedes sync
    }

    doParamSyncIfNeeded();

    if (activeState_ == nullptr)
        return;

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    // Capture pre-processing spectrum
    bool analyzerOn = analyzerEnabled_.load(std::memory_order_relaxed);
    if (analyzerOn)
        inputAnalyzer_.pushBuffer(buffer, numSamples);

    auto& diagChains = activeState_->diagChannelChains;
    auto& diagMask = activeState_->diagChannelMask;
    auto& pathChains = activeState_->pathChains;
    bool hasAnyMimoPaths = !pathChains.empty();

    // Save raw input before diagonal processing.
    // MIMO paths whose input channel is NOT in the diagonal mask read
    // the unprocessed signal; those whose input IS in the mask read
    // the diagonal-processed result (serial: diagonal → MIMO).
    if (hasAnyMimoPaths)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            inputSnapshot_.copyFrom(ch, 0, buffer.getReadPointer(ch), numSamples);
    }

    // Step 1: Apply diagonal chain to channels in mask.
    // Channels not in the mask are left untouched (not muted) — MIMO
    // paths may still route them.
    for (int ch = 0; ch < numChannels; ++ch)
    {
        bool diagActive = diagMask.empty() || diagMask.count(ch + 1) > 0;
        if (diagActive && ch < (int)diagChains.size())
            diagChains[ch]->processBlock(buffer.getWritePointer(ch), numSamples);
    }

    // Step 2: Apply per-path MIMO chains (serial after diagonal).
    // Each MIMO path reads from:
    //   - diagonal output if the input channel had diagonal active
    //   - raw input otherwise
    // MIMO output replaces (not adds to) the affected output channels.
    if (hasAnyMimoPaths)
    {
        workBuffer_.clear();

        std::fill(mimoOutputMask_.begin(), mimoOutputMask_.begin() + numChannels, false);

        for (auto& kv : pathChains)
        {
            int inCh = kv.first.first - 1;  // 1-based to 0-based
            int outCh = kv.first.second - 1;
            auto* chain = kv.second;

            if (inCh < 0 || inCh >= numChannels || outCh < 0 || outCh >= numChannels)
                continue;

            mimoOutputMask_[outCh] = true;

            // Pick source: diagonal output (buffer) or raw input (snapshot)
            bool inChDiagActive = diagMask.empty() || diagMask.count(inCh + 1) > 0;
            const float* source = inChDiagActive
                ? buffer.getReadPointer(inCh)
                : inputSnapshot_.getReadPointer(inCh);

            tempBuffer_.copyFrom(0, 0, source, numSamples);
            if (chain->getNumBands() > 0)
                chain->processBlock(tempBuffer_.getWritePointer(0), numSamples);
            workBuffer_.addFrom(outCh, 0, tempBuffer_.getReadPointer(0), numSamples);
        }

        // Replace output channels that have MIMO paths targeting them
        for (int ch = 0; ch < numChannels; ++ch)
            if (mimoOutputMask_[ch])
                buffer.copyFrom(ch, 0, workBuffer_.getReadPointer(ch), numSamples);
    }

    // Capture post-processing spectrum
    if (analyzerOn)
        outputAnalyzer_.pushBuffer(buffer, numSamples);
}

AudioProcessorEditor* Mcfx_mimoeqAudioProcessor::createEditor()
{
    return new Mcfx_mimoeqAudioProcessorEditor(this);
}

void Mcfx_mimoeqAudioProcessor::getStateInformation(MemoryBlock& destData)
{
    auto* root = new DynamicObject();
    root->setProperty("sample_rate", currentSampleRate_);

    Array<var> sosArray;

    // Diagonal bands
    for (int i = 0; i < diagonalChain_.getNumBands(); ++i)
        sosArray.add(diagonalChain_.getBand(i)->toJson());

    // Per-path bands
    for (auto& kv : pathChains_)
    {
        auto* chain = kv.second;
        for (int i = 0; i < chain->getNumBands(); ++i)
        {
            auto* band = chain->getBand(i);
            band->setDiagonal(false);
            band->setInputChannel(kv.first.first);
            band->setOutputChannel(kv.first.second);
            sosArray.add(band->toJson());
        }
    }

    root->setProperty("sos", sosArray);

    // Diagonal channel mask (omit if empty = all channels)
    if (!diagChannelMask_.empty())
    {
        Array<var> maskArray;
        for (int ch : diagChannelMask_)
            maskArray.add(ch);
        root->setProperty("diag_channels", maskArray);
    }

    String jsonStr = JSON::toString(var(root));
    destData.append(jsonStr.toRawUTF8(), jsonStr.getNumBytesAsUTF8());
}

void Mcfx_mimoeqAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    String jsonStr = String::createStringFromData(data, sizeInBytes);
    var parsed = JSON::parse(jsonStr);

    if (!parsed.isObject())
        return;

    double sr = parsed.getProperty("sample_rate", currentSampleRate_);
    var sos = parsed["sos"];

    if (!sos.isArray())
        return;

    // Diagonal channel mask
    diagChannelMask_.clear();
    var diagCh = parsed["diag_channels"];
    if (diagCh.isArray())
        for (int i = 0; i < diagCh.size(); ++i)
            diagChannelMask_.insert((int)diagCh[i]);

    // Clear model
    diagonalChain_.fromJson(var(Array<var>()), sr);
    pathChains_.clear();
    ownedPathChains_.clear();

    // Separate bands into diagonal and per-path groups
    Array<var> diagBands;
    std::map<PathKey, Array<var>> pathBandMap;

    for (int i = 0; i < sos.size(); ++i)
    {
        auto bandJson = sos[i];
        bool isDiag = bandJson.hasProperty("diagonal") && (bool)bandJson["diagonal"];

        if (isDiag)
        {
            diagBands.add(bandJson);
        }
        else
        {
            int inCh = (int)bandJson.getProperty("input_channel", 1);
            int outCh = (int)bandJson.getProperty("output_channel", 1);
            pathBandMap[{inCh, outCh}].add(bandJson);
        }
    }

    // Rebuild model
    diagonalChain_.fromJson(var(diagBands), sr);
    diagonalChain_.prepare(currentSampleRate_, currentBlockSize_);

    for (auto& kv : pathBandMap)
    {
        auto* chain = getOrCreateChainForPath(kv.first.first, kv.first.second);
        chain->fromJson(var(kv.second), sr);
        chain->prepare(currentSampleRate_, currentBlockSize_);
    }

    // Build and publish new processing state (lock-free)
    rebuildProcessingChains();
    syncModelToAPVTS();
    sendChangeMessage();
}

bool Mcfx_mimoeqAudioProcessor::loadConfigFromFile(const File& file)
{
    String content = file.loadFileAsString();
    auto data = content.toRawUTF8();
    setStateInformation(data, (int)content.getNumBytesAsUTF8());
    return true;
}

bool Mcfx_mimoeqAudioProcessor::saveConfigToFile(const File& file)
{
    MemoryBlock mb;
    getStateInformation(mb);
    String jsonStr = String::createStringFromData(mb.getData(), (int)mb.getSize());
    return file.replaceWithText(jsonStr);
}

// --- Undo / Redo ---

String Mcfx_mimoeqAudioProcessor::captureState()
{
    MemoryBlock mb;
    getStateInformation(mb);
    return String::createStringFromData(mb.getData(), (int)mb.getSize());
}

// --- Click-free undo/redo helpers ---

/** Returns true when the target state differs from the current model only
    in diagonal-chain parameter values (freq/Q/gain/enabled) — no structural
    changes (band count, type, subtype, order, routing, raw-coeff mode). */
static bool bandStructureMatchesJson(const EqBand* band, const var& json)
{
    // Raw-coefficient IIR band?
    bool targetRaw = json.hasProperty("coefficients") && !json.hasProperty("parameters");
    if (targetRaw != band->hasRawCoefficients())
        return false;
    if (targetRaw)
        return band->getType() == EqBandType::IIR; // raw coeff structure matches if both raw IIR

    var params = json["parameters"];
    if (!params.isObject())
        return false;

    String typeStr = params.getProperty("type", "").toString();

    // Map type string → EqBandType + IIRSubType
    if (typeStr == "fir")
    {
        // FIR changes always require a full rebuild (convolver reconfiguration)
        return false;
    }
    if (typeStr == "gain")
        return band->getType() == EqBandType::Gain;
    if (typeStr == "delay")
    {
        if (band->getType() != EqBandType::Delay) return false;
        // Delay sample count is structural (buffer resize)
        int targetSamples = (int)params.getProperty("delay_samples", 0);
        return targetSamples == band->getDelaySamples();
    }

    // IIR subtypes
    if (band->getType() != EqBandType::IIR) return false;

    // Map string to IIRSubType
    IIRSubType targetSub = IIRSubType::Peak; // default
    if      (typeStr == "peak")           targetSub = IIRSubType::Peak;
    else if (typeStr == "low_pass")       targetSub = IIRSubType::LowPass;
    else if (typeStr == "high_pass")      targetSub = IIRSubType::HighPass;
    else if (typeStr == "band_pass")      targetSub = IIRSubType::BandPass;
    else if (typeStr == "notch")          targetSub = IIRSubType::Notch;
    else if (typeStr == "all_pass")       targetSub = IIRSubType::AllPass;
    else if (typeStr == "low_shelf")      targetSub = IIRSubType::LowShelf;
    else if (typeStr == "high_shelf")     targetSub = IIRSubType::HighShelf;
    else if (typeStr == "butterworth_lp") targetSub = IIRSubType::ButterworthLP;
    else if (typeStr == "butterworth_hp") targetSub = IIRSubType::ButterworthHP;
    else if (typeStr == "crossover_lp")   targetSub = IIRSubType::CrossoverLP;
    else if (typeStr == "crossover_hp")   targetSub = IIRSubType::CrossoverHP;
    else if (typeStr == "crossover_ap")   targetSub = IIRSubType::CrossoverAP;
    else return false; // unknown type

    if (targetSub != band->getIIRSubType()) return false;

    // Butterworth/crossover order is structural (cascade section count)
    if (targetSub == IIRSubType::ButterworthLP || targetSub == IIRSubType::ButterworthHP)
    {
        int targetOrder = (int)params.getProperty("order", 2);
        if (targetOrder != band->getButterworthOrder()) return false;
    }
    else if (targetSub == IIRSubType::CrossoverLP || targetSub == IIRSubType::CrossoverHP
             || targetSub == IIRSubType::CrossoverAP)
    {
        int targetOrder = (int)params.getProperty("order", 4);
        if (targetOrder != band->getCrossoverOrder()) return false;
    }

    return true;
}

bool Mcfx_mimoeqAudioProcessor::canSyncRestore(const String& targetState) const
{
    var parsed = JSON::parse(targetState);
    if (!parsed.isObject()) return false;

    // Check if diagonal channel mask changed (structural — needs rebuild)
    std::set<int> targetMask;
    var diagCh = parsed["diag_channels"];
    if (diagCh.isArray())
        for (int i = 0; i < diagCh.size(); ++i)
            targetMask.insert((int)diagCh[i]);
    if (targetMask != diagChannelMask_)
        return false;

    var sos = parsed["sos"];
    if (!sos.isArray()) return false;

    // Separate target bands into diagonal and path groups
    Array<var> diagBands;
    bool hasPathBands = false;

    for (int i = 0; i < sos.size(); ++i)
    {
        auto bandJson = sos[i];
        bool isDiag = bandJson.hasProperty("diagonal") && (bool)bandJson["diagonal"];
        if (isDiag)
            diagBands.add(bandJson);
        else
            hasPathBands = true;
    }

    // If target or current model has path chains → full rebuild (simplification)
    if (hasPathBands || !pathChains_.empty())
        return false;

    // Diagonal band count must match
    if (diagBands.size() != diagonalChain_.getNumBands())
        return false;

    // Each band must be structurally identical
    for (int i = 0; i < diagBands.size(); ++i)
    {
        if (!bandStructureMatchesJson(diagonalChain_.getBand(i), diagBands[i]))
            return false;
    }

    return true;
}

void Mcfx_mimoeqAudioProcessor::syncRestoreState(const String& targetState)
{
    var parsed = JSON::parse(targetState);
    var sos = parsed["sos"];

    // Collect diagonal bands (we already verified structure matches)
    Array<var> diagBands;
    for (int i = 0; i < sos.size(); ++i)
    {
        auto bandJson = sos[i];
        if (bandJson.hasProperty("diagonal") && (bool)bandJson["diagonal"])
            diagBands.add(bandJson);
    }

    // Apply parameter-only changes to the model diagonal chain
    for (int i = 0; i < diagBands.size(); ++i)
    {
        auto* band = diagonalChain_.getBand(i);
        auto bandJson = diagBands[i];
        var params = bandJson["parameters"];

        if (params.isObject())
        {
            if (params.hasProperty("f_Hz"))
                band->setFrequency((float)params["f_Hz"]);
            if (params.hasProperty("Q"))
                band->setQ((float)params["Q"]);

            // Gain band: handle dB vs linear mode
            if (params.hasProperty("gain_linear"))
            {
                band->setUseLinearGain(true);
                band->setLinearGain((float)(double)params["gain_linear"]);
            }
            else
            {
                if (band->getType() == EqBandType::Gain)
                    band->setUseLinearGain(false);
                if (params.hasProperty("gain_db"))
                    band->setGainDB((float)params["gain_db"]);
                if (params.hasProperty("invert"))
                    band->setInvertGain((bool)params["invert"]);
                else if (band->getType() == EqBandType::Gain)
                    band->setInvertGain(false);
            }
        }

        // enabled: absent means true, explicit false means disabled
        bool enabled = !(bandJson.hasProperty("enabled") && !(bool)bandJson["enabled"]);
        band->setEnabled(enabled);
    }

    // Lightweight sync: audio thread will pick up parameter changes via SmoothedValue
    requestParameterSync();
    syncModelToAPVTS();
    sendChangeMessage();
}

void Mcfx_mimoeqAudioProcessor::restoreState(const String& s)
{
    if (canSyncRestore(s))
        syncRestoreState(s);
    else
    {
        auto data = s.toRawUTF8();
        setStateInformation(data, (int)s.getNumBytesAsUTF8());
    }
}

void Mcfx_mimoeqAudioProcessor::pushUndoState()
{
    undoStack_.push_back(captureState());
    if ((int)undoStack_.size() > kMaxUndoSteps)
        undoStack_.erase(undoStack_.begin());
    redoStack_.clear();
}

bool Mcfx_mimoeqAudioProcessor::undo()
{
    if (undoStack_.empty())
        return false;
    redoStack_.push_back(captureState());
    String state = undoStack_.back();
    undoStack_.pop_back();
    restoreState(state);
    return true;
}

bool Mcfx_mimoeqAudioProcessor::redo()
{
    if (redoStack_.empty())
        return false;
    undoStack_.push_back(captureState());
    String state = redoStack_.back();
    redoStack_.pop_back();
    restoreState(state);
    return true;
}

// --- APVTS Parameter Automation ---

AudioProcessorValueTreeState::ParameterLayout Mcfx_mimoeqAudioProcessor::createParameters()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 1; i <= kMaxAutomatedBands; ++i)
    {
        String prefix = "B" + String(i).paddedLeft('0', 2) + "_";
        String name = "Band " + String(i) + " ";

        layout.add(std::make_unique<AudioParameterFloat>(
            ParameterID { prefix + "Freq", 1 },
            name + "Freq",
            NormalisableRange<float>(20.f, 20000.f, 0.f, 0.3f),
            1000.f));

        layout.add(std::make_unique<AudioParameterFloat>(
            ParameterID { prefix + "Q", 1 },
            name + "Q",
            NormalisableRange<float>(0.1f, 20.f, 0.f, 0.4f),
            0.707f));

        layout.add(std::make_unique<AudioParameterFloat>(
            ParameterID { prefix + "Gain", 1 },
            name + "Gain",
            NormalisableRange<float>(-24.f, 24.f, 0.01f),
            0.f));

        layout.add(std::make_unique<AudioParameterBool>(
            ParameterID { prefix + "Enable", 1 },
            name + "Enable",
            true));
    }

    return layout;
}

void Mcfx_mimoeqAudioProcessor::parameterChanged(const String& parameterID, float newValue)
{
    if (updatingFromModel_)
        return;

    // Parse parameter ID: "B01_Freq" -> band index 0, param "Freq"
    if (parameterID.length() < 4 || parameterID[0] != 'B')
        return;

    int bandIdx = parameterID.substring(1, 3).getIntValue() - 1;  // 0-based
    String param = parameterID.substring(4);

    if (bandIdx < 0 || bandIdx >= diagonalChain_.getNumBands())
        return;

    auto* band = diagonalChain_.getBand(bandIdx);
    if (band == nullptr)
        return;

    // Only apply freq/Q/gain to IIR bands (other types don't use these params)
    if (param == "Freq")
    {
        if (band->getType() == EqBandType::IIR)
            band->setFrequency(newValue);
    }
    else if (param == "Q")
    {
        if (band->getType() == EqBandType::IIR)
            band->setQ(newValue);
    }
    else if (param == "Gain")
    {
        if (band->getType() == EqBandType::IIR || band->getType() == EqBandType::Gain)
            band->setGainDB(newValue);
    }
    else if (param == "Enable")
    {
        band->setEnabled(newValue >= 0.5f);
    }

    requestParameterSync();
    sendChangeMessage();
}

void Mcfx_mimoeqAudioProcessor::syncModelToAPVTS()
{
    updatingFromModel_ = true;

    int numBands = diagonalChain_.getNumBands();
    for (int i = 0; i < kMaxAutomatedBands; ++i)
    {
        String prefix = "B" + String(i + 1).paddedLeft('0', 2) + "_";

        if (i < numBands)
        {
            auto* band = diagonalChain_.getBand(i);
            if (auto* p = apvts.getParameter(prefix + "Freq"))
                p->setValueNotifyingHost(p->convertTo0to1(band->getFrequency()));
            if (auto* p = apvts.getParameter(prefix + "Q"))
                p->setValueNotifyingHost(p->convertTo0to1(band->getQ()));
            if (auto* p = apvts.getParameter(prefix + "Gain"))
                p->setValueNotifyingHost(p->convertTo0to1(band->getGainDB()));
            if (auto* p = apvts.getParameter(prefix + "Enable"))
                p->setValueNotifyingHost(band->isEnabled() ? 1.f : 0.f);
        }
        else
        {
            // Reset unused slots to defaults so host doesn't show stale values
            if (auto* p = apvts.getParameter(prefix + "Freq"))
                p->setValueNotifyingHost(p->convertTo0to1(1000.f));
            if (auto* p = apvts.getParameter(prefix + "Q"))
                p->setValueNotifyingHost(p->convertTo0to1(0.707f));
            if (auto* p = apvts.getParameter(prefix + "Gain"))
                p->setValueNotifyingHost(p->convertTo0to1(0.f));
            if (auto* p = apvts.getParameter(prefix + "Enable"))
                p->setValueNotifyingHost(1.f);
        }
    }

    updatingFromModel_ = false;
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Mcfx_mimoeqAudioProcessor();
}
