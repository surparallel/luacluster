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


void* NetCreate(int nodetype, unsigned short listentcp);
void NetDestory();

int NetSendToCancel();
int NetSendToClient(unsigned int id, const char* b, unsigned short s);
int NetSendToEntity(unsigned long long entityid, const char* b, unsigned short s);
void NetUDPAddr2(unsigned int* ip, unsigned char* uportOffset, unsigned short* uport);
void NetSendToNode(char* ip, unsigned short port, unsigned char* b, unsigned short s);
void NetSendToNodeWithUINT(unsigned int ip, unsigned short port, unsigned char* b, unsigned short s);