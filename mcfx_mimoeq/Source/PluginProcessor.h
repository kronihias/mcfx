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
#include <set>

// A path key is (input_channel, output_channel)
using PathKey = std::pair<int, int>;

/** Holds the complete processing state used exclusively by the audio thread.
    Built on the GUI thread and published lock-free via atomic pointer swap. */
struct ProcessingState
{
    OwnedArray<EqChain> diagChannelChains;            // per-channel diagonal copies
    std::map<PathKey, EqChain*> pathChains;            // raw ptrs into ownedPathChains
    OwnedArray<EqChain> ownedPathChains;               // owns the path chain objects
};

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

    // Per-path chain access (model — GUI thread only)
    EqChain* getChainForPath(int inCh, int outCh);
    EqChain* getOrCreateChainForPath(int inCh, int outCh);
    const std::map<PathKey, EqChain*>& getAllPaths() const { return pathChains_; }
    bool removePathChain(int inCh, int outCh);
    std::vector<PathKey> getPathKeys() const;

    // Diagonal chain model (GUI thread edits, audio thread reads parameters for sync)
    EqChain& getDiagonalChain() { return diagonalChain_; }

    // Load/save
    bool loadConfigFromFile(const File& file);
    bool saveConfigToFile(const File& file);

    double getSampleRate_() const { return currentSampleRate_; }
    int getNumChannels_() const { return getTotalNumInputChannels(); }

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
    // ---- Model data (GUI thread writes, audio thread reads floats for sync) ----
    EqChain diagonalChain_;
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

    std::atomic<bool> needsParamSync_ { false };
    void doParamSyncIfNeeded();

    // Click-free undo/redo helpers
    bool canSyncRestore(const String& targetState) const;
    void syncRestoreState(const String& targetState);

    // Undo/redo stacks (JSON strings)
    std::vector<String> undoStack_;
    std::vector<String> redoStack_;
    static constexpr int kMaxUndoSteps = 100;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Mcfx_mimoeqAudioProcessor)
};

#endif
