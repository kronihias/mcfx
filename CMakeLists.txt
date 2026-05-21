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
SET(NUM_CHANNELS 36 CACHE STRING "number of channels for each plugin (per-channel VST2 build)")
SET(MAX_DELAYTIME_S 0.5 CACHE STRING "maximum delaytime for mcfx_delay in seconds")

SET(BUILD_VST3 TRUE CACHE BOOL "build VST3 plugins")
SET(BUILD_VST TRUE CACHE BOOL "build VST2 plugins")
SET(BUILD_AU FALSE CACHE BOOL "build AU plugins (macOS only)")
SET(BUILD_STANDALONE FALSE CACHE BOOL "build standalone application (for those with editor)")
SET(BUILD_LV2 FALSE CACHE BOOL "build LV2 plugins")

# Single-binary multichannel build: produces ONE binary per plugin that the
# host (Reaper, Max 9, ...) negotiates to the actual track channel count, up
# to MCFX_MAX_CHANNELS. VST2 stays per-channel for legacy project compat.
SET(MCFX_MAX_CHANNELS 128 CACHE STRING "max channel count for the single-binary multichannel VST3/AU/Standalone build")
OPTION(MCFX_BUILD_VST2_PER_CHANNEL "Build per-channel VST2 variants (legacy)"        ON)
OPTION(MCFX_BUILD_MC               "Build single multichannel VST3/AU/Standalone"     ON)


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

# NOTE: NUM_CHANNELS is NOT added as a global definition anymore. It is set
# per-target by Subproject.cmake so that the per-channel VST2 build gets the
# configured NUM_CHANNELS and the multichannel VST3/AU/Standalone build gets
# MCFX_MAX_CHANNELS, both within a single cmake configure step.

ADD_DEFINITIONS(-DNUM_OUTPUTS=${NUM_OUTPUTS_DECODER})
ADD_DEFINITIONS(-DNUM_FILTERS=${NUM_FILTERS})
ADD_DEFINITIONS(-DNUM_FILTERS_VMIC=${NUM_FILTERS_VMIC})

# formats
# MCFX_FORMATS is the union of all formats. It is kept for any remaining
# references. But Subproject.cmake picks the actual formats per variant
# from the two split lists below:
#   MCFX_FORMATS_VST2 — formats that go into the per-channel build (VST2, LV2)
#   MCFX_FORMATS_MC   — formats that go into the single multichannel build
#                       (VST3, AU, Standalone)
set (MCFX_FORMATS      "")
set (MCFX_FORMATS_VST2 "")
set (MCFX_FORMATS_MC   "")

# VST2 SDK path is needed both for building VST2 plugins AND for VST2 plugin
# hosting in mcfx_anything (which can run in a VST3-only build). Declare it
# unconditionally so the header include dir can be registered with JUCE even
# when BUILD_VST is OFF.
SET (VST2SDKPATH "~/SDKs/vstsdk2.4" CACHE PATH "Path to the VST2 SDK")

if (BUILD_VST)
    # checking VST2 SDK
    if (VST2SDKPATH STREQUAL "/usr/include")
        CHECK_INCLUDE_FILES(pluginterfaces/vst2.x/aeffectx.h HAVE_VST2_HEADERS)
        IF (NOT HAVE_VST2_HEADERS)
            message (FATAL_ERROR "You have to specify the VST2SDKPATH variable with the path to the VST2 SDK if you want to build VST2 versions.")
        endif()
    endif()
    list (APPEND MCFX_FORMATS      VST)
    list (APPEND MCFX_FORMATS_VST2 VST)
    message ("-- Building VST2 versions")
endif(BUILD_VST)

# Register the VST2 SDK with JUCE whenever the header is available. This
# creates the juce_vst2_sdk INTERFACE target whose include directories are
# propagated to any plugin that hosts VST2 (mcfx_anything), independent of
# whether BUILD_VST is ON.
get_filename_component(_vst2_sdk_abs "${VST2SDKPATH}" ABSOLUTE)
if (EXISTS "${_vst2_sdk_abs}/pluginterfaces/vst2.x/aeffectx.h")
    juce_set_vst2_sdk_path (${VST2SDKPATH})
    SET (MCFX_VST2_SDK_AVAILABLE TRUE)
elseif (BUILD_VST)
    # BUILD_VST=ON implies we must have the SDK; let JUCE raise the error.
    juce_set_vst2_sdk_path (${VST2SDKPATH})
endif()
unset(_vst2_sdk_abs)

if (BUILD_VST3)
    message ("-- Building VST3 versions")
    list (APPEND MCFX_FORMATS    VST3)
    list (APPEND MCFX_FORMATS_MC  VST3)
endif(BUILD_VST3)

if (BUILD_LV2)
    message ("-- Building LV2 versions")
    list (APPEND MCFX_FORMATS      LV2)
    # LV2 keeps per-channel build for now
    list (APPEND MCFX_FORMATS_VST2 LV2)
endif(BUILD_LV2)

if (BUILD_AU)
    if (NOT APPLE)
        message (FATAL_ERROR "AU plugins can only be built on macOS")
    endif()
    list (APPEND MCFX_FORMATS    AU)
    list (APPEND MCFX_FORMATS_MC AU)
    message ("-- Building AU versions (multichannel only)")
endif(BUILD_AU)

if (BUILD_STANDALONE)
    message ("-- Building Standalone versions")
    list (APPEND MCFX_FORMATS    Standalone)
    list (APPEND MCFX_FORMATS_MC Standalone)

    IF(WIN32)
        # JUCE 8.0.13+ ships the ASIO SDK headers under
        # JUCE/modules/juce_audio_devices/native/asio/, so no external SDK
        # path is needed. JUCE_ASIO=1 is enough.
        SET(JUCE_ASIO TRUE CACHE BOOL "build standalone app with ASIO support (bundled with JUCE)")
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
set_directory_properties (PROPERTIES JUCE_VST3_COPY_DIR "${BIN_DIR}/vst3")
set_directory_properties (PROPERTIES JUCE_LV2_COPY_DIR "${BIN_DIR}/lv2")

################################################
# Build type configuration
SET(CMAKE_BUILD_TYPE_OPTIONS "Debug" "Release" "MinSizeRel")
SET(BUILD_TYPE "default" CACHE STRING "Build type: default (MinSizeRel on macOS, Release elsewhere), or specify Debug/Release/MinSizeRel")
SET_PROPERTY(CACHE BUILD_TYPE PROPERTY STRINGS "default" ${CMAKE_BUILD_TYPE_OPTIONS})

IF (BUILD_TYPE STREQUAL "default")
    IF (APPLE)
        SET (CMAKE_BUILD_TYPE MinSizeRel)
        SET (CMAKE_OSX_ARCHITECTURES arm64;x86_64)
        SET (CMAKE_OSX_DEPLOYMENT_TARGET 10.11)

        SET (LINK_FLAGS MINSIZEREL "-dead_strip")
    ELSE (APPLE)
        SET (CMAKE_BUILD_TYPE Release)
    ENDIF (APPLE)
ELSE()
    SET (CMAKE_BUILD_TYPE ${BUILD_TYPE})
    IF (APPLE)
        SET (CMAKE_OSX_ARCHITECTURES arm64;x86_64)
        SET (CMAKE_OSX_DEPLOYMENT_TARGET 10.11)
        IF (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
            SET (LINK_FLAGS MINSIZEREL "-dead_strip")
        ENDIF()
    ENDIF (APPLE)
ENDIF()


IF (UNIX)
    # this is to make the convolver build under linux happy
    IF (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|i386|amd64)")
        SET (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -msse3")
        SET (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -msse3")
    ENDIF()
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
INCLUDE_DIRECTORIES ( BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/common/EqCore )


################################################
# additional cmake find packages
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Force MODULE mode so our cmake/FindFFTW3.cmake is used (sets FFTW3F_LIBRARY
# and FFTW3F_THREADS_LIBRARY). vcpkg installs FFTW3Config.cmake files which
# would otherwise win CONFIG-mode resolution and our targets would see empty
# library variables.
IF (WITH_ZITA_CONVOLVER OR WIN32)
	find_package(FFTW3 MODULE COMPONENTS single)
ENDIF (WITH_ZITA_CONVOLVER OR WIN32)

IF(UNIX AND NOT APPLE AND NOT ANDROID)
	find_package(FFTW3 MODULE COMPONENTS single threads)
ENDIF(UNIX AND NOT APPLE AND NOT ANDROID)

# Locate GNU patch. On Windows it ships with Git for Windows under usr/bin but
# isn't on PowerShell's PATH by default, so probe the usual install locations.
find_program(PATCH_EXECUTABLE
    NAMES patch
    HINTS
        "$ENV{ProgramFiles}/Git/usr/bin"
        "$ENV{ProgramFiles\(x86\)}/Git/usr/bin"
        "$ENV{LOCALAPPDATA}/Programs/Git/usr/bin"
    DOC "GNU patch executable used to apply JUCE patches")

if (NOT PATCH_EXECUTABLE)
    message(FATAL_ERROR
        "Could not find the `patch` executable, needed to apply the JUCE patches "
        "in JUCE_patches/. On Windows, install Git for Windows (which ships "
        "patch.exe in <install>/usr/bin) or add patch.exe to PATH.")
endif()

# Apply a patch idempotently: dry-run first so an already-applied patch on a
# re-configure is reported as a no-op instead of a hard failure. Applies with
# -p1 rooted at the JUCE submodule so a single patch can touch multiple files.
function(apply_juce_patch patch_file)
    set(juce_root "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
    execute_process(
        COMMAND "${PATCH_EXECUTABLE}" --binary --forward --dry-run --silent
                -p1 -d "${juce_root}" -i "${patch_file}"
        RESULT_VARIABLE dry_result
        OUTPUT_VARIABLE dry_output
        ERROR_VARIABLE  dry_error)

    if (dry_result EQUAL 0)
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" --binary --forward
                    -p1 -d "${juce_root}" -i "${patch_file}"
            RESULT_VARIABLE apply_result
            OUTPUT_VARIABLE apply_output
            ERROR_VARIABLE  apply_error)
        if (NOT apply_result EQUAL 0)
            message(FATAL_ERROR
                "Failed to apply ${patch_file}:\n${apply_output}\n${apply_error}")
        endif()
        message(STATUS "Applied patch: ${patch_file}")
    else()
        # GNU patch reports the "no-op" condition with several different
        # phrasings depending on version: "already applied", "previously
        # applied", "Reversed (or previously applied)". Detect any.
        set(combined "${dry_output}${dry_error}")
        string(FIND "${combined}" "already applied"   already_idx)
        string(FIND "${combined}" "previously applied" previously_idx)
        string(FIND "${combined}" "Reversed"          reversed_idx)
        string(FIND "${combined}" "reversed"          reversed_lc_idx)
        if (already_idx GREATER -1
            OR previously_idx GREATER -1
            OR reversed_idx GREATER -1
            OR reversed_lc_idx GREATER -1)
            message(STATUS "Patch already applied: ${patch_file}")
        else()
            message(FATAL_ERROR
                "Patch ${patch_file} cannot be applied under ${juce_root}:\n"
                "${dry_output}\n${dry_error}")
        endif()
    endif()
endfunction()

IF (JUCE_JACK)
    find_package(Jack REQUIRED)
    apply_juce_patch(
        "${CMAKE_CURRENT_SOURCE_DIR}/JUCE_patches/juce_JackAudio.cpp.patch")
ENDIF (JUCE_JACK)

# Always apply: relax the JUCE 8 VST3 host's strict bus-arrangement roundtrip
# check so plugins that rewrite their layout internally (mcfx-style multichannel
# plug-ins) can be resized via setBusesLayout. See the patch's comment for
# detail.
apply_juce_patch(
    "${CMAKE_CURRENT_SOURCE_DIR}/JUCE_patches/juce_VST3PluginFormatImpl.h.patch")

# Always apply: extend getVst3SpeakerArrangement to synthesise an arrangement
# for AudioChannelSet::discreteChannels(N) at any N. Without this the JUCE
# VST3 host wrapper rejects setBusesLayout for arbitrary N (only canonical
# and ambisonic layouts go through), which manifested as the channel-count
# popup in mcfx_graph offering only 1..9 + N^2 values for mcfx VST3 plug-ins.
apply_juce_patch(
    "${CMAKE_CURRENT_SOURCE_DIR}/JUCE_patches/juce_VST3Common.h.patch")

# Implement IPAddress::getInterfaceBroadcastAddress on Windows (upstream JUCE
# leaves it as a TODO that returns 0.0.0.0) and add an explicit loopback
# broadcast in NetworkServiceDiscovery::Advertiser. Without this, Windows peers
# never appear on remote machines and same-host listeners miss advertisements
# (mcfx_send ↔ mcfx_receive discovery).
apply_juce_patch(
    "${CMAKE_CURRENT_SOURCE_DIR}/JUCE_patches/juce_network_discovery.patch")

include_directories(${FFTW3_INCLUDE_DIR} ${JACK_INCLUDE_DIRS})


################################################
# add all subprojects (plugins)

ADD_SUBDIRECTORY ( mcfx_convolver )
ADD_SUBDIRECTORY ( mcfx_delay )
ADD_SUBDIRECTORY ( mcfx_filter )
ADD_SUBDIRECTORY ( mcfx_gain_delay )
ADD_SUBDIRECTORY ( mcfx_meter )
ADD_SUBDIRECTORY ( mcfx_mimoeq )
ADD_SUBDIRECTORY ( mcfx_anything )
ADD_SUBDIRECTORY ( mcfx_graph )
ADD_SUBDIRECTORY ( mcfx_send )
ADD_SUBDIRECTORY ( mcfx_receive )

################################################
# Optional: offline CLI test harness for Python-driven regression tests.
# Build with:  cmake -B _build -DBUILD_TESTHOST=ON  (requires BUILD_VST3=ON)
OPTION(BUILD_TESTHOST "Build mcfx_testhost offline test harness" OFF)
IF(BUILD_TESTHOST)
    message("-- Building mcfx_testhost test harness")
    ADD_SUBDIRECTORY( mcfx_testhost )
ENDIF(BUILD_TESTHOST)

################################################
# Optional: single-process loopback test for mcfx_send + mcfx_receive.
# Drives both stream classes directly (no plugin wrapper, no DAW) over
# localhost UDP so the network audio path can be verified by the
# Python tests in /tests/test_net_loopback.py.
# Build with:  cmake -B _build -DBUILD_NET_TESTS=ON
OPTION(BUILD_NET_TESTS "Build mcfx_net_loopback_test (network audio integration tests)" OFF)
IF(BUILD_NET_TESTS)
    message("-- Building mcfx_net_loopback_test")
    ADD_SUBDIRECTORY( mcfx_net_test )
ENDIF(BUILD_NET_TESTS)
