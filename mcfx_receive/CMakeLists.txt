SET (SPECIFIC_SOURE_DIR ${SRC_DIR}/mcfx_receive)

IF(${NUM_CHANNELS} LESS 10)
	SET(PLUGIN_CODE MNr${NUM_CHANNELS})
elseif(${NUM_CHANNELS} LESS 100)
	SET(PLUGIN_CODE MR${NUM_CHANNELS})
else()
	SET(PLUGIN_CODE R${NUM_CHANNELS})
endif()

SET(MC_PLUGIN_CODE "Nrmc")

file(GLOB _MCFX_NET_SOURCES ${SRC_DIR}/common/net/*.cpp)
file(GLOB _MCFX_NET_HEADERS ${SRC_DIR}/common/net/*.h)
# dbry/audio-resampler — vendored, BSD-3. Used for adaptive sample-rate
# conversion on the receive side (sender and receiver clocks drift; the
# resampler ratio is nudged each block by a control loop on the ring fill).
SET(_MCFX_RESAMPLER_DIR ${SRC_DIR}/common/third_party/audio-resampler)
SET(MCFX_EXTRA_SOURCES  ${_MCFX_NET_SOURCES} ${_MCFX_NET_HEADERS}
                        ${_MCFX_RESAMPLER_DIR}/resampler.c
                        ${_MCFX_RESAMPLER_DIR}/resampler.h)
SET(MCFX_EXTRA_INCLUDES ${SRC_DIR}/common/net ${_MCFX_RESAMPLER_DIR})

# Receiver is JACK-output-only: audio comes in over the network and goes out to
# JACK, so its JACK input is unused. Tell the standalone host to expose output
# ports only (no input ports, no input channel selector). Standalone-only; the
# VST/VST3/AU plug-in keeps its symmetric in/out bus for DAW compatibility.
SET(MCFX_EXTRA_DEFS JSA_STANDALONE_NO_INPUT=1)

INCLUDE(${SRC_DIR}/Subproject.cmake)

UNSET(MCFX_EXTRA_DEFS)

foreach(_tgt IN LISTS MCFX_TARGETS)
    target_link_libraries(${_tgt} PRIVATE juce::juce_cryptography)
    if(APPLE)
        target_link_libraries(${_tgt} PRIVATE
            "-framework SystemConfiguration"
            "-framework CoreFoundation")
    endif()
endforeach()
