SET (SPECIFIC_SOURE_DIR ${SRC_DIR}/mcfx_send)

# Per-channel VST2 plug-in code. Standard mcfx convention:
#   <3-letter-product>{NUM_CHANNELS}, fitted into JUCE's 4-char PLUGIN_CODE.
IF(${NUM_CHANNELS} LESS 10)
	SET(PLUGIN_CODE MNs${NUM_CHANNELS})
elseif(${NUM_CHANNELS} LESS 100)
	SET(PLUGIN_CODE MN${NUM_CHANNELS})
else()
	SET(PLUGIN_CODE N${NUM_CHANNELS})
endif()

SET(MC_PLUGIN_CODE "Nsmc")

# Pull in the shared net code (wire format, packetizer, jitter buffer,
# socket helpers). Header-only or .cpp files under ${SRC_DIR}/common/net.
file(GLOB _MCFX_NET_SOURCES ${SRC_DIR}/common/net/*.cpp)
file(GLOB _MCFX_NET_HEADERS ${SRC_DIR}/common/net/*.h)
# Sender doesn't currently resample, but link the resampler in anyway so a
# future "send-side rate match to a fixed wire rate" stays a one-liner.
SET(_MCFX_RESAMPLER_DIR ${SRC_DIR}/common/third_party/audio-resampler)
SET(MCFX_EXTRA_SOURCES  ${_MCFX_NET_SOURCES} ${_MCFX_NET_HEADERS}
                        ${_MCFX_RESAMPLER_DIR}/resampler.c
                        ${_MCFX_RESAMPLER_DIR}/resampler.h)
SET(MCFX_EXTRA_INCLUDES ${SRC_DIR}/common/net ${_MCFX_RESAMPLER_DIR})

# Sender captures from JACK and ships it over the network; only the input
# channel count matters. But JUCE's standalone host only grows the processor's
# input bus when the audio device opens symmetrically (it has no input-only
# fallback — see findMostSuitableLayout), and the multichannel processor
# requires in == out anyway. So open the device with matching in/out counts
# (JSA_STANDALONE_SYMMETRIC_IO mirrors the input port count onto the output)
# and use the linked "Channels" selector. The output ports exist but are unused.
SET(MCFX_EXTRA_DEFS JSA_STANDALONE_SYMMETRIC_IO=1)

INCLUDE(${SRC_DIR}/Subproject.cmake)

UNSET(MCFX_EXTRA_DEFS)

# Pull in juce_cryptography for SHA-256 (used by the password gate).
# Other mcfx plugins don't need crypto; do it per-plugin.
foreach(_tgt IN LISTS MCFX_TARGETS)
    target_link_libraries(${_tgt} PRIVATE juce::juce_cryptography)
    if(APPLE)
        # SCDynamicStoreCopyComputerName / CFRelease / CFStringGetCString —
        # used by mcfx::net::getFriendlyComputerName for the Sharing-prefs
        # name (better than gethostname, which is what JUCE returns).
        target_link_libraries(${_tgt} PRIVATE
            "-framework SystemConfiguration"
            "-framework CoreFoundation")
    endif()
endforeach()
