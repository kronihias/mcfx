SET (SPECIFIC_SOURE_DIR ${SRC_DIR}/mcfx_meter)


IF(${NUM_CHANNELS} LESS 10)
	SET(PLUGIN_CODE MMe${NUM_CHANNELS})
elseif(${NUM_CHANNELS} LESS 100)
	SET(PLUGIN_CODE MM${NUM_CHANNELS})
else()
	SET(PLUGIN_CODE M${NUM_CHANNELS})
endif()

SET(MC_PLUGIN_CODE "Mmc0")

# No FFT backend is selected here on purpose. The band analyser behind the
# waterfall runs 4096-point transforms through juce::dsp::FFT, and
# JUCE_USE_VDSP_FRAMEWORK was measured against the default fallback on Apple
# Silicon at 4096/8192/32768 points: identical within run-to-run noise. FFTW is
# not linked either — that would add a dependency to a plug-in which currently
# has none, to save ~0.3 ms a tick against a ~20 ms repaint.

INCLUDE(${SRC_DIR}/Subproject.cmake)