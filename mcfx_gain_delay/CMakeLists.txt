SET ( SPECIFIC_SOURCE_DIR ${SRC_DIR}/mcfx_gain_delay)
# SET ( WITH_LIBSOXR TRUE )

# append "c" for "convolver" to the plugin code string
string(APPEND PLUGIN_CODE "g")

# append specific plugin name
string(APPEND PLUGIN_LABEL " Gain Delay")

# soxr library adjustments not yet checked
# SET(OSX_COPY_LIB libsoxr.0.1.0.dylib)

if(NOT APPLE)
# 	SET(OSX_COPY_LIB libfftw3f.3.dylib)
	SET ( WITH_FFTW3 TRUE )
endif(NOT APPLE)

include(Subproject)
# INCLUDE(${SRC_DIR}/CMakeLists_subprojects.txt.inc)