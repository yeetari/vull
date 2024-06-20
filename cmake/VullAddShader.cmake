function(vull_add_shader name)
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/${name})
    set(binary ${CMAKE_CURRENT_BINARY_DIR}/${name}.spv)
    cmake_path(GET source EXTENSION extension)
    cmake_path(GET binary PARENT_PATH binary_dir)
    file(MAKE_DIRECTORY ${binary_dir})
    if(${extension} STREQUAL ".vsl")
        # Vull shader.
        add_custom_command(
            OUTPUT ${binary}
            COMMAND vslc ${source} ${binary}
            DEPENDS ${source}
            VERBATIM)
    else()
        # GLSL.
        add_custom_command(
            OUTPUT ${binary}
            COMMAND ${GLSLC} -MD --target-env=vulkan1.3 ${source} -o ${binary}
            DEPENDS ${source}
            DEPFILE ${binary}.d
            VERBATIM)
    endif()

    string(REPLACE "/" "_" target_name ${name})
    add_custom_target(${target_name} DEPENDS ${binary})
endfunction()
