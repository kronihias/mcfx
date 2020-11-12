X-MCFX-Convolver and suite
==============
new non-uniform partitioned multichannel convolver and other audio mutichannel plug-ins
------------------------------------

- X-MCFX plugin suite contains an alternative version of the mcfx-convolver plugin included in the suite developed by Kronlachner at https://github.com/kronihias/mcfx

- X-MCFX-Convolver involves a new matrix of filters loading mode which do not necessitates any external configuration files

- In X-MCFX-Convolver has been upgraded the performances of the non-uniform partition scheme

- Entire suite has been upgraded to be compiled with the last version of JUCE Framework (www.juce.com, GPLv3), which includes the completely support to CMake (www.cmake.org)

- Cross-platform multiformat for MacOS and Windows (VST2 and VST3)

- Binaries of external libraries included in the repo 
    + libsoxr (LGPL, http://soxr.sourceforge.net) both for Windows and MacOS
    + fftw3 (http://www.fftw.org/) convolver library for Windows
    
- Plug-ins channel configuration editabe at compile stage

License
--------------

X-MCFX is free software and licensed under the GNU General Public License version 3 (GPLv3).

Prerequisites for building
--------------

- An installed CMake build (GUI version also works good) min 3.15
- An Installed system based develope environment (Visual Studio for Windows or XCode for MacOS)
- VST2 SDK for VST2 binaries
- ASIO SDK for Windows standalone

How-to build
--------------

- Open CMake GUI and select this repo folder as source code
- Design a build folder of your choice
- Generate CMake configuration for your system environment
- Design the channel configuration with the following variables:
    + NUM_IN_CHANNELS
    + NUM_OUT_CHANNELS
    + LAST_2CHARS_PLUGINCODE*
this last point an univoque code act for each plug-in channel configuration to be shown as individual by host applications. Choose it differently for any channel setup you want to compile
- Design the VST2 SDK folder (and ASIO SDK folder on Windows) if different from default (../\*USER_FOLDER\*/SDKs/VST_SDK/VST2_SDK)
- Plug-in format flags have been inserted on CMake entries. One can choose the only VST3 version to be compiled as any further SDK won't be necessary

Variant description
==============

X-MCFX_Convolver
--------------
+ Multichannel convolver will now accept "packed" filters matrix version in single wavefile.
+ It will be necessary to insert just the input channels number of the matrix in loading phase. The value will be stored in the host project savings
+ The input channels value can be also stored within the metadata of the filters wavefile through a specific option which will be export the new version file
+ Partitioning scheme has been upgraded to a more efficient version and tested with benchmarks

Other plug-ins of the suite
-----------------------------
+ After the JUCE upgrade some of them have been succesfully test while others dont. They have been actually maintained for completeness of the suite but not all of them will probably work


changelog
==============
- 0.5.11 (2020-05-20) - mcfx_convolver - Mac OS version added a +6dB gain to the filtered output, this is fixed now (Windows version was correct) -> this might influence old projects under OSX since mcfx_convolver will output 6dB less than older versions!

- 1.0.0 (2020-11-12) first stable version
