function(add_library_to_apple_bundle target file)

    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "Could not find library ${file}")
    endif()

	get_filename_component(file_name ${file} NAME)
	target_sources(${target} PRIVATE "${file}")
	set_source_files_properties("${file}" PROPERTIES
		MACOSX_PACKAGE_LOCATION "Frameworks"
	)
	add_custom_command(TARGET ${target}	PRE_BUILD 
		COMMAND 
			${CMAKE_INSTALL_NAME_TOOL} -id @rpath/${file_name} ${file}
	)
	get_filename_component(parent_dir ${file} DIRECTORY)
	add_custom_command(TARGET ${target}	POST_BUILD
		COMMAND 
			${CMAKE_INSTALL_NAME_TOOL} -rpath ${parent_dir} "@loader_path/../Frameworks/"
			$<TARGET_FILE:${target}>
	)
endfunction()