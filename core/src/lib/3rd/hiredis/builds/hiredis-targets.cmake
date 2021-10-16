# Generated by CMake

if("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}" LESS 2.5)
   message(FATAL_ERROR "CMake >= 2.6.0 required")
endif()
cmake_policy(PUSH)
cmake_policy(VERSION 2.6...3.19)
#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Protect against multiple inclusion, which would fail when already imported targets are added once more.
set(_targetsDefined)
set(_targetsNotDefined)
set(_expectedTargets)
foreach(_expectedTarget hiredis::hiredis hiredis::hiredis_static)
  list(APPEND _expectedTargets ${_expectedTarget})
  if(NOT TARGET ${_expectedTarget})
    list(APPEND _targetsNotDefined ${_expectedTarget})
  endif()
  if(TARGET ${_expectedTarget})
    list(APPEND _targetsDefined ${_expectedTarget})
  endif()
endforeach()
if("${_targetsDefined}" STREQUAL "${_expectedTargets}")
  unset(_targetsDefined)
  unset(_targetsNotDefined)
  unset(_expectedTargets)
  set(CMAKE_IMPORT_FILE_VERSION)
  cmake_policy(POP)
  return()
endif()
if(NOT "${_targetsDefined}" STREQUAL "")
  message(FATAL_ERROR "Some (but not all) targets in this export set were already defined.\nTargets Defined: ${_targetsDefined}\nTargets not yet defined: ${_targetsNotDefined}\n")
endif()
unset(_targetsDefined)
unset(_targetsNotDefined)
unset(_expectedTargets)


# Create imported target hiredis::hiredis
add_library(hiredis::hiredis SHARED IMPORTED)

set_target_properties(hiredis::hiredis PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "D:/workround/grypania/core/src/lib/3rd/hiredis"
  INTERFACE_LINK_LIBRARIES "ws2_32;crypt32"
)

# Create imported target hiredis::hiredis_static
add_library(hiredis::hiredis_static STATIC IMPORTED)

set_target_properties(hiredis::hiredis_static PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "D:/workround/grypania/core/src/lib/3rd/hiredis"
  INTERFACE_LINK_LIBRARIES "ws2_32;crypt32"
)

# Import target "hiredis::hiredis" for configuration "Debug"
set_property(TARGET hiredis::hiredis APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(hiredis::hiredis PROPERTIES
  IMPORTED_IMPLIB_DEBUG "D:/workround/grypania/core/src/lib/3rd/hiredis/builds/Debug/hiredisd.lib"
  IMPORTED_LOCATION_DEBUG "D:/workround/grypania/core/src/lib/3rd/hiredis/builds/Debug/hiredisd.dll"
  )

# Import target "hiredis::hiredis_static" for configuration "Debug"
set_property(TARGET hiredis::hiredis_static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(hiredis::hiredis_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "D:/workround/grypania/core/src/lib/3rd/hiredis/builds/Debug/hiredis_staticd.lib"
  )

# Import target "hiredis::hiredis" for configuration "Release"
set_property(TARGET hiredis::hiredis APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(hiredis::hiredis PROPERTIES
  IMPORTED_IMPLIB_RELEASE "D:/workround/grypania/core/src/lib/3rd/hiredis/builds/Release/hiredis.lib"
  IMPORTED_LOCATION_RELEASE "D:/workround/grypania/core/src/lib/3rd/hiredis/builds/Release/hiredis.dll"
  )

# Import target "hiredis::hiredis_static" for configuration "Release"
set_property(TARGET hiredis::hiredis_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(hiredis::hiredis_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "D:/workround/grypania/core/src/lib/3rd/hiredis/builds/Release/hiredis_static.lib"
  )

# Import target "hiredis::hiredis" for configuration "MinSizeRel"
set_property(TARGET hiredis::hiredis APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)
set_target_properties(hiredis::hiredis PROPERTIES
  IMPORTED_IMPLIB_MINSIZEREL "D:/workround/grypania/core/src/lib/3rd/hiredis/builds/MinSizeRel/hiredis.lib"
  IMPORTED_LOCATION_MINSIZEREL "D:/workround/grypania/core/src/lib/3rd/hiredis/builds/MinSizeRel/hiredis.dll"
  )

# Import target "hiredis::hiredis_static" for configuration "MinSizeRel"
set_property(TARGET hiredis::hiredis_static APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)
set_target_properties(hiredis::hiredis_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_MINSIZEREL "C"
  IMPORTED_LOCATION_MINSIZEREL "D:/workround/grypania/core/src/lib/3rd/hiredis/builds/MinSizeRel/hiredis_static.lib"
  )

# Import target "hiredis::hiredis" for configuration "RelWithDebInfo"
set_property(TARGET hiredis::hiredis APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(hiredis::hiredis PROPERTIES
  IMPORTED_IMPLIB_RELWITHDEBINFO "D:/workround/grypania/core/src/lib/3rd/hiredis/builds/RelWithDebInfo/hiredis.lib"
  IMPORTED_LOCATION_RELWITHDEBINFO "D:/workround/grypania/core/src/lib/3rd/hiredis/builds/RelWithDebInfo/hiredis.dll"
  )

# Import target "hiredis::hiredis_static" for configuration "RelWithDebInfo"
set_property(TARGET hiredis::hiredis_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(hiredis::hiredis_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "C"
  IMPORTED_LOCATION_RELWITHDEBINFO "D:/workround/grypania/core/src/lib/3rd/hiredis/builds/RelWithDebInfo/hiredis_static.lib"
  )

# This file does not depend on other imported targets which have
# been exported from the same project but in a separate export set.

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
cmake_policy(POP)
