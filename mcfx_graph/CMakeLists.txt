SET (SPECIFIC_SOURE_DIR ${SRC_DIR}/mcfx_graph)

# 4-char VST2 codes, distinct from existing plugins. Per-channel variant only
# matters for VST2 hosts that need a fixed bus layout; the MC variant adapts.
IF(${NUM_CHANNELS} LESS 10)
    SET(PLUGIN_CODE Gph${NUM_CHANNELS})
elseif(${NUM_CHANNELS} LESS 100)
    SET(PLUGIN_CODE Gp${NUM_CHANNELS})
else()
    SET(PLUGIN_CODE p${NUM_CHANNELS})
endif()

SET(MC_PLUGIN_CODE "GRmc")

# Plugin hosting defines — applied to both variants via MCFX_EXTRA_DEFS.
SET(MCFX_EXTRA_DEFS JUCE_PLUGINHOST_VST3=1)

IF(APPLE)
    LIST(APPEND MCFX_EXTRA_DEFS JUCE_PLUGINHOST_AU=1)
ENDIF(APPLE)

# Enable VST2 hosting only if the VST2 SDK is available.
IF(BUILD_VST OR MCFX_VST2_SDK_AVAILABLE)
    LIST(APPEND MCFX_EXTRA_DEFS JUCE_PLUGINHOST_VST=1)
ENDIF()

INCLUDE(${SRC_DIR}/Subproject.cmake)

#------------------------------------------------------------------------------
# Out-of-process plugin scanner executable (built from shared common/PluginHost
# sources, mirroring mcfx_anything). The exe name is unique per plugin so each
# bundle embeds its own copy without overwriting siblings.
#------------------------------------------------------------------------------
juce_add_gui_app(mcfx_graph_plugin_scanner
    PRODUCT_NAME  mcfx_graph_plugin_scanner
    VERSION       ${VERSION})

juce_generate_juce_header(mcfx_graph_plugin_scanner)

target_sources(mcfx_graph_plugin_scanner PRIVATE
    ${SRC_DIR}/common/PluginHost/PluginScannerMain.cpp)

target_compile_definitions(mcfx_graph_plugin_scanner PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_FLAC=0
    JUCE_USE_OGGVORBIS=0
    JUCE_USE_MP3AUDIOFORMAT=0
    JUCE_USE_LAME_AUDIO_FORMAT=0
    JUCE_USE_WINDOWS_MEDIA_FORMAT=0
    ${MCFX_EXTRA_DEFS})

target_link_libraries(mcfx_graph_plugin_scanner PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags)

# Copy scanner into plugin bundles so the plugin can find it at runtime
foreach(tgt IN LISTS MCFX_TARGETS)
    if(TARGET ${tgt}_VST3)
        if(APPLE)
            add_custom_command(TARGET mcfx_graph_plugin_scanner POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_BUNDLE_DIR:${tgt}_VST3>/Contents/Helpers"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:mcfx_graph_plugin_scanner>"
                    "$<TARGET_BUNDLE_DIR:${tgt}_VST3>/Contents/Helpers/"
                VERBATIM)
        else()
            add_custom_command(TARGET mcfx_graph_plugin_scanner POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:mcfx_graph_plugin_scanner>"
                    "$<TARGET_FILE_DIR:${tgt}_VST3>/"
                VERBATIM)
        endif()
    endif()
    if(TARGET ${tgt}_AU)
        add_custom_command(TARGET mcfx_graph_plugin_scanner POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:${tgt}_AU>/Contents/Helpers"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:mcfx_graph_plugin_scanner>"
                "$<TARGET_BUNDLE_DIR:${tgt}_AU>/Contents/Helpers/"
            VERBATIM)
    endif()
    if(TARGET ${tgt}_Standalone)
        if(APPLE)
            add_custom_command(TARGET mcfx_graph_plugin_scanner POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_BUNDLE_DIR:${tgt}_Standalone>/Contents/Helpers"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:mcfx_graph_plugin_scanner>"
                    "$<TARGET_BUNDLE_DIR:${tgt}_Standalone>/Contents/Helpers/"
                VERBATIM)
        else()
            add_custom_command(TARGET mcfx_graph_plugin_scanner POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:mcfx_graph_plugin_scanner>"
                    "$<TARGET_FILE_DIR:${tgt}_Standalone>/"
                VERBATIM)
        endif()
    endif()
endforeach()

UNSET(MCFX_EXTRA_DEFS)
