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
  - [mcfx_mimoeq](#mcfx_mimoeq)
  - [mcfx_delay](#mcfx_delay)
  - [mcfx_filter](#mcfx_filter)
  - [mcfx_gain_delay](#mcfx_gain_delay)
  - [mcfx_meter](#mcfx_meter)
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

## Prerequisites for Building

CMake and a working build environment are required.

**Linux** — install dependencies (Debian/Ubuntu):

```bash
sudo apt install libasound2-dev libjack-jackd2-dev \
    ladspa-sdk \
    libcurl4-openssl-dev \
    libfreetype6-dev \
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
    libwebkit2gtk-4.0-dev \
    libglu1-mesa-dev mesa-common-dev \
    libfftw3-dev \
    libzita-convolver3 \
    libzita-convolver-dev
```

**Windows x64** — download [FFTW for Windows](https://www.fftw.org/install/windows.html) (64-bit) and generate the import library from the `x64 Native Tools Command Prompt for VS`:

```
lib /machine:x64 /def:libfftw3f-3.def
```

> Note: running `lib /def:libfftw3f-3.def` from the default Developer Command Prompt generates a 32-bit library and will fail to link.

Use fftw-3.3.6-pl2 or later — earlier versions lack `fftwf_make_planner_thread_safe()`.

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

5. After a successful build, find the binaries in `BUILD/_bin/`, `BUILD/vst/`, or `BUILD/vst3/` and copy them to your system plug-in folder:

    | Format | macOS | Windows | Linux |
    |--------|-------|---------|-------|
    | VST2 | `/Library/Audio/Plug-Ins/VST` | `C:\Program Files\Common Files\VST2` | `~/.vst/` or `/usr/local/lib/lxvst` |
    | VST3 | `/Library/Audio/Plug-Ins/VST3` | `C:\Program Files\Common Files\VST3` | `/usr/lib/vst3/` |

---

## Build Parameters

| Parameter | Description | Default |
|-----------|-------------|---------|
| `NUM_CHANNELS` | Number of input/output channels per plugin | `36` |
| `MAX_DELAYTIME_S` | Maximum delay time for mcfx_delay (seconds) | `0.5` |
| `BUILD_STANDALONE` | Build standalone applications | `OFF` |
| `BUILD_VST3` | Build VST3 plugins | **`ON`** |
| `BUILD_VST2` | Build VST2 plugins | `OFF` |
| `BUILD_LV2` | Build LV2 plugins | `OFF` |
| `VST2SDKPATH` | Path to the VST2 SDK | `~/SDKs/vstsdk2.4` |
| `WITH_ZITA_CONVOLVER` | Use zita-convolver for better Linux performance | `OFF` |

**FFTW paths** (set manually if not found automatically):

- **`FFTW3F_LIBRARY`** — path to the FFTW3F library file (note the trailing **f**):
  - Linux: `/usr/lib/x86_64-linux-gnu/libfftw3f-3.so`
  - Windows/MSVC: `C:/Program Files/fftw-3.3.6-pl2-dll64/libfftw3f-3.lib`
  - Windows/MinGW: `C:/Program Files/fftw-3.3.6-pl2-dll64/libfftw3f-3.dll`
- **`FFTW3_INCLUDE_DIR`** — path to the FFTW3 include directory, e.g. `/usr/include`
- **`FFTW3F_THREADS_LIBRARY`** *(Linux only)* — e.g. `/usr/lib/x86_64-linux-gnu/libfftw3f_threads.so`

---

## Changelog

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
