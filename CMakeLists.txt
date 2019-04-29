cmake_minimum_required(VERSION 2.8)

cmake_policy(SET CMP0026 OLD) # supress warnings regarding JUCE STATIC LOCATION

################################################
#set name
PROJECT(mcfx_plugin_suite)

################################################
#version
file(READ VERSION VERSION)
ADD_DEFINITIONS(-DVERSION=${VERSION})

################################################
#choices at compile time
SET(NUM_CHANNELS 36 CACHE STRING "number of channels for each plugin")
SET(MAX_DELAYTIME_S 0.5 CACHE STRING "maximum delaytime for mcfx_delay in seconds")
SET(BUILD_STANDALONE TRUE CACHE BOOL "build standalone application (for those with editor)")

IF(UNIX AND NOT APPLE AND NOT ANDROID)
	SET(WITH_ZITA_CONVOLVER TRUE CACHE BOOL "build with zita-convolver (better performance under linux)")
ENDIF(UNIX AND NOT APPLE AND NOT ANDROID)

IF(BUILD_STANDALONE)

	IF(UNIX AND NOT APPLE AND NOT ANDROID)
		SET(JUCE_JACK TRUE CACHE BOOL "build standalone app with Jack support")
		SET(JUCE_ALSA TRUE CACHE BOOL "build standalone app with ALSA support")
	ENDIF(UNIX AND NOT APPLE AND NOT ANDROID)

	IF(WIN32)
		SET(JUCE_ASIO TRUE CACHE BOOL "build standalone app with ASIO support (you need the Steinberg ASIO SDK)")
	ENDIF(WIN32)

ENDIF(BUILD_STANDALONE)

ADD_DEFINITIONS(-DMAX_DELAYTIME_S=${MAX_DELAYTIME_S})


IF (JUCE_JACK)
	ADD_DEFINITIONS(-DJUCE_JACK=1)
ENDIF (JUCE_JACK)

IF (JUCE_ALSA)
	ADD_DEFINITIONS(-DJUCE_ALSA=1)
ENDIF (JUCE_ALSA)

IF (JUCE_ASIO)
	ADD_DEFINITIONS(-DJUCE_ASIO=1)
  
  SET(ASIO_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ASIOSDK2.3 CACHE PATH "specify the path of the ASIO 2.3 SDK")
  INCLUDE_DIRECTORIES(BEFORE ${ASIO_SDK_DIR}/common)
  
ENDIF (JUCE_ASIO)

# set release and optimization flags
# SET(CMAKE_C_FLAGS_RELEASE "-Os") # use small size?
# SET(CMAKE_CXX_FLAGS_RELEASE "-Os") # use small size?

SET(CMAKE_BUILD_TYPE Release)

ADD_DEFINITIONS(-DNDEBUG=1)
ADD_DEFINITIONS(-D_NDEBUG=1)

#save path for later
SET (SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
SET (BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})

IF(APPLE)
# switch to c++11
   SET(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++")
   
   SET( CMAKE_OSX_ARCHITECTURES i386;x86_64 )
   SET( CMAKE_OSX_DEPLOYMENT_TARGET 10.7 )
ENDIF (APPLE)

################################################
# INCLUDES

INCLUDE_DIRECTORIES ( BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/JUCE )
INCLUDE_DIRECTORIES ( BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/JUCE/modules )

################################################
# additional cmake find packages
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(libsoxr REQUIRED)

IF (WITH_ZITA_CONVOLVER OR WIN32)
	find_package(FFTW3 COMPONENTS single)
ENDIF (WITH_ZITA_CONVOLVER OR WIN32)

include_directories(${EIGEN3_INCLUDE_DIR} ${FFTW3_INCLUDE_DIR} ${LIBLO_INCLUDE_DIRS} ${LIBSOXR_INCLUDE_DIRS})


################################################
# this are the link libraries for juce

IF(APPLE)
   FIND_LIBRARY(ACCELERATE_LIBRARY Accelerate)
   FIND_LIBRARY(AUDIOTOOLBOX_LIBRARY AudioToolbox)
   FIND_LIBRARY(CARBON_LIBRARY Carbon)
   FIND_LIBRARY(COCOA_LIBRARY Cocoa)
   FIND_LIBRARY(COREAUDIO_LIBRARY CoreAudio)
   FIND_LIBRARY(COREMIDI_LIBRARY CoreMIDI)
   FIND_LIBRARY(DISCRECORDING_LIBRARY DiscRecording)
   FIND_LIBRARY(IOKIT_LIBRARY IOKit)
   FIND_LIBRARY(OPENGL_LIBRARY OpenGL)
   FIND_LIBRARY(QTKIT_LIBRARY QTKit)
   FIND_LIBRARY(QUARTZCORE_LIBRARY QuartzCore)
   # FIND_LIBRARY(QUICKTIME_LIBRARY QuickTime)
   FIND_LIBRARY(WEBKIT_LIBRARY WebKit)
   MARK_AS_ADVANCED (ACCELERATE_LIBRARY AUDIOTOOLBOX_LIBRARY CARBON_LIBRARY COCOA_LIBRARY COREAUDIO_LIBRARY COREMIDI_LIBRARY COREMIDI_LIBRARY DISCRECORDING_LIBRARY IOKIT_LIBRARY OPENGL_LIBRARY QTKIT_LIBRARY QUARTZCORE_LIBRARY WEBKIT_LIBRARY)
   SET(JUCE_LIBRARIES ${ACCELERATE_LIBRARY} ${AUDIOTOOLBOX_LIBRARY} ${CARBON_LIBRARY} ${COCOA_LIBRARY} ${COREAUDIO_LIBRARY} ${COREMIDI_LIBRARY} ${COREMIDI_LIBRARY} ${DISCRECORDING_LIBRARY} ${IOKIT_LIBRARY} ${OPENGL_LIBRARY} ${QTKIT_LIBRARY} ${QUARTZCORE_LIBRARY} ${WEBKIT_LIBRARY})
    
    ADD_DEFINITIONS(-DJUCE_MAC=1)
ENDIF (APPLE)


IF(UNIX AND NOT APPLE AND NOT ANDROID)

   ADD_DEFINITIONS(-DLINUX=1)
   ADD_DEFINITIONS(-DJUCE_LINUX=1)
   
   FIND_PACKAGE    ( OpenGL REQUIRED )
   FIND_LIBRARY    ( X11_LIBRARY X11 )
   FIND_LIBRARY    ( XEXT_LIBRARY Xext )
   FIND_LIBRARY    ( XINERAMA_LIBRARY Xinerama )
   FIND_LIBRARY    ( ASOUND_LIBRARY asound )
   FIND_LIBRARY    ( DL_LIBRARY dl )
   FIND_LIBRARY    ( FREETYPE_LIBRARY freetype )
   FIND_LIBRARY    ( PTHREAD_LIBRARY pthread )
   FIND_LIBRARY    ( RT_LIBRARY rt )
   MARK_AS_ADVANCED(GL_LIBRARY X11_LIBRARY XEXT_LIBRARY XINERAMA_LIBRARY ASOUND_LIBRARY DL_LIBRARY FREETYPE_LIBRARY PTHREAD_LIBRARY RT_LIBRARY)
   
   # get freetype includes...
   
   INCLUDE(FindFreetype)

   IF(NOT FREETYPE_FOUND)
      FIND_LIBRARY(FREETYPE_LIBRARIES NAMES libfreetype freetype.dll PATHS "./libs/MacOS" "./libs/Windows" DOC "Freetype library")
      FIND_PATH(FREETYPE_INCLUDE_DIRS ftbuild.h "./includes" DOC "Freetype includes")
   ENDIF(NOT FREETYPE_FOUND)
   
   INCLUDE_DIRECTORIES(${FREETYPE_INCLUDE_DIRS})
	
   SET ( JUCE_LIBRARIES ${X11_LIBRARY} ${XEXT_LIBRARY} ${XINERAMA_LIBRARY} ${ASOUND_LIBRARY} ${DL_LIBRARY} ${FREETYPE_LIBRARY} ${RT_LIBRARY} ${OPENGL_LIBRARIES} ${PTHREAD_LIBRARY} )
   
   FIND_LIBRARY    ( LIBSOXR_LIBRARY soxr )
   
   IF(WITH_ZITA_CONVOLVER)
	FIND_PACKAGE(libzita-convolver REQUIRED)
        INCLUDE_DIRECTORIES(${LIBZITACONVOLVER_INCLUDE_DIRS})
	ADD_DEFINITIONS(-DWITH_ZITA_CONVOLVER=1)
   ENDIF(WITH_ZITA_CONVOLVER)
   
   FIND_LIBRARY    ( FFTW3F_LIBRARY fftw3f )
   
   #add sse
   set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -msse3")
   set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -msse3")
   
ENDIF(UNIX AND NOT APPLE AND NOT ANDROID)


IF(WIN32)
   FIND_PACKAGE(FFTW3 COMPONENTS single)
   ADD_DEFINITIONS(-DJUCE_WINDOWS=1)
ENDIF(WIN32)

################################################
# juce source modules
# build all modules except the vst wrapper -> this is done individually for each plugin

IF(APPLE)

SET ( JUCE_SOURCE
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_basics/juce_audio_basics.mm
#        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_devices/juce_audio_devices.mm
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_formats/juce_audio_formats.mm
#        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_plugin_client/VST/juce_VST_Wrapper.mm
#        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_plugin_client/VST/juce_VST_Wrapper.cpp
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_plugin_client/utility/juce_PluginUtilities.cpp
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_processors/juce_audio_processors.mm
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_utils/juce_audio_utils.mm
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_box2d/juce_box2d.cpp
#        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_browser_plugin_client/juce_browser_plugin.mm
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_core/juce_core.mm
#        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_cryptography/juce_cryptography.mm
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_data_structures/juce_data_structures.mm
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_events/juce_events.mm
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_graphics/juce_graphics.mm
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_gui_basics/juce_gui_basics.mm
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_gui_extra/juce_gui_extra.mm
#        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_opengl/juce_opengl.mm
		${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_osc/juce_osc.cpp
#        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_video/juce_video.mm
)

SET ( JUCE_STANDALONE_SOURCE
	${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_devices/juce_audio_devices.mm
)

SET ( JUCE_PLUGIN_CLIENT_SOURCE
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_plugin_client/VST/juce_VST_Wrapper.mm
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_plugin_client/VST/juce_VST_Wrapper.cpp
)

ELSE(APPLE)

SET ( JUCE_SOURCE
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_basics/juce_audio_basics.cpp
#        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_devices/juce_audio_devices.cpp
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_formats/juce_audio_formats.cpp
#        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_plugin_client/VST/juce_VST_Wrapper.cpp
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_plugin_client/utility/juce_PluginUtilities.cpp
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_processors/juce_audio_processors.cpp
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_utils/juce_audio_utils.cpp
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_box2d/juce_box2d.cpp
#        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_browser_plugin_client/juce_browser_plugin.cpp
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_core/juce_core.cpp
#        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_cryptography/juce_cryptography.cpp
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_data_structures/juce_data_structures.cpp
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_events/juce_events.cpp
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_graphics/juce_graphics.cpp
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_gui_basics/juce_gui_basics.cpp
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_gui_extra/juce_gui_extra.cpp
#        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_opengl/juce_opengl.cpp
		${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_osc/juce_osc.cpp
#        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_video/juce_video.cpp
)

SET ( JUCE_STANDALONE_SOURCE
	${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_devices/juce_audio_devices.cpp
)

SET ( JUCE_PLUGIN_CLIENT_SOURCE
        ${CMAKE_HOME_DIRECTORY}/JUCE/modules/juce_audio_plugin_client/VST/juce_VST_Wrapper.cpp
)

ENDIF(APPLE)

	
################################################
# add all subprojects (plugins)

ADD_SUBDIRECTORY ( JUCE )
ADD_SUBDIRECTORY ( mcfx_convolver )
ADD_SUBDIRECTORY ( mcfx_delay )
ADD_SUBDIRECTORY ( mcfx_filter )
ADD_SUBDIRECTORY ( mcfx_gain_delay )
ADD_SUBDIRECTORY ( mcfx_meter )
