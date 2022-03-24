/* entityid.h
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

typedef void* VPEID;

//0 表示正，1 表示负。接下来的 11 位是阶码, 52位的尾数。
//阶码全1，尾数全0表示无穷大INF。例如1.0/0.0
//阶码全1，尾数非全0的表示无效数NaN
#pragma pack(push,1)
typedef struct _EID {
	unsigned short id;
	unsigned int addr;//ipv4
	unsigned char dock;
	unsigned char port;//UDP端口号的偏移
}*PEID, EID;

typedef union idl64
{
	volatile EID eid;
	volatile unsigned long long u;
	volatile double d;
} idl64;

typedef struct _UINTID {
	unsigned short work;
	unsigned short client;//UDP端口号的偏移
}*PUINTID, UINTID;

typedef union idl32
{
	volatile UINTID id;
	volatile unsigned int u;
//	volatile float d;
} idl32;

#pragma pack(pop)

