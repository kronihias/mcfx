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
#include "SpectrumAnalyzer.h"
#include <map>
#include <atomic>
#include <set>

// A path key is (input_channel, output_channel)
using PathKey = std::pair<int, int>;

/** Holds the complete processing state used exclusively by the audio thread.
    Built on the GUI thread and published lock-free via atomic pointer swap. */
struct ProcessingState
{
    OwnedArray<EqChain> diagChannelChains;            // per-channel diagonal copies
    std::set<int> diagChannelMask;                     // empty = all; 1-based channel set
    std::map<PathKey, EqChain*> pathChains;            // raw ptrs into ownedPathChains
    OwnedArray<EqChain> ownedPathChains;               // owns the path chain objects
};

class Mcfx_mimoeqAudioProcessor : public AudioProcessor,
                                   public ChangeBroadcaster,
                                   private AudioProcessorValueTreeState::Listener
{
public:
    Mcfx_mimoeqAudioProcessor();
    ~Mcfx_mimoeqAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const String getName() const override { return JucePlugin_Name; }

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

    // Per-path chain access (model — GUI thread only)
    EqChain* getChainForPath(int inCh, int outCh);
    EqChain* getOrCreateChainForPath(int inCh, int outCh);
    const std::map<PathKey, EqChain*>& getAllPaths() const { return pathChains_; }
    bool removePathChain(int inCh, int outCh);
    std::vector<PathKey> getPathKeys() const;

    // --- VST3 Parameter Automation (diagonal chain) ---
    AudioProcessorValueTreeState apvts;
    static constexpr int kMaxAutomatedBands = 24;
    /** Push current diagonal chain parameter values to APVTS (call after any model change). */
    void syncModelToAPVTS();

    // Diagonal chain model (GUI thread edits, audio thread reads parameters for sync)
    EqChain& getDiagonalChain() { return diagonalChain_; }

    // Diagonal channel mask: empty = all channels, otherwise only listed channels (1-based)
    const std::set<int>& getDiagChannelMask() const { return diagChannelMask_; }
    void setDiagChannelMask(const std::set<int>& mask) { diagChannelMask_ = mask; }
    bool isDiagChannelEnabled(int ch1based) const
    {
        return diagChannelMask_.empty() || diagChannelMask_.count(ch1based) > 0;
    }

    // Editor view state (persisted across GUI close/reopen)
    bool editorDiagonalMode = true;
    PathKey editorSelectedPath { 1, 1 };
    int editorSelectedBand = -1;
    bool editorRoutingMatrixView = false; // true = matrix, false = wires
    int editorRoutingPopupW = 0;  // 0 = use default
    int editorRoutingPopupH = 0;

    // Load/save
    bool loadConfigFromFile(const File& file);
    bool saveConfigToFile(const File& file);

    // --- Spectrum Analyzer ---
    SpectrumAnalyzer& getInputAnalyzer() { return inputAnalyzer_; }
    SpectrumAnalyzer& getOutputAnalyzer() { return outputAnalyzer_; }
    bool isAnalyzerEnabled() const { return analyzerEnabled_.load(std::memory_order_relaxed); }
    void setAnalyzerEnabled(bool on) { analyzerEnabled_.store(on, std::memory_order_relaxed); }
    bool editorAnalyzerOn = false;       // persisted toggle state
    int editorAnalyzerChannel = 0;       // 0 = sum, 1..N = specific channel
    bool analyzerAutoNormalize = true;    // normalize to 0dB peak
    float analyzerOffset = 0.f;          // dB offset (when auto-normalize is off)

    double getSampleRate_() const { return currentSampleRate_; }
    int getNumChannels_() const
    {
#if MCFX_MULTICHANNEL_BUILD
        // Single-binary multichannel build: return the channel count the host
        // actually negotiated, so routing / path editors only show those.
        return getTotalNumInputChannels();
#elif defined(NUM_CHANNELS)
        return NUM_CHANNELS;
#else
        return getTotalNumInputChannels();
#endif
    }

    /** Full rebuild: builds a new ProcessingState on the GUI thread and
        publishes it lock-free for the audio thread to pick up. */
    void rebuildProcessingChains();

    /** Convenience — same as rebuildProcessingChains(). */
    void requestRebuild() { rebuildProcessingChains(); }

    /** Lightweight parameter sync: propagates parameter changes to per-channel
        copies without rebuilding (preserves filter state for click-free updates). */
    void requestParameterSync() { needsParamSync_.store(true, std::memory_order_release); }

    // --- Undo / Redo ---
    String captureState();                // serialize current state to JSON string
    void restoreState(const String& s);   // deserialize and rebuild
    void pushUndoState();                 // capture current → undoStack_, clear redo
    bool undo();
    bool redo();
    bool canUndo() const { return !undoStack_.empty(); }
    bool canRedo() const { return !redoStack_.empty(); }

private:
    // ---- APVTS ----
    AudioProcessorValueTreeState::ParameterLayout createParameters();
    void parameterChanged(const String& parameterID, float newValue) override;
    bool updatingFromModel_ = false;  // feedback guard for syncModelToAPVTS

    // ---- Model data (GUI thread writes, audio thread reads floats for sync) ----
    EqChain diagonalChain_;
    std::set<int> diagChannelMask_;   // empty = all channels; otherwise 1-based channel set
    std::map<PathKey, EqChain*> pathChains_;
    OwnedArray<EqChain> ownedPathChains_;

    // ---- Lock-free processing state ----
    // GUI builds a new ProcessingState*, publishes via pendingState_.
    // Audio thread picks it up, stashes old state in garbageState_ for GUI to delete.
    ProcessingState* activeState_ = nullptr;                     // audio thread only
    std::atomic<ProcessingState*> pendingState_ { nullptr };     // GUI → audio
    std::atomic<ProcessingState*> garbageState_ { nullptr };     // audio → GUI (for deletion)

    double currentSampleRate_ = 48000.0;
    int currentBlockSize_ = 512;

    AudioSampleBuffer workBuffer_;
    AudioSampleBuffer inputSnapshot_; // saved input for cross-path reads

    std::atomic<bool> needsParamSync_ { false };
    void doParamSyncIfNeeded();

    // Click-free undo/redo helpers
    bool canSyncRestore(const String& targetState) const;
    void syncRestoreState(const String& targetState);

    // Undo/redo stacks (JSON strings)
    std::vector<String> undoStack_;
    std::vector<String> redoStack_;
    static constexpr int kMaxUndoSteps = 100;

    // Spectrum analyzers
    SpectrumAnalyzer inputAnalyzer_;
    SpectrumAnalyzer outputAnalyzer_;
    std::atomic<bool> analyzerEnabled_ { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Mcfx_mimoeqAudioProcessor)
};

#endif
