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

Mcfx_mimoeqAudioProcessor::Mcfx_mimoeqAudioProcessor()
{
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
    rebuildProcessingChains();
}

void Mcfx_mimoeqAudioProcessor::releaseResources()
{
}

void Mcfx_mimoeqAudioProcessor::rebuildProcessingChains()
{
    // Clean up garbage from previous swap (safe: called on GUI/host thread)
    delete garbageState_.exchange(nullptr, std::memory_order_acquire);

    int numCh = getTotalNumInputChannels();
    auto* newState = new ProcessingState();

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

    // Build per-path chain processing copies
    for (auto& kv : pathChains_)
    {
        auto* srcChain = kv.second;
        if (srcChain->getNumBands() > 0)
        {
            var pathJson = srcChain->toJson();
            auto* copy = new EqChain();
            copy->fromJson(pathJson, currentSampleRate_);
            copy->prepare(currentSampleRate_, currentBlockSize_);
            newState->ownedPathChains.add(copy);
            newState->pathChains[kv.first] = copy;
        }
    }

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

void Mcfx_mimoeqAudioProcessor::doParamSyncIfNeeded()
{
    if (needsParamSync_.exchange(false, std::memory_order_acquire))
    {
        // Sync diagonal chain parameters to per-channel processing copies.
        // Reads float parameters from diagonalChain_ (GUI-edited model) —
        // individual float reads are safe on all modern architectures.
        if (activeState_ != nullptr)
        {
            for (int ch = 0; ch < activeState_->diagChannelChains.size(); ++ch)
                activeState_->diagChannelChains[ch]->syncParametersFrom(diagonalChain_);
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

    // Step 1: Apply diagonal chain (same processing on each channel)
    auto& diagChains = activeState_->diagChannelChains;
    for (int ch = 0; ch < numChannels && ch < diagChains.size(); ++ch)
        diagChains[ch]->processBlock(buffer.getWritePointer(ch), numSamples);

    // Step 2: Apply per-path chains
    auto& pathChains = activeState_->pathChains;
    if (!pathChains.empty())
    {
        workBuffer_.setSize(numChannels, numSamples, false, false, true);
        workBuffer_.clear();

        // Track which output channels have cross-path contributions
        std::set<int> crossOutputChannels;

        for (auto& kv : pathChains)
        {
            int inCh = kv.first.first - 1;  // 1-based to 0-based
            int outCh = kv.first.second - 1;
            auto* chain = kv.second;

            if (inCh < 0 || inCh >= numChannels || outCh < 0 || outCh >= numChannels)
                continue;

            if (chain->getNumBands() == 0)
                continue;

            if (inCh == outCh)
            {
                // Diagonal per-path: process in-place
                chain->processBlock(buffer.getWritePointer(outCh), numSamples);
            }
            else
            {
                // Cross-path: copy input, process, add to work buffer
                crossOutputChannels.insert(outCh);
                AudioSampleBuffer temp(1, numSamples);
                temp.copyFrom(0, 0, buffer.getReadPointer(inCh), numSamples);
                chain->processBlock(temp.getWritePointer(0), numSamples);
                workBuffer_.addFrom(outCh, 0, temp.getReadPointer(0), numSamples);
            }
        }

        // Add cross-path contributions to output
        for (int ch : crossOutputChannels)
            buffer.addFrom(ch, 0, workBuffer_.getReadPointer(ch), numSamples);
    }
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

void Mcfx_mimoeqAudioProcessor::restoreState(const String& s)
{
    auto data = s.toRawUTF8();
    setStateInformation(data, (int)s.getNumBytesAsUTF8());
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

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Mcfx_mimoeqAudioProcessor();
}
