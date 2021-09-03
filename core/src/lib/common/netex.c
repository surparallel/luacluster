
/* docker.c - worker thread
*
* Copyright(C) 2021 - 2022, sun shuo <sun.shuo@surparallel.org>
* All rights reserved.
*
* This program is free software : you can redistribute it and / or modify
* it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or(at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
* GNU Affero General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with this program.If not, see < https://www.gnu.org/licenses/>.
*/

#ifdef _WIN32
#include "Win32_Portability.h"
#include "win32_types.h"
#include "win32fixes.h"
#include "win32_wsiocp2.h"
#define ANET_NOTUSED(V) V
#include <Mstcpip.h>
#endif

#include "fmacros.h"

#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#endif
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include "sigar.h"

int anetUdpSockName(int fd, unsigned int* ip, unsigned short* port) {
    struct sockaddr_storage sa;
    socklen_t salen = sizeof(sa);

    if (IF_WIN32(w_getsockname, getsockname)(fd, (struct sockaddr*)&sa, &salen) == -1) {
        if (port) *port = 0;
        ip[0] = '?';
        ip[1] = '\0';
        return -1;
    }
    if (sa.ss_family == AF_INET) {
        struct sockaddr_in* s = (struct sockaddr_in*)&sa;
        if (ip) *ip = ntohl(s->sin_addr.S_un.S_addr);
        if (port) *port = ntohs(s->sin_port);
    }
    else {
        if (port) *port = 0;
        ip[0] = '?';
        ip[1] = '\0';
        return -1;
    }
    return 0;
}

//如果ip为空则获得默认的设备ip
void IntConfigPrimary(int fd, unsigned int* ip, unsigned short* port) {

    if (0 == anetUdpSockName(fd, ip, port)) {
        if (*ip != 0) {
            return;
        }
    }

    sigar_t* sigar;
    sigar_open(&sigar);
    
    sigar_net_interface_config_t ifconfig;
    if (0 == sigar_net_interface_config_primary_get(sigar, &ifconfig)) {
        *ip = ntohl(ifconfig.address.addr.in);
    }

    sigar_close(sigar);
}
unsigned int InetPton(const char* src, void* dst) {
    return IF_WIN32(w_inet_pton, inet_pton)(AF_INET, src, dst);
}

char* InetNtop(const void* src, char* dst, size_t size) {
    return IF_WIN32(w_inet_ntop, inet_ntop)(AF_INET, src, dst, size);
}
