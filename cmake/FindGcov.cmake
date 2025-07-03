#[=======================================================================[.rst:
FindGcov
--------

Find the gcov tool.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``Gcov::gcov``, if
gcov has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``Gcov_FOUND``
  True if gcov tool was found.
``Gcov_EXECUTABLE``
  Path to where the gcov executable is located.
``Gcov_VERSION_STRING``
  The version of the gcov tool, for example "14.2.0".

Functions
^^^^^^^^^

.. command:: gcov_append_compiler_flags
#]=======================================================================]
include(CheckCXXCompilerFlag)
include(FindPackageHandleStandardArgs)

find_program(Gcov_EXECUTABLE NAMES gcov)
mark_as_advanced(Gcov_EXECUTABLE)

if(Gcov_EXECUTABLE AND NOT Gcov_VERSION)
    execute_process(COMMAND "${Gcov_EXECUTABLE}" --version
        OUTPUT_VARIABLE Gcov_VERSION_STRING)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" Gcov_VERSION "${Gcov_VERSION_STRING}")
    unset(Gcov_VERSION_STRING)
endif()

find_package_handle_standard_args(Gcov
    REQUIRED_VARS Gcov_EXECUTABLE
    VERSION_VAR Gcov_VERSION)

if(Gcov_FOUND AND NOT TARGET Gcov::gcov)
    add_executable(Gcov::gcov IMPORTED)
    set_target_properties(Gcov::gcov PROPERTIES
        IMPORTED_LOCATION "${Gcov_EXECUTABLE}")
endif()

function(gcov_append_compiler_flags)
    check_cxx_compiler_flag("-fprofile-abs-path" HAVE_cxx_profile_abs_path)

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage" PARENT_SCOPE)
    if(HAVE_cxx_profile_abs_path)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-abs-path" PARENT_SCOPE)
    endif()
endfunction()
