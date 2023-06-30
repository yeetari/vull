#[=======================================================================[.rst:
FindxxHash
----------

Find the native xxHash headers and libraries.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``xxHash::xxHash``, if
xxHash has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``xxHash_FOUND``
  True if xxHash headers and library was found.
``xxHash_INCLUDE_DIRS``
  Directory where xxHash headers are located.
``xxHash_LIBRARIES``
  xxHash libraries to link against.
#]=======================================================================]

include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_xxHash QUIET libxxhash)
endif()

find_path(xxHash_INCLUDE_DIR
    NAMES xxhash.h
    HINTS ${PC_xxHash_INCLUDE_DIRS})
mark_as_advanced(xxHash_INCLUDE_DIR)

if(NOT xxHash_LIBRARY)
    find_library(xxHash_LIBRARY
        NAMES xxhash
        HINTS ${PC_xxHash_LIBRARY_DIRS})
    mark_as_advanced(xxHash_LIBRARY)
endif()

find_package_handle_standard_args(xxHash
    REQUIRED_VARS xxHash_LIBRARY xxHash_INCLUDE_DIR)

if(xxHash_FOUND)
    set(xxHash_LIBRARIES ${xxHash_LIBRARY})
    set(xxHash_INCLUDE_DIRS ${xxHash_INCLUDE_DIR})

    if(NOT TARGET xxHash::xxHash)
        add_library(xxHash::xxHash UNKNOWN IMPORTED)
        set_target_properties(xxHash::xxHash PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${xxHash_INCLUDE_DIRS}")

        if(EXISTS "${xxHash_LIBRARY}")
            set_target_properties(xxHash::xxHash PROPERTIES
                IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                IMPORTED_LOCATION "${xxHash_LIBRARY}")
        endif()
    endif()
endif()
