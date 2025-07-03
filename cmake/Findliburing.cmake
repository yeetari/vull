#[=======================================================================[.rst:
Findliburing
----------

Find the native liburing headers and libraries.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``liburing::liburing``, if
liburing has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``liburing_FOUND``
  True if liburing headers and library was found.
``liburing_INCLUDE_DIRS``
  Directory where liburing headers are located.
``liburing_LIBRARIES``
  liburing libraries to link against.
#]=======================================================================]

include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_liburing QUIET liburing)
endif()

find_path(liburing_INCLUDE_DIR
    NAMES liburing.h
    HINTS ${PC_liburing_INCLUDE_DIRS})
mark_as_advanced(liburing_INCLUDE_DIR)

if(NOT liburing_LIBRARY)
    find_library(liburing_LIBRARY
        NAMES uring
        HINTS ${PC_liburing_LIBRARY_DIRS})
    mark_as_advanced(liburing_LIBRARY)
endif()

find_package_handle_standard_args(liburing
    REQUIRED_VARS liburing_LIBRARY liburing_INCLUDE_DIR)

if(liburing_FOUND)
    set(liburing_LIBRARIES ${liburing_LIBRARY})
    set(liburing_INCLUDE_DIRS ${liburing_INCLUDE_DIR})

    if(NOT TARGET liburing::liburing)
        add_library(liburing::liburing UNKNOWN IMPORTED)
        set_target_properties(liburing::liburing PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${liburing_INCLUDE_DIRS}")

        if(EXISTS "${liburing_LIBRARY}")
            set_target_properties(liburing::liburing PROPERTIES
                IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                IMPORTED_LOCATION "${liburing_LIBRARY}")
        endif()
    endif()
endif()
