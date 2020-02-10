SET ( SPECIFIC_SOURE_DIR ${SRC_DIR}/mcfx_convolver )
SET ( WITH_LIBSOXR TRUE )

SET(OSX_COPY_LIB libsoxr.0.1.0.dylib)

IF(NOT APPLE)
	SET(OSX_COPY_LIB libfftw3f.3.dylib)
	SET ( WITH_FFTW3 TRUE )
ENDIF(NOT APPLE)

INCLUDE(${SRC_DIR}/CMakeLists_subprojects_standalone.txt.inc)
