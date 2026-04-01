SET (SPECIFIC_SOURE_DIR ${SRC_DIR}/mcfx_mimoeq)

# include shared EqCore sources
FILE ( GLOB_RECURSE EQCORE_SOURCE ${SRC_DIR}/common/EqCore/*.c* )
FILE ( GLOB_RECURSE EQCORE_HEADER ${SRC_DIR}/common/EqCore/*.h* )

IF(${NUM_CHANNELS} LESS 10)
	SET(PLUGIN_CODE MMe${NUM_CHANNELS})
elseif(${NUM_CHANNELS} LESS 100)
	SET(PLUGIN_CODE MM${NUM_CHANNELS})
else()
	SET(PLUGIN_CODE M${NUM_CHANNELS})
endif()

INCLUDE(${SRC_DIR}/Subproject.cmake)

# add EqCore sources to the target
target_sources (${SUBPROJECT_NAME} PRIVATE ${EQCORE_SOURCE} ${EQCORE_HEADER})
target_include_directories (${SUBPROJECT_NAME} PRIVATE ${SRC_DIR}/common/EqCore)
