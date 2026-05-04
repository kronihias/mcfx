#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "NativeNodes/GainNode.h"
#include "NativeNodes/MutePhaseNode.h"
#include "NativeNodes/MatrixMixerNode.h"
#include "NativeNodes/DelayNode.h"

namespace
{
    constexpr const char* kStateXmlTag      = "MCFX_GRAPH_STATE";
    constexpr const char* kStateJsonAttr    = "json";

    juce::Point<int> readPosition (const juce::var& v)
    {
        if (auto* obj = v.getDynamicObject())
            return { (int) obj->getProperty ("x"), (int) obj->getProperty ("y") };
        return { 100, 100 };
    }

    // True iff two plugin descriptions refer to the same plug-in (same format,
    // UID, name) — the fast undo path requires the binary identity to match
    // because we re-use the existing instance and only swap its state.
    bool pluginIdentitiesMatch (const juce::PluginDescription& a,
                                 const juce::PluginDescription& b)
    {
        return a.createIdentifierString() == b.createIdentifierString();
    }

    std::unique_ptr<juce::PluginDescription> readPluginDesc (const juce::var& nv)
    {
        auto* o = nv.getDynamicObject();
        if (o == nullptr) return nullptr;
        const auto descXml = o->getProperty ("pluginDescriptionXml").toString();
        if (descXml.isEmpty()) return nullptr;
        auto xml = juce::parseXML (descXml);
        if (xml == nullptr) return nullptr;
        auto pd = std::make_unique<juce::PluginDescription>();
        if (! pd->loadFromXml (*xml)) return nullptr;
        return pd;
    }

    // Structural-equality check between an incoming rootGraph var and the
    // live controller. "Structural" means: same set of node UUIDs, same kinds,
    // same channel counts, and (for plugin nodes) same plugin identity.
    // Subgraph nodes always force a slow rebuild — recursive comparison would
    // need to descend into the inner controller and we don't bother.
    bool structureMatches (const juce::var& rootGraph,
                            const GraphController& controller)
    {
        auto* obj = rootGraph.getDynamicObject();
        if (obj == nullptr) return false;

        auto* nodesArr = obj->getProperty ("nodes").getArray();
        if (nodesArr == nullptr) return false;

        const auto current = controller.getAllUserNodes();
        if (current.size() != nodesArr->size()) return false;

        for (const auto& nv : *nodesArr)
        {
            auto* nObj = nv.getDynamicObject();
            if (nObj == nullptr) return false;

            const juce::Uuid uid (nObj->getProperty ("uuid").toString());
            const auto kind = nodeKindFromString (nObj->getProperty ("type").toString());
            const int chIn  = (int) nObj->getProperty ("channelCountIn");
            const int chOut = (int) nObj->getProperty ("channelCountOut");

            auto* gn = controller.getNode (uid);
            if (gn == nullptr) return false;
            if (gn->kind != kind) return false;

            // Subgraphs aren't worth the recursive structural check + inner
            // state copy — fall through to slow path so the outer rebuild is
            // straightforward.
            if (kind == NodeKind::Subgraph) return false;

            if (gn->channelCountIn != chIn || gn->channelCountOut != chOut)
                return false;

            if (kind == NodeKind::Plugin)
            {
                auto incoming = readPluginDesc (nv);
                if (incoming == nullptr || gn->pluginDescription == nullptr) return false;
                if (! pluginIdentitiesMatch (*incoming, *gn->pluginDescription)) return false;
            }
        }
        return true;
    }

    void applyBase64State (juce::AudioProcessor& p, const juce::String& b64)
    {
        if (b64.isEmpty()) return;
        juce::MemoryOutputStream mo;
        if (! juce::Base64::convertFromBase64 (mo, b64)) return;
        p.setStateInformation (mo.getData(), (int) mo.getDataSize());
    }
}

Mcfx_graphAudioProcessor::Mcfx_graphAudioProcessor()
    : juce::AudioProcessor (
       #if MCFX_MULTICHANNEL_BUILD
            MCFX_MULTICHANEL_BUSES
       #else
            BusesProperties()
                .withInput  ("Input",  juce::AudioChannelSet::discreteChannels (NUM_CHANNELS), true)
                .withOutput ("Output", juce::AudioChannelSet::discreteChannels (NUM_CHANNELS), true)
       #endif
      ),
      graph_ (std::make_unique<GraphController>())
{
    // Default new graph: input -> output passthrough on every channel.
    // The real channel count gets set by prepareToPlay; this is a placeholder.

    // Build the fixed pool of forwarding parameters. The slot count must NEVER
    // change after construction — DAWs cache this at first scan and silently
    // corrupt projects when it shifts.
    forwardingParameters_.ensureStorageAllocated (kForwardingParameterCount);
    for (int i = 0; i < kForwardingParameterCount; ++i)
    {
        auto* fp = new ForwardingParameter (i);
        forwardingParameters_.add (fp);
        addParameter (fp); // AudioProcessor takes ownership
    }

    // 50 ms GUI sync timer (mirror inner-plugin GUI knob movements back to host).
    startTimerHz (20);

    // Drop any forwarding-parameter binding whose node is about to be destroyed
    // — keeps a stale AudioProcessorParameter* from being read on the audio
    // thread once removeNode/replaceNodeProcessor returns. Also detach the
    // AudioProcessorListener we attached for undo-on-parameter-change, and
    // close any open plugin GUI window for the node so its content editor
    // doesn't outlive the processor (some plug-ins crash on destruction
    // while their GUI is still mounted).
    graph_->setNodeAboutToBeRemovedListener (
        [this] (juce::Uuid uuid)
        {
            closePluginWindowFor (uuid);
            if (auto* gn = graph_->getNode (uuid))
                if (gn->processor != nullptr)
                    gn->processor->removeListener (this);
            unbindAllForNode (uuid);
        });

    // Attach an AudioProcessorListener to every newly-added node so we can
    // capture parameter changes (knob moves on the plug-in's GUI, value
    // edits on native-node UI controls that go through AudioProcessorParameter)
    // for the undo history. Native nodes that don't expose their state as
    // AudioProcessorParameters won't fire these callbacks — those are
    // handled by explicit commits in the side-panel UI.
    graph_->setNodeAddedListener (
        [this] (juce::Uuid uuid)
        {
            if (auto* gn = graph_->getNode (uuid))
                if (gn->processor != nullptr)
                    gn->processor->addListener (this);
        });

    // Seed history with the empty initial state so the user can undo back to it.
    commitHistorySnapshot();
}

Mcfx_graphAudioProcessor::~Mcfx_graphAudioProcessor()
{
    stopTimer();
}

//==============================================================================

bool Mcfx_graphAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   #if MCFX_MULTICHANNEL_BUILD
    return mcfx::isMultichannelLayoutSupported (layouts, NUM_CHANNELS, wrapperType);
   #else
    return layouts.getMainInputChannelSet().size()  == NUM_CHANNELS
        && layouts.getMainOutputChannelSet().size() == NUM_CHANNELS;
   #endif
}

void Mcfx_graphAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    const int prevNumIn  = graph_->getInputChannelCount();
    const int prevNumOut = graph_->getOutputChannelCount();

    graph_->prepareToPlay (sampleRate, samplesPerBlock, numIn, numOut);

    // For an empty (just-constructed) graph, wire input -> output 1:1 as a
    // sensible default so the host hears something while the editor is
    // empty. Skip when the user has already added user nodes — they probably
    // want to place their own routing.
    if (graph_->getAllUserNodes().isEmpty()
        && (prevNumIn == 0 || prevNumOut == 0))
    {
        graph_->clearAllConnections();
        graph_->connectIOPassthrough();
    }

    setLatencySamples (graph_->getLatencySamples());
}

void Mcfx_graphAudioProcessor::releaseResources()
{
    graph_->releaseResources();
}

void Mcfx_graphAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear(); // graph is audio-only

    // Apply pending host-driven automation writes to the bound inner params at
    // the block boundary. The inner param's setValue is realtime-safe in the
    // common case (atomic store on the parameter's own value cache).
    for (auto* fp : forwardingParameters_)
    {
        float v = 0.0f;
        if (fp->consumePending (v))
            if (auto* inner = fp->getInnerParameter())
                inner->setValue (v);
    }

    graph_->processBlock (buffer, midi);
}

//==============================================================================

juce::AudioProcessorEditor* Mcfx_graphAudioProcessor::createEditor()
{
    return new Mcfx_graphAudioProcessorEditor (*this);
}

//==============================================================================

juce::var Mcfx_graphAudioProcessor::exportToVar() const
{
    auto v = GraphSerializer::topLevelToVar (*graph_);

    // Append the active forwarding-parameter bindings so they survive
    // project save/load.
    if (auto* obj = v.getDynamicObject())
    {
        juce::Array<juce::var> arr;
        for (int i = 0; i < forwardingParameters_.size(); ++i)
        {
            auto* fp = forwardingParameters_.getUnchecked (i);
            if (! fp->isBound()) continue;

            auto* o = new juce::DynamicObject();
            o->setProperty ("slot",       i);
            o->setProperty ("nodeUuid",   fp->getNodeUuid().toString());
            o->setProperty ("paramIndex", fp->getParamIndex());
            arr.add (juce::var (o));
        }
        obj->setProperty ("exposedParameters", arr);

        // Editor UI prefs — persisted so panel width etc. survive reload.
        auto* edObj = new juce::DynamicObject();
        edObj->setProperty ("propertyPanelWidth", propertyPanelWidth_);
        obj->setProperty ("editor", juce::var (edObj));
    }

    return v;
}

bool Mcfx_graphAudioProcessor::importFromVar (const juce::var& v, juce::String* errorOut)
{
    GraphController::ScopedSuspend suspend (*this);

    // Tear-down + rebuild fires many topology notifications; suppress history
    // captures so an import shows up as a single undo step (or zero, if the
    // caller is undo/redo).
    //
    // The flag must outlive the synchronous body of this function — the
    // editor's topology listener defers commitHistorySnapshot via
    // MessageManager::callAsync, so the spurious post-rebuild commits that
    // would otherwise wipe the redo history fire AFTER importFromVar returns.
    // Hold the flag with a counter, then release it via another callAsync that
    // queues behind those topology callbacks (FIFO order in the message queue).
    ++suppressHistoryDepth_;

    bool ok = true;
    const bool fastPath = tryFastUpdate (v);

    if (! fastPath)
    {
        // Slow path: full rebuild. Inner-parameter pointers held by slot
        // bindings are about to become dangling — drop the pointers (we
        // re-resolve below).
        for (auto* fp : forwardingParameters_)
            fp->unbind();

        ok = GraphSerializer::topLevelFromVar (
                 v, *graph_, pluginList_.getFormatManager(), errorOut);

        // After rebuild, re-prepare any newly-added nodes with the current rate/bs.
        if (getSampleRate() > 0.0 && getBlockSize() > 0)
            graph_->prepareToPlay (getSampleRate(), getBlockSize(),
                                   getTotalNumInputChannels(),
                                   getTotalNumOutputChannels());
    }
    else
    {
        // Fast path already applied state in place. Drop bindings; we'll
        // re-resolve them below against the (still-live) processors.
        for (auto* fp : forwardingParameters_)
            fp->unbind();
    }

    // Restore parameter bindings from the var, resolving against the freshly-
    // rebuilt graph. Bindings whose node or paramIndex no longer exist are
    // silently skipped (the slot stays unbound).
    if (auto* obj = v.getDynamicObject())
    {
        // Editor UI prefs.
        if (auto* edObj = obj->getProperty ("editor").getDynamicObject())
        {
            const int w = (int) edObj->getProperty ("propertyPanelWidth");
            if (w > 0) propertyPanelWidth_ = w;
        }

        if (auto* arr = obj->getProperty ("exposedParameters").getArray())
        {
            for (const auto& b : *arr)
            {
                auto* o = b.getDynamicObject();
                if (o == nullptr) continue;

                const int  slot       = (int) o->getProperty ("slot");
                const juce::Uuid uuid (o->getProperty ("nodeUuid").toString());
                const int  paramIndex = (int) o->getProperty ("paramIndex");

                if (slot < 0 || slot >= forwardingParameters_.size()) continue;

                if (auto* inner = lookupInnerParameter (uuid, paramIndex))
                {
                    const auto name = inner->getName (64);
                    forwardingParameters_.getUnchecked (slot)->bind (
                        inner, uuid, paramIndex,
                        name.isNotEmpty() ? name
                                          : juce::String ("Param ") + juce::String (paramIndex));
                }
            }
        }
    }

    setLatencySamples (graph_->getLatencySamples());
    sendChangeMessage();

    // Release the suppress flag AFTER any topology-listener callAsyncs queued
    // during the rebuild — same MessageManager queue, FIFO order, so this runs
    // last and we resume committing future user actions normally.
    //
    // Also bump the parameter-debounce epoch: state restore typically
    // emits parameterValueChanged events on every restored parameter; the
    // 450 ms debounced commits scheduled for those events would otherwise
    // fire AFTER the import completes (depth back to 0) and commit a
    // post-undo snapshot that wipes the redo history.
    juce::MessageManager::callAsync ([this]
    {
        ++paramDirtyEpoch_;
        --suppressHistoryDepth_;
    });

    return ok;
}

//==============================================================================
// Undo / redo

void Mcfx_graphAudioProcessor::commitHistorySnapshot()
{
    if (suppressHistoryDepth_ > 0) return;

    // Skip duplicate snapshots — a single user action often fires several
    // notifyTopologyChanged calls in a row (e.g. deleting 3 wires triggers
    // 3 callAsyncs, all of which would push identical states and wipe the
    // redo history because each call truncates entries past the current
    // index). Compare the new state against the current head as JSON.
    auto newState = exportToVar();
    if (historyIndex_ >= 0 && historyIndex_ < (int) history_.size())
    {
        const auto a = juce::JSON::toString (history_[(std::size_t) historyIndex_], true);
        const auto b = juce::JSON::toString (newState, true);
        if (a == b) return;
    }

    // Truncate any redo entries past the current point — a fresh action
    // invalidates redo history.
    if (historyIndex_ + 1 < (int) history_.size())
        history_.erase (history_.begin() + historyIndex_ + 1, history_.end());

    history_.push_back (std::move (newState));
    historyIndex_ = (int) history_.size() - 1;

    // Cap memory: drop oldest entries when we exceed kMaxHistory.
    while ((int) history_.size() > kMaxHistory)
    {
        history_.erase (history_.begin());
        --historyIndex_;
    }
}

bool Mcfx_graphAudioProcessor::canUndo() const { return historyIndex_ > 0; }
bool Mcfx_graphAudioProcessor::canRedo() const { return historyIndex_ + 1 < (int) history_.size(); }

bool Mcfx_graphAudioProcessor::undo()
{
    if (! canUndo()) return false;
    --historyIndex_;
    importFromVar (history_[(std::size_t) historyIndex_]);
    return true;
}

bool Mcfx_graphAudioProcessor::redo()
{
    if (! canRedo()) return false;
    ++historyIndex_;
    importFromVar (history_[(std::size_t) historyIndex_]);
    return true;
}

//==============================================================================
// Parameter exposure

juce::AudioProcessorParameter*
Mcfx_graphAudioProcessor::lookupInnerParameter (juce::Uuid nodeUuid, int paramIndex) const
{
    if (auto* gn = graph_->getNode (nodeUuid))
        if (gn->processor != nullptr)
        {
            auto& params = gn->processor->getParameters();
            if (paramIndex >= 0 && paramIndex < params.size())
                return params.getUnchecked (paramIndex);
        }
    return nullptr;
}

int Mcfx_graphAudioProcessor::getExposedSlotFor (juce::Uuid nodeUuid, int paramIndex) const
{
    for (int i = 0; i < forwardingParameters_.size(); ++i)
    {
        auto* fp = forwardingParameters_.getUnchecked (i);
        if (fp->isBound()
            && fp->getNodeUuid()   == nodeUuid
            && fp->getParamIndex() == paramIndex)
            return i;
    }
    return -1;
}

int Mcfx_graphAudioProcessor::exposeParameter (juce::Uuid nodeUuid, int paramIndex)
{
    // Already exposed? No-op, return the existing slot.
    if (const int existing = getExposedSlotFor (nodeUuid, paramIndex); existing >= 0)
        return existing;

    auto* inner = lookupInnerParameter (nodeUuid, paramIndex);
    if (inner == nullptr) return -1;

    for (int i = 0; i < forwardingParameters_.size(); ++i)
    {
        auto* fp = forwardingParameters_.getUnchecked (i);
        if (! fp->isBound())
        {
            const auto name = inner->getName (64);
            fp->bind (inner, nodeUuid, paramIndex,
                      name.isNotEmpty() ? name : juce::String ("Param ") + juce::String (paramIndex));
            return i;
        }
    }
    return -1; // pool exhausted
}

void Mcfx_graphAudioProcessor::unexposeParameter (juce::Uuid nodeUuid, int paramIndex)
{
    const int slot = getExposedSlotFor (nodeUuid, paramIndex);
    if (slot >= 0)
        forwardingParameters_.getUnchecked (slot)->unbind();
}

void Mcfx_graphAudioProcessor::unbindAllForNode (juce::Uuid nodeUuid)
{
    for (auto* fp : forwardingParameters_)
        if (fp->isBound() && fp->getNodeUuid() == nodeUuid)
            fp->unbind();
}

void Mcfx_graphAudioProcessor::rebindAllSlotsAfterRestore()
{
    // After a JSON-restore the parameter pointers held by each slot point at
    // destroyed instances. Walk the slots, re-resolve the (uuid, paramIndex)
    // bindings against the freshly-restored graph, and re-bind. Slots whose
    // node no longer exists become unbound but keep slot count constant.
    for (auto* fp : forwardingParameters_)
    {
        if (! fp->isBound()) continue;

        const auto uuid       = fp->getNodeUuid();
        const int  paramIndex = fp->getParamIndex();

        if (auto* inner = lookupInnerParameter (uuid, paramIndex))
            fp->bind (inner, uuid, paramIndex,
                      inner->getName (64).isNotEmpty()
                          ? inner->getName (64)
                          : juce::String ("Param ") + juce::String (paramIndex));
        else
            fp->unbind();
    }
}

//==============================================================================
// AudioProcessorListener — captures parameter changes from hosted nodes for
// the undo history. Plug-in GUIs typically wrap drags in a gestureBegin /
// gestureEnd pair; we commit immediately on gestureEnd. Plug-ins that don't
// emit gestures (or quick toggles that change without a gesture) fall back to
// a debounced commit so a slider-tick storm becomes one undo step.

void Mcfx_graphAudioProcessor::audioProcessorParameterChanged (juce::AudioProcessor*,
                                                                 int, float)
{
    // Skip notifications that fire while an import is in flight — those are
    // echoes from setStateInformation / setBusesLayout / prepareToPlay,
    // not user actions. Some plug-ins emit a flurry of parameterValueChanged
    // during state restore; we don't want them triggering debounced commits
    // (the post-import epoch bump would invalidate them anyway, but skipping
    // here avoids the overhead and any reentrancy risks).
    if (suppressHistoryDepth_.load (std::memory_order_relaxed) > 0) return;
    scheduleDebouncedHistoryCommit();
}

void Mcfx_graphAudioProcessor::audioProcessorChanged (juce::AudioProcessor*,
                                                       const juce::AudioProcessorListener::ChangeDetails&) {}

void Mcfx_graphAudioProcessor::audioProcessorParameterChangeGestureBegin
    (juce::AudioProcessor*, int) {}

void Mcfx_graphAudioProcessor::audioProcessorParameterChangeGestureEnd
    (juce::AudioProcessor*, int)
{
    if (suppressHistoryDepth_.load (std::memory_order_relaxed) > 0) return;
    juce::MessageManager::callAsync ([this] { commitHistorySnapshot(); });
}

void Mcfx_graphAudioProcessor::scheduleDebouncedHistoryCommit()
{
    // Bump the epoch and schedule a commit that only fires if no further
    // change arrives within the debounce window. Listener callbacks may
    // come from the audio thread, so the commit is deferred to the message
    // thread via callAfterDelay.
    const int epoch = ++paramDirtyEpoch_;
    juce::Timer::callAfterDelay (450, [this, epoch]
    {
        if (paramDirtyEpoch_.load() == epoch)
            commitHistorySnapshot();
    });
}

void Mcfx_graphAudioProcessor::timerCallback()
{
    // Mirror inner-parameter values back to the host for any slot whose inner
    // value drifted away from the cached host-side value (typically because
    // the user moved a knob on the plugin's GUI). This lets the DAW record
    // automation against GUI-driven movement.
    for (auto* fp : forwardingParameters_)
    {
        if (auto* inner = fp->getInnerParameter())
        {
            const float innerVal  = inner->getValue();
            const float cachedVal = fp->getValue();
            if (std::abs (innerVal - cachedVal) > 1.0e-6f)
                fp->updateFromInner (innerVal);
        }
    }
}

bool Mcfx_graphAudioProcessor::saveToJsonFile (const juce::File& file, juce::String* errorOut) const
{
    auto v = exportToVar();
    // Pretty-printed (multi-line, indented) — the JSON file is meant to be
    // human-readable and hand-editable, not just a machine round-trip.
    auto json = juce::JSON::toString (v, false);
    if (! file.replaceWithText (json))
    {
        if (errorOut) *errorOut = "Failed to write " + file.getFullPathName();
        return false;
    }
    return true;
}

bool Mcfx_graphAudioProcessor::loadFromJsonFile (const juce::File& file, juce::String* errorOut)
{
    if (! file.existsAsFile())
    {
        if (errorOut) *errorOut = "File does not exist";
        return false;
    }
    auto v = juce::JSON::parse (file);
    if (v.isVoid())
    {
        if (errorOut) *errorOut = "Failed to parse JSON";
        return false;
    }
    return importFromVar (v, errorOut);
}

//==============================================================================

void Mcfx_graphAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto v = exportToVar();
    auto jsonStr = juce::JSON::toString (v, false);

    juce::XmlElement xml (kStateXmlTag);
    xml.setAttribute (kStateJsonAttr, jsonStr);
    copyXmlToBinary (xml, destData);
}

void Mcfx_graphAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (kStateXmlTag)) return;

    const auto jsonStr = xml->getStringAttribute (kStateJsonAttr);
    auto v = juce::JSON::parse (jsonStr);
    if (v.isVoid()) return;

    {
        juce::SpinLock::ScopedLockType lock (pendingLock_);
        pendingState_     = std::move (v);
        hasPendingState_  = true;
    }
    triggerAsyncUpdate(); // load on the message thread (plugin instantiation requires it)
}

void Mcfx_graphAudioProcessor::handleAsyncUpdate()
{
    juce::var pending;
    {
        juce::SpinLock::ScopedLockType lock (pendingLock_);
        if (! hasPendingState_) return;
        pending = std::move (pendingState_);
        hasPendingState_ = false;
    }
    importFromVar (pending);
}

//==============================================================================
// Fast undo path. When the only difference between the current graph and the
// snapshot we're restoring is per-node parameter values (or position / mute /
// bypass / connections), there's no need to destroy and recreate any plugin
// instances — applying setStateInformation / fromVar in place leaves the
// hosted plugin GUIs open, preserves DAW automation bindings, and avoids
// triggering plugin teardown bugs (notably Waves YAML write crashes).

bool Mcfx_graphAudioProcessor::tryFastUpdate (const juce::var& v)
{
    auto* topObj = v.getDynamicObject();
    if (topObj == nullptr) return false;

    const auto rootGraph = topObj->getProperty ("rootGraph");
    auto* rootObj = rootGraph.getDynamicObject();
    if (rootObj == nullptr) return false;

    if (! structureMatches (rootGraph, *graph_)) return false;

    // Apply state changes to each existing node.
    if (auto* nodesArr = rootObj->getProperty ("nodes").getArray())
    {
        for (const auto& nv : *nodesArr)
        {
            auto* nObj = nv.getDynamicObject();
            if (nObj == nullptr) continue;

            const juce::Uuid uid (nObj->getProperty ("uuid").toString());
            auto* gn = graph_->getNode (uid);
            if (gn == nullptr) return false; // shouldn't happen — structureMatches verified

            gn->editorPosition = readPosition (nObj->getProperty ("position"));
            gn->displayName    = nObj->getProperty ("displayName").toString();

            // Bypass / mute via the controller so the wrapper's atomic flags
            // are kept in sync.
            graph_->setNodeBypassed (uid, (bool) nObj->getProperty ("bypassed"));
            graph_->setNodeMuted    (uid, (bool) nObj->getProperty ("muted"));

            const auto data = nObj->getProperty ("data");

            switch (gn->kind)
            {
                case NodeKind::Gain:
                    if (auto* n = dynamic_cast<GainNode*>      (gn->processor)) n->fromVar (data);
                    break;
                case NodeKind::MutePhase:
                    if (auto* n = dynamic_cast<MutePhaseNode*> (gn->processor)) n->fromVar (data);
                    break;
                case NodeKind::MatrixMixer:
                    if (auto* n = dynamic_cast<MatrixMixerNode*> (gn->processor)) n->fromVar (data);
                    break;
                case NodeKind::Delay:
                    if (auto* n = dynamic_cast<DelayNode*>     (gn->processor)) n->fromVar (data);
                    break;
                case NodeKind::Plugin:
                    if (gn->processor != nullptr)
                        applyBase64State (*gn->processor, nObj->getProperty ("state").toString());
                    break;
                case NodeKind::Subgraph:
                case NodeKind::InputTerminal:
                case NodeKind::OutputTerminal:
                    break;
            }
        }
    }

    // Reconcile connections by wiping and re-adding from the snapshot.
    // AudioProcessorGraph absorbs each addConnection cheaply (it queues a
    // render-sequence rebuild that fires lazily), so this is fast for any
    // realistic graph.
    graph_->clearAllConnections();
    if (auto* connsArr = rootObj->getProperty ("connections").getArray())
    {
        for (const auto& cv : *connsArr)
        {
            auto* cObj = cv.getDynamicObject();
            if (cObj == nullptr) continue;

            auto* fromObj = cObj->getProperty ("from").getDynamicObject();
            auto* toObj   = cObj->getProperty ("to").getDynamicObject();
            if (fromObj == nullptr || toObj == nullptr) continue;

            const auto fromUuid = GraphSerializer::uuidFromSentinel (
                                      fromObj->getProperty ("uuid").toString(), *graph_);
            const auto toUuid   = GraphSerializer::uuidFromSentinel (
                                      toObj  ->getProperty ("uuid").toString(), *graph_);
            const int  fromCh   = (int) fromObj->getProperty ("channel");
            const int  toCh     = (int) toObj  ->getProperty ("channel");

            graph_->addConnection (fromUuid, fromCh, toUuid, toCh);
        }
    }

    return true;
}

//==============================================================================
// Plugin GUI window registry.

void Mcfx_graphAudioProcessor::registerPluginWindow (juce::Uuid uuid,
                                                     juce::DocumentWindow* dw)
{
    if (dw == nullptr) return;

    // If a window already exists for this node (e.g. user opened it twice),
    // close the older one — keeping two would leave a stale window when the
    // map only tracks the latest.
    closePluginWindowFor (uuid);
    pluginWindows_[uuid.toString()] = dw;
}

void Mcfx_graphAudioProcessor::unregisterPluginWindow (juce::DocumentWindow* dw)
{
    if (dw == nullptr) return;
    for (auto it = pluginWindows_.begin(); it != pluginWindows_.end(); ++it)
    {
        if (it->second == dw)
        {
            pluginWindows_.erase (it);
            return;
        }
    }
}

void Mcfx_graphAudioProcessor::closePluginWindowFor (juce::Uuid uuid)
{
    auto it = pluginWindows_.find (uuid.toString());
    if (it == pluginWindows_.end()) return;

    auto* dw = it->second;
    pluginWindows_.erase (it); // remove first so the dtor's unregister is a no-op
    delete dw;
}

//==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Mcfx_graphAudioProcessor();
}
