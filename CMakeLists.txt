cmake_minimum_required(VERSION 3.19)
project(vull CXX)

include(FetchContent)
option(VULL_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(VULL_BUILD_TESTS "Build tests" OFF)

if(VULL_BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED)
endif()
if(VULL_BUILD_TESTS)
    find_package(GTest REQUIRED)
    include(GoogleTest)
    enable_testing()
endif()
find_package(glfw3 QUIET)
if(NOT TARGET glfw)
    FetchContent_Declare(glfw3
        URL https://github.com/glfw/glfw/archive/3.3.2.tar.gz
        URL_MD5 865e54ff0a100e9041a40429db98be0b)
    FetchContent_MakeAvailable(glfw3)
endif()
find_package(glm QUIET)
if(NOT TARGET glm::glm)
    FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 0.9.9.8)
    FetchContent_MakeAvailable(glm)
endif()
find_package(Threads REQUIRED)
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
