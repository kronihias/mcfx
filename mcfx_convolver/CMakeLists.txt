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

# Keep the standalone's input and output channel counts linked (a single
# combined "Channels" selector). Mismatched in/out counts currently misbehave,
# so require them to be equal.
INCLUDE(${SRC_DIR}/Subproject.cmake)