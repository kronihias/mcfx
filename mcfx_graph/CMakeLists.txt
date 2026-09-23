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

# LV2 hosting is cross-platform (JUCE bundles the host libs). LADSPA hosting is
# Linux/BSD-only — JUCE_INTERNAL_HAS_LADSPA gates it on platform internally, so
# defining the flag elsewhere is harmless.
LIST(APPEND MCFX_EXTRA_DEFS JUCE_PLUGINHOST_LV2=1)
IF(UNIX AND NOT APPLE)
    LIST(APPEND MCFX_EXTRA_DEFS JUCE_PLUGINHOST_LADSPA=1)
ENDIF()

# The graph host routes arbitrary in/out, so keep the standalone's input and
# output channel selectors independent (don't auto-link them).
LIST(APPEND MCFX_EXTRA_DEFS JSA_STANDALONE_INDEPENDENT_IO=1)

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

# Re-seal a macOS bundle after the scanner helper has been dropped into it.
#
# JUCE ad-hoc signs the VST3/AU/Standalone bundle inside its own POST_BUILD,
# which by the ordering described above runs BEFORE ours. The helper therefore
# arrives as an unsealed addition and `codesign -v` reports
#
#   a sealed resource is missing or invalid
#   file added: .../Contents/Helpers/mcfx_graph_plugin_scanner
#
# The bundle still loads locally, because Gatekeeper only enforces on
# quarantined code — which is why this went unnoticed — but it fails strict
# validation, and "the signature is broken" is a thoroughly misleading thing
# to discover while debugging a host that won't load the plug-in.
#
# Order mirrors codesign_bundles() in scripts/build_osx.sh: a nested helper
# has to be signed before the bundle that seals it, or sealing it just
# re-records an unsigned file. Releases re-sign everything afterwards with the
# real Developer ID (and scanner.entitlements), so this only affects dev
# builds — it costs them nothing and makes them verify.
#
# Guarded on Info.plist so a bundle JUCE never copied here (the BIN_DIR mirror
# when JUCE_COPY_PLUGIN_AFTER_BUILD is off) is skipped rather than failing the
# build on a directory holding nothing but Contents/Helpers.
function(mcfx_graph_reseal_bundle target bundle_dir)
    if(NOT APPLE)
        return()
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DBUNDLE=${bundle_dir}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/reseal_bundle.cmake"
        VERBATIM)
endfunction()

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
            mcfx_graph_reseal_bundle(${tgt}_VST3 "$<TARGET_BUNDLE_DIR:${tgt}_VST3>")
            mcfx_graph_reseal_bundle(${tgt}_VST3 "${BIN_DIR}/vst3/${tgt}.vst3")
        elseif(WIN32)
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
        else()
            add_custom_command(TARGET ${tgt}_VST3 POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:mcfx_graph_plugin_scanner>"
                    "$<TARGET_FILE_DIR:${tgt}_VST3>/"
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${BIN_DIR}/vst3/${tgt}.vst3/Contents/x86_64-linux"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:mcfx_graph_plugin_scanner>"
                    "${BIN_DIR}/vst3/${tgt}.vst3/Contents/x86_64-linux/"
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
        mcfx_graph_reseal_bundle(${tgt}_AU "$<TARGET_BUNDLE_DIR:${tgt}_AU>")
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
            mcfx_graph_reseal_bundle(${tgt}_Standalone "$<TARGET_BUNDLE_DIR:${tgt}_Standalone>")
        else()
            add_custom_command(TARGET ${tgt}_Standalone POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:mcfx_graph_plugin_scanner>"
                    "$<TARGET_FILE_DIR:${tgt}_Standalone>/"
                VERBATIM)
        endif()
    endif()
    if(TARGET ${tgt}_LV2)
        # LV2 bundle is a flat <name>.lv2/ folder containing the shared library
        # plus its .ttl manifests. findScannerExecutable() looks for the scanner
        # alongside the loaded binary, so drop it next to the .so. Copy into both
        # the artefact bundle and the JUCE_LV2_COPY_DIR copy (mirrors the VST3
        # handling above so it survives whichever post-build order JUCE picks).
        add_dependencies(${tgt}_LV2 mcfx_graph_plugin_scanner)
        add_custom_command(TARGET ${tgt}_LV2 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:mcfx_graph_plugin_scanner>"
                "$<TARGET_FILE_DIR:${tgt}_LV2>/"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "${BIN_DIR}/lv2/${tgt}.lv2"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:mcfx_graph_plugin_scanner>"
                "${BIN_DIR}/lv2/${tgt}.lv2/"
            VERBATIM)
    endif()
endforeach()

UNSET(MCFX_EXTRA_DEFS)
UNSET(MCFX_EXTRA_SOURCES)
UNSET(MCFX_EXTRA_INCLUDES)
