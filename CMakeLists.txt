# 3.21 required for the use of DEPFILE with both the Makefile and Visual Studio Generators.
cmake_minimum_required(VERSION 3.21)
project(vull CXX)

option(VULL_BUILD_SANDBOX "Build sandbox" ON)
option(VULL_BUILD_TESTS "Build tests" OFF)
option(VULL_BUILD_WARNINGS "Build with warnings" OFF)

find_package(Freetype REQUIRED)
find_package(harfbuzz REQUIRED)
find_package(X11 REQUIRED)
find_program(GLSLC glslc REQUIRED)

function(target_add_shader target shader)
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/${shader})
    set(output ${CMAKE_CURRENT_BINARY_DIR}/${shader}.spv)
    cmake_path(GET output PARENT_PATH parent_dir)
    file(MAKE_DIRECTORY ${parent_dir})
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${GLSLC} -MD -O -g --target-env=vulkan1.2 ${source} -o ${output}
        DEPENDS ${source}
        DEPFILE ${output}.d
        VERBATIM)
    set_property(TARGET ${target} APPEND PROPERTY SOURCES ${output})
endfunction()

add_subdirectory(engine)
if(VULL_BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif()
