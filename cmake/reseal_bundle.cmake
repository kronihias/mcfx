# Re-seal a macOS bundle after the plugin-scanner helper has been copied into
# Contents/Helpers.
#
# Invoked from mcfx_graph/CMakeLists.txt as a POST_BUILD step:
#   cmake -DBUNDLE=<path to .vst3/.component/.app> -P reseal_bundle.cmake
#
# Why a -P script rather than the codesign calls inline: the guard has to skip
# a bundle that was never populated (the JUCE_VST3_COPY_DIR mirror when
# JUCE_COPY_PLUGIN_AFTER_BUILD is off), and doing that with `sh -c` is a trap.
# The whole sh -c lands inside a double-quoted ninja command line, so the outer
# shell gets first crack at any $ in it — a positional like $1 expands to
# nothing before sh ever runs, the test silently fails, and the signing quietly
# never happens with no error to notice. CMake's own EXISTS avoids the shell
# entirely.

if(NOT APPLE AND NOT CMAKE_HOST_APPLE)
    return()
endif()

if(NOT DEFINED BUNDLE)
    message(FATAL_ERROR "reseal_bundle.cmake: BUNDLE not set")
endif()

set(_helper "${BUNDLE}/Contents/Helpers/mcfx_graph_plugin_scanner")

# Skip quietly when there is nothing to do. Two distinct cases:
#   * no helper — then nothing has been added since JUCE sealed the bundle;
#   * no Contents/MacOS — the JUCE_VST3_COPY_DIR mirror when
#     JUCE_COPY_PLUGIN_AFTER_BUILD is off, where the helper copy above has
#     created a stub directory that is not a bundle at all.
#
# Do NOT test for Contents/Info.plist here: JUCE writes that during its own
# copy step, which for the artefact bundle happens after this runs, so the
# check would pass on the mirror and silently fail on the real thing — which
# is exactly the bug this comment exists to stop someone reintroducing.
if(NOT EXISTS "${_helper}" OR NOT IS_DIRECTORY "${BUNDLE}/Contents/MacOS")
    return()
endif()

# Order matters, and mirrors codesign_bundles() in scripts/build_osx.sh: sign
# the nested helper first, then the bundle that seals it. Sealing first would
# just record an unsigned nested executable.
execute_process(COMMAND codesign --force --sign - "${_helper}"
                RESULT_VARIABLE _rc
                ERROR_VARIABLE  _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "codesign failed on ${_helper}: ${_err}")
endif()

execute_process(COMMAND codesign --force --sign - "${BUNDLE}"
                RESULT_VARIABLE _rc
                ERROR_VARIABLE  _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "codesign failed on ${BUNDLE}: ${_err}")
endif()
