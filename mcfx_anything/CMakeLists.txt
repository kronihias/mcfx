SET (SPECIFIC_SOURE_DIR ${SRC_DIR}/mcfx_anything)

IF(${NUM_CHANNELS} LESS 10)
    SET(PLUGIN_CODE ME${NUM_CHANNELS}x)
elseif(${NUM_CHANNELS} LESS 100)
    SET(PLUGIN_CODE ME${NUM_CHANNELS})
else()
    SET(PLUGIN_CODE E${NUM_CHANNELS})
endif()

INCLUDE(${SRC_DIR}/Subproject.cmake)

# Enable plugin hosting capabilities
target_compile_definitions(${SUBPROJECT_NAME} PRIVATE
    JUCE_PLUGINHOST_VST3=1
)

IF(APPLE)
    target_compile_definitions(${SUBPROJECT_NAME} PRIVATE
        JUCE_PLUGINHOST_AU=1
    )
ENDIF(APPLE)

# Enable VST2 hosting if the VST2 SDK is available (independent of BUILD_VST
# which controls whether mcfx_anything itself is built as a VST2 plugin)
IF(BUILD_VST OR EXISTS "${VST2SDKPATH}/pluginterfaces/vst2.x/aeffectx.h")
    target_compile_definitions(${SUBPROJECT_NAME} PRIVATE
        JUCE_PLUGINHOST_VST=1
    )
ENDIF()
