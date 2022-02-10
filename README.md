X-MCFX-Convolver and suite
==============
new multichannel convolver algorithm and matrix loading procedure
------------------------------------

- X-MCFX plugin suite contains an alternative version of the mcfx-convolver plugin included in the suite developed by Kronlachner at https://github.com/kronihias/mcfx

- X-MCFX-Convolver involves a new matrix of filters loading mode which do not necessitates any external configuration files

- In X-MCFX-Convolver has been upgraded the performances of the non-uniform partition algorithm

- The entire suite has been upgraded to be compiled with the last version of JUCE Framework (www.juce.com, GPLv3), which includes the natively support to CMake (www.cmake.org)

- Cross-platform multiformat for MacOS and Windows (VST2 and VST3). Linux not tested

- Binaries of external libraries (statically compiled) included in the repository
    + libsoxr (LGPL, http://soxr.sourceforge.net) both for Windows and MacOS
    + fftw3 (http://www.fftw.org/) convolver library for Windows
    
- Plug-ins channel configuration editabe at compile stage

License
--------------

X-MCFX is free software and licensed under the GNU General Public License version 3 (GPLv3).

Prerequisites for building
--------------

- An installed CMake build (GUI version also works good) min 3.15
- An Installed IDE (Visual Studio for Windows or XCode for MacOS)
- VST2 SDK for VST2 binaries
- ASIO SDK for Windows standalone

How-to build
--------------

- Open CMake GUI and select this repository folder as source code
- Design a build folder of your choice
- Generate CMake configuration for your chosen IDE
- Design the channel configuration with the following variables:
    + NUM_IN_CHANNELS
    + NUM_OUT_CHANNELS
    + LAST_2CHARS_PLUGINCODE
this last variable is an univoque code acts for the plug-in to be shown as individual by host applications. Choose it differently for any channel configuration you want to compile
- Design the VST2 SDK folder (and ASIO SDK folder on Windows) if different from default (../\*USER_FOLDER\*/SDKs/VST_SDK/VST2_SDK for MacOS
 ../\*CMAKE_SOURCE_DIR\*/SDKs/VST2_SDK for Windows)
- Plug-in format flags have been inserted on CMake entries. You can choose the only VST3 version to be compiled so no any further SDK will be necessary

Variant description
==============

X-MCFX_Convolver
--------------
+ Multichannel convolver will now accept "packed" filters matrix version in single wavefile.
+ It will be necessary to specify just the input channels number of the matrix in loading phase. The value will be stored in the host project saving settings
+ The input channels value can be also stored within the metadata of the filters wavefile through a specific option that will export the new wave file version
+ Partitioning scheme has been upgraded to a more efficient version and tested with benchmarks

Other plug-ins of the suite
-----------------------------
+ After the JUCE upgrade some of them have been succesfully tested while others dont. They have been actually maintained for completeness of the suite but not all of them will probably work


changelog
==============
- 1.0.4 (2021-11-14) plug-in external parameters control (bugfixes)
- 1.0.2 (2021-01-15) metadata option restored
- 1.0.0 (2020-11-12) first stable version
