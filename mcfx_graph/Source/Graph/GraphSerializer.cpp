#include "GraphSerializer.h"
#include "SubgraphNode.h"
#include "../NativeNodes/GainNode.h"
#include "../NativeNodes/MutePhaseNode.h"
#include "../NativeNodes/MatrixMixerNode.h"
#include "../NativeNodes/DelayNode.h"
#include "../Hosting/PluginInstanceFactory.h"

namespace
{
    juce::var makePosition (juce::Point<int> p)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("x", p.x);
        obj->setProperty ("y", p.y);
        return juce::var (obj);
    }

    juce::Point<int> readPosition (const juce::var& v)
    {
        if (auto* obj = v.getDynamicObject())
            return { (int) obj->getProperty ("x"), (int) obj->getProperty ("y") };
        return { 100, 100 };
    }

    juce::String stateToBase64 (juce::AudioProcessor& p)
    {
        juce::MemoryBlock mb;
        p.getStateInformation (mb);
        if (mb.getSize() == 0) return {};
        return juce::Base64::toBase64 (mb.getData(), mb.getSize());
    }

    void applyBase64State (juce::AudioProcessor& p, const juce::String& b64)
    {
        if (b64.isEmpty()) return;
        juce::MemoryOutputStream mo;
        if (! juce::Base64::convertFromBase64 (mo, b64)) return;
        p.setStateInformation (mo.getData(), (int) mo.getDataSize());
    }
}

//==============================================================================

juce::String GraphSerializer::uuidOrSentinel (const juce::Uuid& u, const GraphController& c)
{
    if (u == c.getInputTerminalUuid())  return kInputSentinel;
    if (u == c.getOutputTerminalUuid()) return kOutputSentinel;
    return u.toString();
}

juce::Uuid GraphSerializer::uuidFromSentinel (const juce::String& s, const GraphController& c)
{
    if (s == kInputSentinel)  return c.getInputTerminalUuid();
    if (s == kOutputSentinel) return c.getOutputTerminalUuid();
    return juce::Uuid (s);
}

//==============================================================================

juce::var GraphSerializer::nodeVarFromGraphNode (const GraphNode& gn)
{
    if (gn.kind == NodeKind::InputTerminal || gn.kind == NodeKind::OutputTerminal)
        return {};

    auto* nodeObj = new juce::DynamicObject();
    nodeObj->setProperty ("uuid",            gn.uuid.toString());
    nodeObj->setProperty ("type",            nodeKindToString (gn.kind));
    nodeObj->setProperty ("displayName",     gn.displayName);
    nodeObj->setProperty ("position",        makePosition (gn.editorPosition));
    nodeObj->setProperty ("channelCountIn",  gn.channelCountIn);
    nodeObj->setProperty ("channelCountOut", gn.channelCountOut);
    if (gn.bypassed) nodeObj->setProperty ("bypassed", true);
    if (gn.muted)    nodeObj->setProperty ("muted",    true);

    switch (gn.kind)
    {
        case NodeKind::Gain:
            if (auto* n = dynamic_cast<GainNode*> (gn.processor))
                nodeObj->setProperty ("data", n->toVar());
            break;
        case NodeKind::MutePhase:
            if (auto* n = dynamic_cast<MutePhaseNode*> (gn.processor))
                nodeObj->setProperty ("data", n->toVar());
            break;
        case NodeKind::MatrixMixer:
            if (auto* n = dynamic_cast<MatrixMixerNode*> (gn.processor))
                nodeObj->setProperty ("data", n->toVar());
            break;
        case NodeKind::Delay:
            if (auto* n = dynamic_cast<DelayNode*> (gn.processor))
                nodeObj->setProperty ("data", n->toVar());
            break;
        case NodeKind::Subgraph:
            if (auto* sub = dynamic_cast<SubgraphNode*> (gn.processor))
                nodeObj->setProperty ("data", graphToVar (sub->getInner()));
            break;
        case NodeKind::Plugin:
            // Cross-platform-portable plugin reference: store JUCE's
            // location-independent identifier plus enough human-readable
            // fields to give a useful error if the plugin isn't installed
            // on the loading machine. NO absolute file paths are written.
            if (gn.pluginDescription != nullptr)
            {
                nodeObj->setProperty ("pluginIdentifier",
                                       gn.pluginDescription->createIdentifierString());
                nodeObj->setProperty ("pluginFormatName",
                                       gn.pluginDescription->pluginFormatName);
                nodeObj->setProperty ("pluginName",
                                       gn.pluginDescription->name);
                nodeObj->setProperty ("pluginManufacturer",
                                       gn.pluginDescription->manufacturerName);
            }
            if (gn.processor != nullptr)
                nodeObj->setProperty ("state", stateToBase64 (*gn.processor));
            break;
        case NodeKind::InputTerminal:
        case NodeKind::OutputTerminal:
            break;
    }

    return juce::var (nodeObj);
}

juce::var GraphSerializer::graphToVar (const GraphController& controller)
{
    auto* root = new juce::DynamicObject();

    juce::Array<juce::var> nodesArr;
    for (auto* gn : controller.getAllUserNodes())
    {
        auto v = nodeVarFromGraphNode (*gn);
        if (! v.isVoid())
            nodesArr.add (v);
    }
    root->setProperty ("nodes", nodesArr);

    juce::Array<juce::var> connsArr;
    for (const auto& c : controller.getAllConnections())
    {
        auto* fromObj = new juce::DynamicObject();
        fromObj->setProperty ("uuid",    uuidOrSentinel (c.fromUuid, controller));
        fromObj->setProperty ("channel", c.fromCh);

        auto* toObj = new juce::DynamicObject();
        toObj->setProperty ("uuid",    uuidOrSentinel (c.toUuid, controller));
        toObj->setProperty ("channel", c.toCh);

        auto* connObj = new juce::DynamicObject();
        connObj->setProperty ("from", juce::var (fromObj));
        connObj->setProperty ("to",   juce::var (toObj));
        connsArr.add (juce::var (connObj));
    }
    root->setProperty ("connections", connsArr);

    // Persist the I/O terminal positions so the user's layout survives reload
    // and undo/redo. Terminals themselves are not in the "nodes" array (their
    // identity is implicit), so positions ride along as separate fields.
    if (auto* inGn = controller.getNode (controller.getInputTerminalUuid()))
        root->setProperty ("inputTerminalPosition",  makePosition (inGn->editorPosition));
    if (auto* outGn = controller.getNode (controller.getOutputTerminalUuid()))
        root->setProperty ("outputTerminalPosition", makePosition (outGn->editorPosition));

    return juce::var (root);
}

juce::var GraphSerializer::topLevelToVar (const GraphController& controller)
{
    auto* obj = new juce::DynamicObject();
    // These three keys go first so anyone scanning the file knows what they're
    // looking at within the first few lines.
    obj->setProperty ("format",      "mcfx_graph");
    obj->setProperty ("description", "mcfx_graph signal flow / patchbay graph "
                                     "(human-readable JSON; may be edited by hand "
                                     "if you know what you're doing).");
    obj->setProperty ("version",     kCurrentVersion);
    obj->setProperty ("rootGraph",   graphToVar (controller));
    return juce::var (obj);
}

//==============================================================================

bool GraphSerializer::topLevelFromVar (const juce::var& v,
                                       GraphController& controller,
                                       juce::AudioPluginFormatManager& formatManager,
                                       const juce::KnownPluginList* knownPluginList,
                                       juce::String* errorOut)
{
    auto* obj = v.getDynamicObject();
    if (obj == nullptr) { if (errorOut) *errorOut = "Top-level is not an object"; return false; }

    const auto root = obj->getProperty ("rootGraph");
    return graphFromVar (root, controller, formatManager, knownPluginList, errorOut);
}

namespace
{
    // Resolve a plugin reference to a PluginDescription that points at a
    // locally-installed file. Tries (in order):
    //   1. Identifier lookup in the local KnownPluginList — strict, exact.
    //   2. (format, name) match in the local KnownPluginList — handles minor
    //      identifier drift across plugin upgrades.
    //   3. Legacy XML payload (older files) — works only on the original
    //      machine; we still attempt the identifier lookup against the local
    //      KPL using the XML-derived identifier so old files become portable
    //      automatically when the plugin is installed locally.
    std::unique_ptr<juce::PluginDescription> resolvePluginDescription (
        juce::DynamicObject& nObj,
        const juce::KnownPluginList* kpl)
    {
        const auto identifier   = nObj.getProperty ("pluginIdentifier").toString();
        const auto formatName   = nObj.getProperty ("pluginFormatName").toString();
        const auto pluginName   = nObj.getProperty ("pluginName").toString();
        const auto pluginMfg    = nObj.getProperty ("pluginManufacturer").toString();

        auto tryKplIdLookup = [kpl] (const juce::String& id)
            -> std::unique_ptr<juce::PluginDescription>
        {
            if (kpl == nullptr || id.isEmpty()) return nullptr;
            return kpl->getTypeForIdentifierString (id);
        };

        auto tryKplFallbackMatch = [kpl] (const juce::String& fmt,
                                          const juce::String& name)
            -> std::unique_ptr<juce::PluginDescription>
        {
            if (kpl == nullptr || fmt.isEmpty() || name.isEmpty()) return nullptr;
            for (const auto& d : kpl->getTypes())
                if (d.pluginFormatName == fmt && d.name == name)
                    return std::make_unique<juce::PluginDescription> (d);
            return nullptr;
        };

        // 1. Strict identifier lookup against the local KPL.
        if (auto found = tryKplIdLookup (identifier))
            return found;

        // 2. Fallback: (format, name) match against the local KPL.
        if (auto found = tryKplFallbackMatch (formatName, pluginName))
            return found;

        // 3. Legacy XML payload — used for old files and as a last resort
        //    when the plugin isn't in the local KPL but its full description
        //    (with the original machine's file path) was saved.
        const auto descXml = nObj.getProperty ("pluginDescriptionXml").toString();
        if (descXml.isNotEmpty())
        {
            if (auto xml = juce::parseXML (descXml))
            {
                auto desc = std::make_unique<juce::PluginDescription>();
                if (desc->loadFromXml (*xml))
                {
                    // Even with old XML, prefer a local KPL entry if one
                    // exists for this identifier — that keeps the loaded
                    // description's fileOrIdentifier pointing at the local
                    // installation rather than a stale path from another
                    // machine.
                    if (auto local = tryKplIdLookup (desc->createIdentifierString()))
                        return local;
                    return desc;
                }
            }
        }

        juce::ignoreUnused (pluginMfg); // currently informational only
        return nullptr;
    }
}

std::unique_ptr<juce::AudioProcessor> GraphSerializer::buildProcessorFromNodeVar (
    const juce::var& nodeObj,
    juce::AudioPluginFormatManager& formatManager,
    std::unique_ptr<juce::PluginDescription>& outPluginDesc,
    const juce::KnownPluginList* knownPluginList)
{
    outPluginDesc.reset();

    auto* nObj = nodeObj.getDynamicObject();
    if (nObj == nullptr) return nullptr;

    const auto type   = nObj->getProperty ("type").toString();
    const auto kind   = nodeKindFromString (type);
    const int  chIn   = (int) nObj->getProperty ("channelCountIn");
    const int  chOut  = (int) nObj->getProperty ("channelCountOut");
    const auto data   = nObj->getProperty ("data");

    switch (kind)
    {
        case NodeKind::Gain:
        {
            auto p = std::make_unique<GainNode> (chIn);
            p->fromVar (data);
            return p;
        }
        case NodeKind::MutePhase:
        {
            auto p = std::make_unique<MutePhaseNode> (chIn);
            p->fromVar (data);
            return p;
        }
        case NodeKind::MatrixMixer:
        {
            auto p = std::make_unique<MatrixMixerNode> (chIn, chOut);
            p->fromVar (data);
            return p;
        }
        case NodeKind::Delay:
        {
            auto p = std::make_unique<DelayNode> (chIn);
            p->fromVar (data);
            return p;
        }
        case NodeKind::Subgraph:
        {
            auto p = std::make_unique<SubgraphNode> (chIn, chOut);

            // Size the inner controller's IO terminals to match the
            // subgraph's declared in/out before restoring connections.
            // Otherwise __input__ / __output__ have 0 channels, every
            // connection involving them fails canConnect(), and the
            // saved routing inside the subgraph gets silently dropped.
            // The real sample rate / block size are applied later by
            // SubgraphNode::prepareToPlay; the dummy values here are
            // only to construct the IO node and make connections valid.
            p->getInner().prepareToPlay (48000.0, 512, chIn, chOut);

            if (! data.isVoid())
                graphFromVar (data, p->getInner(), formatManager, knownPluginList);
            return p;
        }
        case NodeKind::Plugin:
        {
            auto pluginDesc = resolvePluginDescription (*nObj, knownPluginList);
            if (pluginDesc == nullptr) return nullptr;

            juce::String err;
            auto inst = formatManager.createPluginInstance (
                            *pluginDesc, 44100.0, 512, err);
            if (inst == nullptr) return nullptr;

            // Only disable non-main buses if there are any — for
            // most plug-ins this is a no-op, but it avoids touching
            // the bus layout on instances that don't need it (which
            // some plug-ins handle poorly mid-state-restore).
            if (inst->getBusCount (true)  > 1
             || inst->getBusCount (false) > 1)
                PluginInstanceFactory::disableNonMainBuses (*inst);

            const auto stateB64 = nObj->getProperty ("state").toString();
            if (stateB64.isNotEmpty())
                applyBase64State (*inst, stateB64);

            // Apply the saved channel count to the actual plug-in
            // bus, BUT only if the state restore didn't already
            // place us at the right size. trySetMainBusChannels
            // does release-and-reset which destabilises some
            // plug-ins (notably Waves) when called unnecessarily.
            // Many plug-ins encode their bus layout in their state,
            // so the post-setStateInformation channel count is
            // already correct and this branch becomes a no-op.
            if (chIn > 0 && chIn == chOut
                && (inst->getMainBusNumInputChannels()  != chIn
                 || inst->getMainBusNumOutputChannels() != chOut))
            {
                PluginInstanceFactory::trySetMainBusChannels (*inst, chIn);
            }

            outPluginDesc = std::move (pluginDesc);
            return inst;
        }
        case NodeKind::InputTerminal:
        case NodeKind::OutputTerminal:
            break;
    }
    return nullptr;
}

bool GraphSerializer::graphFromVar (const juce::var& v,
                                    GraphController& controller,
                                    juce::AudioPluginFormatManager& formatManager,
                                    const juce::KnownPluginList* knownPluginList,
                                    juce::String* errorOut)
{
    auto* obj = v.getDynamicObject();
    if (obj == nullptr) { if (errorOut) *errorOut = "Graph is not an object"; return false; }

    controller.clearAllUserNodes();
    controller.clearAllConnections();

    if (auto* nodesArr = obj->getProperty ("nodes").getArray())
    {
        for (const auto& nv : *nodesArr)
        {
            auto* nObj = nv.getDynamicObject();
            if (nObj == nullptr) continue;

            const auto type      = nObj->getProperty ("type").toString();
            const auto kind      = nodeKindFromString (type);
            const auto displayN  = nObj->getProperty ("displayName").toString();
            const auto pos       = readPosition (nObj->getProperty ("position"));
            const int  chIn      = (int) nObj->getProperty ("channelCountIn");
            const int  chOut     = (int) nObj->getProperty ("channelCountOut");
            const auto uuidStr   = nObj->getProperty ("uuid").toString();
            const juce::Uuid uid (uuidStr);

            std::unique_ptr<juce::PluginDescription> pluginDesc;
            auto proc = buildProcessorFromNodeVar (nv, formatManager, pluginDesc,
                                                   knownPluginList);

            if (proc == nullptr) continue;

            auto newUuid = controller.addNode (std::move (proc), kind, displayN,
                                               chIn, chOut, pos, uid);
            if (kind == NodeKind::Plugin && pluginDesc != nullptr)
            {
                if (auto* gn = controller.getNode (newUuid))
                {
                    gn->pluginDescription = std::move (pluginDesc);

                    // Sync the GraphNode's channel-count metadata to whatever
                    // the underlying plug-in's bus actually ended up at —
                    // some plug-ins refuse to honour the saved channel count
                    // (especially via VST3 where JUCE's host can't construct
                    // a SpeakerArrangement for arbitrary discrete sizes), and
                    // we don't want pins on screen for channels that won't
                    // carry audio.
                    if (gn->processor != nullptr)
                    {
                        const int actualIn  = gn->processor->getMainBusNumInputChannels();
                        const int actualOut = gn->processor->getMainBusNumOutputChannels();
                        if (actualIn != gn->channelCountIn || actualOut != gn->channelCountOut)
                        {
                            gn->channelCountIn  = actualIn;
                            gn->channelCountOut = actualOut;
                        }
                    }
                }
            }

            // Restore bypass/mute. Bypass is applied immediately; mute is
            // applied AFTER connections are restored below so the
            // disconnect-on-mute logic has connections to stash.
            if (auto* gn = controller.getNode (newUuid))
            {
                if ((bool) nObj->getProperty ("bypassed"))
                    controller.setNodeBypassed (newUuid, true);
                gn->muted = (bool) nObj->getProperty ("muted"); // will apply below
            }
        }
    }

    if (auto* connsArr = obj->getProperty ("connections").getArray())
    {
        for (const auto& cv : *connsArr)
        {
            auto* cObj = cv.getDynamicObject();
            if (cObj == nullptr) continue;

            auto* fromObj = cObj->getProperty ("from").getDynamicObject();
            auto* toObj   = cObj->getProperty ("to").getDynamicObject();
            if (fromObj == nullptr || toObj == nullptr) continue;

            const auto fromUuid = uuidFromSentinel (fromObj->getProperty ("uuid").toString(), controller);
            const auto toUuid   = uuidFromSentinel (toObj  ->getProperty ("uuid").toString(), controller);
            const int  fromCh   = (int) fromObj->getProperty ("channel");
            const int  toCh     = (int) toObj  ->getProperty ("channel");

            controller.addConnection (fromUuid, fromCh, toUuid, toCh);
        }
    }

    // Apply mute via the wrapper now that everything is in place.
    for (auto* gn : controller.getAllUserNodes())
        if (gn->muted)
            controller.setNodeMuted (gn->uuid, true);

    // Restore I/O terminal positions if the file recorded them; older saves
    // that pre-date this field leave the terminals at the controller's
    // default positions (60,240) / (720,240).
    const auto inPosVar  = obj->getProperty ("inputTerminalPosition");
    const auto outPosVar = obj->getProperty ("outputTerminalPosition");
    if (! inPosVar.isVoid())
        controller.setNodePosition (controller.getInputTerminalUuid(),
                                    readPosition (inPosVar));
    if (! outPosVar.isVoid())
        controller.setNodePosition (controller.getOutputTerminalUuid(),
                                    readPosition (outPosVar));

    return true;
}
