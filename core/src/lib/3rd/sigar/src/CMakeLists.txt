include_directories(../include/)

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
}
")
   CHECK_C_SOURCE_COMPILES("${_CHECK_STRUCT_MEMBER_SOURCE_CODE}" ${_RESULT})

endmacro (CHECK_STRUCT_MEMBER)

## linux
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(SIGAR_SRC os/linux/linux_sigar.c)
  include_directories(os/linux/)
  set(LIBSYS_NAME "linux")
endif(CMAKE_SYSTEM_NAME STREQUAL "Linux")

## macosx, freebsd
if(CMAKE_SYSTEM_NAME MATCHES "(Darwin|FreeBSD)")
  SET(SIGAR_SRC os/darwin/darwin_sigar.c)

  include_directories(os/darwin/)
  if(CMAKE_SYSTEM_NAME MATCHES "(Darwin)")
    add_definitions(-DDARWIN)
    set(SIGAR_LINK_FLAGS "-framework CoreServices -framework IOKit")
    set(LIBSYS_NAME "macosx")
else(CMAKE_SYSTEM_NAME MATCHES "(Darwin)")
    ## freebsd needs libkvm
    set(SIGAR_LINK_FLAGS "-lkvm")
    set(LIBSYS_NAME "freebsd")
  endif(CMAKE_SYSTEM_NAME MATCHES "(Darwin)")
endif(CMAKE_SYSTEM_NAME MATCHES "(Darwin|FreeBSD)")

## solaris
if (CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)" )
  set(SIGAR_SRC
	os/solaris/solaris_sigar.c
	os/solaris/get_mib2.c
	os/solaris/kstats.c
	os/solaris/procfs.c
  )

  include_directories(os/solaris/)
  add_definitions(-DSOLARIS)
  set(LIBSYS_NAME "solaris")
  set(SIGAR_LINK_FLAGS -lkstat -ldl -lnsl -lsocket -lresolv)
endif(CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)" )

## solaris
if (CMAKE_SYSTEM_NAME MATCHES "(hpux)" )
  set(SIGAR_SRC os/hpux/hpux_sigar.c)
  include_directories(os/hpux/)
  add_definitions(-DSIGAR_HPUX)
  set(SIGAR_LINK_FLAGS -lnm)
  set(LIBSYS_NAME "hpux")
endif(CMAKE_SYSTEM_NAME MATCHES "(hpux)" )

## aix
if (CMAKE_SYSTEM_NAME MATCHES "(AIX)" )
  set(SIGAR_SRC os/aix/aix_sigar.c)

  include_directories(os/aix/)
  set(SIGAR_LINK_FLAGS -lodm -lcfg)
  set(LIBSYS_NAME "aix")
endif(CMAKE_SYSTEM_NAME MATCHES "(AIX)" )

if(WIN32)
  add_definitions(-DSIGAR_SHARED)
  set(SIGAR_SRC os/win32/peb.c os/win32/win32_sigar.c)
  include_directories(os/win32)
  CHECK_STRUCT_MEMBER(MIB_IPADDRROW wType "windows.h;iphlpapi.h" wType_in_MIB_IPADDRROW)
  add_definitions(-DHAVE_MIB_IPADDRROW_WTYPE=${wType_in_MIB_IPADDRROW})
endif(WIN32)

file(REMOVE sigar_version.c)
file(READ sigar_version.c.in SIGAR_VERSION_FILE)
string(TIMESTAMP SIGAR_BUILD_DATE)
string(REPLACE "@@BUILD_DATE@@" ${SIGAR_BUILD_DATE} SIGAR_VERSION_FILE "${SIGAR_VERSION_FILE}")
string(REPLACE "@@BUILD_DATE@@" ${SIGAR_BUILD_DATE} SIGAR_VERSION_FILE "${SIGAR_VERSION_FILE}")
execute_process(COMMAND "git" "rev-parse" "HEAD" OUTPUT_VARIABLE SIGAR_REVISION)
string(REPLACE "\n" "" SIGAR_REVISION "${SIGAR_REVISION}")
string(REPLACE "@@SCM_REVISION@@" "${SIGAR_REVISION}" SIGAR_VERSION_FILE "${SIGAR_VERSION_FILE}")
string(REPLACE "@@VERSION_STRING@@" "${SIGAR_VERSION_MAJOR}.${SIGAR_VERSION_MINOR}.${SIGAR_VERSION_PATCH}" SIGAR_VERSION_FILE "${SIGAR_VERSION_FILE}")
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(ARCHNAME "amd64")
    string(REPLACE "@@ARCHNAME@@" "${ARCHNAME}" SIGAR_VERSION_FILE "${SIGAR_VERSION_FILE}")
else()
    string(REPLACE "@@ARCHNAME@@" "unknown" SIGAR_VERSION_FILE "${SIGAR_VERSION_FILE}")
endif()
string(REPLACE "@@ARCHLIB@@" "${CMAKE_SHARED_LIBRARY_PREFIX}sigar-${ARCHNAME}-${LIBSYS_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}" SIGAR_VERSION_FILE "${SIGAR_VERSION_FILE}")
string(REPLACE "@@ARCHLIB@@" "sigar-${ARCHNAME}-${LIBSYS_NAME}" SIGAR_VERSION_FILE "${SIGAR_VERSION_FILE}")

string(REPLACE "@@BINNAME@@" "libsigar.so" ${SIGAR_VERSION_FILE} ${SIGAR_VERSION_FILE})
string(REPLACE "@@VERSION_MAJOR@@" "${SIGAR_VERSION_MAJOR}" SIGAR_VERSION_FILE "${SIGAR_VERSION_FILE}")
string(REPLACE "@@VERSION_MINOR@@" "${SIGAR_VERSION_MINOR}" SIGAR_VERSION_FILE "${SIGAR_VERSION_FILE}")
string(REPLACE "@@VERSION_MAINT@@" "${SIGAR_VERSION_PATCH}" SIGAR_VERSION_FILE "${SIGAR_VERSION_FILE}")
string(REPLACE "@@VERSION_BUILD@@" "0" SIGAR_VERSION_FILE "${SIGAR_VERSION_FILE}")
file(WRITE sigar_version.c "${SIGAR_VERSION_FILE}")

set(SIGAR_SRC ${SIGAR_SRC}
  sigar.c
  sigar_cache.c
  sigar_fileinfo.c
  sigar_format.c
  sigar_getline.c
  sigar_ptql.c
  sigar_signal.c
  sigar_util.c
  sigar_version.c
)
add_library(sigar_obj OBJECT ${SIGAR_SRC})
set_property(TARGET sigar_obj PROPERTY POSITION_INDEPENDENT_CODE True)
add_library(sigar SHARED $<TARGET_OBJECTS:sigar_obj>)
add_library(sigar_static STATIC $<TARGET_OBJECTS:sigar_obj>)
add_custom_command(TARGET sigar POST_BUILD COMMAND "${CMAKE_COMMAND}" "-E" "copy" "$<TARGET_FILE:sigar>" "${CMAKE_BINARY_DIR}/build-tests/${CMAKE_BUILD_TYPE}/$<TARGET_FILE_NAME:sigar>")
if(WIN32)
	target_link_libraries(sigar ws2_32 netapi32 version)
	target_link_libraries(sigar_static ws2_32 netapi32 version)

endif(WIN32)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_options(sigar_obj PUBLIC -Wall -Werror)
endif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
if(SIGAR_LINK_FLAGS)
  set_target_properties(sigar PROPERTIES LINK_FLAGS "${SIGAR_LINK_FLAGS}")
endif(SIGAR_LINK_FLAGS)

install(TARGETS sigar
	RUNTIME DESTINATION bin
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	)
