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

# Pass EqCore + MtxConv to both variants via the MCFX_EXTRA_* hooks
SET(MCFX_EXTRA_SOURCES  ${EQCORE_SOURCE} ${EQCORE_HEADER} ${MTXCONV_SOURCES})
SET(MCFX_EXTRA_INCLUDES ${SRC_DIR}/common/EqCore ${SRC_DIR}/mcfx_convolver/Source)

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
