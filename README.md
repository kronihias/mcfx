mcfx - multichannel cross platform audio plug-in suite
==============

- mcfx is a suite of multichannel vst plug-ins or standalone applications (standalone currently meter and convolver only)

- channel count is configurable with compile time flag

- cross plattform VST for MacOSX, Windows and Linux

- uses the JUCE framework (www.juce.com, GPLv3), libsoxr (LGPL, http://soxr.sourceforge.net)

- ready to use binaries for MacOSX (> 10.5, 32/64 bit) and Windows (32/64 bit) can be found at http://www.matthiaskronlachner.com/?p=1910

- these plug-ins have been developed to be used with the ambiX Ambisonic plug-ins: http://www.matthiaskronlachner.com/?p=2015

license
--------------

mcfx is free software and licensed under the GNU General Public License version 3 (GPLv3).

prerequisites for building
--------------

CMake, working build environment

- **Linux:**  
    Install LINUX Libraries (Debian, Ubuntu):

    ```
    sudo apt install libasound2-dev libjack-jackd2-dev \
        ladspa-sdk \
        libcurl4-openssl-dev  \
        libfreetype6-dev \
        libx11-dev libxcomposite-dev libxcursor-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
        libwebkit2gtk-4.0-dev \
        libglu1-mesa-dev mesa-common-dev \
        libfftw3-dev \
        libzita-convolver3 \
        libzita-convolver-dev
    ```

- **Windows x64:**  
    Download [FFTW](https://www.fftw.org/) for [Windows](https://www.fftw.org/install/windows.html) 64bit and run the command `lib /machine:x64 /def:libfftw3f-3.def` from the Visual Studio Developer Command Prompt to generate *libfftw3f-3.lib*.  
    Alternatively, run the command `lib /def:libfftw3f-3.def` from the `x64 Native Tools Command Prompt for VS`.
    Note that this simpler command, instructed by the FFTW documentation, will generate a 32bit library when called from the default Developer Command Prompt or Powershell (and therefore fail to link)

Make sure of using fftw-3.3.6-pl2 or later, as earlier versions do not contain the `fftwf_make_planner_thread_safe()` function.

Clone the repository with the submodules:
```
git clone --recurse-submodules https://github.com/kronihias/mcfx/
```



Howto build yourself:
--------------

Use **cmake gui** or **cmake/ccmake** from terminal to configure the build.

1. Create a folder in the *mcfx* folder eg. *BUILD*
    ```
    mkdir BUILD # On Linux, MacOS or Windows Powershell    
    ```
2. Run cmake or ccmake from the *BUILD* folder, or use the cmake gui and press *Configure*.  
    In the terminal:
    ```
    cd BUILD
    ccmake ..
    ```
    Press the *C* key.
      
    If using the GUI instead, select the target build system (e.g., in Windows use Visual Studio or MinGW).
3. Set the [Build Parameters](#build-parameters),  *Configure* again. Then *Generate*.
4. Build the project with the selected build system (e.g., Visual Studio, Xcode, or make).  
    E.g., for Linux:
    ```
    make -j$(nproc) config=Release
    ```
    `-j$(nproc)` will instruct make to use all available cores for the build.
5. After a successful build, find the binaries in `BUILD/_bin/` or `BUILD/vst/` and copy to system VST folder  

    **VST installation folders:**


    - MacOSX: `/Library/Audio/Plug-Ins/VST`
    - Windows: `C:\Program Files\Steinberg\VstPlugins`, `C:\Program Files\Common Files\VST2`, or `C:\Program Files\VSTPlugins`
    - Linux: `~/.vst/`, `/usr/lib/lxvst` or `/usr/local/lib/lxvst`

## Build Parameters

- **``NUM_CHANNELS``** - Number of input/output channels for each plugin. Default is 36.
- **``MAX_DELAYTIME_S``** - Maximum delay time for mcfx_delay in seconds. Default is 0.5s.
- **``BUILD_STANDALONE``** - Build standalone applications. Default is OFF.
- **``BUILD_VST2``** - Build VST2 plugins. Default is OFF.
- **``BUILD_VST3``** - Build VST3 plugins. Default is ON.
- **``BUILD_LV2``** - Build LV2 plugins. Default is ON.
- **``VST2SDKPATH``** - Path to the VST2 SDK. Default is "~/SDKs/vstsdk2.4"
- **``WITH_ZITA_CONVOLVER``** - Build with zita-convolver (better performance under Linux). Default is OFF.
---
- **``FFTW3F_LIBRARY``** - Path to the FFTW3F library file (Note the final **f**!).  
    - On Linux this should point to the `.so` shared library file, e.g., `/usr/lib/x86_64-linux-gnu/libfftw3f-3.so`.
    - On Windows with Visual Studio, this should point to the `.lib` file generated in a [previous step](#prerequisites-for-building), e.g., `C:/Program Files/fftw-3.3.6-pl2-dll64/libfftw3f-3.lib`.
    - On Windows with MinGW, this should point to the `.dll` dynamic library file, e.g., `C:/Program Files/fftw-3.3.6-pl2-dll64/libfftw3f-3.dll`.
- **``FFTW3F_INCLUDE_DIR``** - Path to the FFTW3F include directory, e.g., `/usr/include`.
- [**``FFTW3F_THREADS_LIBRARY``**] - If present (e.g., on Linux), this should point to the `.so` file for the FFTW3F threads library, e.g., `/usr/lib/x86_64-linux-gnu/libfftw3f_threads.so`.

plug-ins explained:
==============

mcfx_convolver
--------------
multichannel convolution matrix

loads configuration files (compatible to jconvolver .conf files)

supports loading `.wav` files directly (optionally reading input channel metadata)

just drag/drop a `.conf` or `.wav` file into the GUI to load it

have a look at `CONVOLVER_CONFIG_HOWTO.txt` for details about the configuration files

check the `MATLAB` folder for simple export scripts

searches for configuration file in following folders:
- Windows 7,8: `C:\Users\username\AppData\Roaming\mcfx\convolver_presets\`
- MacOS: `~/Library/mcfx/convolver_presets/`
- Linux: `~/mcfx/convolver_presets/`


mcfx_delay
--------------
delay each channel about the same time (maximum delay time in seconds is a compile time flag (default 0.5s): MAX_DELAYTIME_S)


mcfx_filter
--------------
filter each channel with the same low/high cut, peak filter and high/low shelf filter settings, frequency analyzer that displays a sum of all channels

low and high pass: 2nd order butterworth filter or 2x 2nd order butterworth cascaded (resulting in linkwitz riley characteristic) for use as x-over network

plus 2x parametric filter +- 18dB

plus low and high shelf filters +- 18dB

filter parameters can be adjusted during playback without introducing audible glitches (except switching lp/hp bypass and order)


mcfx_gain_delay
--------------
set different delay time and gain setting for each channel (good for multispeaker calibration), includes a signal generator for testing individual channels

the GUI allows to paste a list of float gain and delay values from the clipboard with semicolon, comma, newline, tab or space separated.

maximum delay time in seconds is a compile time flag (MAX_DELAYTIME_S)

mcfx_meter
--------------

multichannel level meter with RMS, peak and peak hold


changelog:
==============
- 0.6.4 (2024-03-20) - mcfx_convolver: add master gain parameter and rotary control, add mechanism to save channel count in wav IR files, fix debug window. Add build and installer creation scripts.

- 0.6.3 (2023-12-21) - mcfx_filter: fix parameter smoothing to avoid instabilities and glitches while changing filter parameters, performance optimizations.

- 0.6.2 (2023-12-11) - add 128 channel version of all plugins - since REAPER does allow for 128 channels per track, adjust mcfx_meter and mcfx_gain_delay GUI to display 128 channels properly

- 0.6.1 (2023-12-08) - mcfx_convolver: support loading `.wav` files (a `.conf` file will be written to disk and loaded in the background!), support drag/drop `.wav` or `.conf` files onto the GUI for loading

- 0.6.0 (2023-04-16) - new builds optimized for Apple Silicon and 64 bit Intel Mac; Win 64 bit; Update to JUCE 7; removed soxr dependency to simplify build; mcfx_gain_delay: allow sine generator to start at f=10 Hz

- 0.5.11 (2020-05-20) - mcfx_convolver - Mac OS version added a +6dB gain to the filtered output, this is fixed now (Windows version was correct) -> this might influence old projects under OSX since mcfx_convolver will output 6dB less than older versions!

- 0.5.10 (2020-05-19) - mcfx_filter - High-Shelf Q was not stored in the plugin state, this is fixed now

- 0.5.9 (2020-02-05) - mcfx_convolver - fix dropouts/artifacts for hosts that send incomplete block sizes (eg. Adobe, Steinberg), fix reloading of stored presets, add filter length and latency debug messages, fix gui crash in Adobe hosts

- 0.5.8 (2020-01-31) - mcfx_convolver - option to store preset within the project -> allows to exchange a DAW (eg. Reaper) project without need to provide the preset files extra, allow to export stored preset as .zip file for recovering it from the project

- 0.5.7 (2019-04-28) - mcfx_convolver - osc receive support: /reload, /load <preset.conf> -> allows remote control of reloading/loading presets, port can be set in GUI

- 0.5.6 (2019-03-20) - mcfx_convolver - maintain fir filter gain if resampled, add plugin parameter to trigger reload of configuration

- 0.5.5 (2018-03-16) - filter, gain_delay, delay: slider behavior changed for more accurate control; gain_delay: ctrl+click for exclusive solo/phase/mute, add toneburst for signalgenerator, bugfix saving channel state of signalgenerator

- 0.5.4 (2017-05-20) - mcfx_convolver and mcfx_filter: fixed threadsafety to avoid startup crash if other plugins use fftw

- 0.5.3 (2017-05-02) - mcfx_delay and mcfx_gain_delay fixed glitch in delayline

- 0.5.2 (2017-03-20) - various bugfixes; mcfx_convolver: performance optimizations, adjustable maximum partition size

- 0.5.1 (2016-04-25) - mcfx_convolver: fixed bug in loading packed (dense) matrix; mcfx_gain_delay gui fix

- 0.5.0 (2016-04-08) - add signal generator to mcfx_gain_delay; convolver: support for packed wav file to load a dense FIR matrix from only one .wav file -> have a look at CONVOLVER_CONFIG_HOWTO.txt; filter: smooth iir filter to avoid clicks when parameters change

- 0.4.2 (2016-02-19) - fixed one more convolver bug

- 0.4.1 (2016-02-17) - fixed convolver bug which resulted in mixed up partitions

- 0.4.0 (2015-11-04) - gui for mcfx_delay, gui for mcfx_filter (with fft analyzer), added phase, solo and mute buttons to mcfx_gain_delay

- 0.3.3 (2015-07-19) - performance improvements for mcfx_convolver
_
- 0.3.2 (2014-12-28) - audiomulch compatibility, gui for mcfx_gain_delay with paste from clipboard functionality, mcfx_meter added scale offset

- 0.3.1 (2014-06-16) - fixed vst id for bidule compatibility

- 0.3 (2014-03-15) - added mcfx_convolver

- 0.2 (2014-02-25) - removed some license incompatible code, juce update

- 0.1 (2014-01-10) - first release 

______________________________
(C) 2013-2017 Matthias Kronlachner
m.kronlachner@gmail.com