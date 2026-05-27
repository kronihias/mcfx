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

# Shared preset-folder helper used by both mcfx_graph and mcfx_mimoeq.
SET(MCFX_EXTRA_SOURCES
    ${SRC_DIR}/common/Presets/PresetManager.cpp
    ${SRC_DIR}/common/Presets/PresetManager.h)
SET(MCFX_EXTRA_INCLUDES ${SRC_DIR}/common/Presets)

INCLUDE(${SRC_DIR}/Subproject.cmake)

#------------------------------------------------------------------------------
# Out-of-process plugin scanner executable (built from shared common/PluginHost
# sources, mirroring mcfx_anything). The exe name is unique per plugin so each
# bundle embeds its own copy without overwriting siblings.
#------------------------------------------------------------------------------
# See mcfx_anything/CMakeLists.txt for why HARDENED_RUNTIME_OPTIONS is set
# here — same library-validation issue applies to this scanner exe.
juce_add_gui_app(mcfx_graph_plugin_scanner
    PRODUCT_NAME              mcfx_graph_plugin_scanner
    VERSION                   ${VERSION}
    HARDENED_RUNTIME_ENABLED  TRUE
    HARDENED_RUNTIME_OPTIONS  "com.apple.security.cs.disable-library-validation"
                              "com.apple.security.cs.allow-unsigned-executable-memory")

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

# Copy scanner into plugin bundles so the plugin can find it at runtime.
#
# The post-build is hooked on the *host plugin* target rather than the scanner,
# with an explicit add_dependencies edge: this guarantees the scanner exists
# before the plugin's post-build chain runs, and — crucially — places the copy
# AFTER JUCE's own JUCE_COPY_PLUGIN_AFTER_BUILD post-build (registered inside
# juce_add_plugin). Hooking it on the scanner target instead races with JUCE's
# copy and produces installer bundles missing Contents/Helpers/.
foreach(tgt IN LISTS MCFX_TARGETS)
    if(TARGET ${tgt}_VST3)
        add_dependencies(${tgt}_VST3 mcfx_graph_plugin_scanner)
        if(APPLE)
            add_custom_command(TARGET ${tgt}_VST3 POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_BUNDLE_DIR:${tgt}_VST3>/Contents/Helpers"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:mcfx_graph_plugin_scanner>"
                    "$<TARGET_BUNDLE_DIR:${tgt}_VST3>/Contents/Helpers/"
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${BIN_DIR}/vst3/${tgt}.vst3/Contents/Helpers"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:mcfx_graph_plugin_scanner>"
                    "${BIN_DIR}/vst3/${tgt}.vst3/Contents/Helpers/"
                VERBATIM)
        else()
            # Windows: VST3 is a directory bundle; the runtime DLL lives in
            # Contents\x86_64-win\, and the scanner exe must sit beside it.
            add_custom_command(TARGET ${tgt}_VST3 POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:mcfx_graph_plugin_scanner>"
                    "$<TARGET_FILE_DIR:${tgt}_VST3>/"
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${BIN_DIR}/vst3/${tgt}.vst3/Contents/x86_64-win"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:mcfx_graph_plugin_scanner>"
                    "${BIN_DIR}/vst3/${tgt}.vst3/Contents/x86_64-win/"
                VERBATIM)
        endif()
    endif()
    if(TARGET ${tgt}_AU)
        add_dependencies(${tgt}_AU mcfx_graph_plugin_scanner)
        add_custom_command(TARGET ${tgt}_AU POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:${tgt}_AU>/Contents/Helpers"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:mcfx_graph_plugin_scanner>"
                "$<TARGET_BUNDLE_DIR:${tgt}_AU>/Contents/Helpers/"
            VERBATIM)
    endif()
    if(TARGET ${tgt}_Standalone)
        add_dependencies(${tgt}_Standalone mcfx_graph_plugin_scanner)
        if(APPLE)
            add_custom_command(TARGET ${tgt}_Standalone POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_BUNDLE_DIR:${tgt}_Standalone>/Contents/Helpers"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:mcfx_graph_plugin_scanner>"
                    "$<TARGET_BUNDLE_DIR:${tgt}_Standalone>/Contents/Helpers/"
                VERBATIM)
        else()
            add_custom_command(TARGET ${tgt}_Standalone POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:mcfx_graph_plugin_scanner>"
                    "$<TARGET_FILE_DIR:${tgt}_Standalone>/"
                VERBATIM)
        endif()
    endif()
endforeach()

UNSET(MCFX_EXTRA_DEFS)
UNSET(MCFX_EXTRA_SOURCES)
UNSET(MCFX_EXTRA_INCLUDES)
