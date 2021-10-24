# Copyright (C) 2007-2013 LuaDist.
# Created by Peter Drahoš, David Manura
# Redistribution and use of this file is allowed according to the terms of the MIT license.
# For details see the COPYRIGHT file distributed with LuaDist.
# Please note that the package source code is licensed under its own license.

project ( luasocket C )
cmake_minimum_required ( VERSION 2.8.12 )
include ( cmake/dist.cmake )
#include ( lua )

# Configuration
option ( LUASOCKET_UNIX "Use only unix domain features." OFF )

# The standard luasocket build has this enabled in release mode.
# It is required for running the test suite.
add_definitions ( -DLUASOCKET_DEBUG )
if ( MINGW )
	add_definitions ( -DLUASOCKET_INET_PTON -D_WIN32_WINNT=0x501 )
endif ( )

# Shared library exporting.
if ( WIN32 )
  add_definitions ( "-DMIME_API=__declspec(dllexport)" "-DLUASOCKET_API=__declspec(dllexport)" )
endif ( )

if ( WIN32 AND NOT CYGWIN )
  set ( SOCKETC src/wsocket.c )
else ( )
  set ( SOCKETC src/usocket.c )
endif ( )

set ( SRC_SOCKET src/luasocket.c src/timeout.c src/buffer.c src/io.c src/auxiliar.c 
  src/options.c src/inet.c src/tcp.c src/udp.c src/except.c src/select.c ${SOCKETC} )
  
set ( UNIX_SOCKET src/unix.c src/io.c src/auxiliar.c src/timeout.c src/options.c
  src/buffer.c ${SOCKETC} )

if ( WIN32 AND NOT CYGWIN )
  set ( LIB_SOCKET ws2_32 )
endif ()

# Solaris needs specific links
if ( CMAKE_SYSTEM_NAME STREQUAL "SunOS" )
	set ( LIB_SOCKET socket resolv rt )
endif ( )

# Binary
#install_lua_module ( socket.core ${SRC_SOCKET} LINK ${LIB_SOCKET} )
#install_lua_module ( mime.core src/mime.c )
add_library(socket STATIC ${SRC_SOCKET})
target_link_libraries(socket ${LIB_SOCKET})
add_library(mime STATIC src/mime.c)
# Lua 
#install_lua_module ( ltn12 src/ltn12.lua )
#install_lua_module ( mime src/mime.lua )
#install_lua_module ( socket src/socket.lua )
#install_lua_module ( socket.ftp src/ftp.lua )
#install_lua_module ( socket.http src/http.lua )
#install_lua_module ( socket.smtp src/smtp.lua )
#install_lua_module ( socket.tp src/tp.lua )
#install_lua_module ( socket.url src/url.lua )
#install_lua_module ( socket.headers src/headers.lua )

if ( LUASOCKET_UNIX )
  #install_lua_module ( socket.unix ${UNIX_SOCKET} LINK ${LIB_SOCKET} )
endif ()

install_foo ( etc/ )
install_test ( test/ )
install_example ( samples/ )
install_doc ( doc/ )
install_data ( README NEW LICENSE )
