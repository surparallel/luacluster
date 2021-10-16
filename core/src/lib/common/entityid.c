/* entity.c
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
* 1. 很严重的问题，在原来设想的entity id中node的端口都被固定使用9577或7795。那么在计算机上就不能开启多个node否在根据entityid的规则将无法找到对应的node。
* 因为可以使用多线程，所以并不是非常严重的问题。在一台服务器上也建议使用多线程而不是多开。仅仅在某些测试条件下需要躲开。
* 这时候需要一个unsigned short来标识在同一个机器上的不同端口。如果拿出一个short来标识端口就要增加entity id的长度。
* 如果重新划分uint64的使用那么entity id就只剩下255个显然不够使用。还有一个方法是仍然使用uint64。
* 一个uint表示ip,一个char表示dock,一个char标识从upd端口的偏移。剩下一个u short标识id。这样一个线程最大可以创建65535个对象。
* 2. 上面我们知道指定端口号非常重要。因为如果端口被占用应当要顺序后延来创建端口。例如5597被占用那么就尝试创建5598直到超过255仍然创建失败就停止创建。
*/

#include "plateform.h"
#include "entityid.h"

#pragma pack(push,1)
typedef struct _EID {
	unsigned short id;
	unsigned char dock;
	unsigned char port;//UDP端口号的偏移
	unsigned int addr;//ipv4
}*PEID, EID;
#pragma pack(pop)

unsigned char GetDockFromEID(VPEID pVPEID) {
	PEID pEID = (PEID) pVPEID;
	return pEID->dock;
}

void SetDockFromEID(VPEID pVPEID, unsigned char dock) {
	PEID pEID = (PEID)pVPEID;
	pEID->dock = dock;
}

unsigned short GetIDFromEID(VPEID pVPEID) {
	PEID pEID = (PEID)pVPEID;
	return pEID->id;
}

void SetIDFromEID(VPEID pVPEID, unsigned int id) {
	PEID pEID = (PEID)pVPEID;
	pEID->id = id;
}

void SetAddrFromEID(VPEID pVPEID, unsigned int addr) {
	PEID pEID = (PEID)pVPEID;
	pEID->addr = addr;
}

unsigned int GetAddrFromEID(VPEID pVPEID) {
	PEID pEID = (PEID)pVPEID;
	return pEID->addr;
}

void SetPortFromEID(VPEID pVPEID, unsigned char port) {
	PEID pEID = (PEID)pVPEID;
	pEID->port = port;
}

unsigned char GetPortFromEID(VPEID pVPEID) {
	PEID pEID = (PEID)pVPEID;
	return pEID->port;
}

VPEID CreateEID(unsigned int addr, unsigned char port, unsigned char dock, unsigned short id) {

	PEID pEID = (PEID)calloc(1, sizeof(EID));
	pEID->addr = addr;
	pEID->port = port;
	pEID->dock = dock;
	pEID->id = id;
	return pEID;
}

void DestoryEID(VPEID pVPEID) {
	PEID pEID = (PEID)pVPEID;
	free(pEID);
}

unsigned long long GetEID(VPEID pVPEID) {
	if (sizeof(EID) == sizeof(unsigned long long)) {
		unsigned long long ret = 0;
		memcpy(&ret, pVPEID, sizeof(EID));
		return ret;
	}
	return 0;
}

void SetEID(VPEID pVPEID, unsigned long long eid) {
	if (sizeof(EID) == sizeof(unsigned long long)) {
		memcpy(pVPEID, &eid, sizeof(EID));
	}
}

VPEID CreateEIDFromLongLong(unsigned long long eid) {
	if (sizeof(EID) == sizeof(unsigned long long)) {
		PEID pEID = (PEID)calloc(1, sizeof(EID));
		memcpy(pEID, &eid, sizeof(EID));
		return pEID;
	}
	return NULL;
}