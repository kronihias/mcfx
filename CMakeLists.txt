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
#version and name
file(READ VERSION VERSION)
add_definitions(-DVERSION=${VERSION}) #?? FOR THE DELAY

project(mcfx_plugin_suite VERSION ${VERSION})

set(MANUFACTURER_CODE "Kron")
set(PLUGIN_CODE "M")
set(PLUGIN_LABEL "McFx")

################################################
# set(CMAKE_BUILD_TYPE Release)
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

add_definitions(-DMAX_DELAYTIME_S=${MAX_DELAYTIME_S})

# Possible different convolver library under Linux systems
if(UNIX AND NOT APPLE AND NOT ANDROID)
	set(WITH_ZITA_CONVOLVER FALSE CACHE BOOL "build with zita-convolver (better performance under linux)")
endif(UNIX AND NOT APPLE AND NOT ANDROID)

################################################
# additional cmake find library packages
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${SRC_DIR}/cmake)

find_package(libsoxr REQUIRED)

if (WITH_ZITA_CONVOLVER OR WIN32)
	find_package(FFTW3 COMPONENTS single)
endif (WITH_ZITA_CONVOLVER OR WIN32)

if(UNIX AND NOT APPLE AND NOT ANDROID)
	find_package(FFTW3 COMPONENTS single threads)
endif(UNIX AND NOT APPLE AND NOT ANDROID)

include_directories(${EIGEN3_INCLUDE_DIR} ${FFTW3_INCLUDE_DIR} ${LIBLO_INCLUDE_DIRS} ${LIBSOXR_INCLUDE_DIRS})

################################################
# Linux specifi directives
		# get freetype includes...
if(UNIX AND NOT APPLE AND NOT ANDROID)

	# INCLUDE(FindFreetype)

	# IF(NOT FREETYPE_FOUND)
	# FIND_LIBRARY(FREETYPE_LIBRARIES NAMES libfreetype freetype.dll PATHS "./libs/MacOS" "./libs/Windows" DOC "Freetype library")
	# FIND_PATH(FREETYPE_INCLUDE_DIRS ftbuild.h "./includes" DOC "Freetype includes")
	# ENDIF(NOT FREETYPE_FOUND)

	# INCLUDE_DIRECTORIES(${FREETYPE_INCLUDE_DIRS})
	# INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
	# INCLUDE_DIRECTORIES(${WEBKIT_INCLUDE_DIRS})

	# SET ( JUCE_LIBRARIES ${GTK3_LIBRARIES} ${WEBKIT_LIBRARIES} ${CURL_LIBRARY} ${X11_LIBRARY} ${XEXT_LIBRARY} ${XINERAMA_LIBRARY} ${ASOUND_LIBRARY} ${DL_LIBRARY} ${FREETYPE_LIBRARY} ${RT_LIBRARY} ${OPENGL_LIBRARIES} ${PTHREAD_LIBRARY} )

	find_library(LIBSOXR_LIBRARY soxr)

	if(WITH_ZITA_CONVOLVER)
		find_package(libzita-convolver REQUIRED)
		include_directories(${LIBZITACONVOLVER_INCLUDE_DIRS})
		add_definitions(-DWITH_ZITA_CONVOLVER=1)
	endif(WITH_ZITA_CONVOLVER)

	#add sse
	# set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -msse3")
	# set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -msse3")

endif(UNIX AND NOT APPLE AND NOT ANDROID)


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
add_subdirectory(mcfx_delay)
add_subdirectory(mcfx_filter)
add_subdirectory(mcfx_gain_delay)
add_subdirectory(mcfx_meter)
