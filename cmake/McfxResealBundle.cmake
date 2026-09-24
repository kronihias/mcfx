# No include_guard: the variable below is directory-scoped, so every plug-in
# directory that calls mcfx_reseal_bundle() has to include this itself.
set(_MCFX_RESEAL_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/reseal_bundle.cmake")

# Re-seal a macOS bundle after the scanner helper has been dropped into it.
#
# Used by the plug-ins that embed an out-of-process scanner (mcfx_anything,
# mcfx_graph). JUCE ad-hoc signs the VST3/AU/Standalone bundle inside its own
# POST_BUILD, which runs BEFORE the plug-in's helper-copy POST_BUILD. The
# helper therefore arrives as an unsealed addition and `codesign -v` reports
#
#   a sealed resource is missing or invalid
#   file added: .../Contents/Helpers/<name>_plugin_scanner
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
# reseal_bundle.cmake skips a bundle that was never populated (the BIN_DIR
# mirror when JUCE_COPY_PLUGIN_AFTER_BUILD is off) rather than failing the
# build on a directory holding nothing but Contents/Helpers.
function(mcfx_reseal_bundle target bundle_dir helper)
    if(NOT APPLE)
        return()
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DBUNDLE=${bundle_dir}"
            "-DHELPER=${helper}"
            -P "${_MCFX_RESEAL_SCRIPT}"
        VERBATIM)
endfunction()
