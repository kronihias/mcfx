/*
  ==============================================================================

   GraphEditorComponent — canvas that renders a GraphController as draggable
   node boxes connected by bezier wires. Handles drag-to-connect, right-click
   context menus, and stores the temporary "in-progress" connection drawn
   while the user drags from a pin.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "NodeComponent.h"
#include "PinComponent.h"
#include "../Graph/GraphController.h"  // for ConnectionInfo nested type

class Mcfx_graphAudioProcessor;

class GraphEditorComponent : public juce::Component
{
public:
    GraphEditorComponent (Mcfx_graphAudioProcessor& processor,
                          GraphController& controller);
    ~GraphEditorComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& key) override;

    /** Called by NodeComponent when its position changes. */
    void onNodeMoved (NodeComponent& node);

    /** Called by PinComponent on mouse-down — start drag-to-connect. */
    void beginPinDrag (PinComponent& pin);
    /** Called by PinComponent on mouse-drag — update floating wire. */
    void updatePinDrag (juce::Point<int> screenOrLocal);
    /** Called by PinComponent on mouse-up — try to commit a connection.
        If `shiftHeld`, also chain the subsequent output→input channel pairs
        starting after the dropped one (up to whichever side runs out first). */
    void endPinDrag (juce::Point<int> screenOrLocal, bool shiftHeld = false);

    /** The active controller — root by default, points at a SubgraphNode's
        inner controller when the user has descended into a subgraph. */
    GraphController& getController() noexcept { return *activeController_; }
    Mcfx_graphAudioProcessor& getProcessor() noexcept { return processor_; }

    /** Rebuild the editor from scratch (after JSON load or major topology change). */
    void rebuildAll();

    NodeComponent* findNodeComponent (const juce::Uuid& uuid) const;
    PinComponent*  findPinAt (juce::Point<int> localPos) const;

    /** Show "Add Node" menu at the given position. */
    void showAddNodeMenu (juce::Point<int> localPos);

    /** Show the searchable plugin selector callout, anchored at the given
        canvas position. The selected plugin will be added there. */
    void showPluginSearchPopup (juce::Point<int> localPos);

    //==============================================================================
    // Zoom + pan
    void  setZoom (float newZoom, juce::Point<int> anchorScreen = {});
    float getZoom() const noexcept { return zoom_; }
    void  resetZoom();


    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& wheel) override;

    //==============================================================================
    // Selection. Multi-select supported: shift/cmd-click toggles, drag on
    // empty canvas marquee-selects, plain click selects single. The properties
    // panel and primary getters use the LAST-added selection as "primary".
    void setSelectedNode (juce::Uuid uuid);              // single-select shortcut
    void addToSelection  (juce::Uuid uuid);
    void toggleSelection (juce::Uuid uuid);
    void clearSelection();
    bool isSelected (juce::Uuid uuid) const;

    /** Last-added selection — used by single-node consumers like the
        properties panel. Returns null Uuid if nothing is selected. */
    juce::Uuid  getSelectedNodeUuid() const;
    GraphNode*  getSelectedNode() const;

    /** Full selection set, in selection order (last is "primary"). */
    const std::vector<juce::Uuid>& getSelection() const { return selectedNodes_; }

    //==============================================================================
    // Wire (connection) selection — separate set from node selection. Both are
    // cleared together by clearSelection() and by Delete/Backspace.
    using ConnectionInfo = GraphController::ConnectionInfo;

    void addConnectionToSelection    (const ConnectionInfo& c);
    void toggleConnectionSelection   (const ConnectionInfo& c);
    void clearConnectionSelection();
    bool isConnectionSelected        (const ConnectionInfo& c) const;
    const std::vector<ConnectionInfo>& getSelectedConnections() const { return selectedConnections_; }

    /** Remove every currently-selected wire from the active graph. */
    void deleteSelectedConnections();

    using SelectionListener = std::function<void (juce::Uuid)>;
    void setSelectionListener (SelectionListener cb) { selectionListener_ = std::move (cb); }

    /** Connect every selected node's outputs to the next-rightward selected
        node's inputs, channel-by-channel up to min(out, in). Sorted by
        world-x ascending. Bound to the 'C' key. */
    void chainConnectSelection();

    //==============================================================================
    // Subgraph navigation. The canvas always edits whatever GraphController is
    // "active". The root is the outer-plugin controller; descending pushes a
    // SubgraphNode's inner controller. The breadcrumb path is exposed for the
    // surrounding editor (PluginEditor) to render.
    GraphController& getActiveController() noexcept { return *activeController_; }
    bool isAtRoot() const noexcept { return navStack_.empty(); }
    void descendInto (const juce::Uuid& subgraphNodeUuid);
    void ascendOneLevel();
    void ascendToRoot();

    /** Path entries for breadcrumb display: each entry has a controller pointer
        (for click-to-jump) and a label. The first entry is always the root. */
    struct BreadcrumbEntry
    {
        GraphController* controller;
        juce::String     label;
    };
    std::vector<BreadcrumbEntry> getBreadcrumbPath() const;
    void jumpToBreadcrumb (std::size_t index);

    using BreadcrumbListener = std::function<void()>;
    void setBreadcrumbListener (BreadcrumbListener cb) { breadcrumbListener_ = std::move (cb); }

private:
    void drawConnection (juce::Graphics& g,
                         juce::Point<float> from,
                         juce::Point<float> to,
                         bool highlighted) const;

    /** Build the cubic-bezier path used for both rendering and hit-testing. */
    juce::Path makeConnectionPath (juce::Point<float> from,
                                   juce::Point<float> to) const;

    /** Hit-test all visible connections; returns the connection whose stroked
        path is within `tolerance` pixels of `pos`, or nullopt. */
    std::optional<std::size_t> findConnectionAt (juce::Point<float> pos,
                                                  float tolerance = 6.0f) const;

    /** True if the bezier wire from `from` to `to` intersects the screen-space
        rectangle (used for marquee selection). */
    bool wireIntersectsRect (juce::Point<float> from,
                             juce::Point<float> to,
                             juce::Rectangle<float> rect) const;

    void setHighlightedTargetPin (PinComponent* p);

    void hookTopologyListener();
    void layoutNodeComponents();

    Mcfx_graphAudioProcessor& processor_;
    GraphController&  rootController_;
    GraphController*  activeController_ = nullptr;

    // Breadcrumb path: each entry is a (controller, label) pair representing
    // a level above the current one. The active controller itself is NOT
    // in the stack; it's the "tip".
    struct NavStackEntry
    {
        GraphController* controller;
        juce::String     label;     // shown in the breadcrumb bar
        juce::Uuid       enteredFromSubgraphUuid; // node in the parent that we descended through
    };
    std::vector<NavStackEntry> navStack_;
    juce::String activeLabel_ { "Root" };

    juce::OwnedArray<NodeComponent> nodeComps_;

    PinComponent*    activePin_ = nullptr;
    juce::Point<int> activeDragPos_;
    PinComponent*    highlightedTargetPin_ = nullptr;

    std::vector<juce::Uuid>        selectedNodes_;        // last is primary
    std::vector<ConnectionInfo>    selectedConnections_;  // selected wires
    SelectionListener  selectionListener_;
    BreadcrumbListener breadcrumbListener_;

    // Marquee-select state (drag on empty canvas)
    bool             marqueeActive_ = false;
    juce::Point<int> marqueeStart_;
    juce::Point<int> marqueeEnd_;

    // Zoom only — pan is handled by the surrounding juce::Viewport.
    // World coords are stored in GraphNode.editorPosition; child node
    // positions = world * zoom_.
    float zoom_ = 1.0f;

    static constexpr float kMinZoom  = 0.25f;
    static constexpr float kMaxZoom  = 3.0f;

    static constexpr int kWorldW = 4000;
    static constexpr int kWorldH = 3000;
};
