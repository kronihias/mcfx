/*
  mcfx_graph_test — checks mcfx_graph's graph logic without the plug-in
  wrapper or the editor. Driven by tests/test_graph_subgraph.py; exits
  non-zero and prints what failed.

    mcfx_graph_test                   run every scenario
    mcfx_graph_test --scenario NAME   run one

  Scenarios cover "Convert to subgraph" (SubgraphConversion): the converted
  graph has to sound exactly like the original, with fan-out and summing on
  both sides of the boundary, feedback pairs moving whole, and refusals that
  change nothing.
*/

#include <JuceHeader.h>
#include "Graph/GraphController.h"
#include "Graph/SubgraphConversion.h"
#include "Graph/SubgraphNode.h"
#include "NativeNodes/DelayNode.h"
#include "NativeNodes/FeedbackNodes.h"
#include "NativeNodes/GainNode.h"

#include <functional>
#include <iostream>
#include <map>

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 256;
    constexpr int    kNumBlocks  = 16;
    constexpr int    kChannels   = 4;

    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        if (! ok)
        {
            ++failures;
            std::cout << "  FAIL: " << what << std::endl;
        }
    }

    juce::Uuid addGain (GraphController& g, int ch, float db, juce::Point<int> pos)
    {
        auto p = std::make_unique<GainNode> (ch);
        for (int c = 0; c < ch; ++c) p->setGainDb (c, db);
        return g.addNode (std::move (p), NodeKind::Gain, "Gain", ch, ch, pos);
    }

    juce::Uuid addDelay (GraphController& g, int ch, float samples, juce::Point<int> pos)
    {
        auto p = std::make_unique<DelayNode> (ch);
        for (int c = 0; c < ch; ++c) p->setDelaySamples (c, samples);
        return g.addNode (std::move (p), NodeKind::Delay, "Delay", ch, ch, pos);
    }

    /** A graph plus the named nodes a scenario refers to. */
    struct Fixture
    {
        GraphController graph;
        std::map<juce::String, juce::Uuid> nodes;

        Fixture() { graph.prepareToPlay (kSampleRate, kBlockSize, kChannels, kChannels); }

        juce::Uuid in()  const { return graph.getInputTerminalUuid(); }
        juce::Uuid out() const { return graph.getOutputTerminalUuid(); }
        juce::Uuid operator[] (const juce::String& name) const { return nodes.at (name); }

        void wire (const juce::Uuid& a, int ac, const juce::Uuid& b, int bc)
        {
            if (! graph.addConnection (a, ac, b, bc))
                check (false, "fixture wire refused");
        }
    };

    /** Deterministic noise through the graph; the whole output. */
    juce::AudioBuffer<float> render (GraphController& g)
    {
        juce::AudioBuffer<float> all (kChannels, kBlockSize * kNumBlocks);
        juce::Random rng (1234);
        juce::MidiBuffer midi;

        for (int b = 0; b < kNumBlocks; ++b)
        {
            juce::AudioBuffer<float> block (kChannels, kBlockSize);
            for (int c = 0; c < kChannels; ++c)
                for (int i = 0; i < kBlockSize; ++i)
                    block.setSample (c, i, rng.nextFloat() * 2.0f - 1.0f);

            g.processBlock (block, midi);

            for (int c = 0; c < kChannels; ++c)
                all.copyFrom (c, b * kBlockSize, block, c, 0, kBlockSize);
        }
        return all;
    }

    float maxDifference (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        float d = 0.0f;
        for (int c = 0; c < a.getNumChannels(); ++c)
            for (int i = 0; i < a.getNumSamples(); ++i)
                d = juce::jmax (d, std::abs (a.getSample (c, i) - b.getSample (c, i)));
        return d;
    }

    float peak (const juce::AudioBuffer<float>& a)
    {
        float p = 0.0f;
        for (int c = 0; c < a.getNumChannels(); ++c)
            p = juce::jmax (p, a.getMagnitude (c, 0, a.getNumSamples()));
        return p;
    }

    juce::AudioPluginFormatManager& formats()
    {
        static juce::AudioPluginFormatManager fm;   // native nodes only: no formats needed
        return fm;
    }

    SubgraphConversion::Result convert (Fixture& f, std::vector<juce::Uuid> sel)
    {
        return SubgraphConversion::convert (f.graph, sel, formats(), nullptr);
    }

    SubgraphNode* subgraphOf (Fixture& f, const juce::Uuid& uuid)
    {
        if (auto* gn = f.graph.getNode (uuid))
            return dynamic_cast<SubgraphNode*> (gn->processor);
        return nullptr;
    }

    /** Build the same graph twice, convert `select` in one, and require
        identical audio. Returns the converted fixture for further checks. */
    std::unique_ptr<Fixture> expectSameAudio (const std::function<void (Fixture&)>& build,
                                              const std::vector<juce::String>& select,
                                              SubgraphConversion::Result& result)
    {
        Fixture reference;
        build (reference);

        auto converted = std::make_unique<Fixture>();
        build (*converted);

        std::vector<juce::Uuid> sel;
        for (const auto& n : select) sel.push_back ((*converted)[n]);
        result = convert (*converted, sel);
        check (result.succeeded(), "conversion succeeds: " + result.error);

        const auto a = render (reference.graph);
        const auto b = render (converted->graph);
        check (peak (a) > 0.01f, "reference graph produces audio");
        const float diff = maxDifference (a, b);
        check (diff == 0.0f, "converted graph sounds identical (max diff " + juce::String (diff) + ")");
        return converted;
    }

    //==============================================================================
    // in0 fans out to a node inside and one outside; G2.0 sums two inside
    // sources; G2.0 fans out to two outside pins; out3 sums an inside and an
    // outside source.
    void buildMixed (Fixture& f)
    {
        f.nodes["G1"] = addGain  (f.graph, 2, -6.0f, { 300, 100 });
        f.nodes["D1"] = addDelay (f.graph, 2,  7.0f, { 300, 300 });
        f.nodes["G2"] = addGain  (f.graph, 2,  3.0f, { 550, 150 });
        f.nodes["D2"] = addDelay (f.graph, 1,  3.0f, { 300, 500 });

        f.wire (f.in(), 0, f["G1"], 0);
        f.wire (f.in(), 1, f["G1"], 1);
        f.wire (f.in(), 0, f["D2"], 0);
        f.wire (f.in(), 2, f["D1"], 0);
        f.wire (f.in(), 3, f["D1"], 1);
        f.wire (f["G1"], 0, f["G2"], 0);
        f.wire (f["G1"], 1, f["G2"], 1);
        f.wire (f["D1"], 0, f["G2"], 0);
        f.wire (f["G2"], 0, f.out(), 0);
        f.wire (f["G2"], 1, f.out(), 1);
        f.wire (f["G2"], 0, f.out(), 2);
        f.wire (f["D2"], 0, f.out(), 3);
        f.wire (f["D1"], 1, f.out(), 3);
    }

    void scenarioMixed()
    {
        SubgraphConversion::Result r;
        auto f = expectSameAudio (buildMixed, { "G1", "D1", "G2" }, r);
        if (! r.succeeded()) return;

        // Distinct sources: in0..in3 feed the set; G2.0, G2.1, D1.1 leave it.
        auto* sub = subgraphOf (*f, r.subgraphUuid);
        check (sub != nullptr, "subgraph node exists");
        if (sub == nullptr) return;
        check (sub->getNumIn()  == 4, "4 subgraph inputs, got "  + juce::String (sub->getNumIn()));
        check (sub->getNumOut() == 3, "3 subgraph outputs, got " + juce::String (sub->getNumOut()));

        // Moved nodes are gone from the parent and live inside, same UUIDs.
        auto& inner = sub->getInner();
        for (const char* n : { "G1", "D1", "G2" })
        {
            check (f->graph.getNode ((*f)[n]) == nullptr, juce::String (n) + " left the parent");
            check (inner.getNode ((*f)[n])    != nullptr, juce::String (n) + " is inside, same UUID");
        }
        check (f->graph.getNode ((*f)["D2"]) != nullptr, "unselected D2 stays in the parent");
        check (f->graph.getAllUserNodes().size() == 2, "parent holds D2 + the subgraph");

        // One output wire per distinct inside source, even though G2.0 feeds
        // two outside pins.
        int toInnerOut = 0;
        for (const auto& c : inner.getAllConnections())
            if (c.toUuid == inner.getOutputTerminalUuid()) ++toInnerOut;
        check (toInnerOut == 3, "3 wires into the inner output, got " + juce::String (toInnerOut));
    }

    void scenarioIsolatedNode()
    {
        // A node with no wires at all: 1-in/1-out subgraph (the minimum), and
        // the rest of the graph untouched.
        auto build = [] (Fixture& f)
        {
            f.nodes["G"] = addGain (f.graph, 2, 0.0f, { 300, 100 });
            f.nodes["L"] = addGain (f.graph, 2, -12.0f, { 300, 300 });
            for (int c = 0; c < kChannels; ++c)
                f.wire (f.in(), c, f.out(), c);
        };
        SubgraphConversion::Result r;
        auto f = expectSameAudio (build, { "L" }, r);
        if (auto* sub = subgraphOf (*f, r.subgraphUuid))
        {
            check (sub->getNumIn() == 1 && sub->getNumOut() == 1, "unwired node gives a 1/1 subgraph");
            check (sub->getInner().getAllConnections().empty(), "no inner wires");
        }
    }

    void scenarioFeedbackPairMovesWhole()
    {
        // in -> send ... return -> out, plus the dry path. The pair moves
        // together and the loop keeps its one block of delay.
        auto build = [] (Fixture& f)
        {
            f.nodes["S"] = f.graph.addNode (std::make_unique<FeedbackSendNode> (2),
                                            NodeKind::FeedbackSend, "Send", 2, 0, { 300, 100 });
            f.nodes["R"] = f.graph.addNode (std::make_unique<FeedbackReturnNode> (2),
                                            NodeKind::FeedbackReturn, "Return", 0, 2, { 300, 300 });
            check (f.graph.addFeedbackLink (f["S"], f["R"]), "fixture link");
            f.wire (f.in(), 0, f["S"], 0);
            f.wire (f.in(), 1, f["S"], 1);
            f.wire (f["R"], 0, f.out(), 0);
            f.wire (f["R"], 1, f.out(), 1);
            f.wire (f.in(), 2, f.out(), 2);
            f.wire (f.in(), 3, f.out(), 3);
        };
        SubgraphConversion::Result r;
        auto f = expectSameAudio (build, { "S", "R" }, r);
        if (auto* sub = subgraphOf (*f, r.subgraphUuid))
        {
            check (sub->getInner().getAllFeedbackLinks().size() == 1, "link moved inside");
            check (f->graph.getAllFeedbackLinks().empty(), "no link left in the parent");
        }
    }

    void scenarioRefusals()
    {
        // Splitting a feedback pair is refused and changes nothing.
        Fixture f;
        f.nodes["S"] = f.graph.addNode (std::make_unique<FeedbackSendNode> (2),
                                        NodeKind::FeedbackSend, "Send", 2, 0, { 300, 100 });
        f.nodes["R"] = f.graph.addNode (std::make_unique<FeedbackReturnNode> (2),
                                        NodeKind::FeedbackReturn, "Return", 0, 2, { 300, 300 });
        f.graph.addFeedbackLink (f["S"], f["R"]);
        f.wire (f.in(), 0, f["S"], 0);

        const auto before = f.graph.getAllConnections().size();
        auto r = convert (f, { f["S"] });
        check (! r.succeeded() && r.error.isNotEmpty(), "split feedback pair is refused");
        check (f.graph.getAllUserNodes().size() == 2, "refusal leaves the nodes");
        check (f.graph.getAllConnections().size() == before, "refusal leaves the wires");
        check (f.graph.getAllFeedbackLinks().size() == 1, "refusal leaves the link");

        // Terminals alone, or nothing, are refused.
        check (! convert (f, { f.in(), f.out() }).succeeded(), "terminals-only selection is refused");
        check (! convert (f, {}).succeeded(), "empty selection is refused");
    }

    void scenarioNested()
    {
        // Convert, then convert again inside the new subgraph: still identical.
        Fixture reference;
        buildMixed (reference);

        Fixture f;
        buildMixed (f);
        auto r1 = convert (f, { f["G1"], f["D1"], f["G2"] });
        check (r1.succeeded(), "outer conversion: " + r1.error);
        auto* sub = subgraphOf (f, r1.subgraphUuid);
        if (sub == nullptr) return;

        // Rendering prepares the inner graph at the real rate first.
        f.graph.prepareToPlay (kSampleRate, kBlockSize, kChannels, kChannels);
        auto r2 = SubgraphConversion::convert (sub->getInner(), { f["G1"], f["G2"] }, formats(), nullptr);
        check (r2.succeeded(), "inner conversion: " + r2.error);

        check (maxDifference (render (reference.graph), render (f.graph)) == 0.0f,
               "doubly nested graph sounds identical");
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;

    const std::vector<std::pair<juce::String, std::function<void()>>> scenarios {
        { "mixed",     scenarioMixed },
        { "isolated",  scenarioIsolatedNode },
        { "feedback",  scenarioFeedbackPairMovesWhole },
        { "refusals",  scenarioRefusals },
        { "nested",    scenarioNested },
    };

    juce::String only;
    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--scenario")
            only = argv[i + 1];

    int ran = 0;
    for (const auto& [name, fn] : scenarios)
    {
        if (only.isNotEmpty() && name != only) continue;
        std::cout << name << std::endl;
        const int before = failures;
        fn();
        std::cout << "  " << (failures == before ? "ok" : "FAILED") << std::endl;
        ++ran;
    }

    if (ran == 0)
    {
        std::cout << "unknown scenario: " << only << std::endl;
        return 2;
    }
    return failures == 0 ? 0 : 1;
}
