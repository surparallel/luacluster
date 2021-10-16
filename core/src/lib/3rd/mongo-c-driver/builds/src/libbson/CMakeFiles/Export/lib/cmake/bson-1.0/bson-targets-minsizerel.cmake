#----------------------------------------------------------------
# Generated CMake target import file for configuration "MinSizeRel".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "mongo::bson_shared" for configuration "MinSizeRel"
set_property(TARGET mongo::bson_shared APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)
set_target_properties(mongo::bson_shared PROPERTIES
  IMPORTED_IMPLIB_MINSIZEREL "${_IMPORT_PREFIX}/lib/bson-1.0.lib"
  IMPORTED_LOCATION_MINSIZEREL "${_IMPORT_PREFIX}/bin/bson-1.0.dll"
  )

list(APPEND _IMPORT_CHECK_TARGETS mongo::bson_shared )
list(APPEND _IMPORT_CHECK_FILES_FOR_mongo::bson_shared "${_IMPORT_PREFIX}/lib/bson-1.0.lib" "${_IMPORT_PREFIX}/bin/bson-1.0.dll" )

# Import target "mongo::bson_static" for configuration "MinSizeRel"
set_property(TARGET mongo::bson_static APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)
set_target_properties(mongo::bson_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_MINSIZEREL "C"
  IMPORTED_LOCATION_MINSIZEREL "${_IMPORT_PREFIX}/lib/bson-static-1.0.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS mongo::bson_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_mongo::bson_static "${_IMPORT_PREFIX}/lib/bson-static-1.0.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
