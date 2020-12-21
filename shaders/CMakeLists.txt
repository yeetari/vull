find_program(GLSLC glslc)
if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found!")
endif()

function(target_add_shader target name)
    set(output ${CMAKE_CURRENT_BINARY_DIR}/${name}.spv)
    add_custom_command(
        TARGET ${target}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
    )
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${GLSLC} -O ${CMAKE_SOURCE_DIR}/${name} -o ${output}
        DEPENDS ${CMAKE_SOURCE_DIR}/${name}
        IMPLICIT_DEPENDS CXX ${CMAKE_SOURCE_DIR}/${name}
        VERBATIM
    )
    set_source_files_properties(${output} PROPERTIES GENERATED TRUE)
    target_sources(${target} PRIVATE ${output})
endfunction()
