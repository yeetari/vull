cmake_minimum_required(VERSION 3.16)
project(vull CXX)

option(VULL_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(VULL_BUILD_TESTS "Build tests" OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

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
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.2
        GIT_SHALLOW ON
        GIT_PROGRESS ON)
    FetchContent_MakeAvailable(glfw3)
endif()
find_package(glm QUIET)
if(NOT TARGET glm::glm)
    FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 0.9.9.8
        GIT_SHALLOW ON
        GIT_PROGRESS ON)
    FetchContent_MakeAvailable(glm)
endif()
find_package(Threads REQUIRED)
find_package(Vulkan REQUIRED)
find_program(GLSLC glslc REQUIRED)

# TODO: Remove this policy, generator checks and use target_sources instead of set_property when the cmake minimum
#       version is bumped to >= 3.20.
if(CMAKE_VERSION VERSION_GREATER "3.19.0")
    cmake_policy(SET CMP0116 NEW)
endif()
function(target_add_shader target shader)
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/${shader})
    set(output ${CMAKE_CURRENT_BINARY_DIR}/${shader}.spv)
    if(CMAKE_GENERATOR MATCHES "Ninja")
        add_custom_command(
            OUTPUT ${output}
            COMMAND ${GLSLC} -MD -O -g ${source} -o ${output}
            DEPENDS ${source}
            DEPFILE ${output}.d
            VERBATIM)
    elseif(CMAKE_GENERATOR MATCHES "Unix Makefiles")
        add_custom_command(
            OUTPUT ${output}
            COMMAND ${GLSLC} -O -g ${source} -o ${output}
            DEPENDS ${source}
            IMPLICIT_DEPENDS CXX ${source}
            VERBATIM)
    else()
        add_custom_command(
            OUTPUT ${output}
            COMMAND ${GLSLC} -O -g ${source} -o ${output}
            DEPENDS ${source}
            VERBATIM)
    endif()
    set_property(TARGET ${target} APPEND PROPERTY SOURCES ${output})
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
add_subdirectory(tools)
