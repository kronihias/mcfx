SET (SPECIFIC_SOURE_DIR ${SRC_DIR}/mcfx_mimoeq)

# include shared EqCore sources
FILE ( GLOB_RECURSE EQCORE_SOURCE ${SRC_DIR}/common/EqCore/*.c* )
FILE ( GLOB_RECURSE EQCORE_HEADER ${SRC_DIR}/common/EqCore/*.h* )

# MtxConv partitioned convolution engine (for long FIR filters)
SET(MTXCONV_SOURCES
    ${SRC_DIR}/mcfx_convolver/Source/MtxConv.cpp
    ${SRC_DIR}/mcfx_convolver/Source/MtxConv.h)

IF(${NUM_CHANNELS} LESS 10)
	SET(PLUGIN_CODE MMe${NUM_CHANNELS})
elseif(${NUM_CHANNELS} LESS 100)
	SET(PLUGIN_CODE MM${NUM_CHANNELS})
else()
	SET(PLUGIN_CODE M${NUM_CHANNELS})
endif()

SET(MC_PLUGIN_CODE "Emc0")

IF(NOT APPLE)
    SET(WITH_FFTW3 TRUE)
ENDIF()

IF(APPLE)
    # juce::dsp::FFT only compiles its vDSP engine when this is defined; without
    # it JUCE silently falls back to its own transform. Measured on Apple
    # Silicon, the fallback is 2.1x slower at 4096 points and 3.4x slower at
    # 32768 — the size the spectrogram's constant-Q frame uses every refresh.
    # Accelerate is already linked below for MtxConv.
    SET(MCFX_EXTRA_DEFS JUCE_USE_VDSP_FRAMEWORK=1)
ENDIF()

# Pass EqCore + MtxConv + shared PresetManager to both variants via the
# MCFX_EXTRA_* hooks. PresetManager is shared with mcfx_graph.
SET(MCFX_EXTRA_SOURCES  ${EQCORE_SOURCE} ${EQCORE_HEADER} ${MTXCONV_SOURCES}
                        ${SRC_DIR}/common/Presets/PresetManager.cpp
                        ${SRC_DIR}/common/Presets/PresetManager.h)
SET(MCFX_EXTRA_INCLUDES ${SRC_DIR}/common/EqCore ${SRC_DIR}/mcfx_convolver/Source
                        ${SRC_DIR}/common/Presets)

# Keep the standalone's input and output channel counts linked (a single
# combined "Channels" selector). Mismatched in/out counts currently misbehave,
# so require them to be equal.
INCLUDE(${SRC_DIR}/Subproject.cmake)

# MtxConv requires platform-specific FFT libraries
foreach(tgt IN LISTS MCFX_TARGETS)
    if(APPLE)
        target_link_libraries(${tgt} PRIVATE "-framework Accelerate")
    endif()
    if(WITH_FFTW3)
        target_link_libraries(${tgt} PRIVATE
            ${FFTW3F_LIBRARY}
            ${FFTW3F_THREADS_LIBRARY})
    endif()
endforeach()

# clear so other plugins don't inherit
UNSET(MCFX_EXTRA_SOURCES)
UNSET(MCFX_EXTRA_INCLUDES)
UNSET(MCFX_EXTRA_DEFS)
