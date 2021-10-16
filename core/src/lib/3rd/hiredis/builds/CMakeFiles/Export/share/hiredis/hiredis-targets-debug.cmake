#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "hiredis::hiredis" for configuration "Debug"
set_property(TARGET hiredis::hiredis APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(hiredis::hiredis PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/lib/hiredisd.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/hiredisd.dll"
  )

list(APPEND _IMPORT_CHECK_TARGETS hiredis::hiredis )
list(APPEND _IMPORT_CHECK_FILES_FOR_hiredis::hiredis "${_IMPORT_PREFIX}/lib/hiredisd.lib" "${_IMPORT_PREFIX}/bin/hiredisd.dll" )

# Import target "hiredis::hiredis_static" for configuration "Debug"
set_property(TARGET hiredis::hiredis_static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(hiredis::hiredis_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/hiredis_staticd.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS hiredis::hiredis_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_hiredis::hiredis_static "${_IMPORT_PREFIX}/lib/hiredis_staticd.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
