cmake_minimum_required(VERSION 3.13)

# mcfx_testhost — offline CLI test harness for mcfx VST3 plugins
#
# Usage (from repo root):
#   cmake -B _build -DBUILD_VST3=ON -DBUILD_TESTHOST=ON
#   cmake --build _build --target mcfx_testhost
#
# Then from Python:
#   subprocess.run(["_build/testhost/mcfx_testhost",
#                   "--plugin", "_build/vst3/mcfx_filter.vst3",
#                   "--params", "params.json",
#                   "--input",  "in.wav",
#                   "--channels", "2",
#                   "--output", "out.wav"], check=True)
#
# params.json format:
#   { "Peak 1 Freq": 0.5, "Peak 1 Q": 0.27, "Peak 1 Gain": 0.75, ... }

juce_add_console_app(mcfx_testhost
    PRODUCT_NAME "mcfx_testhost"
    VERSION      "1.0.0"
)

juce_generate_juce_header(mcfx_testhost)

target_sources(mcfx_testhost PRIVATE
    Source/Main.cpp
)

target_compile_definitions(mcfx_testhost PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=0
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_LADSPA=0
)

target_link_libraries(mcfx_testhost PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_data_structures
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# Output to _build/testhost/
set_target_properties(mcfx_testhost PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/testhost"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE   "${CMAKE_BINARY_DIR}/testhost"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG     "${CMAKE_BINARY_DIR}/testhost"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/testhost"
)
