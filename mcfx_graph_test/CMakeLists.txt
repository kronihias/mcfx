cmake_minimum_required(VERSION 3.13)

# mcfx_graph_test — checks mcfx_graph's graph logic ("Convert to subgraph",
# Option/Alt-drag node insertion) without the plug-in wrapper or the editor. Used by
# tests/test_graph_subgraph.py. Built only when -DBUILD_GRAPH_TESTS=ON.
#
# Usage (from repo root):
#   cmake -B _build -DBUILD_GRAPH_TESTS=ON
#   cmake --build _build --target mcfx_graph_test -j8
#   _build/testhost/mcfx_graph_test [--scenario NAME]

juce_add_console_app(mcfx_graph_test
    PRODUCT_NAME "mcfx_graph_test"
    VERSION      "1.0.0"
)

juce_generate_juce_header(mcfx_graph_test)

# Compile the graph sources straight in, like mcfx_net_test does with the
# stream sources: no dependency on the plug-in's build flags. Only the
# non-UI parts: controller, serializer, subgraphs and the native nodes.
set(_GRAPH_SRC ${SRC_DIR}/mcfx_graph/Source)

target_sources(mcfx_graph_test PRIVATE
    Source/Main.cpp
    ${_GRAPH_SRC}/Graph/BypassMuteWrapper.cpp
    ${_GRAPH_SRC}/Graph/GraphController.cpp
    ${_GRAPH_SRC}/Graph/GraphSerializer.cpp
    ${_GRAPH_SRC}/Graph/NodeInsertion.cpp
    ${_GRAPH_SRC}/Graph/SubgraphConversion.cpp
    ${_GRAPH_SRC}/Graph/SubgraphNode.cpp
    ${_GRAPH_SRC}/NativeNodes/DelayNode.cpp
    ${_GRAPH_SRC}/NativeNodes/FeedbackNodes.cpp
    ${_GRAPH_SRC}/NativeNodes/GainNode.cpp
    ${_GRAPH_SRC}/NativeNodes/MatrixMixerNode.cpp
    ${_GRAPH_SRC}/NativeNodes/MutePhaseNode.cpp
    ${_GRAPH_SRC}/Hosting/PluginInstanceFactory.cpp
)

target_include_directories(mcfx_graph_test PRIVATE ${_GRAPH_SRC})

target_compile_definitions(mcfx_graph_test PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    _USE_MATH_DEFINES=1
)

target_link_libraries(mcfx_graph_test PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

# Output to _build/testhost/ alongside mcfx_testhost, where tests/conftest.py
# looks.
set_target_properties(mcfx_graph_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY            "${CMAKE_BINARY_DIR}/testhost"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE    "${CMAKE_BINARY_DIR}/testhost"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG      "${CMAKE_BINARY_DIR}/testhost"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/testhost"
)
