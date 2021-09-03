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
typedef void* VPESID;

unsigned char GetDockFromEID(VPEID pVPEID);
void SetDockFromEID(VPEID pVPEID, unsigned char dock);
unsigned short GetIDFromEID(VPEID pVPEID);
void SetIDFromEID(VPEID pVPEID, unsigned int id);
void SetAddrFromEID(VPEID pVPEID, unsigned int addr);
unsigned int GetAddrFromEID(VPEID pVPEID);
void SetPortFromEID(VPEID pVPEID, unsigned char port);
unsigned char GetPortFromEID(VPEID pVPEID);
VPEID CreateEID(unsigned int addr, unsigned char port, unsigned char dock, unsigned short id);
void DestoryEID(VPEID pVPEID);
VPEID CreateEIDFromLongLong(unsigned long long eid);
void SetEID(VPEID pVPEID, unsigned long long eid);
unsigned long long GetEID(VPEID pVPEID);
