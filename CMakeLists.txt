cmake_minimum_required (VERSION 3.13)
include(CheckIncludeFiles)

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

SET(BUILD_VST TRUE CACHE BOOL "build VST plugins")
SET(BUILD_STANDALONE FALSE CACHE BOOL "build standalone application (for those with editor)")
SET(BUILD_LV2 FALSE CACHE BOOL "build LV2 plugins")


IF(UNIX AND NOT APPLE AND NOT ANDROID)
	SET(WITH_ZITA_CONVOLVER FALSE CACHE BOOL "build with zita-convolver (better performance under linux)")
ENDIF(UNIX AND NOT APPLE AND NOT ANDROID)

################################################
# add JUCE

ADD_SUBDIRECTORY ( JUCE )

#########
# preprocessor flags

# need this to make the file picker work (legacy code)
ADD_DEFINITIONS(-DJUCE_MODAL_LOOPS_PERMITTED=1)

# we use some deprecated stuff, yeah...
SET(CMAKE_WARN_DEPRECATED OFF)
SET(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

ADD_DEFINITIONS(-DNUM_CHANNELS=${NUM_CHANNELS})

ADD_DEFINITIONS(-DNUM_OUTPUTS=${NUM_OUTPUTS_DECODER})
ADD_DEFINITIONS(-DNUM_FILTERS=${NUM_FILTERS})
ADD_DEFINITIONS(-DNUM_FILTERS_VMIC=${NUM_FILTERS_VMIC})

# formats
set (MCFX_FORMATS "")

if (BUILD_VST)
    SET (VST2SDKPATH "~/SDKs/vstsdk2.4" CACHE PATH "Path to the VST2 SDK")
    # checking VST2 SDK
    if (VST2SDKPATH STREQUAL "/usr/include")
        CHECK_INCLUDE_FILES(pluginterfaces/vst2.x/aeffectx.h HAVE_VST2_HEADERS)
        IF (NOT HAVE_VST2_HEADERS)
            message (FATAL_ERROR "You have to specify the VST2SDKPATH variable with the path to the VST2 SDK if you want to build VST2 versions.")
        endif()
    endif()
    juce_set_vst2_sdk_path (${VST2SDKPATH})
    list (APPEND MCFX_FORMATS VST)
    message ("-- Building VST2 versions")
endif(BUILD_VST)

if (BUILD_LV2)
    message ("-- Building LV2 versions")
    list (APPEND MCFX_FORMATS LV2)
endif(BUILD_LV2)

if (BUILD_STANDALONE)
    message ("-- Building Standalone versions")
    list (APPEND MCFX_FORMATS Standalone)

    IF(WIN32)
        SET(JUCE_ASIO TRUE CACHE BOOL "build standalone app with ASIO support (you need the Steinberg ASIO SDK)")
        SET(ASIO_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SDKs/ASIOSDK2.3 CACHE PATH "specify the path of the ASIO 2.3 SDK")
        INCLUDE_DIRECTORIES(BEFORE ${ASIO_SDK_DIR}/common)
    ENDIF(WIN32)

    IF(UNIX AND NOT APPLE AND NOT ANDROID)
        SET(JUCE_ALSA TRUE CACHE BOOL "build standalone app with ALSA support")
    ENDIF(UNIX AND NOT APPLE AND NOT ANDROID)

    IF(UNIX AND NOT ANDROID)
        SET(JUCE_JACK TRUE CACHE BOOL "build standalone app with Jack support")
    ENDIF(UNIX AND NOT ANDROID)

    IF(JUCE_ASIO)
        ADD_DEFINITIONS(-DJUCE_ASIO=1)
    ENDIF(JUCE_ASIO)

    IF (JUCE_JACK)
        ADD_DEFINITIONS(-DJUCE_JACK=1)
    ENDIF (JUCE_JACK)

    IF (JUCE_ALSA)
        ADD_DEFINITIONS(-DJUCE_ALSA=1)
    ENDIF (JUCE_ALSA)

endif(BUILD_STANDALONE)


#save path for later
SET (SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
SET (BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})


# set output path
set_directory_properties (PROPERTIES JUCE_COPY_PLUGIN_AFTER_BUILD ON)
set_directory_properties (PROPERTIES JUCE_VST_COPY_DIR "${BIN_DIR}/vst")
set_directory_properties (PROPERTIES JUCE_LV2_COPY_DIR "${BIN_DIR}/lv2")

IF (APPLE)
    SET (CMAKE_BUILD_TYPE MinSizeRel)
    SET (CMAKE_OSX_ARCHITECTURES arm64;x86_64)
    SET (CMAKE_OSX_DEPLOYMENT_TARGET 10.9)

    SET (LINK_FLAGS MINSIZEREL "-dead_strip")
ELSE (APPLE)
    SET (CMAKE_BUILD_TYPE Release)
ENDIF (APPLE)


IF (UNIX)
    # this is to make the convolver build under linux happy
    SET (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -msse3")
    SET (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -msse3")
ENDIF (UNIX)

################################################
# INCLUDES

INCLUDE_DIRECTORIES ( BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/JUCE )
INCLUDE_DIRECTORIES ( BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/JUCE/modules )

INCLUDE_DIRECTORIES ( BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/common )
INCLUDE_DIRECTORIES ( BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/common/Ressources )
INCLUDE_DIRECTORIES ( BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/common/SphericalHarmonic )
INCLUDE_DIRECTORIES ( BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/common/SphFilter )
INCLUDE_DIRECTORIES ( BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/common/LegendreU )


################################################
# additional cmake find packages
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

IF (WITH_ZITA_CONVOLVER OR WIN32)
	find_package(FFTW3 COMPONENTS single)
ENDIF (WITH_ZITA_CONVOLVER OR WIN32)

IF(UNIX AND NOT APPLE AND NOT ANDROID)
	find_package(FFTW3 COMPONENTS single threads)
ENDIF(UNIX AND NOT APPLE AND NOT ANDROID)

IF (JUCE_JACK)
    find_package(Jack REQUIRED)

    message ("-- Patching JUCE Jack implementation")
    execute_process(COMMAND patch ${CMAKE_CURRENT_SOURCE_DIR}/JUCE/modules/juce_audio_devices/native/juce_linux_JackAudio.cpp ${CMAKE_CURRENT_SOURCE_DIR}/JUCE_patches/juce_linux_JackAudio.cpp.patch --forward OUTPUT_VARIABLE patch_result)
    message(${patch_result})
ENDIF (JUCE_JACK)

include_directories(${FFTW3_INCLUDE_DIR} ${JACK_INCLUDE_DIRS})


################################################
# add all subprojects (plugins)

ADD_SUBDIRECTORY ( mcfx_convolver )
ADD_SUBDIRECTORY ( mcfx_delay )
ADD_SUBDIRECTORY ( mcfx_filter )
ADD_SUBDIRECTORY ( mcfx_gain_delay )
ADD_SUBDIRECTORY ( mcfx_meter )
