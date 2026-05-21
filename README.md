# mcfx — Multichannel Cross-Platform Audio Plug-in Suite

A suite of multichannel VST/VST3/AU plug-ins and standalone applications for macOS, Windows, and Linux.

- Channel count is configurable at compile time (VST2 plug-ins support up to 128 channels)
- VST3: one binary per plug-in, automatically adjusts to the track's channel count (up to 64 channels)
- Built on the [JUCE](https://www.juce.com) framework (GPLv3)
- Designed to complement the [ambiX Ambisonic plug-ins](http://www.matthiaskronlachner.com/?p=2015)
- Ready-to-use binaries available at [https://github.com/kronihias/mcfx/releases](https://github.com/kronihias/mcfx/releases)

---

## Table of Contents

- [License](#license)
- [Plug-ins](#plug-ins)
  - [mcfx_convolver](#mcfx_convolver)
  - [mcfx_anything](#mcfx_anything)
  - [mcfx_graph](#mcfx_graph)
  - [mcfx_mimoeq](#mcfx_mimoeq)
  - [mcfx_delay](#mcfx_delay)
  - [mcfx_filter](#mcfx_filter)
  - [mcfx_gain_delay](#mcfx_gain_delay)
  - [mcfx_meter](#mcfx_meter)
  - [mcfx_send / mcfx_receive](#mcfx_send--mcfx_receive)
- [Prerequisites for Building](#prerequisites-for-building)
- [How to Build](#how-to-build)
- [Build Parameters](#build-parameters)
- [Changelog](#changelog)

---

## License

mcfx is free software licensed under the **GNU General Public License version 3 (GPLv3)**.

---

## Plug-ins

### mcfx_convolver

Multichannel convolution matrix.

- Highly optimized non-uniformly partitioned fast convolution using SIMD for Intel and Apple Silicon
- Loads configuration files compatible with jconvolver `.conf` format
- Supports loading `.wav` files directly, optionally reading input channel metadata
- Drag and drop a `.conf` or `.wav` file onto the GUI to load it
- Option to embed the preset inside the DAW project (no external files needed)
- OSC remote control: `/reload`, `/load <preset.conf>` — configurable port
- See `CONVOLVER_CONFIG_HOWTO.txt` for configuration details and the `MATLAB/` folder for export scripts
- For an easier-to-configure convolver, see [mcfx_mimoeq](#mcfx_mimoeq)

**Preset search paths:**

| Platform | Path |
|----------|------|
| Windows | `C:\Users\<username>\AppData\Roaming\mcfx\convolver_presets\` |
| macOS | `~/Library/mcfx/convolver_presets/` |
| Linux | `~/mcfx/convolver_presets/` |

---

### mcfx_anything

Transforms (almost) any audio plug-in into a multichannel plug-in.

- Scans and hosts VST2, VST3, and AU plug-ins; scanning runs out-of-process for speed and crash-resistance
- Runs as many instances as needed to cover all channels of the mcfx multichannel bus — one instance per stereo (or N-channel) pair
- All instances stay in sync: parameter changes on the master instance are automatically mirrored to all slave instances
- Exposes up to 256 host-automatable forwarding parameters so DAW automation works transparently across all instances
- Supports sidechain routing: any input channel can be routed to the plug-in's sidechain bus
- Saves and restores the full plug-in state (including which plug-in is loaded) with the DAW project

---

### mcfx_graph

Flexible plug-in graph / patchbay. Load VST2 / VST3 / AU plug-ins as nodes, wire them together with bezier connections, and build entire signal flows inside one mcfx_graph instance. Multiple connections feeding the same input are summed automatically.

- **Native nodes:** Gain, Mute / Phase invert, Matrix Mixer (NxM), Delay, and recursive Subgraph nodes
- **Editor:** drag-to-connect pins, multi-select + group drag, snapshot undo / redo, Cmd-scroll zoom, drag-drop JSON load/save
- **Hosting:** VST2 / VST3 / AU with out-of-process scanning shared with mcfx_anything; per-node channel count probed against the plug-in's actual accepted layouts
- **DAW automation:** 256 forwarding parameters dynamically bindable to any inner-plug-in parameter
- **State:** human-readable JSON, embedded in the DAW project state and exportable as a `.json` file

See [mcfx_graph/README.md](mcfx_graph/README.md) for the full feature list and the keyboard / mouse shortcut reference.

---

### mcfx_mimoeq

Multichannel MIMO (Multiple Input Multiple Output) parametric equalizer.

- Applies per-channel EQ on the diagonal (optionally restricted to a subset of channels) and per input-to-output path EQ chains for cross-channel processing
- Diagonal chain supports up to 24 automated IIR bands (HP, low shelf, peak, high shelf, LP) with host automation via VST3 parameters
- Individual input-to-output path chains for routing-aware corrections (e.g. speaker crosstalk compensation), supporting IIR, FIR (partitioned convolution), delay, and gain nodes
- Routing can be visualized as a matrix or wires view
- Built-in spectrum analyzer with per-channel or summed display
- Loads and saves configuration as JSON files (allows importing automated speaker/room EQ configurations for large multichannel installations)
- Built-in undo/redo
- Can also be seen as a more flexible `mcfx_convolver`, but for dense filter matrices `mcfx_convolver` will be more efficient

---

### mcfx_delay

Delays all channels by the same amount.

- Maximum delay time is set at compile time via `MAX_DELAYTIME_S` (default: 0.5 s)

---

### mcfx_filter

Applies identical filter settings to all channels, with a frequency analyzer showing the sum of all channels.

- Low/high pass: 2nd-order Butterworth or cascaded 4th-order (Linkwitz–Riley) for crossover use
- 2× parametric peak filters ±18 dB
- Low and high shelf filters ±18 dB
- All filter parameters can be adjusted during playback without audible glitches
- For a more flexible multichannel EQ, see [mcfx_mimoeq](#mcfx_mimoeq)

---

### mcfx_gain_delay

Per-channel gain and delay calibration tool, useful for multi-speaker setups.

- Individual gain and delay per channel with phase, solo, and mute buttons
- Built-in signal generator (sine, toneburst) for testing individual channels; sine frequency range down to 10 Hz
- Paste gain/delay values directly from the clipboard (semicolon, comma, newline, tab, or space separated)
- Maximum delay time set at compile time via `MAX_DELAYTIME_S`

---

### mcfx_meter

Multichannel level meter with RMS, peak, and peak hold.

---

### mcfx_send / mcfx_receive

Simply send multichannel audio via network, with low latency, auto-discovery and optional password protection.

Note: Don't use for sensitive audio content, there is no encryption for lowest CPU load and latency.

Stream multichannel audio between machines on a LAN over UDP. `mcfx_send` packetises its input bus (up to 64 channels via VST3, 128 in AU/Standalone) as PCM 16-bit, 24-bit, or 32-bit float and unicasts it to one or more `mcfx_receive` instances; `mcfx_receive` decodes incoming streams and mixes them into its output bus through an adaptive jitter buffer and per-peer variable-ratio resampler that keeps it phase-locked to each sender even when the hardware audio clocks drift. Peers find each other automatically via Bonjour (or by typing IP and port directly), connections are symmetric and gated by an INVITE handshake with an optional shared password, and packets are marked DSCP-EF / WMM Voice for high-priority delivery on Wi-Fi and managed networks. See [mcfx_send/README.md](mcfx_send/README.md) and [mcfx_receive/README.md](mcfx_receive/README.md) for protocol and tuning details.

---

## Prerequisites for Building

CMake and a working build environment are required.

**Linux** — install dependencies (Debian/Ubuntu):

```bash
sudo apt install libasound2-dev libjack-jackd2-dev \
    ladspa-sdk \
    libcurl4-openssl-dev \
    libfreetype-dev libfontconfig1-dev \
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
    libwebkit2gtk-4.1-dev \
    libglu1-mesa-dev mesa-common-dev
```

> On older Ubuntu/Debian where `libwebkit2gtk-4.1-dev` is not available, substitute `libwebkit2gtk-4.0-dev` — JUCE 8 loads whichever is present at runtime. Likewise, fall back to `libfreetype6-dev` if `libfreetype-dev` is unavailable. If you intend to enable `WITH_ZITA_CONVOLVER` (off by default), also install `libzita-convolver-dev` and `libfftw3-dev` (zita brings its own FFTW dependency).

**Windows x64** — no extra setup. The FFT used by `mcfx_convolver`, `mcfx_filter`, and `mcfx_mimoeq` comes from the in-tree [chowdsp_fft](https://github.com/Chowdhury-DSP/chowdsp_fft) submodule (a fork of pffft, BSD-style licence) — statically linked, with runtime AVX/SSE dispatch. macOS uses Apple Accelerate (vDSP) instead, same as before.

Clone the repository including submodules:

```bash
git clone --recurse-submodules https://github.com/kronihias/mcfx/
```

---

## How to Build

Use **cmake-gui** or **cmake/ccmake** from the terminal.

1. Create a build folder inside the `mcfx` directory:
    ```bash
    mkdir BUILD
    ```

2. Configure with CMake (terminal):
    ```bash
    cd BUILD
    ccmake ..
    ```
    Press `C` to configure. When using the GUI, select your target build system (e.g. Visual Studio or MinGW on Windows).

3. Set the [Build Parameters](#build-parameters), configure again, then generate.

4. Build with your chosen build system. Example for Linux:
    ```bash
    make -j$(nproc) config=Release
    ```

5. After a successful build, find the binaries in `BUILD/vst/`, `BUILD/vst3/`, `BUILD/au/`, `BUILD/standalone/`, or `BUILD/lv2/` (depending on which formats you enabled) and copy them to your system plug-in folder:

    | Format | macOS | Windows | Linux |
    |--------|-------|---------|-------|
    | VST2 | `/Library/Audio/Plug-Ins/VST` | `C:\Program Files\Common Files\VST2` | `~/.vst/` or `/usr/local/lib/lxvst` |
    | VST3 | `/Library/Audio/Plug-Ins/VST3` | `C:\Program Files\Common Files\VST3` | `/usr/lib/vst3/` |

---

## Build Parameters

| Parameter | Description | Default |
|-----------|-------------|---------|
| `NUM_CHANNELS` | Channel count for the per-channel VST2 build | `36` |
| `MCFX_MAX_CHANNELS` | Max channel count for the single-binary multichannel VST3/AU/Standalone build | `128` |
| `MAX_DELAYTIME_S` | Maximum delay time for `mcfx_delay` (seconds) | `0.5` |
| `BUILD_VST` | Build VST2 plugins (requires `VST2SDKPATH`) | **`ON`** |
| `BUILD_VST3` | Build VST3 plugins | `OFF` |
| `BUILD_AU` | Build AU plugins (macOS only) | `OFF` |
| `BUILD_STANDALONE` | Build standalone applications | `OFF` |
| `BUILD_LV2` | Build LV2 plugins | `OFF` |
| `MCFX_BUILD_VST2_PER_CHANNEL` | Build per-channel VST2 variants (legacy) | `ON` |
| `MCFX_BUILD_MC` | Build single multichannel VST3/AU/Standalone | `ON` |
| `VST2SDKPATH` | Path to the VST2 SDK | `~/SDKs/vstsdk2.4` |
| `WITH_ZITA_CONVOLVER` | Use zita-convolver for better Linux performance (Linux only) | `OFF` |

> Note: VST3 is the recommended format for new projects but currently defaults to OFF — pass `-DBUILD_VST3=ON` when configuring.

---

## Changelog

### 0.8.4 (2026-05-17)

- `mcfx_mimoeq`: add advanced iir filters, add linearphase fir designer, add phase display
- `mcfx_mimoeq`, `mcfx_graph`: add preset factory, some gui improvements
- `mcfx_graph`: allow to rename nodes


### 0.8.3 (2026-05-07)

- mcfx_graph/anything: make sure plugin scanner is included in win build as well


### 0.8.2 (2026-05-07)

- mcfx_graph/anything: make sure plugin scanner is included in build, avoid having plugin scanner ghost processes, include input/output nodes in marque selection

- mcfx_send/receive: auto-reconnect when reloading session, report IP address in GUI, avoid duplicate entries in connection list

### 0.8.1 (2026-05-04)

- mcfx_graph: build for windows, add copy/paste, fix vst3 channel count selection, make json files portable by not saving the absolute plugin path, save/recall input/output node positions, add scrolling to matrix parameter view
- mcfx_convolver: add individual ir .wav export to inspector

### 0.8.0 (2026-05-03)

- improved `mcfx_anything` robustness, disable editing of children's GUI - all parameters should stay in sync with the main plugin instance. use `mcfx_graph` if you want to have different parameters for each plugin instance.
- **New:** (beta version) `mcfx_graph` — flexible plug-in graph / patchbay. Host any number of VST2/VST3/AU plug-ins as nodes, connect them with wires, summing on shared inputs, with native gain / mute-phase / matrix mixer / delay nodes, nested subgraphs, multi-select + chain-connect, undo/redo, drag-drop JSON load/save, 256 DAW-automatable forwarding parameters, and a searchable plug-in selector with format filters
- **New:** (beta version) `mcfx_send` and `mcfx_receive` - send/receive multichannel audio via local network, with low latency and auto-discovery - for really fast setup eg. in computer music ensembles, spatial audio concerts, ...
- Refactor: out-of-process plug-in scanner code (`OutOfProcessPluginScanner.h`, `findScannerExecutable`, scanner `Main.cpp`) moved to `common/PluginHost/` and is shared between `mcfx_anything` and `mcfx_graph`

### 0.7.0 (2026-04-19)

> *Dedicated to Angelo Farina, whose pioneering work on multichannel audio, Ambisonics, and room acoustics measurement was a great inspiration for these plug-ins.*

- `mcfx_convolver`: performance optimizations (avoid allocation in audio callback, convolver engine improvements) - thanks to Angelo Farina, Luca Battisti and Domenico Stefani
- **New:** `mcfx_anything` — transforms (almost) any plug-in into a multichannel plug-in via multi-instance hosting, parameter sync, DAW automation forwarding, and out-of-process scanning
- **New:** `mcfx_mimoeq` — multichannel MIMO parametric filter (IIR, FIR, delay, gain) with diagonal and per-path chains, up to 24 automated bands, spectrum analyzer, and JSON preset support
- `mcfx_delay`: fix delay time rounding issue
- VST3 plug-ins now automatically adjust to the track's channel count — one binary per plug-in, up to 64 channels (AU and VST2 support up to 128); VST3 is recommended going forward
- update to JUCE 8

### 0.6.4 (2024-03-20)

`mcfx_convolver`: add master gain parameter and rotary control, mechanism to save channel count in WAV IR files, fix debug window. Add build and installer creation scripts.

### 0.6.3 (2023-12-21)

`mcfx_filter`: fix parameter smoothing to avoid instabilities and glitches while changing filter parameters; performance optimizations.

### 0.6.2 (2023-12-11)

Add 128-channel version of all plug-ins; adjust `mcfx_meter` and `mcfx_gain_delay` GUI to display 128 channels properly.

### 0.6.1 (2023-12-08)

`mcfx_convolver`: support loading `.wav` files directly (a `.conf` file is written to disk and loaded in the background); support drag/drop of `.wav` or `.conf` files onto the GUI.

### 0.6.0 (2023-04-16)

New builds optimized for Apple Silicon and 64-bit Intel Mac; Windows 64-bit. Update to JUCE 7; removed soxr dependency. `mcfx_gain_delay`: sine generator now starts at 10 Hz.

### 0.5.11 (2020-05-20)

`mcfx_convolver`: fix +6 dB gain on the macOS version (Windows was correct). **Note:** macOS projects using this plug-in will output 6 dB less than with older versions.

### 0.5.10 (2020-05-19)

`mcfx_filter`: High-Shelf Q was not stored in the plug-in state — fixed.

### 0.5.9 (2020-02-05)

`mcfx_convolver`: fix dropouts/artifacts for hosts sending incomplete block sizes (e.g. Adobe, Steinberg); fix reloading stored presets; add filter length and latency debug messages; fix GUI crash in Adobe hosts.

### 0.5.8 (2020-01-31)

`mcfx_convolver`: option to store preset inside the DAW project; allow exporting the stored preset as a `.zip` file.

### 0.5.7 (2019-04-28)

`mcfx_convolver`: OSC receive support (`/reload`, `/load <preset.conf>`); configurable OSC port in GUI.

### 0.5.6 (2019-03-20)

`mcfx_convolver`: maintain FIR filter gain when resampled; add plug-in parameter to trigger preset reload.

### 0.5.5 (2018-03-16)

`mcfx_filter`, `mcfx_gain_delay`, `mcfx_delay`: improved slider behavior for finer control. `mcfx_gain_delay`: Ctrl+click for exclusive solo/phase/mute; add toneburst to signal generator; fix saving channel state of signal generator.

### 0.5.4 (2017-05-20)

`mcfx_convolver`, `mcfx_filter`: fix thread-safety to avoid startup crash when other plug-ins use FFTW.

### 0.5.3 (2017-05-02)

`mcfx_delay`, `mcfx_gain_delay`: fix glitch in delay line.

### 0.5.2 (2017-03-20)

Various bugfixes. `mcfx_convolver`: performance optimizations; adjustable maximum partition size.

### 0.5.1 (2016-04-25)

`mcfx_convolver`: fix bug loading packed (dense) matrix. `mcfx_gain_delay`: GUI fix.

### 0.5.0 (2016-04-08)

Add signal generator to `mcfx_gain_delay`. `mcfx_convolver`: support packed WAV file for dense FIR matrix (see `CONVOLVER_CONFIG_HOWTO.txt`). `mcfx_filter`: smooth IIR filter to avoid clicks on parameter changes.

### 0.4.2 (2016-02-19)

Fix convolver bug.

### 0.4.1 (2016-02-17)

Fix convolver bug causing mixed-up partitions.

### 0.4.0 (2015-11-04)

GUI for `mcfx_delay` and `mcfx_filter` (with FFT analyzer). Add phase, solo, and mute buttons to `mcfx_gain_delay`.

### 0.3.3 (2015-07-19)

Performance improvements for `mcfx_convolver`.

### 0.3.2 (2014-12-28)

AudioMulch compatibility. GUI for `mcfx_gain_delay` with paste-from-clipboard. `mcfx_meter`: add scale offset.

### 0.3.1 (2014-06-16)

Fix VST ID for Bidule compatibility.

### 0.3 (2014-03-15)

Add `mcfx_convolver`.

### 0.2 (2014-02-25)

Remove license-incompatible code; JUCE update.

### 0.1 (2014-01-10)

First release.

---

&copy; 2013–2026 Matthias Kronlachner — m.kronlachner@gmail.com
