# Example Audio Plugin CMakeLists.txt

# To get started on a new plugin, copy this entire folder (containing this file and C++ sources) to
# a convenient location, and then start making modifications.

# The first line of any CMake project should be a call to `cmake_minimum_required`, which checks
# that the installed CMake will be able to understand the following CMakeLists, and ensures that
# CMake's behaviour is compatible with the named version. This is a standard CMake command, so more
# information can be found in the CMake docs.

cmake_minimum_required(VERSION 3.15)

# The top-level CMakeLists.txt file for a project must contain a literal, direct call to the
# `project()` command. `project()` sets up some helpful variables that describe source/binary
# directories, and the current project version. This is a standard CMake command.

################################################
#version
file(READ VERSION VERSION)


project(MCFX_PLUGIN_SUITE VERSION ${VERSION})

################################################
set(CMAKE_BUILD_TYPE Release)
# ADD_DEFINITIONS(-DNDEBUG=1)
# ADD_DEFINITIONS(-D_NDEBUG=1)

#save path for later
set (SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set (BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})

################################################
#choices at compile time
set(NUM_CHANNELS 36 CACHE STRING "number of channels for each plugin")
set(MAX_DELAYTIME_S 0.5 CACHE STRING "maximum delaytime for mcfx_delay in seconds")
set(BUILD_STANDALONE TRUE CACHE BOOL "build standalone application (for those with editor)")

add_definitions(-DVERSION=${VERSION}) #?? FOR THE DELAY
add_definitions(-DMAX_DELAYTIME_S=${MAX_DELAYTIME_S})

################################################
# additional cmake find packages
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${SRC_DIR}/cmake)

find_package(libsoxr REQUIRED)

IF (WITH_ZITA_CONVOLVER OR WIN32)
	find_package(FFTW3 COMPONENTS single)
ENDIF (WITH_ZITA_CONVOLVER OR WIN32)

IF(UNIX AND NOT APPLE AND NOT ANDROID)
	find_package(FFTW3 COMPONENTS single threads)
ENDIF(UNIX AND NOT APPLE AND NOT ANDROID)

include_directories(${EIGEN3_INCLUDE_DIR} ${FFTW3_INCLUDE_DIR} ${LIBLO_INCLUDE_DIRS} ${LIBSOXR_INCLUDE_DIRS} ${VSTSDK_PATH})

################################################
# Different convolver library under Linux systems
IF(UNIX AND NOT APPLE AND NOT ANDROID)
	SET(WITH_ZITA_CONVOLVER FALSE CACHE BOOL "build with zita-convolver (better performance under linux)")
ENDIF(UNIX AND NOT APPLE AND NOT ANDROID)

# If you've installed JUCE somehow (via a package manager, or directly using the CMake install
# target), you'll need to tell this project that it depends on the installed copy of JUCE. If you've
# included JUCE directly in your source tree (perhaps as a submodule), you'll need to tell CMake to
# include that subdirectory as part of the build.

# find_package(JUCE CONFIG REQUIRED)        # If you've installed JUCE to your system
# or
add_subdirectory(JUCE)                    # If you've put JUCE in a subdirectory called JUCE

# If you are building a VST2 or AAX plugin, CMake needs to be told where to find these SDKs on your
# system. This setup should be done before calling `juce_add_plugin`.

juce_set_vst2_sdk_path("$ENV{HOME}/SDKs/VST3")
# juce_set_aax_sdk_path(...)


add_subdirectory(mcfx_convolver)
