/* docker.h - worker thread
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


void DocksCreate(unsigned int ip, unsigned char uportOffset, unsigned short uport, const char* assetsPath, unsigned short dockerSize, int nodetype);
void DocksDestory();
int DockerLoop(void* pVoid, lua_State* L, long long msec);
unsigned int DockerSize();
unsigned long long AllocateID(void* pDockerHandle);
void UnallocateID(void* pDockerHandle, unsigned long long id);

void DockerPushMsg(unsigned int dockerId, unsigned char* b, unsigned short s);
void DockerRandomPushMsg(unsigned char* b, unsigned short s);
void DockerSend(void* pVoid, unsigned long long id, const char* pc, size_t s);
void DockerCreateEntity(void* pVoid, int type, const char* c, size_t s);
void DockerSendToClient(void* pVoid, unsigned long long did, unsigned long long pid, const char* pc, size_t s);
void DockerCopyRpcToClient(void* pVoid);