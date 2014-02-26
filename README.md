mcfx - multichannel cross plattform audio plug-in suite
==============

- mcfx is a suite of multichannel vst plug-ins or standalone applications (currently meter only)

- channel count is configurable with compile time flag

- cross plattform VST for MacOSX, Windows and Linux

- uses the JUCE framework (www.juce.com, GPLv3)

- ready to use binaries for MacOSX (> 10.5, 32/64 bit) and Windows (32/64 bit) can be found at http://www.matthiaskronlachner.com/?p=1910

- these plug-ins have been developed to be used with the ambiX Ambisonic plug-ins: http://www.matthiaskronlachner.com/?p=2015

license
--------------

mcfx is free software and licensed under the GNU General Public License version 3 (GPLv3).

Please note that Steinbergs VST SDK is not free in the sense of the Free Software Foundation, but is the de facto standard for creating audio plug-ins. Don't use the VST version of this software if you are not comfortable with mixing free and non-free code.

prerequisites for building
--------------

- cmake, working build environment
- Steinberg VST 2.4 SDK

Install LINUX Libraries (Debian, Ubuntu):
*$ sudo apt-get install libasound-dev libfreetype6-dev libgl1-mesa-dev libx11-dev libxext-dev libxinerama-dev libxcursor-dev freeglut3-dev libxmu-dev libxi-dev*

howto build yourself:
--------------

copy the Steinberg VST 2.4 SDK into the folder *mcfx/vstsdk2.4* (do to legal reasons those can not be included here)

- use cmake gui or cmake/ccmake from terminal:

- *NUM_CHANNELS* adjusts the number of input/output channels

**TERMINAL:**

- create a folder in the *mcfx* folder eg. *BUILD*

*mcfx/BUILD> $ ccmake ..*

- adjust NUM_CHANNELS 

then
*mcfx/BUILD> $ make*

- find the binaries in the *mcfx/BUILD/_bin* folder and copy to system VST folder

**VST installation folders:**


- MacOSX: /Library/Audio/Plug-Ins/VST or ~/Library/Audio/Plug-Ins/VST
- Windows: eg. C:\Programm Files\Steinberg\VstPlugins
- Linux: /usr/lib/lxvst or /usr/local/lib/lxvst

plug-ins explained:
==============

mcfx_delay
--------------
delay each channel about the same time (maximum delay time in seconds is a compile time flag (default 0.5s): MAX_DELAYTIME_S)


mcfx_filter
--------------
filter each channel with the same low/high cut, peak filter and high/low shelf filter settings

low and high pass: 2nd order butterworth filter or 2x 2nd order butterworth cascaded (resulting in linkwitz riley characteristic) for use as x-over network
plus 2x parametric filter +- 18dB
plus low and high shelf filters +- 18dB


mcfx_gain_delay
--------------
set different delay time and gain setting for each channel (good for multispeaker calibration)
maximum delay time in seconds is a compile time flag (MAX_DELAYTIME_S)

mcfx_meter
--------------

multichannel level meter with RMS, peak and peak hold


changelog:
==============

- 0.2 (2014-02-25) - removed some license incompatible code, juce update

- 0.1 (2014-01-10) - first release 

______________________________
(C) 2013 Matthias Kronlachner
m.kronlachner@gmail.com
