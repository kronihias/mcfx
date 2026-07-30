SET (SPECIFIC_SOURE_DIR ${SRC_DIR}/mcfx_meter)


IF(${NUM_CHANNELS} LESS 10)
	SET(PLUGIN_CODE MMe${NUM_CHANNELS})
elseif(${NUM_CHANNELS} LESS 100)
	SET(PLUGIN_CODE MM${NUM_CHANNELS})
else()
	SET(PLUGIN_CODE M${NUM_CHANNELS})
endif()

SET(MC_PLUGIN_CODE "Mmc0")

# The band analyser's transform goes through common/Dsp/RealFFT, which is only
# fast off Apple when FFTW is linked — without this it silently compiles the
# juce::dsp::FFT fallback it was written to avoid.
IF(NOT APPLE)
    SET(WITH_FFTW3 TRUE)
ENDIF()

INCLUDE(${SRC_DIR}/Subproject.cmake)

# The band analyser's transform goes straight to Accelerate (common/Dsp/RealFFT).
IF(APPLE)
    foreach(tgt IN LISTS MCFX_TARGETS)
        target_link_libraries(${tgt} PRIVATE "-framework Accelerate")
    endforeach()
ENDIF()