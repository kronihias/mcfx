SET ( SPECIFIC_SOURE_DIR ${SRC_DIR}/mcfx_filter )

IF(NOT APPLE)
	SET(OSX_COPY_LIB libfftw3f.3.dylib)
	SET ( WITH_FFTW3 TRUE )
ENDIF(NOT APPLE)

INCLUDE(${SRC_DIR}/CMakeLists_subprojects.txt.inc)
