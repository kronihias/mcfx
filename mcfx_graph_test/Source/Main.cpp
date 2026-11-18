/*
  mcfx_graph_test — checks mcfx_graph's graph logic without the plug-in
  wrapper or the editor. Driven by tests/test_graph_subgraph.py; exits
  non-zero and prints what failed.

    mcfx_graph_test                   run every scenario
    mcfx_graph_test --scenario NAME   run one

  Scenarios cover "Convert to subgraph" (SubgraphConversion): the converted
  graph has to sound exactly like the original, with fan-out and summing on
  both sides of the boundary, feedback pairs moving whole, and refusals that
  change nothing. And Option/Alt-drag insertion (NodeInsertion): dropping a node
  on a wire has to sound like wiring it in by hand.
*/

#include <JuceHeader.h>
#include "Graph/GraphController.h"
#include "Graph/GraphSerializer.h"
#include "Graph/NodeInsertion.h"
#include "Graph/SubgraphConversion.h"
#include "Graph/SubgraphNode.h"
#include "NativeNodes/DelayNode.h"
#include "NativeNodes/FeedbackNodes.h"
#include "NativeNodes/GainNode.h"
#include "NativeNodes/MatrixMixerNode.h"
#include "Hosting/PluginInstanceFactory.h"

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

    int wiresTouching (const GraphController& g, const juce::Uuid& terminal)
    {
        int n = 0;
        for (const auto& c : g.getAllConnections())
            if (c.fromUuid == terminal || c.toUuid == terminal) ++n;
        return n;
    }

    void scenarioResize()
    {
        // Changing a subgraph's channel count keeps what's inside it.
        Fixture reference;
        buildMixed (reference);

        Fixture f;
        buildMixed (f);
        auto r = convert (f, { f["G1"], f["D1"], f["G2"] });
        check (r.succeeded(), "conversion: " + r.error);
        auto* sub = subgraphOf (f, r.subgraphUuid);
        if (sub == nullptr) return;

        const auto innerWires = sub->getInner().getAllConnections().size();

        // Grow by one input: nothing is lost and it sounds the same.
        f.graph.replaceNodeProcessor (r.subgraphUuid, sub->withChannelCounts (5, 3, formats(), nullptr),
                                      NodeKind::Subgraph, "Subgraph", 5, 3);
        sub = subgraphOf (f, r.subgraphUuid);
        check (sub != nullptr && sub->getNumIn() == 5, "grown to 5 inputs");
        if (sub == nullptr) return;
        check (sub->getInner().getAllUserNodes().size() == 3, "grow keeps the inner nodes");
        check (sub->getInner().getAllConnections().size() == innerWires, "grow keeps the inner wires");
        f.graph.prepareToPlay (kSampleRate, kBlockSize, kChannels, kChannels);
        check (maxDifference (render (reference.graph), render (f.graph)) == 0.0f,
               "grown subgraph sounds identical");

        // Shrink to 2 inputs: the nodes stay, only wires from the dropped
        // inputs 2 and 3 go (in2 -> D1.0, in3 -> D1.1).
        const int inWiresBefore = wiresTouching (sub->getInner(), sub->getInner().getInputTerminalUuid());
        f.graph.replaceNodeProcessor (r.subgraphUuid, sub->withChannelCounts (2, 3, formats(), nullptr),
                                      NodeKind::Subgraph, "Subgraph", 2, 3);
        sub = subgraphOf (f, r.subgraphUuid);
        if (sub == nullptr) { check (false, "shrunk subgraph exists"); return; }
        check (sub->getInner().getAllUserNodes().size() == 3, "shrink keeps the inner nodes");
        const int inWiresAfter = wiresTouching (sub->getInner(), sub->getInner().getInputTerminalUuid());
        check (inWiresAfter == inWiresBefore - 2,
               "shrink drops just the 2 wires from removed inputs ("
               + juce::String (inWiresBefore) + " -> " + juce::String (inWiresAfter) + ")");
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

namespace
{
    // G1 -> G2 over two wires, both driven from in0/in1 and ending at out0/1.
    void buildChain (Fixture& f, bool delayWiredIn)
    {
        f.nodes["G1"] = addGain  (f.graph, 2, -6.0f, { 200, 100 });
        f.nodes["G2"] = addGain  (f.graph, 2,  3.0f, { 600, 100 });
        f.nodes["D"]  = addDelay (f.graph, 2,  5.0f, { 400, 300 });
        f.wire (f.in(), 0, f["G1"], 0);
        f.wire (f.in(), 1, f["G1"], 1);
        f.wire (f["G2"], 0, f.out(), 0);
        f.wire (f["G2"], 1, f.out(), 1);
        if (delayWiredIn)
        {
            f.wire (f["G1"], 0, f["D"], 0);
            f.wire (f["G1"], 1, f["D"], 1);
            f.wire (f["D"],  0, f["G2"], 0);
            f.wire (f["D"],  1, f["G2"], 1);
        }
        else
        {
            f.wire (f["G1"], 0, f["G2"], 0);
            f.wire (f["G1"], 1, f["G2"], 1);
        }
    }

    void scenarioInsert()
    {
        Fixture reference;
        buildChain (reference, true);

        Fixture f;
        buildChain (f, false);
        check (NodeInsertion::describeRefusal (f.graph, f["D"], f["G1"], f["G2"]).isEmpty(),
               "unwired node can go into the wire");
        check (NodeInsertion::insert (f.graph, f["D"], f["G1"], f["G2"]) == 2,
               "both parallel wires go through the node");
        check (! f.graph.isConnected (f["G1"], 0, f["G2"], 0)
               && ! f.graph.isConnected (f["G1"], 1, f["G2"], 1), "direct wires are gone");
        check (maxDifference (render (reference.graph), render (f.graph)) == 0.0f,
               "inserted node sounds like wiring it by hand");
    }

    void scenarioInsertPartial()
    {
        // A 1-channel node on a 2-wire link takes the first wire only (by
        // destination channel); the other stays direct.
        Fixture f;
        buildChain (f, false);
        const auto mono = addDelay (f.graph, 1, 5.0f, { 400, 500 });
        check (NodeInsertion::insert (f.graph, mono, f["G1"], f["G2"]) == 1, "one wire rerouted");
        check (f.graph.isConnected (f["G1"], 0, mono, 0) && f.graph.isConnected (mono, 0, f["G2"], 0),
               "wire 0 runs through the mono node");
        check (f.graph.isConnected (f["G1"], 1, f["G2"], 1), "wire 1 stays direct");
    }

    void scenarioInsertRefusals()
    {
        Fixture f;
        buildChain (f, false);
        const auto before = f.graph.getAllConnections().size();

        auto refused = [&] (const juce::Uuid& node, const juce::Uuid& from, const juce::Uuid& to,
                            const juce::String& what)
        {
            check (NodeInsertion::describeRefusal (f.graph, node, from, to).isNotEmpty(), what + " is refused");
            check (NodeInsertion::insert (f.graph, node, from, to) == 0, what + " changes nothing");
        };

        refused (f["G1"], f.in(), f["G2"], "an already-wired node");
        refused (f["D"],  f["G2"], f["G1"], "a node pair with no wire");
        refused (f.in(),  f["G1"], f["G2"], "a terminal");
        refused (f["G1"], f["G1"], f["G2"], "a node into its own wire");

        const auto send = f.graph.addNode (std::make_unique<FeedbackSendNode> (2),
                                           NodeKind::FeedbackSend, "Send", 2, 0, { 400, 600 });
        refused (send, f["G1"], f["G2"], "a node without outputs");

        check (f.graph.getAllConnections().size() == before, "refusals leave every wire");
    }
}

namespace
{
    // The top-level "node about to be removed" listener is what closes a
    // node's open window; it has to hear about every level of nesting.
    void scenarioRemovalNotices()
    {
        Fixture f;
        std::vector<juce::Uuid> heard;
        f.graph.setNodeAboutToBeRemovedListener ([&heard] (juce::Uuid u) { heard.push_back (u); });
        auto heardOf = [&heard] (const juce::Uuid& u)
        {
            return std::find (heard.begin(), heard.end(), u) != heard.end();
        };

        buildMixed (f);
        auto r = convert (f, { f["G1"], f["D1"], f["G2"] });
        check (r.succeeded(), "conversion: " + r.error);
        check (heardOf (f["G1"]) && heardOf (f["D1"]) && heardOf (f["G2"]),
               "moving nodes into a subgraph reports the originals");
        auto* sub = subgraphOf (f, r.subgraphUuid);
        if (sub == nullptr) return;

        // A second level, built by converting inside the first.
        f.graph.prepareToPlay (kSampleRate, kBlockSize, kChannels, kChannels);
        auto r2 = SubgraphConversion::convert (sub->getInner(), { f["G1"], f["G2"] }, formats(), nullptr);
        check (r2.succeeded(), "inner conversion: " + r2.error);
        auto* innerSub = sub != nullptr ? dynamic_cast<SubgraphNode*> (sub->getInner().getNode (r2.subgraphUuid)->processor)
                                        : nullptr;
        if (innerSub == nullptr) { check (false, "inner subgraph exists"); return; }

        // Removing a node two levels down reaches the top.
        heard.clear();
        innerSub->getInner().removeNode (f["G2"]);
        check (heard.size() == 1 && heardOf (f["G2"]), "removal two levels down is reported at the top");

        // Removing the outer subgraph reports everything inside it, innermost
        // first, then the subgraph itself.
        heard.clear();
        f.graph.removeNode (r.subgraphUuid);
        check (heardOf (f["G1"]) && heardOf (f["D1"]) && heardOf (r2.subgraphUuid),
               "removing a subgraph reports every nested node");
        check (! heard.empty() && heard.back() == r.subgraphUuid, "the subgraph itself is reported last");
        const auto posG1  = std::find (heard.begin(), heard.end(), f["G1"]);
        const auto posSub = std::find (heard.begin(), heard.end(), r2.subgraphUuid);
        check (posG1 < posSub, "nested nodes before the subgraph that holds them");
    }
}

namespace
{
    /** Stands in for a plug-in with latency (e.g. a lookahead limiter): a
        pure delay that reports its delay as latency, so the graph's delay
        compensation should line a parallel dry path up with it. */
    class LatencyNode : public juce::AudioProcessor
    {
    public:
        LatencyNode (int numChannels, int delaySamples)
            : juce::AudioProcessor (BusesProperties()
                  .withInput  ("In",  juce::AudioChannelSet::discreteChannels (numChannels), true)
                  .withOutput ("Out", juce::AudioChannelSet::discreteChannels (numChannels), true))
        {
            setDelay (delaySamples);
        }

        /** Change the delay (and reported latency), e.g. while running. */
        void setDelay (int samples)
        {
            delay_.store (samples);
            setLatencySamples (samples);
        }

        void prepareToPlay (double, int) override
        {
            ring_.setSize (getTotalNumInputChannels(), kRingSize);
            ring_.clear();
            writePos_ = 0;
        }
        void releaseResources() override {}

        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
        {
            const int d = delay_.load();
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const int readPos = (writePos_ - d + kRingSize) % kRingSize;
                for (int c = 0; c < ring_.getNumChannels(); ++c)
                {
                    ring_.setSample (c, writePos_, buffer.getSample (c, i));
                    buffer.setSample (c, i, ring_.getSample (c, readPos));
                }
                writePos_ = (writePos_ + 1) % kRingSize;
            }
        }

        const juce::String getName() const override { return "Latency"; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        bool hasEditor() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

    private:
        static constexpr int kRingSize = 8192;
        juce::AudioBuffer<float> ring_;
        int writePos_ = 0;
        std::atomic<int> delay_ { 0 };
    };

    void pumpMessages()
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    }

    /** out0 (through the latent path) against out1 (dry), from sample `from`. */
    float pathDifference (const juce::AudioBuffer<float>& out, int from = 0)
    {
        float d = 0.0f;
        for (int i = from; i < out.getNumSamples(); ++i)
            d = juce::jmax (d, std::abs (out.getSample (0, i) - out.getSample (1, i)));
        return d;
    }

    /** in0 -> [latent path] -> out0, and in0 -> out1 dry. With the latent
        path inside a subgraph if `nested`. Returns the LatencyNode. */
    LatencyNode* buildLatentAndDry (Fixture& f, int latency, bool nested)
    {
        auto latent = std::make_unique<LatencyNode> (1, latency);
        auto* raw = latent.get();

        if (nested)
        {
            auto sub = std::make_unique<SubgraphNode> (1, 1);
            auto& inner = sub->getInner();
            inner.prepareToPlay (kSampleRate, kBlockSize, 1, 1);
            const auto l = inner.addNode (std::move (latent), NodeKind::Plugin, "Latency", 1, 1, { 300, 100 });
            inner.addConnection (inner.getInputTerminalUuid(), 0, l, 0);
            inner.addConnection (l, 0, inner.getOutputTerminalUuid(), 0);
            f.nodes["P"] = f.graph.addNode (std::move (sub), NodeKind::Subgraph, "Subgraph", 1, 1, { 300, 100 });
        }
        else
        {
            f.nodes["P"] = f.graph.addNode (std::move (latent), NodeKind::Plugin, "Latency", 1, 1, { 300, 100 });
        }

        f.wire (f.in(), 0, f["P"], 0);
        f.wire (f["P"], 0, f.out(), 0);
        f.wire (f.in(), 0, f.out(), 1);
        return raw;
    }

    void scenarioLatencyParallel()
    {
        Fixture f;
        buildLatentAndDry (f, 64, false);
        check (f.graph.getLatencySamples() == 64,
               "graph latency is 64, got " + juce::String (f.graph.getLatencySamples()));
        const auto out = render (f.graph);
        check (peak (out) > 0.01f, "latent path produces audio");
        check (pathDifference (out) == 0.0f, "dry path is delayed to match the latent one");
    }

    void scenarioLatencySubgraph()
    {
        // The same, with the latent node inside a subgraph: the subgraph has
        // to report its inner latency for the parent to compensate.
        Fixture f;
        buildLatentAndDry (f, 64, true);
        check (f.graph.getLatencySamples() == 64,
               "graph latency is 64 through a subgraph, got " + juce::String (f.graph.getLatencySamples()));
        check (pathDifference (render (f.graph)) == 0.0f,
               "dry path matches a latent path inside a subgraph");
    }

    void scenarioLatencyRuntime()
    {
        // A node inside a subgraph changes its latency while running: the
        // compensation re-plans and the new total reaches the top-level
        // listener (which tells the host).
        Fixture f;
        int reported = -1;
        f.graph.setLatencyListener ([&] { reported = f.graph.getLatencySamples(); });

        auto* latent = buildLatentAndDry (f, 64, true);
        render (f.graph);

        latent->setDelay (200);
        pumpMessages();   // the re-plan is asynchronous

        check (f.graph.getLatencySamples() == 200,
               "graph latency follows to 200, got " + juce::String (f.graph.getLatencySamples()));
        check (reported == 200, "the top-level listener heard 200, got " + juce::String (reported));

        // Past the switch-over transient, the paths line up again.
        check (pathDifference (render (f.graph), 1024) == 0.0f,
               "dry path matches after the latency change");
    }
}

namespace
{
    void scenarioLatencyIgnored()
    {
        // "Ignore latency" on the latent node: no compensation for it, so the
        // dry path stays undelayed and the total drops to 0. Undo it and the
        // compensation is back.
        Fixture f;
        buildLatentAndDry (f, 64, false);
        render (f.graph);

        f.graph.setNodeIgnoreLatency (f["P"], true);
        pumpMessages();
        check (f.graph.getLatencySamples() == 0,
               "ignored latency isn't counted, got " + juce::String (f.graph.getLatencySamples()));
        check (f.graph.getNodeLatency (f["P"]) == 64, "the node still knows its own latency");

        // Dry path undelayed: out1 is the input itself. Render the same noise
        // straight through for comparison.
        const auto out = render (f.graph);
        Fixture straight;
        for (int c = 0; c < kChannels; ++c) straight.wire (straight.in(), c, straight.out(), c);
        const auto input = render (straight.graph);
        float dryDiff = 0.0f;
        for (int i = 0; i < out.getNumSamples(); ++i)
            dryDiff = juce::jmax (dryDiff, std::abs (out.getSample (1, i) - input.getSample (0, i)));
        check (dryDiff == 0.0f, "dry path is no longer delayed");

        f.graph.setNodeIgnoreLatency (f["P"], false);
        pumpMessages();
        check (f.graph.getLatencySamples() == 64, "switching it back restores the total");
        check (pathDifference (render (f.graph), 1024) == 0.0f, "and the compensation");

        // The flag is saved and restored with the graph.
        Fixture g;
        g.nodes["G"] = addGain (g.graph, 2, 0.0f, { 300, 100 });
        g.graph.setNodeIgnoreLatency (g["G"], true);
        const auto saved = GraphSerializer::graphToVar (g.graph);
        Fixture h;
        GraphSerializer::graphFromVar (saved, h.graph, formats(), nullptr);
        auto* restored = h.graph.getNode (g["G"]);
        check (restored != nullptr && restored->ignoreLatency, "ignoreLatency survives save / load");
    }
}

//==============================================================================
// Hosted plug-ins with more than one bus per direction.
namespace
{
    /** Stands in for a hosted plug-in with `numBuses` stereo buses in and
        out. Halves every channel. Like a real plug-in it trusts the host to
        hand it a buffer covering all of its enabled buses; here it only
        notes when the host didn't, where the real one runs off the end. */
    struct MultiBusPlugin : juce::AudioPluginInstance
    {
        MultiBusPlugin (int numInBuses, int numOutBuses, bool auxEnabled, bool lockedLayout)
            : juce::AudioPluginInstance (makeBuses (numInBuses, numOutBuses, auxEnabled)),
              locked (lockedLayout) {}

        static BusesProperties makeBuses (int numIn, int numOut, bool auxEnabled)
        {
            BusesProperties p;
            for (int i = 0; i < numIn; ++i)
                p.addBus (true,  "In "  + juce::String (i + 1), juce::AudioChannelSet::stereo(), i == 0 || auxEnabled);
            for (int i = 0; i < numOut; ++i)
                p.addBus (false, "Out " + juce::String (i + 1), juce::AudioChannelSet::stereo(), i == 0 || auxEnabled);
            return p;
        }

        bool isBusesLayoutSupported (const BusesLayout& l) const override
        {
            if (locked) return false;      // SynthEdit-style: buses can't be switched
            for (const auto& set : l.inputBuses)
                if (! set.isDisabled() && set != juce::AudioChannelSet::stereo()) return false;
            for (const auto& set : l.outputBuses)
                if (! set.isDisabled() && set != juce::AudioChannelSet::stereo()) return false;
            return true;
        }

        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
        {
            const int needed = juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());
            if (buffer.getNumChannels() < needed)
            {
                bufferTooSmall = true;
                return;
            }
            buffer.applyGain (0.5f);
        }

        const juce::String getName() const override              { return "MultiBus"; }
        void prepareToPlay (double, int) override                {}
        void releaseResources() override                         {}
        bool acceptsMidi() const override                        { return false; }
        bool producesMidi() const override                       { return false; }
        double getTailLengthSeconds() const override             { return 0.0; }
        bool hasEditor() const override                          { return false; }
        juce::AudioProcessorEditor* createEditor() override      { return nullptr; }
        int getNumPrograms() override                            { return 1; }
        int getCurrentProgram() override                         { return 0; }
        void setCurrentProgram (int) override                    {}
        const juce::String getProgramName (int) override         { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override   {}
        void setStateInformation (const void*, int) override     {}
        void fillInPluginDescription (juce::PluginDescription& d) const override { d.name = getName(); }

        bool locked;
        bool bufferTooSmall = false;
    };

    /** Add the plug-in the way the editor does: set up its buses, then size
        the node from what is enabled. */
    MultiBusPlugin* addMultiBus (Fixture& f, const juce::String& name,
                                 int numInBuses, int numOutBuses, bool auxEnabled, bool locked)
    {
        auto p = std::make_unique<MultiBusPlugin> (numInBuses, numOutBuses, auxEnabled, locked);
        auto* raw = p.get();
        PluginInstanceFactory::configureBuses (*p);
        const int chIn  = p->getTotalNumInputChannels();
        const int chOut = p->getTotalNumOutputChannels();
        f.nodes[name] = f.graph.addNode (std::move (p), NodeKind::Plugin, name, chIn, chOut, { 300, 100 });
        return raw;
    }

    /** in0 through the plug-in's pin `ch` to out0; true if it came out halved. */
    bool passesHalved (Fixture& f, const juce::String& name, int ch)
    {
        f.wire (f.in(), 0, f[name], ch);
        f.wire (f[name], ch, f.out(), 0);
        const auto out = render (f.graph);

        Fixture straight;
        straight.wire (straight.in(), 0, straight.out(), 0);
        const auto in = render (straight.graph);

        float diff = 0.0f;
        for (int i = 0; i < out.getNumSamples(); ++i)
            diff = juce::jmax (diff, std::abs (out.getSample (0, i) - 0.5f * in.getSample (0, i)));
        return peak (out) > 0.01f && diff == 0.0f;
    }

    void scenarioMultiBus()
    {
        {   // 32 stereo buses the plug-in won't let us switch off: the crash.
            Fixture f;
            auto* p = addMultiBus (f, "P", 32, 32, true, true);
            auto* gn = f.graph.getNode (f["P"]);
            check (gn != nullptr && gn->channelCountIn == 64 && gn->channelCountOut == 64,
                   "32 locked stereo buses give a 64-channel node");
            check (passesHalved (f, "P", 63), "audio passes on the last channel of the last bus");
            check (! p->bufferTooSmall, "locked multi-bus plug-in gets a full-size buffer");
        }
        {   // Same plug-in with only the main bus on by default: switch all on.
            Fixture f;
            auto* p = addMultiBus (f, "P", 32, 32, false, false);
            check (p->getTotalNumInputChannels() == 64 && p->getTotalNumOutputChannels() == 64,
                   "more than two buses are all switched on");
            check (passesHalved (f, "P", 63), "audio passes on the last bus once enabled");
            check (! p->bufferTooSmall, "enabled multi-bus plug-in gets a full-size buffer");
        }
        {   // Main + sidechain: the sidechain goes, as before.
            Fixture f;
            auto* p = addMultiBus (f, "P", 2, 1, true, false);
            check (p->getTotalNumInputChannels() == 2, "a single aux input is switched off");
            check (passesHalved (f, "P", 1), "audio passes through the main bus");
            check (! p->bufferTooSmall, "sidechain plug-in gets a full-size buffer");
        }
        {   // A sidechain the plug-in insists on keeping: pins for it, no overrun.
            Fixture f;
            auto* p = addMultiBus (f, "P", 2, 1, true, true);
            auto* gn = f.graph.getNode (f["P"]);
            check (gn != nullptr && gn->channelCountIn == 4 && gn->channelCountOut == 2,
                   "a sidechain that can't be switched off shows up as pins");
            check (passesHalved (f, "P", 0), "audio passes with the sidechain kept");
            check (! p->bufferTooSmall, "kept sidechain is covered by the buffer");
        }
        {   // The plug-in grows after the node was built (a state restore can
            // switch buses back on): the node keeps its width, nothing overruns.
            Fixture f;
            auto* p = addMultiBus (f, "P", 2, 2, true, false);
            check (p->getTotalNumInputChannels() == 2, "starts on the main bus");
            p->enableAllBuses();
            check (p->getTotalNumInputChannels() == 4, "then switches its aux bus back on");
            render (f.graph);
            check (! p->bufferTooSmall, "a plug-in that grew mid-run is not handed too small a buffer");

            f.graph.releaseResources();
            f.graph.prepareToPlay (kSampleRate, kBlockSize, kChannels, kChannels);
            check (passesHalved (f, "P", 1), "after the next prepare, audio passes through the main bus again");
            check (! p->bufferTooSmall, "a plug-in that grew is run in a buffer of its own size");
        }
    }
}

namespace
{
    // Through JSON text, like presets and the host state.
    juce::var viaJson (const juce::var& v)
    {
        return juce::JSON::parse (juce::JSON::toString (v, true));
    }

    bool sameMatrix (const MatrixMixerNode& a, const MatrixMixerNode& b)
    {
        if (a.getNumIn() != b.getNumIn() || a.getNumOut() != b.getNumOut()) return false;
        for (int o = 0; o < a.getNumOut(); ++o)
            for (int i = 0; i < a.getNumIn(); ++i)
                if (a.getMatrix (o, i) != b.getMatrix (o, i)) return false;
        return true;
    }

    void scenarioMatrixState()
    {
        // Sparse: a 256 x 256 routing matrix is stored as its non-zero cells.
        {
            MatrixMixerNode src (256, 256), dst (256, 256);
            src.setMatrix (3, 7, 0.5f);
            src.setMatrix (0, 0, 0.0f);
            src.setMatrix (255, 0, -0.25f);
            const auto v = src.toVar();
            check (v["gains"].isArray() && ! v.hasProperty ("matrixF32"), "sparse matrix is stored as gains");
            const auto json = juce::JSON::toString (v, true);
            check (json.length() < 16000, "sparse 256x256 state is small (" + juce::String (json.length()) + " chars)");
            dst.setMatrix (10, 20, 1.0f);   // stale value that must be cleared
            dst.fromVar (juce::JSON::parse (json));
            check (sameMatrix (src, dst), "sparse 256x256 round-trips");
        }

        // Dense: every cell set, stored as one base64 float32 block, bit exact.
        {
            MatrixMixerNode src (256, 256), dst (256, 256);
            juce::Random rng (42);
            for (int o = 0; o < 256; ++o)
                for (int i = 0; i < 256; ++i)
                    src.setMatrix (o, i, rng.nextFloat() * 2.0f - 1.0f + 1.0e-3f);
            const auto v = src.toVar();
            check (v["matrixF32"].isString() && ! v.hasProperty ("gains"), "dense matrix is stored as matrixF32");
            dst.fromVar (viaJson (v));
            check (sameMatrix (src, dst), "dense 256x256 round-trips bit exact");
        }

        // All zero: loading it clears the destination's identity diagonal.
        {
            MatrixMixerNode src (4, 4), dst (4, 4);
            for (int c = 0; c < 4; ++c) src.setMatrix (c, c, 0.0f);
            dst.fromVar (viaJson (src.toVar()));
            check (sameMatrix (src, dst), "all-zero matrix round-trips");
        }

        // Legacy nested "matrix" rows (out-major) still load.
        {
            juce::Array<juce::var> rows;
            rows.add (juce::Array<juce::var> { 0.0, 0.5, 0.0 });   // out 0
            rows.add (juce::Array<juce::var> { 1.0, 0.0, 0.25 });  // out 1
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("matrix", rows);
            MatrixMixerNode dst (3, 2);
            dst.fromVar (viaJson (juce::var (obj)));
            check (dst.getMatrix (0, 0) == 0.0f && dst.getMatrix (0, 1) == 0.5f
                   && dst.getMatrix (1, 0) == 1.0f && dst.getMatrix (1, 2) == 0.25f,
                   "legacy nested matrix loads");
        }

        // Channel-count change (node rebuilt with fromVar (old->toVar())):
        // the overlap is taken over, the rest keeps the identity diagonal.
        {
            MatrixMixerNode src (4, 4);                  // sparse
            src.setMatrix (0, 0, 0.0f);
            src.setMatrix (1, 2, 0.75f);
            MatrixMixerNode grown (6, 6);
            grown.fromVar (viaJson (src.toVar()));
            check (grown.getMatrix (1, 2) == 0.75f && grown.getMatrix (0, 0) == 0.0f
                   && grown.getMatrix (1, 1) == 1.0f && grown.getMatrix (5, 5) == 1.0f,
                   "sparse resize keeps the overlap and the new diagonal");
        }
        {
            MatrixMixerNode src (80, 80);                // 6400 cells: dense
            for (int o = 0; o < 80; ++o)
                for (int i = 0; i < 80; ++i)
                    src.setMatrix (o, i, 0.01f * (float) ((o + i) % 7 + 1));
            check (src.toVar().hasProperty ("matrixF32"), "80x80 full matrix uses the dense form");

            MatrixMixerNode grown (100, 90);             // numIn 100, numOut 90
            grown.fromVar (viaJson (src.toVar()));
            bool ok = grown.getMatrix (85, 85) == 1.0f && grown.getMatrix (85, 10) == 0.0f;
            for (int o = 0; o < 80 && ok; ++o)
                for (int i = 0; i < 80 && ok; ++i)
                    ok = grown.getMatrix (o, i) == src.getMatrix (o, i);
            check (ok, "dense resize keeps the overlap and the new diagonal");

            MatrixMixerNode shrunk (50, 40);
            shrunk.fromVar (viaJson (src.toVar()));
            ok = true;
            for (int o = 0; o < 40 && ok; ++o)
                for (int i = 0; i < 50 && ok; ++i)
                    ok = shrunk.getMatrix (o, i) == src.getMatrix (o, i);
            check (ok, "dense shrink keeps the overlap");
        }
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
        { "resize",    scenarioResize },
        { "insert",           scenarioInsert },
        { "insert-partial",   scenarioInsertPartial },
        { "insert-refusals",  scenarioInsertRefusals },
        { "removal-notices",  scenarioRemovalNotices },
        { "latency-parallel", scenarioLatencyParallel },
        { "latency-subgraph", scenarioLatencySubgraph },
        { "latency-runtime",  scenarioLatencyRuntime },
        { "latency-ignored",  scenarioLatencyIgnored },
        { "multibus",         scenarioMultiBus },
        { "matrix-state",     scenarioMatrixState },
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
