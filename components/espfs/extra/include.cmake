function(target_add_espfs target name)
    if(${ARGC} GREATER 1)
        set(dir ${ARGV2})
    else()
        set(dir ${name})
    endif()

    if(IS_ABSOLUTE ${dir})
        file(RELATIVE_PATH dir ${PROJECT_SOURCE_DIR} ${dir})
    endif()

    set(output ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${target}.dir/${name})
    file(RELATIVE_PATH rel_output ${CMAKE_CURRENT_BINARY_DIR} ${output})

    get_filename_component(output_dir ${output} DIRECTORY)

    add_custom_target(espfs_image_${name}
        BYPRODUCTS ${dir}/espfs.paths
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/libespfs.dir/requirements.stamp
        COMMAND ${python} ${libespfs_DIR}/tools/pathlist.py ${dir}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Updating pathlist for espfs_image_${name}"
        VERBATIM
    )
    add_dependencies(${target} espfs_image_${name})

    add_custom_command(OUTPUT ${output}.bin
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir}
        COMMAND ${python} ${libespfs_DIR}/tools/mkespfsimage.py ${dir} ${output}.bin
        DEPENDS ${dir}/espfs.paths
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Building espfs binary ${rel_output}"
        USES_TERMINAL
        VERBATIM
    )

    add_custom_command(OUTPUT ${output}.bin.c
        COMMAND ${python} ${libespfs_DIR}/tools/bin2c.py ${output}.bin ${output}.bin.c
        DEPENDS ${output}.bin
        COMMENT "Building source file ${rel_output}.c"
        VERBATIM
    )
    target_sources(${target} PRIVATE ${output}.bin.c)
endfunction()