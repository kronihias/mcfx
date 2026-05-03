#include "GraphEditorComponent.h"
#include "../PluginProcessor.h"
#include "../NativeNodes/GainNode.h"
#include "../NativeNodes/MutePhaseNode.h"
#include "../NativeNodes/MatrixMixerNode.h"
#include "../NativeNodes/DelayNode.h"
#include "../Graph/SubgraphNode.h"
#include "../Hosting/PluginInstanceFactory.h"
#include "PluginSearchPopup.h"

GraphEditorComponent::GraphEditorComponent (Mcfx_graphAudioProcessor& processor,
                                            GraphController& controller)
    : processor_ (processor), rootController_ (controller)
{
    activeController_ = &rootController_;

    // Receive clicks even where no child component is — covers right-clicks on
    // bezier wires and on the empty canvas.
    setInterceptsMouseClicks (true, true);
    setWantsKeyboardFocus (true);

    // Fixed world size; the surrounding Viewport handles pan via scrollbars.
    setSize (kWorldW, kWorldH);

    hookTopologyListener();
    rebuildAll();
}

GraphEditorComponent::~GraphEditorComponent()
{
    if (activeController_ != nullptr)
        activeController_->setTopologyListener (nullptr);
}

void GraphEditorComponent::hookTopologyListener()
{
    if (activeController_ == nullptr) return;

    // Defer rebuild via the message queue so we don't destroy components from
    // within their own mouse-event call stack (e.g. pin-drag adds connection).
    // The same callback also captures an undo/redo snapshot.
    juce::Component::SafePointer<GraphEditorComponent> safe (this);
    activeController_->setTopologyListener ([safe]
    {
        juce::MessageManager::callAsync ([safe]
        {
            if (auto* p = safe.getComponent())
            {
                p->processor_.commitHistorySnapshot();
                p->rebuildAll();
            }
        });
    });
}

//==============================================================================

void GraphEditorComponent::rebuildAll()
{
    nodeComps_.clear();

    for (auto* gn : activeController_->getAllNodesIncludingTerminals())
    {
        auto* nc = nodeComps_.add (new NodeComponent (*this, *gn));
        addAndMakeVisible (nc);
    }

    layoutNodeComponents();
    repaint();
}

NodeComponent* GraphEditorComponent::findNodeComponent (const juce::Uuid& uuid) const
{
    for (auto* nc : nodeComps_)
        if (nc->getNode().uuid == uuid)
            return nc;
    return nullptr;
}

PinComponent* GraphEditorComponent::findPinAt (juce::Point<int> localPos) const
{
    for (auto* nc : nodeComps_)
    {
        auto inside = nc->getLocalPoint (this, localPos);
        if (! nc->getLocalBounds().contains (inside)) continue;

        // Test pin children
        for (auto* child : nc->getChildren())
        {
            if (auto* pin = dynamic_cast<PinComponent*> (child))
            {
                auto pinLocal = pin->getLocalPoint (this, localPos);
                if (pin->getLocalBounds().contains (pinLocal))
                    return pin;
            }
        }
    }
    return nullptr;
}

//==============================================================================

void GraphEditorComponent::onNodeMoved (NodeComponent& node)
{
    // node_.editorPosition has already been updated to world coords by
    // NodeComponent::mouseDrag — persist it. Group-drag siblings have
    // their editor positions updated in mouseDrag too; re-layout every
    // node so the rest of the selection visually follows the cursor.
    activeController_->setNodePosition (node.getNode().uuid, node.getNode().editorPosition);

    if (selectedNodes_.size() > 1)
    {
        for (auto* nc : nodeComps_)
        {
            if (nc == &node) continue;
            if (! isSelected (nc->getNode().uuid)) continue;
            const auto world = nc->getNode().editorPosition;
            nc->setTopLeftPosition (
                (int) std::round (world.x * zoom_),
                (int) std::round (world.y * zoom_));
        }
    }
    repaint();
}

//==============================================================================

void GraphEditorComponent::setHighlightedTargetPin (PinComponent* p)
{
    if (highlightedTargetPin_ == p) return;
    if (highlightedTargetPin_ != nullptr) highlightedTargetPin_->setDragTargetHighlight (false);
    highlightedTargetPin_ = p;
    if (highlightedTargetPin_ != nullptr) highlightedTargetPin_->setDragTargetHighlight (true);
}

void GraphEditorComponent::beginPinDrag (PinComponent& pin)
{
    activePin_ = &pin;
    activeDragPos_ = pin.getCenterInGraphCoords();
    setHighlightedTargetPin (nullptr);
    repaint();
}

void GraphEditorComponent::updatePinDrag (juce::Point<int> pos)
{
    activeDragPos_ = pos;

    // Highlight the candidate target pin under the cursor — must be a pin
    // on a different node and of the opposite direction.
    PinComponent* target = nullptr;
    if (auto* p = findPinAt (pos))
    {
        if (activePin_ != nullptr
            && p != activePin_
            && p->getNodeUuid() != activePin_->getNodeUuid()
            && p->getDirection() != activePin_->getDirection())
        {
            target = p;
        }
    }
    setHighlightedTargetPin (target);
    repaint();
}

void GraphEditorComponent::endPinDrag (juce::Point<int> pos, bool shiftHeld)
{
    if (activePin_ == nullptr)
    {
        setHighlightedTargetPin (nullptr);
        repaint();
        return;
    }

    auto* target = findPinAt (pos);
    if (target != nullptr && target != activePin_
        && target->getNodeUuid() != activePin_->getNodeUuid()
        && target->getDirection() != activePin_->getDirection())
    {
        // Normalize so source is the Output side
        const PinComponent* src = activePin_;
        const PinComponent* dst = target;
        if (src->isInput()) std::swap (src, dst);

        const auto srcUuid  = src->getNodeUuid();
        const auto dstUuid  = dst->getNodeUuid();
        const int  srcStart = src->getChannelIndex();
        const int  dstStart = dst->getChannelIndex();

        activeController_->addConnection (srcUuid, srcStart, dstUuid, dstStart);

        // Shift held: keep chaining subsequent (out_i, in_i) pairs until one
        // side runs out. Useful for "wire all 16 outs of A to all 16 ins of B"
        // by dragging just the first one.
        //
        // Special cases:
        //   - destination has only one input pin: fan all remaining source
        //     outputs INTO that same pin (the graph engine sums them).
        //   - source has only one output pin: fan that single output OUT to
        //     every remaining input pin on the destination.
        // Both make "drop a wide bus into mono" / "broadcast mono to a wide
        // bus" a single-gesture operation.
        if (shiftHeld)
        {
            const auto* srcNode = activeController_->getNode (srcUuid);
            const auto* dstNode = activeController_->getNode (dstUuid);
            if (srcNode != nullptr && dstNode != nullptr)
            {
                const int srcMax = srcNode->channelCountOut;
                const int dstMax = dstNode->channelCountIn;

                if (srcMax == 1)
                {
                    for (int dstCh = dstStart + 1; dstCh < dstMax; ++dstCh)
                        activeController_->addConnection (srcUuid, srcStart, dstUuid, dstCh);
                }
                else if (dstMax == 1)
                {
                    for (int srcCh = srcStart + 1; srcCh < srcMax; ++srcCh)
                        activeController_->addConnection (srcUuid, srcCh, dstUuid, dstStart);
                }
                else
                {
                    for (int offset = 1;; ++offset)
                    {
                        const int srcCh = srcStart + offset;
                        const int dstCh = dstStart + offset;
                        if (srcCh >= srcMax || dstCh >= dstMax) break;
                        activeController_->addConnection (srcUuid, srcCh, dstUuid, dstCh);
                    }
                }
            }
        }
    }

    activePin_ = nullptr;
    setHighlightedTargetPin (nullptr);
    repaint();
}

//==============================================================================

void GraphEditorComponent::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.getPosition();
    const auto hit = findConnectionAt (pos.toFloat());

    if (e.mods.isPopupMenu())
    {
        if (hit.has_value())
        {
            const auto c = activeController_->getAllConnections()[*hit];
            juce::PopupMenu m;
            m.addItem ("Disconnect",
                [this, c]
                {
                    activeController_->removeConnection (c.fromUuid, c.fromCh,
                                                  c.toUuid,   c.toCh);
                });
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
            return;
        }

        showAddNodeMenu (pos);
        return;
    }

    // Left-click on a wire: SELECT it. Shift/Cmd toggles in the existing
    // selection, plain click replaces the selection with just this wire (and
    // clears any selected nodes — selection acts as one set from the user's
    // point of view, even though we track nodes and wires separately).
    if (hit.has_value())
    {
        const auto c = activeController_->getAllConnections()[*hit];

        if (e.mods.isShiftDown() || e.mods.isCommandDown())
        {
            toggleConnectionSelection (c);
        }
        else
        {
            selectedNodes_.clear();
            selectedConnections_.clear();
            selectedConnections_.push_back (c);
            if (selectionListener_) selectionListener_ (juce::Uuid::null());
            for (auto* nc : nodeComps_) nc->repaint();
            repaint();
        }
        grabKeyboardFocus();
        return;
    }

    // Left-click on empty canvas: clear selection (unless modifier held — then
    // keep the existing selection while we begin a marquee), and start the
    // marquee. Final commit happens on mouseUp once we know whether the user
    // actually dragged or just clicked.
    if (! hit.has_value())
    {
        if (! (e.mods.isShiftDown() || e.mods.isCommandDown()))
            clearSelection();

        marqueeActive_ = true;
        marqueeStart_  = pos;
        marqueeEnd_    = pos;
        grabKeyboardFocus();
    }
}

void GraphEditorComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! marqueeActive_) return;
    marqueeEnd_ = e.getPosition();
    repaint();
}

void GraphEditorComponent::mouseUp (const juce::MouseEvent& e)
{
    if (! marqueeActive_) return;
    marqueeActive_ = false;

    const auto rect = juce::Rectangle<int>::leftTopRightBottom (
        juce::jmin (marqueeStart_.x, marqueeEnd_.x),
        juce::jmin (marqueeStart_.y, marqueeEnd_.y),
        juce::jmax (marqueeStart_.x, marqueeEnd_.x),
        juce::jmax (marqueeStart_.y, marqueeEnd_.y));

    // Treat sub-3px drags as plain clicks — don't disturb selection.
    if (rect.getWidth() < 3 && rect.getHeight() < 3)
    {
        repaint();
        return;
    }

    // If no modifier, start fresh (mouseDown already cleared selection).
    // With shift/cmd, ADD nodes/wires that intersect the marquee.
    for (auto* nc : nodeComps_)
    {
        if (! nc->getBounds().intersects (rect)) continue;
        // Don't include terminals in marquee selections by default — they
        // tend to be left in fixed positions.
        const auto kind = nc->getNode().kind;
        if (kind == NodeKind::InputTerminal || kind == NodeKind::OutputTerminal) continue;

        addToSelection (nc->getNode().uuid);
    }

    // Wires whose bezier crosses or sits inside the marquee.
    const auto rectF = rect.toFloat();
    for (const auto& c : activeController_->getAllConnections())
    {
        auto* fromNc = findNodeComponent (c.fromUuid);
        auto* toNc   = findNodeComponent (c.toUuid);
        if (fromNc == nullptr || toNc == nullptr) continue;

        auto* fromPin = fromNc->findPin (PinComponent::Direction::Output, c.fromCh);
        auto* toPin   = toNc  ->findPin (PinComponent::Direction::Input,  c.toCh);
        if (fromPin == nullptr || toPin == nullptr) continue;

        if (wireIntersectsRect (fromPin->getCenterInGraphCoords().toFloat(),
                                toPin  ->getCenterInGraphCoords().toFloat(),
                                rectF))
            addConnectionToSelection (c);
    }

    repaint();
}

bool GraphEditorComponent::keyPressed (const juce::KeyPress& key)
{
    const auto ch    = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    const bool noMod = ! key.getModifiers().isCommandDown()
                    && ! key.getModifiers().isAltDown();

    if (ch == 'c' && noMod)
    {
        chainConnectSelection();
        return true;
    }

    // Toggle bypass / mute on every selected node.
    if ((ch == 'b' || ch == 'm') && noMod)
    {
        if (selectedNodes_.empty()) return false;

        // Use the most-recently-selected node as the "anchor" — toggle reads
        // its current state and applies the inverse to every selection (so
        // mixed-state selections all align after the keypress).
        auto* anchor = activeController_->getNode (selectedNodes_.back());
        if (anchor == nullptr) return true;

        if (ch == 'b')
        {
            const bool target = ! anchor->bypassed;
            for (const auto& uuid : selectedNodes_)
                activeController_->setNodeBypassed (uuid, target);
        }
        else
        {
            const bool target = ! anchor->muted;
            for (const auto& uuid : selectedNodes_)
                activeController_->setNodeMuted (uuid, target);
        }

        for (auto* nc : nodeComps_) nc->repaint();
        return true;
    }

    if (key == juce::KeyPress::escapeKey)
    {
        clearSelection();
        return true;
    }
    return false;
}

//==============================================================================

juce::Path GraphEditorComponent::makeConnectionPath (juce::Point<float> from,
                                                      juce::Point<float> to) const
{
    juce::Path p;
    p.startNewSubPath (from);
    const float dx = juce::jmax (40.0f, std::abs (to.x - from.x) * 0.5f);
    p.cubicTo (from.x + dx, from.y,
               to.x   - dx, to.y,
               to.x,        to.y);
    return p;
}

bool GraphEditorComponent::wireIntersectsRect (juce::Point<float> from,
                                                juce::Point<float> to,
                                                juce::Rectangle<float> rect) const
{
    // Trivial rejects via overall bounding box. The bezier's bounding box is
    // contained in the convex hull of (from, to, control-points) — because our
    // control points lie on the same horizontal as the endpoints, the path
    // never strays vertically beyond [min(from.y, to.y), max(from.y, to.y)].
    const float minX = juce::jmin (from.x, to.x) - 40.0f;
    const float maxX = juce::jmax (from.x, to.x) + 40.0f;
    const float minY = juce::jmin (from.y, to.y);
    const float maxY = juce::jmax (from.y, to.y);
    if (maxX < rect.getX() || minX > rect.getRight()
        || maxY < rect.getY() || minY > rect.getBottom())
        return false;

    auto path = makeConnectionPath (from, to);
    juce::PathFlatteningIterator it (path);
    while (it.next())
    {
        const juce::Point<float> p1 (it.x1, it.y1);
        const juce::Point<float> p2 (it.x2, it.y2);

        if (rect.contains (p1) || rect.contains (p2))
            return true;

        // Segment vs each rectangle edge.
        const juce::Line<float> seg (p1, p2);
        const juce::Point<float> tl = rect.getTopLeft();
        const juce::Point<float> tr = rect.getTopRight();
        const juce::Point<float> bl = rect.getBottomLeft();
        const juce::Point<float> br = rect.getBottomRight();

        juce::Point<float> hit;
        if (seg.intersects (juce::Line<float> (tl, tr), hit)) return true;
        if (seg.intersects (juce::Line<float> (tr, br), hit)) return true;
        if (seg.intersects (juce::Line<float> (br, bl), hit)) return true;
        if (seg.intersects (juce::Line<float> (bl, tl), hit)) return true;
    }
    return false;
}

std::optional<std::size_t>
GraphEditorComponent::findConnectionAt (juce::Point<float> pos, float tolerance) const
{
    const auto conns = activeController_->getAllConnections();
    for (std::size_t i = 0; i < conns.size(); ++i)
    {
        const auto& c = conns[i];
        auto* fromNc = findNodeComponent (c.fromUuid);
        auto* toNc   = findNodeComponent (c.toUuid);
        if (fromNc == nullptr || toNc == nullptr) continue;

        auto* fromPin = fromNc->findPin (PinComponent::Direction::Output, c.fromCh);
        auto* toPin   = toNc  ->findPin (PinComponent::Direction::Input,  c.toCh);
        if (fromPin == nullptr || toPin == nullptr) continue;

        auto path = makeConnectionPath (fromPin->getCenterInGraphCoords().toFloat(),
                                        toPin  ->getCenterInGraphCoords().toFloat());

        juce::Path stroked;
        juce::PathStrokeType (tolerance * 2.0f).createStrokedPath (stroked, path);
        if (stroked.contains (pos))
            return i;
    }
    return std::nullopt;
}

//==============================================================================

void GraphEditorComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff202028));

    g.setColour (juce::Colours::white.withAlpha (0.04f));
    for (int x = 20; x < getWidth(); x += 20)
        g.drawVerticalLine (x, 0.0f, (float) getHeight());
    for (int y = 20; y < getHeight(); y += 20)
        g.drawHorizontalLine (y, 0.0f, (float) getWidth());

    // Existing connections — selected ones drawn highlighted on top.
    for (const auto& c : activeController_->getAllConnections())
    {
        auto* fromNc = findNodeComponent (c.fromUuid);
        auto* toNc   = findNodeComponent (c.toUuid);
        if (fromNc == nullptr || toNc == nullptr) continue;

        auto* fromPin = fromNc->findPin (PinComponent::Direction::Output, c.fromCh);
        auto* toPin   = toNc  ->findPin (PinComponent::Direction::Input,  c.toCh);
        if (fromPin == nullptr || toPin == nullptr) continue;

        drawConnection (g, fromPin->getCenterInGraphCoords().toFloat(),
                           toPin  ->getCenterInGraphCoords().toFloat(),
                           isConnectionSelected (c));
    }

    // Floating drag-in-progress wire
    if (activePin_ != nullptr)
    {
        auto fromPos = activePin_->getCenterInGraphCoords().toFloat();
        auto toPos   = activeDragPos_.toFloat();
        if (activePin_->isInput()) std::swap (fromPos, toPos);
        drawConnection (g, fromPos, toPos, true);
    }

    // Marquee selection rectangle
    if (marqueeActive_)
    {
        const auto rect = juce::Rectangle<int>::leftTopRightBottom (
            juce::jmin (marqueeStart_.x, marqueeEnd_.x),
            juce::jmin (marqueeStart_.y, marqueeEnd_.y),
            juce::jmax (marqueeStart_.x, marqueeEnd_.x),
            juce::jmax (marqueeStart_.y, marqueeEnd_.y)).toFloat();

        g.setColour (juce::Colours::yellow.withAlpha (0.12f));
        g.fillRect (rect);
        g.setColour (juce::Colours::yellow.withAlpha (0.7f));
        g.drawRect (rect, 1.0f);
    }
}

void GraphEditorComponent::resized()
{
    layoutNodeComponents();
}

void GraphEditorComponent::drawConnection (juce::Graphics& g,
                                           juce::Point<float> from,
                                           juce::Point<float> to,
                                           bool highlighted) const
{
    const auto p = makeConnectionPath (from, to);
    g.setColour (highlighted ? juce::Colours::yellow
                             : juce::Colours::white.withAlpha (0.6f));
    g.strokePath (p, juce::PathStrokeType (highlighted ? 2.5f : 1.6f));
}

//==============================================================================

void GraphEditorComponent::showAddNodeMenu (juce::Point<int> localPos)
{
    juce::PopupMenu m;
    juce::PopupMenu nativeMenu;

    auto addNativeNode = [this, localPos] (auto factory, NodeKind kind,
                                           const juce::String& name,
                                           int chIn, int chOut)
    {
        auto proc = factory();
        activeController_->addNode (std::move (proc), kind, name, chIn, chOut, localPos);
    };

    // Default channel count for newly-added native nodes: match the host bus
    const int defaultCh = juce::jmax (2, activeController_->getInputChannelCount());

    nativeMenu.addItem ("Gain (" + juce::String (defaultCh) + " ch)",
                        [this, addNativeNode, defaultCh]
                        { addNativeNode ([defaultCh] { return std::make_unique<GainNode>      (defaultCh); },
                                         NodeKind::Gain,      "Gain",      defaultCh, defaultCh); });
    nativeMenu.addItem ("Mute / Invert (" + juce::String (defaultCh) + " ch)",
                        [this, addNativeNode, defaultCh]
                        { addNativeNode ([defaultCh] { return std::make_unique<MutePhaseNode> (defaultCh); },
                                         NodeKind::MutePhase, "Mute/Inv",  defaultCh, defaultCh); });
    nativeMenu.addItem ("Matrix Mixer (" + juce::String (defaultCh) + " in / "
                                         + juce::String (defaultCh) + " out)",
                        [this, addNativeNode, defaultCh]
                        { addNativeNode ([defaultCh] { return std::make_unique<MatrixMixerNode>    (defaultCh, defaultCh); },
                                         NodeKind::MatrixMixer,    "Matrix Mixer", defaultCh, defaultCh); });
    nativeMenu.addItem ("Delay (" + juce::String (defaultCh) + " ch)",
                        [this, addNativeNode, defaultCh]
                        { addNativeNode ([defaultCh] { return std::make_unique<DelayNode>     (defaultCh); },
                                         NodeKind::Delay,     "Delay",     defaultCh, defaultCh); });
    nativeMenu.addItem ("Subgraph (" + juce::String (defaultCh) + " in / "
                                     + juce::String (defaultCh) + " out)",
                        [this, addNativeNode, defaultCh]
                        { addNativeNode ([defaultCh] { return std::make_unique<SubgraphNode>  (defaultCh, defaultCh); },
                                         NodeKind::Subgraph,  "Subgraph",  defaultCh, defaultCh); });

    m.addSubMenu ("Add native node", nativeMenu);

    auto& kpl = processor_.getPluginList().getKnownPluginList();
    const bool hasPlugins = ! kpl.getTypes().isEmpty();

    m.addItem ("Add plugin...", hasPlugins, false,
               [this, localPos] { showPluginSearchPopup (localPos); });

    if (! hasPlugins)
        m.addItem ("(scan plugins via toolbar first)", false, false, [] {});

    m.showMenuAsync (juce::PopupMenu::Options()
                         .withTargetComponent (this)
                         .withMousePosition());
}

void GraphEditorComponent::showPluginSearchPopup (juce::Point<int> localPos)
{
    auto& kpl = processor_.getPluginList().getKnownPluginList();
    auto types = kpl.getTypes();
    if (types.isEmpty()) return;

    // Sort by manufacturer then name for a stable, readable list.
    auto sorted = types;
    std::sort (sorted.begin(), sorted.end(),
               [] (const juce::PluginDescription& a, const juce::PluginDescription& b)
               {
                   const int m = a.manufacturerName.compareIgnoreCase (b.manufacturerName);
                   if (m != 0) return m < 0;
                   return a.name.compareIgnoreCase (b.name) < 0;
               });

    auto popup = std::make_unique<PluginSearchPopup> (sorted,
        [this, sorted, localPos] (int index)
        {
            if (index < 0 || index >= sorted.size()) return;
            const auto desc = sorted.getReference (index);

            juce::String err;
            auto inst = PluginInstanceFactory::create (
                            processor_.getPluginList().getFormatManager(),
                            desc,
                            processor_.getSampleRate() > 0.0 ? processor_.getSampleRate() : 44100.0,
                            processor_.getBlockSize()  > 0   ? processor_.getBlockSize()  : 512,
                            0,
                            err);
            if (inst == nullptr)
            {
                juce::NativeMessageBox::showAsync (
                    juce::MessageBoxOptions()
                        .withIconType (juce::MessageBoxIconType::WarningIcon)
                        .withTitle ("Plugin load failed")
                        .withMessage (err.isEmpty() ? "Unknown error" : err),
                    nullptr);
                return;
            }

            const int chIn  = inst->getMainBusNumInputChannels();
            const int chOut = inst->getMainBusNumOutputChannels();
            auto pluginDesc = std::make_unique<juce::PluginDescription> (desc);

            auto uuid = activeController_->addNode (std::move (inst), NodeKind::Plugin,
                                                    desc.name, chIn, chOut, localPos);
            if (auto* gn = activeController_->getNode (uuid))
                gn->pluginDescription = std::move (pluginDesc);
        });

    // Anchor the call-out at the canvas position the user right-clicked.
    auto screenAnchor = localAreaToGlobal (juce::Rectangle<int> (localPos.x, localPos.y, 1, 1));
    juce::CallOutBox::launchAsynchronously (std::move (popup), screenAnchor, nullptr);
}

//==============================================================================
// Selection

void GraphEditorComponent::setSelectedNode (juce::Uuid uuid)
{
    selectedNodes_.clear();
    if (! uuid.isNull())
        selectedNodes_.push_back (uuid);
    if (selectionListener_) selectionListener_ (getSelectedNodeUuid());
    repaint();
    for (auto* nc : nodeComps_) nc->repaint();
}

void GraphEditorComponent::addToSelection (juce::Uuid uuid)
{
    if (uuid.isNull() || isSelected (uuid)) return;
    selectedNodes_.push_back (uuid);
    if (selectionListener_) selectionListener_ (getSelectedNodeUuid());
    repaint();
    for (auto* nc : nodeComps_) nc->repaint();
}

void GraphEditorComponent::toggleSelection (juce::Uuid uuid)
{
    if (uuid.isNull()) return;
    auto it = std::find (selectedNodes_.begin(), selectedNodes_.end(), uuid);
    if (it != selectedNodes_.end()) selectedNodes_.erase (it);
    else                            selectedNodes_.push_back (uuid);
    if (selectionListener_) selectionListener_ (getSelectedNodeUuid());
    repaint();
    for (auto* nc : nodeComps_) nc->repaint();
}

void GraphEditorComponent::clearSelection()
{
    const bool hadAnything = ! selectedNodes_.empty() || ! selectedConnections_.empty();
    selectedNodes_.clear();
    selectedConnections_.clear();
    if (! hadAnything) return;
    if (selectionListener_) selectionListener_ (juce::Uuid::null());
    repaint();
    for (auto* nc : nodeComps_) nc->repaint();
}

//==============================================================================
// Connection (wire) selection

void GraphEditorComponent::addConnectionToSelection (const ConnectionInfo& c)
{
    if (isConnectionSelected (c)) return;
    selectedConnections_.push_back (c);
    repaint();
}

void GraphEditorComponent::toggleConnectionSelection (const ConnectionInfo& c)
{
    auto it = std::find (selectedConnections_.begin(), selectedConnections_.end(), c);
    if (it != selectedConnections_.end()) selectedConnections_.erase (it);
    else                                  selectedConnections_.push_back (c);
    repaint();
}

void GraphEditorComponent::clearConnectionSelection()
{
    if (selectedConnections_.empty()) return;
    selectedConnections_.clear();
    repaint();
}

bool GraphEditorComponent::isConnectionSelected (const ConnectionInfo& c) const
{
    return std::find (selectedConnections_.begin(), selectedConnections_.end(), c)
           != selectedConnections_.end();
}

void GraphEditorComponent::deleteSelectedConnections()
{
    if (selectedConnections_.empty() || activeController_ == nullptr) return;
    // Snapshot — removeConnection notifies the topology listener which clears
    // selection state via async rebuild, so we copy first.
    auto victims = selectedConnections_;
    selectedConnections_.clear();
    for (const auto& c : victims)
        activeController_->removeConnection (c.fromUuid, c.fromCh, c.toUuid, c.toCh);
    repaint();
}

bool GraphEditorComponent::isSelected (juce::Uuid uuid) const
{
    return std::find (selectedNodes_.begin(), selectedNodes_.end(), uuid) != selectedNodes_.end();
}

juce::Uuid GraphEditorComponent::getSelectedNodeUuid() const
{
    return selectedNodes_.empty() ? juce::Uuid::null() : selectedNodes_.back();
}

GraphNode* GraphEditorComponent::getSelectedNode() const
{
    const auto u = getSelectedNodeUuid();
    if (u.isNull() || activeController_ == nullptr) return nullptr;
    return activeController_->getNode (u);
}

void GraphEditorComponent::chainConnectSelection()
{
    if (activeController_ == nullptr) return;
    if (selectedNodes_.size() < 2) return;

    std::vector<GraphNode*> ns;
    ns.reserve (selectedNodes_.size());
    for (const auto& u : selectedNodes_)
        if (auto* gn = activeController_->getNode (u))
            ns.push_back (gn);
    if (ns.size() < 2) return;

    std::sort (ns.begin(), ns.end(),
               [] (GraphNode* a, GraphNode* b)
               { return a->editorPosition.x < b->editorPosition.x; });

    for (std::size_t i = 0; i + 1 < ns.size(); ++i)
    {
        auto* L = ns[i];
        auto* R = ns[i + 1];
        const int n = juce::jmin (L->channelCountOut, R->channelCountIn);
        for (int ch = 0; ch < n; ++ch)
            activeController_->addConnection (L->uuid, ch, R->uuid, ch);
    }
}

//==============================================================================
// Subgraph navigation

void GraphEditorComponent::descendInto (const juce::Uuid& subgraphNodeUuid)
{
    if (activeController_ == nullptr) return;
    auto* gn = activeController_->getNode (subgraphNodeUuid);
    if (gn == nullptr || gn->kind != NodeKind::Subgraph) return;

    auto* sub = dynamic_cast<SubgraphNode*> (gn->processor);
    if (sub == nullptr) return;

    // Stop listening on the soon-to-be-parent before swapping.
    activeController_->setTopologyListener (nullptr);

    navStack_.push_back ({ activeController_, activeLabel_, subgraphNodeUuid });
    activeController_ = &sub->getInner();
    activeLabel_ = gn->displayName.isNotEmpty() ? gn->displayName : juce::String ("Subgraph");
    selectedNodes_.clear();

    hookTopologyListener();
    rebuildAll();

    if (selectionListener_)   selectionListener_ (juce::Uuid::null());
    if (breadcrumbListener_)  breadcrumbListener_();
}

void GraphEditorComponent::ascendOneLevel()
{
    if (navStack_.empty() || activeController_ == nullptr) return;

    activeController_->setTopologyListener (nullptr);

    auto top = navStack_.back();
    navStack_.pop_back();
    activeController_ = top.controller;
    activeLabel_ = top.label;
    selectedNodes_.clear();

    hookTopologyListener();
    rebuildAll();

    if (selectionListener_)   selectionListener_ (juce::Uuid::null());
    if (breadcrumbListener_)  breadcrumbListener_();
}

void GraphEditorComponent::ascendToRoot()
{
    while (! navStack_.empty()) ascendOneLevel();
}

std::vector<GraphEditorComponent::BreadcrumbEntry>
GraphEditorComponent::getBreadcrumbPath() const
{
    std::vector<BreadcrumbEntry> out;
    out.reserve (navStack_.size() + 1);
    for (const auto& e : navStack_)
        out.push_back ({ e.controller, e.label });
    out.push_back ({ activeController_, activeLabel_ });
    return out;
}

void GraphEditorComponent::jumpToBreadcrumb (std::size_t index)
{
    auto path = getBreadcrumbPath();
    if (index >= path.size()) return;
    // Index navStack_.size() is the active level; anything before that means
    // pop until we land there.
    while (navStack_.size() > index) ascendOneLevel();
}

//==============================================================================
// Zoom (pan is handled by the surrounding juce::Viewport)

void GraphEditorComponent::setZoom (float newZoom, juce::Point<int> anchorScreen)
{
    newZoom = juce::jlimit (kMinZoom, kMaxZoom, newZoom);
    if (std::abs (newZoom - zoom_) < 1.0e-4f) return;

    // Capture the world point under the cursor BEFORE the zoom changes.
    auto* viewport = findParentComponentOfClass<juce::Viewport>();
    const auto worldUnderCursor = juce::Point<float> (anchorScreen.x / zoom_,
                                                       anchorScreen.y / zoom_);

    // Capture the cursor's viewport-local position so we can pin it after.
    juce::Point<int> viewportLocal;
    if (viewport != nullptr)
        viewportLocal = viewport->getLocalPoint (this, anchorScreen);

    zoom_ = newZoom;

    // Resize the canvas to the new world-times-zoom so the Viewport's
    // scrollbars cover the right range.
    setSize ((int) std::ceil (kWorldW * zoom_),
             (int) std::ceil (kWorldH * zoom_));
    layoutNodeComponents();

    // Adjust the Viewport's scroll position so the same world point lands
    // back under the cursor — feels like zooming-into-the-cursor.
    if (viewport != nullptr)
    {
        const auto newCanvasPoint = juce::Point<int> (
            (int) std::round (worldUnderCursor.x * zoom_),
            (int) std::round (worldUnderCursor.y * zoom_));
        viewport->setViewPosition (
            newCanvasPoint.x - viewportLocal.x,
            newCanvasPoint.y - viewportLocal.y);
    }

    repaint();
}

void GraphEditorComponent::resetZoom()
{
    zoom_ = 1.0f;
    setSize (kWorldW, kWorldH);
    layoutNodeComponents();

    // Scroll the viewport to the upper-left of the typical Input/Output area.
    if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
        viewport->setViewPosition (0, 0);

    repaint();
}

void GraphEditorComponent::mouseWheelMove (const juce::MouseEvent& e,
                                           const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown() || e.mods.isAltDown())
    {
        const float factor = std::pow (2.0f, wheel.deltaY * 0.5f);
        setZoom (zoom_ * factor, e.getPosition());
        return;
    }

    // No modifier: let the surrounding Viewport scroll the canvas.
    juce::Component::mouseWheelMove (e, wheel);
}

void GraphEditorComponent::layoutNodeComponents()
{
    for (auto* nc : nodeComps_)
    {
        const auto world = nc->getNode().editorPosition;
        nc->setTopLeftPosition (
            (int) std::round (world.x * zoom_),
            (int) std::round (world.y * zoom_));
        nc->setTransform (juce::AffineTransform::scale (zoom_));
    }
}
