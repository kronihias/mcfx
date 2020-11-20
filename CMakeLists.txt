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

project(x-mcfx_plugin_suite VERSION ${VERSION})

add_definitions(-DVERSION=${VERSION}) #?? FOR THE DELAY


set(MANUFACTURER_CODE "Unpr")
set(PLUGIN_CODE "M")
set(PLUGIN_LABEL "X-MCFX")

################################################
set(CMAKE_BUILD_TYPE Release)
# ADD_DEFINITIONS(-DNDEBUG=1)
# ADD_DEFINITIONS(-D_NDEBUG=1)

#save path for later
set (SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set (BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})

################################################
#choices at compile time
# set(NUM_CHANNELS 64 CACHE STRING "number of channels for each plugin")

set(NUM_IN_CHANNELS 64 CACHE STRING "number of input channels for each plugin")
set(NUM_OUT_CHANNELS 64 CACHE STRING "number of output channels for each plugin")

set(MAX_DELAYTIME_S 0.5 CACHE STRING "maximum delaytime for mcfx_delay in seconds")

set(BUILD_STANDALONE TRUE CACHE BOOL "build standalone application (for those with editor)")
set(BUILD_VST2 TRUE CACHE BOOL "build VST format")
set(BUILD_VST3 TRUE CACHE BOOL "build VST3 format")

set(LAST_2CHARS_PLUGINCODE 64 CACHE STRING "to allow more configurations, specify different characters for the same plugin code")


add_definitions(-DMAX_DELAYTIME_S=${MAX_DELAYTIME_S})

# Possible different convolver library under Linux systems
if(UNIX AND NOT APPLE AND NOT ANDROID)
	set(WITH_ZITA_CONVOLVER FALSE CACHE BOOL "build with zita-convolver (better performance under linux)")
endif(UNIX AND NOT APPLE AND NOT ANDROID)

################################################
# additional cmake find library packages
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${SRC_DIR}/cmake)

if (WITH_ZITA_CONVOLVER)
	find_package(FFTW3 REQUIRED COMPONENTS single)
endif(WITH_ZITA_CONVOLVER)

if (WIN32)
	set(FFTW3_ROOT_DIR "${SRC_DIR}/Libraries/Windows/fftw" CACHE STRING "path to the fftw3 include folder library")
	set(FFTW3F_LIBRARY "${SRC_DIR}/Libraries/Windows/fftw/lib/fftw3f.lib" CACHE STRING "path to the fftw3 library")
	find_package(FFTW3 REQUIRED COMPONENTS single)

	set(SOXR_ROOT_DIR "${SRC_DIR}/Libraries/Windows/soxr" CACHE STRING "path to the installed soxr library")
endif (WIN32)

if(UNIX AND NOT APPLE AND NOT ANDROID)
	find_package(FFTW3 REQUIRED COMPONENTS single threads)
endif(UNIX AND NOT APPLE AND NOT ANDROID)

if(APPLE)
	set(LIBSOXR_INCLUDE_DIRS "${SRC_DIR}/Libraries/MacOS/soxr/include" CACHE STRING "path to the include folder of soxr library")
	find_library(LIBSOXR_LIBRARY
		NAMES
			soxr
		PATHS
			"${SRC_DIR}/Libraries/MacOS/soxr/lib"
		NO_DEFAULT_PATH
	)
	set(LIBSOXR_LIBRARIES ${LIBSOXR_LIBRARY})
endif(APPLE)
find_package(libsoxr REQUIRED)

include_directories(${EIGEN3_INCLUDE_DIR} ${FFTW3_INCLUDE_DIR} ${LIBLO_INCLUDE_DIRS} ${LIBSOXR_INCLUDE_DIRS})

################################################
# Windows specific directives

# Static link to external libraries
#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>")
if(WIN32)
	if(MSVC)
    	add_compile_options(
        	$<$<CONFIG:>:/MT> #---------|
        	$<$<CONFIG:Debug>:/MTd> #---|-- Dinamically link the runtime libraries
        	$<$<CONFIG:Release>:/MT> #--|
    	)
	endif(MSVC)
	
	IF(BUILD_STANDALONE)
		SET(JUCE_ASIO TRUE CACHE BOOL "build standalone app with ASIO support (you need the Steinberg ASIO SDK)")
		ADD_DEFINITIONS(-DJUCE_ASIO=1)
		SET(ASIO_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SDKs/ASIOSDK2.3 CACHE PATH "specify the path of the ASIO 2.3 SDK")
		INCLUDE_DIRECTORIES(BEFORE ${ASIO_SDK_DIR}/common)
	ENDIF(BUILD_STANDALONE)

endif(WIN32)

################################################
# # MacOS specific directives
IF(APPLE)
#	# switch to c++11
# 	ADD_DEFINITIONS(
# 		-std=c++11
# 	)
	SET( CMAKE_OSX_DEPLOYMENT_TARGET 10.15)
ENDIF (APPLE)

################################################
# Linux specific directives
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
	
	IF(BUILD_STANDALONE)	
		SET(JUCE_JACK TRUE CACHE BOOL "build standalone app with Jack support")
		SET(JUCE_ALSA TRUE CACHE BOOL "build standalone app with ALSA support")
	ENDIF(BUILD_STANDALONE)
	
	IF (JUCE_JACK)
		ADD_DEFINITIONS(-DJUCE_JACK=1)
	ENDIF (JUCE_JACK)

	IF (JUCE_ALSA)
		ADD_DEFINITIONS(-DJUCE_ALSA=1)
	ENDIF (JUCE_ALSA)

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
if(BUILD_VST2)
	if(APPLE OR UNIX)
		SET(VSTSDK_PATH $ENV{HOME}/SDKs/VST_SDK/VST2_SDK CACHE PATH "specify the path of the VST SDK")
	endif(APPLE OR UNIX)

	if(WIN32)
		SET(VSTSDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/SDKs/VST2_SDK CACHE PATH "specify the path of the VST SDK")
	endif(WIN32)

	juce_set_vst2_sdk_path("${VSTSDK_PATH}")
endif(BUILD_VST2)


# juce_set_aax_sdk_path(...)


add_subdirectory(x-mcfx-convolver)
add_subdirectory(x-mcfx-delay)
add_subdirectory(x-mcfx-filter)
add_subdirectory(x-mcfx-gain-delay)
add_subdirectory(x-mcfx-meter)
