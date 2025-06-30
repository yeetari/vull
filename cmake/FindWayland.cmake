#[=======================================================================[.rst:
FindWayland
-----------

Try to find Wayland and supporting tools.

This is a component-based find module, which makes use of the COMPONENTS
and OPTIONAL_COMPONENTS arguments to find_module. The following components
are available::

  client cursor egl protocols scanner server

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines the :prop_tgt:`IMPORTED` targets:

``Wayland::client``
  wayland-client native library.
``Wayland::cursor``
  wayland-cursor native library.
``Wayland::egl``
  wayland-egl native library.
``Wayland::scanner``
  wayland-scanner executable.
``Wayland::server``
  wayland-server native library.

Result Variables
^^^^^^^^^^^^^^^^

``Wayland_FOUND``
  True if all required Wayland components are found.
``Wayland_VERSION_STRING``
  Version number of Wayland.
``Wayland_<component>_FOUND``
  True if this component has been found.

.. TODO: The above is incomplete.

Functions
^^^^^^^^^

.. command:: wayland_client_protocol_add

  This function generates the header and client code for a provided
  Wayland protocol. The generated code can be added to a target's
  sources, or stored in a variable.

  ::

    wayland_client_protocol_add(targetOrSources
      PROTOCOL <path to protocol xml>
      BASENAME <protocol name>)

#]=======================================================================]
include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)

# Find native libraries
set(Wayland_KNOWN_LIBRARIES client cursor egl server)
foreach(library IN LISTS Wayland_KNOWN_LIBRARIES)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(PC_Wayland_${library} QUIET wayland-${library})
    endif()

    if(PC_Wayland_${library}_FOUND AND NOT Wayland_VERSION_STRING)
        set(Wayland_VERSION_STRING ${PC_Wayland_${library}_VERSION})
    endif()

    find_path(Wayland_${library}_INCLUDE_DIR
        NAMES wayland-${library}.h
        HINTS ${PC_Wayland_${library}_INCLUDE_DIR})
    mark_as_advanced(Wayland_${library}_INCLUDE_DIR)

    if(NOT Wayland_${library}_LIBRARY)
        find_library(Wayland_${library}_LIBRARY
            NAMES wayland-${library}
            HINTS ${PC_Wayland_${library}_LIBRARY_DIRS})
        mark_as_advanced(Wayland_${library}_LIBRARY)
    endif()

    if(Wayland_${library}_INCLUDE_DIR AND Wayland_${library}_LIBRARY)
        set(Wayland_${library}_FOUND TRUE)
    endif()
endforeach()

# Find wayland-protocols directory
pkg_check_modules(PC_Wayland_protocols QUIET wayland-protocols)
if(PC_Wayland_protocols_FOUND)
    pkg_get_variable(Wayland_protocols_dir wayland-protocols pkgdatadir)
    mark_as_advanced(Wayland_protocols_dir)
endif()
if(Wayland_protocols_dir)
    set(Wayland_protocols_FOUND TRUE)
endif()

# Find wayland-scanner
pkg_check_modules(PC_Wayland_scanner QUIET wayland-scanner)
if(PC_Wayland_protocols_FOUND)
    pkg_get_variable(PC_Wayland_scanner_EXECUTABLE_DIRS wayland-scanner bindir)
    mark_as_advanced(PC_Wayland_scanner_EXECUTABLE_DIRS)
endif()
find_program(Wayland_scanner_EXECUTABLE
    NAMES wayland-scanner
    HINTS ${PC_Wayland_scanner_EXECUTABLE_DIRS})
if(Wayland_scanner_EXECUTABLE)
    set(Wayland_scanner_FOUND TRUE)
endif()

find_package_handle_standard_args(Wayland
    VERSION_VAR Wayland_VERSION_STRING
    HANDLE_COMPONENTS)

# Create Wayland:: imported targets for native libraries.
foreach(library IN LISTS Wayland_KNOWN_LIBRARIES)
    if(Wayland_${library}_FOUND AND NOT TARGET Wayland::${library})
        add_library(Wayland::${library} UNKNOWN IMPORTED)
        set_target_properties(Wayland::${library} PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${Wayland_${library}_INCLUDE_DIR}"
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${Wayland_${library}_LIBRARY}")
    endif()
endforeach()

# Create Wayland::scanner target
if(Wayland_scanner_FOUND AND NOT TARGET Wayland::scanner)
    add_executable(Wayland::scanner IMPORTED)
    set_target_properties(Wayland::scanner PROPERTIES
        IMPORTED_LOCATION "${Wayland_scanner_EXECUTABLE}")
endif()

# Help helper.
function(wayland_client_protocol_add target_or_sources_var)
    set(oneValueArgs PROTOCOL BASENAME)
    cmake_parse_arguments(ARGS "" "${oneValueArgs}" "" ${ARGN})

    if(ARGS_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown keywords: \"${ARGS_UNPARSED_ARGUMENTS}\"")
    endif()

    get_filename_component(_infile ${ARGS_PROTOCOL} ABSOLUTE)
    set(_client_header "${CMAKE_CURRENT_BINARY_DIR}/wayland-${ARGS_BASENAME}-client-protocol.h")
    set(_client_impl "${CMAKE_CURRENT_BINARY_DIR}/wayland-${ARGS_BASENAME}-protocol.c")
    set_source_files_properties("${_client_header}" "${_client_impl}" GENERATED)

    add_custom_command(OUTPUT "${_client_header}"
        COMMAND Wayland::scanner client-header "${_infile}" "${_client_header}"
        DEPENDS "${_infile}" VERBATIM)
    add_custom_command(OUTPUT "${_client_impl}"
        COMMAND Wayland::scanner public-code "${_infile}" "${_client_impl}"
        DEPENDS "${_infile}" "${_client_header}" VERBATIM)

    set_source_files_properties("${_client_header}" "${_client_impl}" PROPERTIES
        COMPILE_FLAGS -w
        GENERATED ON
    )

    if(TARGET ${target_or_sources_var})
        target_sources(${target_or_sources_var} PRIVATE "${_client_header}" "${_client_impl}")
        target_include_directories(${target_or_sources_var} SYSTEM PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
    else()
        list(APPEND ${target_or_sources_var} "${_client_header}" "${_client_impl}")
        set(${target_or_sources_var} ${${target_or_sources_var}} PARENT_SCOPE)
    endif()
endfunction()
