cmake_minimum_required(VERSION 3.5)
project(sigar C)
cmake_policy(SET CMP0053 NEW)

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckTypeSize)
include(CTest)
include(InstallRequiredSystemLibraries)

enable_testing()

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug")
endif()

file(STRINGS ${PROJECT_SOURCE_DIR}/version.properties PROJECT_PROPERTIES)
list(LENGTH PROJECT_PROPERTIES PROJECT_PROPERTIES_LENGTH)
foreach(PROJECT_PROPERTY IN LISTS PROJECT_PROPERTIES)
	if(NOT PROJECT_PROPERTY STREQUAL "")
		string(REPLACE "=" ";" PROJECT_PROPERTY_PARTS ${PROJECT_PROPERTY})
		list(GET PROJECT_PROPERTY_PARTS 0 PROP_KEY)
		list(GET PROJECT_PROPERTY_PARTS 1 PROP_VALUE)
		if(PROP_KEY STREQUAL "version.major")
            set(SIGAR_VERSION_MAJOR ${PROP_VALUE})
		endif()
		if(PROP_KEY STREQUAL "version.minor")
			set(SIGAR_VERSION_MINOR ${PROP_VALUE})
		endif()
		if(PROP_KEY STREQUAL "version.maint")
			set(SIGAR_VERSION_PATCH ${PROP_VALUE})
		endif()
	endif()
endforeach()

# CPack Configuration ##########################################################
set(CPACK_PACKAGE_VERSION_MAJOR ${SIGAR_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${SIGAR_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${SIGAR_VERSION_PATCH})
set(CPACK_PACKAGE_VENDOR "Hyperic")
include(CPack)
################################################################################

if(WIN32)
	## make sure we only use the smallest set of
	## headers on win32. Otherwise we get clashes
	## between winsock2.h and winsock.h
	add_definitions(-DWIN32_LEAN_AND_MEAN)

	# force the correct version for the redist manifest
	add_definitions(-D_BIND_TO_CURRENT_MFC_VERSION=1)
	add_definitions(-D_BIND_TO_CURRENT_CRT_VERSION=1)

	# turn off security warnings for system calls
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif(WIN32)

add_subdirectory(src build-src)
add_subdirectory(tests build-tests)
add_subdirectory(include build-include)
#add_subdirectory(info_dump build-info_dump)
