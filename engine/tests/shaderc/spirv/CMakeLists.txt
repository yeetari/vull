function(add_spirv_test name)
    add_test(NAME "shaderc.spirv.${name}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMAND "run_test.bash" "$<TARGET_FILE:vslc-new>" "${name}")
    if(${ARGC} GREATER 1)
        set_tests_properties("shaderc.spirv.${name}" PROPERTIES ${ARGN})
    endif()
endfunction()

add_spirv_test(dot)
add_spirv_test(ext_inst)
add_spirv_test(fragment_entry)
add_spirv_test(multiplication)
add_spirv_test(overload)
add_spirv_test(pipeline)
add_spirv_test(push_constant)
add_spirv_test(validation WILL_FAIL TRUE)
add_spirv_test(vertex_entry)
