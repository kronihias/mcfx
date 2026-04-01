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
#include "EqChain.h"
#include <map>
#include <atomic>

// A path key is (input_channel, output_channel)
using PathKey = std::pair<int, int>;

class Mcfx_mimoeqAudioProcessor : public AudioProcessor,
                                   public ChangeBroadcaster
{
public:
    Mcfx_mimoeqAudioProcessor();
    ~Mcfx_mimoeqAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const String getName() const override { return JucePlugin_Name; }

    int getNumParameters() override { return 0; }
    float getParameter(int) override { return 0.f; }
    void setParameter(int, float) override {}
    const String getParameterName(int) override { return {}; }
    const String getParameterText(int) override { return {}; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override { return {}; }
    void changeProgramName(int, const String&) override {}

    void getStateInformation(MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Per-path chain access
    EqChain* getChainForPath(int inCh, int outCh);
    EqChain* getOrCreateChainForPath(int inCh, int outCh);
    const std::map<PathKey, EqChain*>& getAllPaths() const { return pathChains_; }

    // Diagonal chain (applied to all channels identically)
    EqChain& getDiagonalChain() { return diagonalChain_; }

    // Load/save
    bool loadConfigFromFile(const File& file);
    bool saveConfigToFile(const File& file);

    double getSampleRate_() const { return currentSampleRate_; }
    int getNumChannels_() const { return getTotalNumInputChannels(); }

    void rebuildProcessingChains();
    void requestRebuild() { needsRebuild_.store(true); }

    /** Lightweight parameter sync: propagates parameter changes to per-channel
        copies without rebuilding (preserves filter state for click-free updates). */
    void requestParameterSync() { needsParamSync_.store(true); }

private:
    void doRebuildIfNeeded();
    // Diagonal bands: applied to all channels
    EqChain diagonalChain_;

    // Per-path chains: keyed by (in_ch, out_ch)
    std::map<PathKey, EqChain*> pathChains_;
    OwnedArray<EqChain> ownedPathChains_;

    // Per-channel diagonal chain copies (each needs own filter state)
    OwnedArray<EqChain> diagChannelChains_;

    double currentSampleRate_ = 48000.0;
    int currentBlockSize_ = 512;

    AudioSampleBuffer workBuffer_;

    std::atomic<bool> needsRebuild_ { false };
    std::atomic<bool> needsParamSync_ { false };
    void doParamSyncIfNeeded();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Mcfx_mimoeqAudioProcessor)
};

#endif
