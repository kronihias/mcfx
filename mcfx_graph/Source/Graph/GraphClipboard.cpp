#include "GraphClipboard.h"
#include "GraphSerializer.h"
#include <set>

namespace
{
    constexpr const char* kClipboardFormat = "mcfx_graph_clipboard";
    constexpr int         kClipboardVersion = 1;
    constexpr int         kFallbackPasteOffset = 30;

    juce::var makePoint (juce::Point<int> p)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("x", p.x);
        obj->setProperty ("y", p.y);
        return juce::var (obj);
    }

    juce::Point<int> readPoint (const juce::var& v, juce::Point<int> fallback)
    {
        if (auto* obj = v.getDynamicObject())
            return { (int) obj->getProperty ("x"), (int) obj->getProperty ("y") };
        return fallback;
    }
}

bool GraphClipboard::copySelection (const GraphController& controller,
                                    const std::vector<juce::Uuid>& selectedNodes)
{
    // Build the set of UUIDs we're actually copying (skip terminals and
    // anything the controller no longer recognises).
    std::set<juce::Uuid> selectedSet;
    juce::Array<GraphNode*> nodesToCopy;

    for (const auto& uuid : selectedNodes)
    {
        auto* gn = controller.getNode (uuid);
        if (gn == nullptr) continue;
        if (gn->kind == NodeKind::InputTerminal || gn->kind == NodeKind::OutputTerminal)
            continue;
        if (selectedSet.insert (uuid).second)
            nodesToCopy.add (gn);
    }

    if (nodesToCopy.isEmpty())
        return false;

    // Serialize the nodes and compute the bounding-box anchor (top-left).
    juce::Array<juce::var> nodesArr;
    int minX = std::numeric_limits<int>::max();
    int minY = std::numeric_limits<int>::max();

    for (auto* gn : nodesToCopy)
    {
        auto v = GraphSerializer::nodeVarFromGraphNode (*gn);
        if (v.isVoid()) continue;
        nodesArr.add (v);
        minX = juce::jmin (minX, gn->editorPosition.x);
        minY = juce::jmin (minY, gn->editorPosition.y);
    }

    if (nodesArr.isEmpty())
        return false;

    // Serialize internal connections — both endpoints must be in the selection.
    juce::Array<juce::var> connsArr;
    for (const auto& c : controller.getAllConnections())
    {
        if (selectedSet.find (c.fromUuid) == selectedSet.end()) continue;
        if (selectedSet.find (c.toUuid)   == selectedSet.end()) continue;

        auto* fromObj = new juce::DynamicObject();
        fromObj->setProperty ("uuid",    c.fromUuid.toString());
        fromObj->setProperty ("channel", c.fromCh);

        auto* toObj = new juce::DynamicObject();
        toObj->setProperty ("uuid",    c.toUuid.toString());
        toObj->setProperty ("channel", c.toCh);

        auto* connObj = new juce::DynamicObject();
        connObj->setProperty ("from", juce::var (fromObj));
        connObj->setProperty ("to",   juce::var (toObj));
        connsArr.add (juce::var (connObj));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("format",      kClipboardFormat);
    root->setProperty ("version",     kClipboardVersion);
    root->setProperty ("anchor",      makePoint ({ minX, minY }));
    root->setProperty ("nodes",       nodesArr);
    root->setProperty ("connections", connsArr);

    juce::SystemClipboard::copyTextToClipboard (juce::JSON::toString (juce::var (root)));
    return true;
}

bool GraphClipboard::clipboardHasGraphData()
{
    const auto text = juce::SystemClipboard::getTextFromClipboard();
    if (text.isEmpty()) return false;

    // Cheap pre-check before paying for full JSON parse — the format key is
    // always near the very start of the envelope.
    if (! text.contains (kClipboardFormat))
        return false;

    juce::var root;
    if (juce::JSON::parse (text, root).failed()) return false;
    auto* obj = root.getDynamicObject();
    return obj != nullptr
        && obj->getProperty ("format").toString() == kClipboardFormat;
}

std::vector<juce::Uuid> GraphClipboard::pasteFromClipboard (
    GraphController& controller,
    juce::AudioPluginFormatManager& formatManager,
    const juce::KnownPluginList* knownPluginList,
    std::optional<juce::Point<int>> pasteOriginWorld)
{
    std::vector<juce::Uuid> newUuids;

    const auto text = juce::SystemClipboard::getTextFromClipboard();
    if (text.isEmpty())
        return newUuids;

    juce::var root;
    if (juce::JSON::parse (text, root).failed())
        return newUuids;

    auto* obj = root.getDynamicObject();
    if (obj == nullptr) return newUuids;
    if (obj->getProperty ("format").toString() != kClipboardFormat)
        return newUuids;

    const auto anchor = readPoint (obj->getProperty ("anchor"), { 0, 0 });
    const auto delta  = pasteOriginWorld.has_value()
                            ? (*pasteOriginWorld - anchor)
                            : juce::Point<int> { kFallbackPasteOffset, kFallbackPasteOffset };

    auto* nodesArr = obj->getProperty ("nodes").getArray();
    if (nodesArr == nullptr) return newUuids;

    // Map clipboard UUIDs → freshly-generated UUIDs so connections within the
    // pasted set can be re-resolved in the destination graph.
    std::map<juce::Uuid, juce::Uuid> remap;
    // Track muted nodes so we can apply mute AFTER connections are wired up,
    // mirroring GraphSerializer::graphFromVar's ordering.
    std::vector<juce::Uuid> mutedAfter;

    for (const auto& nv : *nodesArr)
    {
        auto* nObj = nv.getDynamicObject();
        if (nObj == nullptr) continue;

        const auto type     = nObj->getProperty ("type").toString();
        const auto kind     = nodeKindFromString (type);
        if (kind == NodeKind::InputTerminal || kind == NodeKind::OutputTerminal)
            continue;

        const auto displayN = nObj->getProperty ("displayName").toString();
        const int  chIn     = (int) nObj->getProperty ("channelCountIn");
        const int  chOut    = (int) nObj->getProperty ("channelCountOut");

        // Original UUID from the clipboard — used as a key in the remap table
        // so connection endpoints written with the same UUID can be redirected
        // to the freshly-pasted node. Never reused as the new node's UUID.
        const juce::Uuid origUuid (nObj->getProperty ("uuid").toString());

        const auto origPos = [&]
        {
            if (auto* posObj = nObj->getProperty ("position").getDynamicObject())
                return juce::Point<int> { (int) posObj->getProperty ("x"),
                                          (int) posObj->getProperty ("y") };
            return juce::Point<int> { 100, 100 };
        }();

        std::unique_ptr<juce::PluginDescription> pluginDesc;
        auto proc = GraphSerializer::buildProcessorFromNodeVar (nv, formatManager, pluginDesc,
                                                                knownPluginList);
        if (proc == nullptr) continue;

        const juce::Uuid freshUuid;
        const auto newUuid = controller.addNode (std::move (proc), kind, displayN,
                                                 chIn, chOut, origPos + delta, freshUuid);

        if (kind == NodeKind::Plugin && pluginDesc != nullptr)
        {
            if (auto* gn = controller.getNode (newUuid))
            {
                gn->pluginDescription = std::move (pluginDesc);

                // Same channel-count reconciliation as GraphSerializer — some
                // plug-ins refuse the saved channel count and end up at a
                // different size. Sync the metadata so pin counts match the
                // actual bus.
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

        if ((bool) nObj->getProperty ("bypassed"))
            controller.setNodeBypassed (newUuid, true);
        if ((bool) nObj->getProperty ("muted"))
            mutedAfter.push_back (newUuid);

        remap[origUuid] = newUuid;
        newUuids.push_back (newUuid);
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

            const juce::Uuid origFrom (fromObj->getProperty ("uuid").toString());
            const juce::Uuid origTo   (toObj  ->getProperty ("uuid").toString());

            const auto itFrom = remap.find (origFrom);
            const auto itTo   = remap.find (origTo);
            if (itFrom == remap.end() || itTo == remap.end()) continue;

            const int fromCh = (int) fromObj->getProperty ("channel");
            const int toCh   = (int) toObj  ->getProperty ("channel");

            controller.addConnection (itFrom->second, fromCh, itTo->second, toCh);
        }
    }

    for (const auto& u : mutedAfter)
        controller.setNodeMuted (u, true);

    return newUuids;
}
