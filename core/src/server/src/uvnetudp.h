/*
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


void UdpDestory(void* pVoid);
void* UdpCreate(char* ip, unsigned short uportBegin, unsigned char uportOffset, unsigned long long* retId, int inOrOut);
void UdpSendTo(char* ip, unsigned short port, unsigned char* b, unsigned int s);
void UdpSendToWithUINT(unsigned int ip, unsigned short port, unsigned char* b, unsigned int s);