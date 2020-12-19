find_program(GLSLC glslc)
if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found!")
endif()

function(target_add_shader target name)
    add_custom_command(
        TARGET ${target}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
    )
    add_custom_command(
        POST_BUILD
        TARGET ${target}
        COMMAND ${GLSLC} -O ${CMAKE_SOURCE_DIR}/${name} -o ${CMAKE_CURRENT_BINARY_DIR}/${name}.spv
    )
endfunction()
