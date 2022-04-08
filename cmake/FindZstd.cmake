#[=======================================================================[.rst:
FindZstd
--------

Find the native Zstd headers and libraries.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``Zstd::Zstd``, if
Zstd has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``Zstd_FOUND``
  True if Zstd headers and library was found.
``Zstd_INCLUDE_DIRS``
  Directory where Zstd headers are located.
``Zstd_LIBRARIES``
  Zstd libraries to link against.
``Zstd_VERSION_STRING``
  The version of the Zstd library as a string (ex: "1.5.2").
#]=======================================================================]

include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_Zstd QUIET libzstd)
  if(PC_Zstd_FOUND)
    set(Zstd_VERSION_STRING ${PC_Zstd_VERSION})
  endif()
endif()

find_path(Zstd_INCLUDE_DIR
  NAMES zstd.h
  HINTS ${PC_Zstd_INCLUDE_DIRS})
mark_as_advanced(Zstd_INCLUDE_DIR)

if(NOT Zstd_LIBRARY)
  find_library(Zstd_LIBRARY
    NAMES zstd
    HINTS ${PC_Zstd_LIBRARY_DIRS})
  mark_as_advanced(Zstd_LIBRARY)
endif()

if(Zstd_INCLUDE_DIR AND NOT Zstd_VERSION_STRING)
  if(EXISTS "${Zstd_INCLUDE_DIR}/zstd.h")
    foreach(_Zstd_version_component MAJOR MINOR RELEASE)
      file(STRINGS
	"${Zstd_INCLUDE_DIR}/zstd.h"
	Zstd_${_zstd_version_component}_str
	REGEX "^#define ZSTD_VERSION_${_Zstd_version_component}")
      string(REGEX REPLACE "^#define ZSTD_VERSION_${_Zstd_version_component}.+([0-9]+)" "\\1" Zstd_${_Zstd_version_component} "${Zstd_${_zstd_version_component}_str}")
      unset(Zstd_${_zstd_version_component}_str)
    endforeach()
    set(Zstd_VERSION_STRING "${Zstd_MAJOR}.${Zstd_MINOR}.${Zstd_RELEASE}")
    unset(Zstd_MAJOR)
    unset(Zstd_MINOR)
    unset(Zstd_RELEASE)
  endif()
endif()

find_package_handle_standard_args(Zstd
  REQUIRED_VARS Zstd_LIBRARY Zstd_INCLUDE_DIR
  VERSION_VAR Zstd_VERSION_STRING)

if(Zstd_FOUND)
  set(Zstd_LIBRARIES ${Zstd_LIBRARY})
  set(Zstd_INCLUDE_DIRS ${Zstd_INCLUDE_DIR})

  if(NOT TARGET Zstd::Zstd)
    add_library(Zstd::Zstd UNKNOWN IMPORTED)
    set_target_properties(Zstd::Zstd PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${Zstd_INCLUDE_DIRS}")

    if(EXISTS "${Zstd_LIBRARY}")
      set_target_properties(Zstd::Zstd PROPERTIES
	IMPORTED_LINK_INTERFACE_LANGUAGES "C"
	IMPORTED_LOCATION "${Zstd_LIBRARY}")
    endif()
  endif()
endif()
