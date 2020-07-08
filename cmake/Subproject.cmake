get_filename_component(SUBDIRNAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

add_definitions(-DNUM_CHANNELS=${NUM_CHANNELS})

IF( DEFINED SPECIFIC_PROJECTNAME )
	# this is for ambix_decoder (which shares the code of ambix_binaural)
	SET (SUBPROJECT_NAME ${SPECIFIC_PROJECTNAME}${NUM_CHANNELS})
ELSE( DEFINED SPECIFIC_PROJECTNAME )
	# this is the normal way...
	SET (SUBPROJECT_NAME ${SUBDIRNAME}${NUM_CHANNELS})
ENDIF(DEFINED SPECIFIC_PROJECTNAME )

# add all c, cpp, cc files from the Source directory
# file ( GLOB_RECURSE SOURCE Source/*.c* )
# file ( GLOB_RECURSE HEADER Source/*.h* )

IF(DEFINED SPECIFIC_SOURCE_DIR)
	FILE ( GLOB_RECURSE SOURCE ${SPECIFIC_SOURCE_DIR}/Source/*.c* )
	FILE ( GLOB_RECURSE HEADER ${SPECIFIC_SOURCE_DIR}/Source/*.h* )
	if (BUILD_STANDALONE)
		list(APPEND SOURCE ${SRC_DIR}/JUCE/standalone-filter/Main.cpp)
		list(APPEND HEADER ${SRC_DIR}/JUCE/standalone-filter/CustomStandaloneFilterWindow.h)
	endif(BUILD_STANDALONE)
ENDIF(DEFINED SPECIFIC_SOURCE_DIR)

# ignore mac hidden files
# LIST ( FILTER SOURCE EXCLUDE REGEX ".*\\/\\..+")
# LIST ( FILTER HEADER EXCLUDE REGEX ".*\\/\\..+")

# LIST ( SORT SOURCE )
# LIST ( SORT HEADER )

# `juce_add_plugin` adds a static library target with the name passed as the first argument
# (AudioPluginExample here). This target is a normal CMake target, but has a lot of extra properties set
# up by default. As well as this shared code static library, this function adds targets for each of
# the formats specified by the FORMATS arguments. This function accepts many optional arguments.
# Check the readme at `docs/CMake API.md` in the JUCE repo for the full list.
string(APPEND PLUGIN_LABEL ${NUM_CHANNELS})

if(BUILD_STANDALONE)
	set(STANDALONE "Standalone")
endif(BUILD_STANDALONE)

if(BUILD_VST)
	set(VST "VST")
endif(BUILD_VST)

if(BUILD_VST3)
	set(VST3 "VST3")
endif(BUILD_VST3)

string(APPEND PLUGIN_CODE ${NUM_CHANNELS})

file(READ "${SRC_DIR}/osx_resources/MacOSXBundleInfo.plist" PLIST)

# if(APPLE AND STANDALONE)
# 	#prototype Info.plist
# 	# SET_TARGET_PROPERTIES(${SUBPROJECT_NAME}_Standalone PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${SRC_DIR}/osx_ressources/MacOSXBundleInfo.plist.in)

# 	SET_TARGET_PROPERTIES(${SUBPROJECT_NAME}_Standalone PROPERTIES BUNDLE TRUE)
# 	SET_TARGET_PROPERTIES(${SUBPROJECT_NAME}_Standalone PROPERTIES BUNDLE_EXTENSION app)
# 	SET_TARGET_PROPERTIES(${SUBPROJECT_NAME}_Standalone PROPERTIES MACOSX_BUNDLE_BUNDLE_VERSION ${VERSION})
# 	SET_TARGET_PROPERTIES(${SUBPROJECT_NAME}_Standalone PROPERTIES MACOSX_BUNDLE_SHORT_VERSION_STRING ${VERSION})
# 	SET_TARGET_PROPERTIES(${SUBPROJECT_NAME}_Standalone PROPERTIES MACOSX_BUNDLE_LONG_VERSION_STRING ${VERSION})
# 	SET_TARGET_PROPERTIES(${SUBPROJECT_NAME}_Standalone PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME ${SUBPROJECT_NAME})
# endif()
# message ("plist: ${PLIST}")

juce_add_plugin(${SUBPROJECT_NAME}
	PRODUCT_NAME 				${PLUGIN_LABEL}  		# The name of the final executable, which can differ from the target name
    VERSION 					${VERSION}          		# Set this if the plugin version is different to the project version
    # ICON_BIG ...                              			# ICON_* arguments specify a path to an image file to use as an icon for the Standalone
	# ICON_SMALL ...
	COMPANY_COPYRIGHT			"Matthias Kronlachner"
	COMPANY_NAME 				"kronlachner"       		# Specify the name of the plugin's author
	COMPANY_WEBSITE				"fasullo@123.com"
	COMPANY_EMAIL				"support@yourcompany.com"
    # IS_SYNTH TRUE/FALSE                       			# Is this a synth or an effect?
    # NEEDS_MIDI_INPUT TRUE/FALSE               			# Does the plugin need midi input?
    # NEEDS_MIDI_OUTPUT TRUE/FALSE              			# Does the plugin need midi output?
    # IS_MIDI_EFFECT TRUE/FALSE                 			# Is this plugin a MIDI effect?
    # EDITOR_WANTS_KEYBOARD_FOCUS TRUE/FALSE    			# Does the editor need keyboard focus?
	# COPY_PLUGIN_AFTER_BUILD TRUE/FALSE        			# Should the plugin be installed to a default location after building?
	# PLIST_TO_MERGE				${PLIST}
	FORMATS 					U ${VST3} ${VST} ${STANDALONE}   	# The formats to build. Other valid formats are: AAX Unity VST AU AUv3
	PLUGIN_MANUFACTURER_CODE 	${PLUGIN_MANUFACTURER_CODE}#'Kron'             			# A four-character manufacturer id with at least one upper-case character
	PLUGIN_CODE 				${PLUGIN_CODE}#'mc36'                  	# A unique four-character plugin id with at least one upper-case character
	DESCRIPTION 				"Plugin part of the MCFX Suite"
	VST2_CATEGORY				kPlugCategEffect
	VST3_CATEGORIES				tools
	UNITY_COPY_DIR			    "${SRC_DIR}/WinSetup/ToInstall"
	VST_COPY_DIR                "${SRC_DIR}/WinSetup/ToInstall"
)

# `juce_generate_juce_header` will create a JuceHeader.h for a given target, which will be generated
# into your build tree. This should be included with `#include <JuceHeader.h>`. The include path for
# this header will be automatically added to the target. The main function of the JuceHeader is to
# include all your JUCE module headers; if you're happy to include module headers directly, you
# probably don't need to call this.
juce_generate_juce_header(${SUBPROJECT_NAME})

# `target_sources` adds source files to a target. We pass the target that needs the sources as the
# first argument, then a visibility parameter for the sources (PRIVATE is normally best practice,
# although it doesn't really affect executable targets). Finally, we supply a list of source files
# that will be built into the target. This is a standard CMake command.
target_sources(${SUBPROJECT_NAME} PRIVATE
	${SOURCE}
	${HEADER}
)

# `target_compile_definitions` adds some preprocessor definitions to our target. In a Projucer
# project, these might be passed in the 'Preprocessor Definitions' field. JUCE modules also make use
# of compile definitions to switch certain features on/off, so if there's a particular feature you
# need that's not on by default, check the module header for the correct flag to set here. These
# definitions will be visible both to your code, and also the JUCE module code, so for new
# definitions, pick unique names that are unlikely to collide! This is a standard CMake command.

target_compile_definitions(${SUBPROJECT_NAME} PUBLIC
    # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
    JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
    JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
	JUCE_VST3_CAN_REPLACE_VST2=0
	JUCE_DISPLAY_SPLASH_SCREEN=0
	JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
	# JucePlugin_MaxNumInputChannels=${NUM_CHANNELS}
	# JucePlugin_MaxNumOutputChannels=${NUM_CHANNELS}
	# JUCE_MODULE_AVAILABLE_juce_audio_plugin_client=1
)

# If your target needs extra binary assets, you can add them here. The first argument is the name of
# a new static library target that will include all the binary resources. There is an optional
# `NAMESPACE` argument that can specify the namespace of the generated binary data class. Finally,
# the SOURCES argument should be followed by a list of source files that should be built into the
# static library. These source files can be of any kind (wav data, images, fonts, icons etc.).
# Conversion to binary-data will happen when your target is built.

# juce_add_binary_data(AudioPluginData SOURCES ...)

# `target_link_libraries` links libraries and JUCE modules to other libraries or executables. Here,
# we're linking our executable target to the `juce::juce_audio_utils` module. Inter-module
# dependencies are resolved automatically, so `juce_core`, `juce_events` and so on will also be
# linked automatically. If we'd generated a binary data target above, we would need to link to it
# here too. This is a standard CMake command.
target_link_libraries(${SUBPROJECT_NAME} PRIVATE
    # AudioPluginData           # If we'd created a binary data target, we'd link to it here
	juce::juce_audio_utils
	juce::juce_osc
	juce::juce_audio_plugin_client
)

include(addToBundle)

if(WITH_LIBSOXR)
	target_link_libraries(${SUBPROJECT_NAME} PRIVATE 
		${LIBSOXR_LIBRARIES}
	)
	add_library_to_apple_bundle(${SUBPROJECT_NAME}_Standalone "${LIBSOXR_LIBRARIES}")
	add_library_to_apple_bundle(${SUBPROJECT_NAME}_VST "${LIBSOXR_LIBRARIES}")
	add_library_to_apple_bundle(${SUBPROJECT_NAME}_VST3 "${LIBSOXR_LIBRARIES}")
endif(WITH_LIBSOXR)

# old way for copy libraries into bundle
# if(APPLE)
# 	IF( DEFINED OSX_COPY_LIB )
# 	#copy additional files (eg. dynamic libraries)
# 	ADD_CUSTOM_COMMAND(
# 		TARGET ${SUBPROJECT_NAME} POST_BUILD 
# 		COMMAND ${CMAKE_COMMAND} 
# 		ARGS -E copy ${SRC_DIR}/mac-libs/${OSX_COPY_LIB} ${BIN_DIR}/_bin/${SUBPROJECT_NAME}.vst/Contents/Frameworks/${OSX_COPY_LIB}
# 		)
# 	ENDIF( DEFINED OSX_COPY_LIB )

# endif(APPLE)

if(WITH_ZITA_CONVOLVER)
	target_link_libraries( ${SUBPROJECT_NAME} PRIVATE
		${LIBZITACONVOLVER_LIBRARIES}
	)
endif(WITH_ZITA_CONVOLVER)


if(WITH_FFTW3)
	# MESSAGE( STATUS "LINKING FFTW3F: " ${FFTW3F_LIBRARY} )
	target_link_libraries( ${SUBPROJECT_NAME} PRIVATE
		${FFTW3F_LIBRARY}
		#${FFTW3F_THREADS_LIBRARY}
	)
	juce_add_bundle_resources_directory(${SUBPROJECT_NAME}_Standalone "${SRC_DIR}/Bundle-resources/fftw3")
endif(WITH_FFTW3)

# ???????
# if(WIN32)
# 	IF (WITH_LIBLO OR WITH_FFTW3)
# 		target_link_libraries( ${SUBPROJECT_NAME} PRIVATE
# 			${CMAKE_THREAD_LIBS_INIT}
# 		)
# 	ENDIF (WITH_LIBLO OR WITH_FFTW3)
# endif(WIN32)