#[=======================================================================[.rst:
FindGcovr
--------

Find the gcovr tool.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``Gcovr::gcovr``, if
gcovr has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``Gcovr_FOUND``
  True if gcovr tool was found.
``Gcovr_EXECUTABLE``
  Path to where the gcovr executable is located.
``Gcovr_VERSION_STRING``
  The version of the gcovr tool, for example "7.2".

Functions
^^^^^^^^^

.. command:: gcovr_add_report_target
#]=======================================================================]
include(FindPackageHandleStandardArgs)

find_program(Gcovr_EXECUTABLE NAMES gcovr)
mark_as_advanced(Gcovr_EXECUTABLE)

if(Gcovr_EXECUTABLE AND NOT Gcovr_VERSION)
    execute_process(COMMAND "${Gcovr_EXECUTABLE}" --version
        OUTPUT_VARIABLE Gcovr_VERSION_STRING)
    string(REGEX MATCH "[0-9]+\\.[0-9]+" Gcovr_VERSION "${Gcovr_VERSION_STRING}")
    unset(Gcovr_VERSION_STRING)
endif()

find_package_handle_standard_args(Gcovr
    REQUIRED_VARS Gcovr_EXECUTABLE
    VERSION_VAR Gcovr_VERSION)

if(Gcovr_FOUND AND NOT TARGET Gcovr::gcovr)
    add_executable(Gcovr::gcovr IMPORTED)
    set_target_properties(Gcovr::gcovr PROPERTIES
        IMPORTED_LOCATION "${Gcovr_EXECUTABLE}")
endif()

function(gcovr_add_report_target)
    set(oneValueArgs DESTINATION FORMAT)
    set(multiValueArgs "")
    cmake_parse_arguments(ARGS "" "${oneValueArgs}" "" ${ARGN})
    if(ARGS_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown keywords: \"${ARGS_UNPARSED_ARGUMENTS}\"")
    endif()
endfunction()
