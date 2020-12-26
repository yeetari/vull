cmake_minimum_required(VERSION 3.19)
project(vull CXX)

find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Vulkan REQUIRED)
find_program(GLSLC glslc REQUIRED)

# TODO: Make this generate and use .d files.
function(target_add_shader target shader)
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/${shader})
    set(output ${CMAKE_CURRENT_BINARY_DIR}/${shader}.spv)
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${GLSLC} -O ${source} -o ${output}
        DEPENDS ${source}
        DEPENDS ${CMAKE_SOURCE_DIR}/engine/shaders/common.glsl # TODO: This isn't right.
        VERBATIM)
    add_custom_target(${shader}-target DEPENDS ${output})
    add_dependencies(${target} ${shader}-target)
endfunction()

# TODO: Enable fast math for MSVC too.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-ffast-math" COMPILER_SUPPORTS_FAST_MATH)
function(target_enable_optimisations target)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        if(COMPILER_SUPPORTS_FAST_MATH)
            target_compile_options(${target} PRIVATE "-ffast-math")
            target_link_libraries(${target} PRIVATE "-ffast-math")
        endif()
        if(NOT DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION)
            set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)
        endif()
    endif()
endfunction()

add_subdirectory(engine)
add_subdirectory(sandbox)
