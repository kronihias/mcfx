#==============================================================================
# Subproject.cmake — shared helper that registers one or both variants of a
# single mcfx plugin:
#
#   1. Per-channel VST2 target   — name: ${SUBDIRNAME}${NUM_CHANNELS}
#                                   formats: ${MCFX_FORMATS_VST2}
#                                   NUM_CHANNELS=${NUM_CHANNELS}
#                                   JucePlugin_PreferredChannelConfigurations
#                                       locked to {N,N}
#
#   2. Multichannel MC target    — name: ${SUBDIRNAME}
#                                   formats: ${MCFX_FORMATS_MC} (VST3/AU/Standalone)
#                                   NUM_CHANNELS=${MCFX_MAX_CHANNELS}
#                                   MCFX_MULTICHANNEL_BUILD=1
#                                   no PreferredChannelConfigurations — host
#                                       negotiates layout via
#                                       isBusesLayoutSupported
#
# Caller contract (plugin's own CMakeLists.txt):
#   - set(SPECIFIC_SOURE_DIR ...)       optional: where source/ lives
#   - set(PLUGIN_CODE    ...)           4-char code for the VST2 variant
#   - set(MC_PLUGIN_CODE ...)           4-char code for the MC variant
#   - (optional) set(MCFX_EXTRA_SOURCES ...) extra source files
#   - (optional) set(MCFX_EXTRA_INCLUDES ...) extra include directories
#   - (optional) set(MCFX_EXTRA_DEFS   ...)   extra compile definitions
#   - include(${SRC_DIR}/Subproject.cmake)
#
# After the include, ${MCFX_TARGETS} contains the list of created targets so
# the caller can attach plugin-specific extras uniformly, e.g.
#       foreach(tgt IN LISTS MCFX_TARGETS)
#           target_compile_definitions(${tgt} PRIVATE JUCE_PLUGINHOST_VST3=1)
#       endforeach()
#==============================================================================

get_filename_component(SUBDIRNAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# Collect sources once
if(DEFINED SPECIFIC_SOURE_DIR)
    file(GLOB_RECURSE _MCFX_SOURCE ${SPECIFIC_SOURE_DIR}/Source/*.c*)
    file(GLOB_RECURSE _MCFX_HEADER ${SPECIFIC_SOURE_DIR}/Source/*.h*)
else()
    file(GLOB_RECURSE _MCFX_SOURCE Source/*.c*)
    file(GLOB_RECURSE _MCFX_HEADER Source/*.h*)
endif()

if(BUILD_STANDALONE)
    list(APPEND _MCFX_SOURCE ${SRC_DIR}/standalone-filter/Main.cpp)
    list(APPEND _MCFX_HEADER ${SRC_DIR}/standalone-filter/CustomStandaloneFilterWindow.h)
endif()

list(SORT _MCFX_SOURCE)

set(MCFX_TARGETS "")

#------------------------------------------------------------------------------
# Per-channel VST2 variant
#------------------------------------------------------------------------------
if(MCFX_BUILD_VST2_PER_CHANNEL AND MCFX_FORMATS_VST2)
    if(DEFINED SPECIFIC_PROJECTNAME)
        set(_vst2_target ${SPECIFIC_PROJECTNAME}${NUM_CHANNELS})
    else()
        set(_vst2_target ${SUBDIRNAME}${NUM_CHANNELS})
    endif()

    juce_add_plugin(${_vst2_target}
        PLUGIN_MANUFACTURER_CODE Kron
        PLUGIN_CODE              ${PLUGIN_CODE}
        COMPANY_NAME             "kronlachner"
        PRODUCT_NAME             ${_vst2_target}
        FORMATS                  ${MCFX_FORMATS_VST2}
        VERSION                  ${VERSION}
        LV2URI                   http://www.matthiaskronlachner.com/${_vst2_target})

    juce_generate_juce_header(${_vst2_target})

    target_sources(${_vst2_target} PRIVATE
        ${_MCFX_SOURCE}
        ${_MCFX_HEADER}
        ${MCFX_EXTRA_SOURCES})

    set(_vst2_chcfg "\{${NUM_CHANNELS}, ${NUM_CHANNELS}\}")

    target_compile_definitions(${_vst2_target} PRIVATE
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
        JUCE_USE_FLAC=0
        JUCE_USE_OGGVORBIS=0
        JUCE_USE_MP3AUDIOFORMAT=0
        JUCE_USE_LAME_AUDIO_FORMAT=0
        JUCE_USE_WINDOWS_MEDIA_FORMAT=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        NUM_CHANNELS=${NUM_CHANNELS}
        MCFX_MULTICHANNEL_BUILD=0
        JucePlugin_PreferredChannelConfigurations=${_vst2_chcfg}
        ${MCFX_EXTRA_DEFS})

    if(MCFX_EXTRA_INCLUDES)
        target_include_directories(${_vst2_target} PRIVATE ${MCFX_EXTRA_INCLUDES})
    endif()

    target_link_libraries(${_vst2_target} PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
        juce::juce_osc
        juce::juce_dsp
        juce::juce_opengl
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

    if(WITH_FFTW3)
        target_link_libraries(${_vst2_target} PRIVATE
            ${FFTW3F_LIBRARY}
            ${FFTW3F_THREADS_LIBRARY})
    endif()

    list(APPEND MCFX_TARGETS ${_vst2_target})
endif()

#------------------------------------------------------------------------------
# Single multichannel VST3/AU/Standalone variant
#------------------------------------------------------------------------------
if(MCFX_BUILD_MC AND MCFX_FORMATS_MC AND DEFINED MC_PLUGIN_CODE)
    if(DEFINED SPECIFIC_PROJECTNAME)
        set(_mc_target ${SPECIFIC_PROJECTNAME})
    else()
        set(_mc_target ${SUBDIRNAME})
    endif()

    juce_add_plugin(${_mc_target}
        PLUGIN_MANUFACTURER_CODE Kron
        PLUGIN_CODE              ${MC_PLUGIN_CODE}
        COMPANY_NAME             "kronlachner"
        PRODUCT_NAME             ${_mc_target}
        FORMATS                  ${MCFX_FORMATS_MC}
        VERSION                  ${VERSION}
        LV2URI                   http://www.matthiaskronlachner.com/${_mc_target})

    juce_generate_juce_header(${_mc_target})

    target_sources(${_mc_target} PRIVATE
        ${_MCFX_SOURCE}
        ${_MCFX_HEADER}
        ${MCFX_EXTRA_SOURCES})

    # NO JucePlugin_PreferredChannelConfigurations — host negotiates bus layout.
    target_compile_definitions(${_mc_target} PRIVATE
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
        JUCE_USE_FLAC=0
        JUCE_USE_OGGVORBIS=0
        JUCE_USE_MP3AUDIOFORMAT=0
        JUCE_USE_LAME_AUDIO_FORMAT=0
        JUCE_USE_WINDOWS_MEDIA_FORMAT=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        NUM_CHANNELS=${MCFX_MAX_CHANNELS}
        MCFX_MULTICHANNEL_BUILD=1
        ${MCFX_EXTRA_DEFS})

    if(MCFX_EXTRA_INCLUDES)
        target_include_directories(${_mc_target} PRIVATE ${MCFX_EXTRA_INCLUDES})
    endif()

    target_link_libraries(${_mc_target} PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
        juce::juce_osc
        juce::juce_dsp
        juce::juce_opengl
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

    if(WITH_FFTW3)
        target_link_libraries(${_mc_target} PRIVATE
            ${FFTW3F_LIBRARY}
            ${FFTW3F_THREADS_LIBRARY})
    endif()

    list(APPEND MCFX_TARGETS ${_mc_target})

    # JUCE provides JUCE_VST_COPY_DIR / JUCE_VST3_COPY_DIR / JUCE_LV2_COPY_DIR
    # for automatic post-build install, but has no equivalent for AU or
    # Standalone. Mirror the same "flat folder next to vst/vst3" layout used
    # for the other formats so every built artefact ends up under
    # ${BIN_DIR}/{vst,vst3,au,standalone} for easy access without digging
    # through the artefacts/ tree.
    if("AU" IN_LIST MCFX_FORMATS_MC AND TARGET ${_mc_target}_AU)
        add_custom_command(TARGET ${_mc_target}_AU POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory  "${BIN_DIR}/au"
            COMMAND ${CMAKE_COMMAND} -E rm -rf          "${BIN_DIR}/au/${_mc_target}.component"
            COMMAND ${CMAKE_COMMAND} -E copy_directory  "$<TARGET_BUNDLE_DIR:${_mc_target}_AU>"
                                                        "${BIN_DIR}/au/${_mc_target}.component"
            VERBATIM)
    endif()

    if("Standalone" IN_LIST MCFX_FORMATS_MC AND TARGET ${_mc_target}_Standalone)
        if(APPLE)
            add_custom_command(TARGET ${_mc_target}_Standalone POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory  "${BIN_DIR}/standalone"
                COMMAND ${CMAKE_COMMAND} -E rm -rf          "${BIN_DIR}/standalone/${_mc_target}.app"
                COMMAND ${CMAKE_COMMAND} -E copy_directory  "$<TARGET_BUNDLE_DIR:${_mc_target}_Standalone>"
                                                            "${BIN_DIR}/standalone/${_mc_target}.app"
                VERBATIM)
        else()
            add_custom_command(TARGET ${_mc_target}_Standalone POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory  "${BIN_DIR}/standalone"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:${_mc_target}_Standalone>"
                        "${BIN_DIR}/standalone/"
                VERBATIM)
        endif()
    endif()
endif()

# Back-compat: a few plugin CMakeLists.txt used to reference SUBPROJECT_NAME
# directly to attach extras AFTER the include. Set it to the first target so
# existing code keeps working, but new code should iterate MCFX_TARGETS.
if(MCFX_TARGETS)
    list(GET MCFX_TARGETS 0 SUBPROJECT_NAME)
endif()
