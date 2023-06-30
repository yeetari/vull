# >= 3.20 required for cmake_path and DEPFILE support with the Makefile generator
# >= 3.21 required for DEPFILE support with the Visual Studio generator
# >= 3.24 required for the X11::xcb_randr target
# >= 3.25 required for SYSTEM in FetchContent_Declare
cmake_minimum_required(VERSION 3.25)
project(vull VERSION 0.1.0 LANGUAGES ASM CXX)

# Set IPO policy for meshoptimizer.
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(VullModule)
include(VullVpak)

option(VULL_BUILD_GLTF "Build the vpak tool with glTF support" OFF)
option(VULL_BUILD_PNG "Build the vpak tool with PNG support" OFF)
option(VULL_BUILD_SANDBOX "Build sandbox" ON)
option(VULL_BUILD_TESTS "Build tests" OFF)

if(VULL_BUILD_TESTS)
    enable_testing()
endif()

find_package(Freetype)
find_package(harfbuzz)
find_package(X11)
find_package(xxHash REQUIRED)
find_package(Zstd REQUIRED)
find_program(GLSLC glslc REQUIRED)

function(add_shader name)
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

    # TODO: This shouldn't be needed.
    set_property(TARGET vull-vulkan APPEND PROPERTY SOURCES ${binary})
endfunction()

add_subdirectory(docs)
add_subdirectory(engine)
if(VULL_BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif()
add_subdirectory(tools)
