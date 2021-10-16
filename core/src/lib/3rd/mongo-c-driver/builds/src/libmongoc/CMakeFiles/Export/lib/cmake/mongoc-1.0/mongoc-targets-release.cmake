#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "mongo::mongoc_shared" for configuration "Release"
set_property(TARGET mongo::mongoc_shared APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(mongo::mongoc_shared PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/mongoc-1.0.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/mongoc-1.0.dll"
  )

list(APPEND _IMPORT_CHECK_TARGETS mongo::mongoc_shared )
list(APPEND _IMPORT_CHECK_FILES_FOR_mongo::mongoc_shared "${_IMPORT_PREFIX}/lib/mongoc-1.0.lib" "${_IMPORT_PREFIX}/bin/mongoc-1.0.dll" )

# Import target "mongo::mongoc_static" for configuration "Release"
set_property(TARGET mongo::mongoc_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(mongo::mongoc_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/mongoc-static-1.0.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS mongo::mongoc_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_mongo::mongoc_static "${_IMPORT_PREFIX}/lib/mongoc-static-1.0.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
