
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



#include "plateform.h"
#include "sigar.h"


//如果ip为空则获得默认的设备ip
void IntConfigPrimary(int fd, unsigned int* ip, unsigned short* port) {
    sigar_t* sigar;
    sigar_open(&sigar);
    
    sigar_net_interface_config_t ifconfig;
    if (0 == sigar_net_interface_config_primary_get(sigar, &ifconfig)) {
        *ip = ntohl(ifconfig.address.addr.in);
    }

    sigar_close(sigar);
}
