set(CURR_SUBDIR mcfx_convolver)

SET(SPECIFIC_SOURCE_DIR ${SRC_DIR}/${CURR_SUBDIR})
SET(WITH_LIBSOXR TRUE)

# append "c" for "convolver" to the plugin code string
string(APPEND PLUGIN_CODE "c")

# append specific plugin name
string(APPEND PLUGIN_LABEL "_Convolver")

# library adjustments not yet checked
# SET(OSX_COPY_LIB libsoxr.0.1.0.dylib)

if(NOT APPLE)
	# SET(OSX_COPY_LIB libfftw3f.3.dylib)
	SET (WITH_FFTW3 TRUE)
endif(NOT APPLE)

include(Subproject)
# INCLUDE(${SRC_DIR}/CMakeLists_subprojects.txt.inc)