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

#pragma pack(push,1)
typedef struct _EID {
	unsigned short id;
	unsigned char dock;
	unsigned char port;//UDP¶Ë¿ÚºÅµÄÆ«ÒÆ
	unsigned int addr;//ipv4
}*PEID, EID;
#pragma pack(pop)

unsigned char GetDockFromEID(PEID pEID);
void SetDockFromEID(PEID pEID, unsigned char dock);
unsigned short GetIDFromEID(PEID pEID);
void SetIDFromEID(PEID pEID, unsigned int id);
void SetAddrFromEID(PEID pEID, unsigned int addr);
unsigned int GetAddrFromEID(PEID pEID);
void SetPortFromEID(PEID pEID, unsigned char port);
unsigned char GetPortFromEID(PEID pEID);
void CreateEID(PEID pEID, unsigned int addr, unsigned char port, unsigned char dock, unsigned short id);
void CreateEIDFromLongLong(unsigned long long eid, PEID pEID);
void SetEID(PEID pEID, unsigned long long eid);
unsigned long long GetEID(PEID pEID);
