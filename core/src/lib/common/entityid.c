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

unsigned char GetDockFromEID(PEID pEID) {
	return pEID->dock;
}

void SetDockFromEID(PEID pEID, unsigned char dock) {
	pEID->dock = dock;
}

unsigned short GetIDFromEID(PEID pEID) {
	return pEID->id;
}

void SetIDFromEID(PEID pEID, unsigned int id) {
	pEID->id = id;
}

void SetAddrFromEID(PEID pEID, unsigned int addr) {
	pEID->addr = addr;
}

unsigned int GetAddrFromEID(PEID pEID) {
	return pEID->addr;
}

void SetPortFromEID(PEID pEID, unsigned char port) {
	pEID->port = port;
}

unsigned char GetPortFromEID(PEID pEID) {
	return pEID->port;
}

void CreateEID(PEID pEID, unsigned int addr, unsigned char port, unsigned char dock, unsigned short id) {
	pEID->addr = addr;
	pEID->port = port;
	pEID->dock = dock;
	pEID->id = id;
	return pEID;
}

unsigned long long GetEID(PEID pEID) {
	if (sizeof(EID) == sizeof(unsigned long long)) {
		unsigned long long ret = 0;
		memcpy(&ret, pEID, sizeof(EID));
		return ret;
	}
	return 0;
}

void SetEID(PEID pEID, unsigned long long eid) {
	if (sizeof(EID) == sizeof(unsigned long long)) {
		memcpy(pEID, &eid, sizeof(EID));
	}
}

void CreateEIDFromLongLong(unsigned long long eid, PEID pEID) {
	if (sizeof(EID) == sizeof(unsigned long long)) {
		memcpy(pEID, &eid, sizeof(EID));
	}
}