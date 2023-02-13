# >= 3.20 required for cmake_path and DEPFILE support with the Makefile generator
# >= 3.21 required for DEPFILE support with the Visual Studio generator
# >= 3.24 required for the X11::xcb_randr target
cmake_minimum_required(VERSION 3.24)
project(vull ASM CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(VULL_BUILD_GLTF "Build the vpak tool with glTF support" OFF)
option(VULL_BUILD_SANDBOX "Build sandbox" ON)
option(VULL_BUILD_TESTS "Build tests" OFF)
option(VULL_BUILD_WARNINGS "Build with warnings" OFF)

find_package(Freetype REQUIRED)
find_package(harfbuzz REQUIRED)
find_package(X11 REQUIRED)
find_package(Zstd REQUIRED)
find_program(GLSLC glslc REQUIRED)

function(target_add_shader target shader)
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/${shader})
    set(output ${CMAKE_CURRENT_BINARY_DIR}/${shader}.spv)
    cmake_path(GET output PARENT_PATH parent_dir)
    cmake_path(GET source EXTENSION extension)
    string(SUBSTRING ${extension} 1 -1 extension)
    string(TOUPPER ${extension} extension)
    file(MAKE_DIRECTORY ${parent_dir})
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${GLSLC} -MD -O -D${extension}_SHADER --target-env=vulkan1.3 ${source} -o ${output}
        DEPENDS ${source}
        DEPFILE ${output}.d
        VERBATIM)
    set_property(TARGET ${target} APPEND PROPERTY SOURCES ${output})
endfunction()

add_subdirectory(engine)
if(VULL_BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif()
add_subdirectory(tools)
