SET (SPECIFIC_SOURE_DIR ${SRC_DIR}/mcfx_convolver)

IF(NOT APPLE)
	SET (WITH_FFTW3 TRUE)
ENDIF(NOT APPLE)


IF(${NUM_CHANNELS} LESS 10)
	SET(PLUGIN_CODE MCO${NUM_CHANNELS})
elseif(${NUM_CHANNELS} LESS 100)
	SET(PLUGIN_CODE MC${NUM_CHANNELS})
else()
	SET(PLUGIN_CODE C${NUM_CHANNELS})
endif()

SET(MC_PLUGIN_CODE "Cmc0")

# Convolver has genuinely independent input/output channel counts, so keep the
# standalone's input and output channel selectors separate (don't auto-link).
SET(MCFX_EXTRA_DEFS JSA_STANDALONE_INDEPENDENT_IO=1)

INCLUDE(${SRC_DIR}/Subproject.cmake)

UNSET(MCFX_EXTRA_DEFS)