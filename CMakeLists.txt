# >= 3.20 required for cmake_path and DEPFILE support with the Makefile generator
# >= 3.21 required for DEPFILE support with the Visual Studio generator
# >= 3.24 required for the X11::xcb_randr target
# >= 3.25 required for SYSTEM in FetchContent_Declare
cmake_minimum_required(VERSION 3.25)
project(vull VERSION 0.1.0 LANGUAGES ASM CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CMakeDependentOption)
include(FetchContent)
include(VullAddExecutable)
include(VullAddShader)
include(VullVpak)

set(VULL_ALL_COMPONENTS "GRAPHICS;PHYSICS;SCRIPT;UI;X11_WINDOW")
set(VULL_ENABLED_COMPONENTS "${VULL_ALL_COMPONENTS}" CACHE STRING "")

# Ensure enabled components are valid.
foreach(component ${VULL_ENABLED_COMPONENTS})
    if(NOT "${component}" IN_LIST VULL_ALL_COMPONENTS)
        message(FATAL_ERROR "Unknown vull component ${component}")
    endif()
endforeach()

# Set variables for enabled components.
foreach(component ${VULL_ALL_COMPONENTS})
    if("${component}" IN_LIST VULL_ENABLED_COMPONENTS)
        set(VULL_BUILD_${component} TRUE)
    else()
        set(VULL_BUILD_${component} FALSE)
    endif()
endforeach()

if(VULL_BUILD_UI AND NOT VULL_BUILD_GRAPHICS)
    message(FATAL_ERROR "Cannot build UI component with graphics component")
endif()

# TODO: Make vpak tool not require graphics component.
cmake_dependent_option(VULL_BUILD_VPAK "Build the vpak tool" ON "VULL_BUILD_GRAPHICS" OFF)
cmake_dependent_option(VULL_BUILD_SANDBOX "Build the vull sandbox" ${PROJECT_IS_TOP_LEVEL}
    "VULL_BUILD_GRAPHICS;VULL_BUILD_PHYSICS;VULL_BUILD_UI;VULL_BUILD_X11_WINDOW;VULL_BUILD_VPAK" OFF)
option(VULL_BUILD_TESTS "Build the vull tests" ${PROJECT_IS_TOP_LEVEL})
option(VULL_ENABLE_COVERAGE "Enable code coverage for testing" OFF)

if(VULL_ENABLE_COVERAGE)
    find_package(Gcov REQUIRED)
    gcov_append_compiler_flags()

    find_package(Gcovr REQUIRED)
    add_custom_target(coverage.xml
      COMMAND Gcovr::gcovr
        --cobertura "${CMAKE_CURRENT_BINARY_DIR}/coverage.xml"
        --gcov-ignore-parse-errors
        --filter "engine/(include|sources)"
        --verbose
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      VERBATIM)
endif()

find_package(liburing REQUIRED)
find_package(xxHash REQUIRED)
find_package(Zstd REQUIRED)
find_program(GLSLC glslc REQUIRED)

if(VULL_BUILD_UI)
    find_package(Freetype REQUIRED)
    find_package(harfbuzz REQUIRED)
endif()

if(VULL_BUILD_X11_WINDOW)
    find_package(X11 REQUIRED)
endif()

if(VULL_BUILD_VPAK)
    find_package(PNG REQUIRED)
endif()

if(VULL_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(docs)
add_subdirectory(engine)
if(VULL_BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif()
add_subdirectory(tools)
