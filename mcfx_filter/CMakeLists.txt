SET (SPECIFIC_SOURE_DIR ${SRC_DIR}/mcfx_filter)

SET (MCFX_USES_FFT TRUE)  # FFT on non-Apple via chowdsp_fft; vDSP on Apple


IF(${NUM_CHANNELS} LESS 10)
	SET(PLUGIN_CODE MFi${NUM_CHANNELS})
elseif(${NUM_CHANNELS} LESS 100)
	SET(PLUGIN_CODE MF${NUM_CHANNELS})
else()
	SET(PLUGIN_CODE F${NUM_CHANNELS})
endif()

# Stable, distinct code for the single multichannel build (never collides
# with per-channel VST2 codes).
SET(MC_PLUGIN_CODE "Fmc0")

INCLUDE(${SRC_DIR}/Subproject.cmake)