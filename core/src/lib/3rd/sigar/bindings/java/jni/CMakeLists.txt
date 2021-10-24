cmake_minimum_required(VERSION 3.5)
project(sigar_jni)

# Path to main sigar library headers
include_directories(${CMAKE_SOURCE_DIR}/../../../include)

set(LINK_LIBRARIES "")

find_package(JNI)
if(JNI_FOUND)
    include_directories("${JNI_INCLUDE_DIRS}")
    set(LINK_LIBRARIES ${LINK_LIBRARIES} ${JNI_LIBRARIES})
else()
    message(FATAL_ERROR "JNI not found")
endif()
include_directories(include)

set(SRCS src/javasigar.c
    ../../../src/sigar.c
    ../../../src/sigar_cache.c
    ../../../src/sigar_fileinfo.c
    ../../../src/sigar_format.c
    ../../../src/sigar_getline.c
    ../../../src/sigar_ptql.c
    ../../../src/sigar_signal.c
    ../../../src/sigar_util.c
    ../../../src/sigar_version.c
)

include(CheckCSourceCompiles)
macro (CHECK_STRUCT_MEMBER _STRUCT _MEMBER _HEADER _RESULT)
    set(_INCLUDE_FILES)
    foreach (it ${_HEADER})
        set(_INCLUDE_FILES "${_INCLUDE_FILES}#include <${it}>\n")
    endforeach (it)

    set(_CHECK_STRUCT_MEMBER_SOURCE_CODE "
        ${_INCLUDE_FILES}
        int main()
        {
        static ${_STRUCT} tmp;
        if (sizeof(tmp.${_MEMBER}))
            return 0;
        return 0;
        }")
    CHECK_C_SOURCE_COMPILES("${_CHECK_STRUCT_MEMBER_SOURCE_CODE}" ${_RESULT})
endmacro (CHECK_STRUCT_MEMBER)
if(WIN32)
  add_definitions(-DSIGAR_SHARED)
  set(SRCS ${SRCS}
        win32/pdh.c
        win32/eventlog.c
        win32/util.c
        win32/nls.cpp
        win32/registrykey.cpp
        win32/service.cpp
        ../../../src/os/win32/peb.c 
        ../../../src/os/win32/win32_sigar.c)
  include_directories(os/win32)
  CHECK_STRUCT_MEMBER(MIB_IPADDRROW wType "windows.h;iphlpapi.h" wType_in_MIB_IPADDRROW)
  add_definitions(-DHAVE_MIB_IPADDRROW_WTYPE=${wType_in_MIB_IPADDRROW})
endif(WIN32)

set(LIBRARY_NAME "sigar")

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
    set(LIBRARY_NAME "${LIBRARY_NAME}-amd64")
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7l")
    set(LIBRARY_NAME "${LIBRARY_NAME}-armv7")
else()
    message(FATAL_ERROR "Processor architecture not supported: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LIBRARY_NAME "${LIBRARY_NAME}-linux")
    include_directories(${CMAKE_SOURCE_DIR}/../../../src/os/linux)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(LIBRARY_NAME "${LIBRARY_NAME}-darwin")
    include_directories(${CMAKE_SOURCE_DIR}/../../../src/os/darwin)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LIBRARY_NAME "${LIBRARY_NAME}-winnt")
    include_directories(${CMAKE_SOURCE_DIR}/../../../src/os/win32)
else()
    message(FATAL_ERROR "Operating system not supported: ${CMAKE_SYSTEM_NAME}")
endif()

message(STATUS "Library name: ${LIBRARY_NAME}")

set(CMAKE_CXX_FLAGS "-g")
add_library(${LIBRARY_NAME} SHARED ${SRCS})
if(WIN32)
	target_link_libraries(${LIBRARY_NAME} ws2_32 netapi32 version pdh)
endif(WIN32)
add_custom_command(TARGET ${LIBRARY_NAME} POST_BUILD COMMAND "${CMAKE_COMMAND}" "-E" "copy" "$<TARGET_FILE:${LIBRARY_NAME}>" "${CMAKE_SOURCE_DIR}/../src/main/resources/org/hyperic/sigar/$<TARGET_FILE_NAME:${LIBRARY_NAME}>")
target_link_libraries(${LIBRARY_NAME} ${LINK_LIBRARIES})