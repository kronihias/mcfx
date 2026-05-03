cmake_minimum_required(VERSION 3.13)

# mcfx_net_loopback_test — single-process integration test that drives
# SendStream + RecvStream over a localhost UDP loopback. Used by the
# Python tests in /tests/test_net_loopback.py. Built only when
# -DBUILD_NET_TESTS=ON is passed at configure time.
#
# Usage (from repo root):
#   cmake -B _build -DBUILD_VST3=ON -DBUILD_NET_TESTS=ON
#   cmake --build _build --target mcfx_net_loopback_test -j8
#   _build/testhost/mcfx_net_loopback_test --scenario smoke
#
# The CLI is documented in Source/Main.cpp.

juce_add_console_app(mcfx_net_loopback_test
    PRODUCT_NAME "mcfx_net_loopback_test"
    VERSION      "1.0.0"
)

juce_generate_juce_header(mcfx_net_loopback_test)

# We compile the SendStream + RecvStream sources directly into the test
# binary (rather than linking against the plugin's static library) so the
# test target stays independent of plugin build flags and channel-count
# variants. Same convention the plugins themselves use.
set(_NET_DIR ${SRC_DIR}/common/net)
set(_RESAMPLER_DIR ${SRC_DIR}/common/third_party/audio-resampler)

target_sources(mcfx_net_loopback_test PRIVATE
    Source/Main.cpp
    ${_NET_DIR}/mcfx_net_send.cpp
    ${_NET_DIR}/mcfx_net_recv.cpp
    ${_RESAMPLER_DIR}/resampler.c
)

target_include_directories(mcfx_net_loopback_test PRIVATE
    ${_NET_DIR}
    ${_RESAMPLER_DIR}
)

target_compile_definitions(mcfx_net_loopback_test PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    # Streams use juce::SHA256 for password hashing — pull in cryptography.
    # Discovery is not built in (we use Direct-IP only).
)

target_link_libraries(mcfx_net_loopback_test PRIVATE
    juce::juce_core
    juce::juce_data_structures
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_events
    juce::juce_cryptography
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

if(APPLE)
    # SCDynamicStoreCopyComputerName / CFRelease etc. used by
    # mcfx::net::getFriendlyComputerName via mcfx_net_socket.h. The streams
    # don't currently call it but the helper is in the same translation
    # unit; keep the linkage so the binary builds clean.
    target_link_libraries(mcfx_net_loopback_test PRIVATE
        "-framework SystemConfiguration"
        "-framework CoreFoundation")
endif()

# Output to _build/testhost/ alongside mcfx_testhost — matches the
# tests/conftest.py convention so the pytest wrapper can find it.
set_target_properties(mcfx_net_loopback_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY            "${CMAKE_BINARY_DIR}/testhost"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE    "${CMAKE_BINARY_DIR}/testhost"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG      "${CMAKE_BINARY_DIR}/testhost"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/testhost"
)
