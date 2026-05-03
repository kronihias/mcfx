# mcfx_graph

Flexible plug-in graph / patchbay for the mcfx suite. Load any number of VST2 / VST3 / AU plug-ins as nodes inside a single mcfx_graph instance, wire them together with bezier connections, and build entire signal flows. Multiple connections feeding the same input are summed automatically by the graph engine.

---

## Native nodes

| Node | What it does |
|---|---|
| **Gain** | Per-channel gain. dB range **-120 dB ... +40 dB**; double-click resets to 0 dB. **Linked** toggle: one slider drives all channels. **Linear / dB** mode toggle; type `0.5 lin` / `0.5 x` to enter linear in dB mode, or `-6 dB` to enter dB in linear mode. |
| **Mute / Phase invert** | Per-channel mute and polarity invert, both with a **Linked** toggle that mirrors all channels. |
| **Matrix Mixer** | N × M matrix with independent input / output counts. **Copy / Paste** matrix from clipboard as TSV. **dB** display toggle (-120 dB ≡ 0.0 linear). Focused-cell row / column header highlight makes it easy to see which entry is being edited. |
| **Delay** | Per-channel sample-accurate fractional delay (Lagrange-3rd interpolation). **Linked** mode broadcasts one slider to all channels. **Samples / ms** unit toggle. |
| **Subgraph** | A node that contains its own graph with configurable in / out channel counts — nest as deep as you like. |

## Hosting

- VST2 / VST3 / AU support
- Out-of-process plug-in scanning (crash-isolated, parallel) — same scanner code as mcfx_anything but with its own scan cache so the two plug-ins don't interfere
- Per-node configurable channel count via a popup that probes the plug-in's accepted layouts via real `setBusesLayout` round-trips, so the menu only lists counts the plug-in actually supports
- Plug-in format chip (VST3 / VST / AudioUnit) shown on every plug-in node and in the properties panel
- VST3 channel-count negotiation patched to accept any discrete-channel mcfx-style layout (see `JUCE_patches/juce_VST3PluginFormatImpl.h.patch`)
- Hosted plug-in GUIs open in floating windows; they're closed automatically before the underlying processor is destroyed (some plug-ins crash if the editor outlives them)

## Editor

- Drag-to-connect pins; hold **Shift** at drop to chain subsequent output→input pairs, or — if the target has a single input pin — fan all source outputs into it
- Marquee-select nodes + wires; **Shift / Cmd-click** to add to selection
- Group-drag any selected node moves the entire selection
- Wire selection: click to select, shift / cmd-click to add, marquee-drag selects every wire and node it crosses; **Backspace / Delete** removes everything
- **Snapshot-based undo / redo** (Cmd+Z / Cmd+Shift+Z, capped at 64 states). Smart undo: parameter / connection / position changes are applied **in place** — no plug-in instance recreation, no plug-in GUI flicker
- **Per-node Bypass / Mute** with visual indicator on the node, both persisted in JSON; mute zeroes the node's output without disconnecting wires so the diagram stays readable
- **Subgraph navigation:** double-click a subgraph node to descend; breadcrumb bar to jump back; subgraphs work audibly even without descending
- **Zoom + scroll:** Cmd-scroll zooms around the cursor (0.25 × – 3 ×), `+` / `−` toolbar buttons step zoom, `1:1` resets, plain scroll pans via the canvas Viewport's scrollbars
- **DAW automation:** 256 forwarding parameters can be dynamically bound to any inner-plug-in parameter via the "P" toggles in the properties panel; bindings survive project save/load
- Resizable side panel + tooltips on every toolbar button; in-app help popup (`?` button) lists every shortcut
- Drag and drop a `.json` file onto the editor to load it
- Outer in / out channel count is determined by the host (DAW track) and rebuilt automatically when the host re-negotiates the bus

## Persistence

Graph state — topology, plug-in states (base64), parameter bindings, bypass / mute, panel width — saves as a pretty-printed, human-readable JSON file with a `format: "mcfx_graph"` header. The state is embedded in the DAW project state, and is also exportable / loadable as a standalone `.json` file (including drag-drop onto the editor window).

## Delay compensation

mcfx_graph performs **automatic plug-in delay compensation across the whole graph**, end-to-end and across parallel paths.

- The internal `juce::AudioProcessorGraph` walks every path during render-sequence building. For each node, it pads each input whose source-side latency is below the maximum of all inputs into that node, so the sum is sample-aligned. Series chains accumulate latency normally.
- Hosted plug-ins' reported latency is forwarded up through mcfx_graph's per-node bypass / mute wrapper (both the initial value at `prepareToPlay` and any runtime change), so the graph engine sees the real numbers.
- The total latency of the whole graph (longest input-to-output path) is reported to the DAW, so the host compensates the rest of the project against mcfx_graph as a whole.

**Caveats:**

- The native **Delay** node reports 0 latency on purpose — its delay is musical intent, not algorithmic latency, so the graph engine doesn't try to compensate it away. If you need an algorithmic delay that participates in PDC, use a hosted plug-in that reports its own latency.
- Plug-ins that mis-report their latency (some VSTs lie about FFT block sizes or lookahead) will still misalign. That's outside mcfx_graph's control.

---

## Keyboard / mouse shortcuts

| Action | Shortcut |
|---|---|
| Undo / redo | **Cmd+Z** / **Cmd+Shift+Z** (or **Cmd+Y**) |
| Delete selection (nodes + wires) | **Backspace** or **Delete** |
| Clear selection | **Esc** |
| Toggle bypass on selected node(s) | **B** |
| Toggle mute on selected node(s) | **M** |
| Chain-connect selected nodes left → right | **C** |
| Add to / remove from selection | **Shift-click** or **Cmd-click** on node / wire |
| Marquee-select | drag on empty canvas (Shift / Cmd to add to existing selection) |
| Group-drag | drag any selected node — all selected nodes move together |
| Connect a pin | drag from a pin to another pin of the opposite direction |
| Fan-out connect | hold **Shift** while releasing the drag — chains the next output→input pairs until one side runs out; if the target has a single input pin, all source outputs fan into it (summed by the graph engine) |
| Add node / plug-in menu | **right-click** on empty canvas |
| Node context menu (Bypass / Mute / Change channel count / Delete / …) | **right-click** on a node |
| Disconnect single wire | **right-click** on the wire → Disconnect |
| Open plug-in GUI | **double-click** plug-in node |
| Open native-node parameter window | **double-click** Gain / MutePhase / Matrix Mixer / Delay |
| Descend into subgraph | **double-click** subgraph node |
| Zoom around cursor | **Cmd-scroll** on canvas |
| Zoom step / reset | toolbar **+** / **−** / **1:1** |
| Scroll the canvas | scrollbars or wheel without modifier |
| Show this help | toolbar **?** |
