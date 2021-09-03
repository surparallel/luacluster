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
#include <pthread.h>
#include "adlist.h"
#include "dict.h"
#include "dicthelp.h"
#include "cJSON.h"
#include "equeue.h"
#include "sds.h"
#include "zmalloc.h"
#include "lua.h"
#include "lauxlib.h"
#include "lvm.h"
#include "filesys.h"
#include "dockerapi.h"
#include "proto.h"
#include "elog.h"
#include "entityid.h"
#include "networking.h"
#include "redishelp.h"
#include "timesys.h"
#include "int64.h"

#define MAX_DOCKER 255

enum entity {
	DockerCurrent = 0, //当前ddocker
	DockerRandom,//当前节点的随机ddocker
	NodeInside,//任意内部节点
	NodeOutside,//任意有对外部通信节点
	NodeRandom//随机节点
};

//消息句柄是静态全局变量
typedef struct _DockerHandle {
	pthread_t pthreadHandle;
	void* eQueue;
	void* LVMHandle;
	unsigned short id;//docker id
	//entity循环创建后，如果没有销毁相关的引用，就会导致将消息发送给一个不存在的对象或已经销毁又被重新创建的对象?
	//反复的创建和销毁会导致id无法有效的分配?
	//entity id。
	unsigned short entityCount;
	//entity的短id和长id的对应表
	dict* id_unallocate;
	dict* id_allocate;
	unsigned char* pBuf;
}*PDockerHandle, DockerHandle;

typedef struct _DocksHandle {
	PDockerHandle listPDockerHandle[MAX_DOCKER]; //每个线程的句柄
	unsigned short dockerSize;
	sds scriptPath;
	sds assetsPath;
	unsigned int ip;
	unsigned char uportOffset;
	unsigned short uport;
	int	nodetype;

	sds entryfile;
	sds entryfunction;
}*PDocksHandle, DocksHandle;

static PDocksHandle pDocksHandle = 0;

//第一个阶段顺序分配
//第二个当循环完所有65535个对象后，每删除一个对象都要回收，并从回收中分配
unsigned long long AllocateID(void* pVoid) {
	PDockerHandle pDockerHandle = pVoid;
	unsigned int id = 0;
	if (pDockerHandle->entityCount <= 65535) {
		id = ++pDockerHandle->entityCount;
		dictAddWithUint(pDockerHandle->id_allocate, id, 0);
	}
	else if (dictSize(pDockerHandle->id_unallocate) > 0) {
		dictEntry* entry = dictGetRandomKey(pDockerHandle->id_unallocate);

		if (entry != NULL) {
			id = *(unsigned int*)dictGetKey(entry);
			dictAddWithUint(pDockerHandle->id_allocate, id, 0);
		}
	}
	else {
		n_error("AllocateID::entity number is greater than 65535!");
	}

	VPEID pVPEID = CreateEID(pDocksHandle->ip, pDocksHandle->uportOffset, pDockerHandle->id, id);
	unsigned long long retId = GetEID(pVPEID);
	DestoryEID(pVPEID);

	return retId;
}

void UnallocateID(void* pVoid, unsigned long long id) {

	PDockerHandle pDockerHandle = pVoid;
	VPEID pVPEID = CreateEIDFromLongLong(id);
	unsigned int t_id = GetIDFromEID(pVPEID);
	if (!(pDocksHandle->ip == GetAddrFromEID(pVPEID) && pDocksHandle->uportOffset == GetPortFromEID(pVPEID) && pDockerHandle->id == GetDockFromEID(pVPEID))) {
		n_error("UnallocateID::entity id is error! ip:%i port:%i dock:%i", GetAddrFromEID(pVPEID), GetPortFromEID(pVPEID), GetDockFromEID(pVPEID));
		DestoryEID(pVPEID);
		return;
	}
	DestoryEID(pVPEID);

	dictEntry* entry = dictFind(pDockerHandle->id_allocate, &t_id);
	if (entry != NULL) {
		dictDelete(pDockerHandle->id_allocate, &t_id);
		dictAddWithUint(pDockerHandle->id_unallocate, t_id, 0);
	}
}

static void doJsonParseFile(char* config, PDocksHandle pDocksHandle)
{
	if (config == NULL) {
		config = "./res/server/config.json";
		if (access_t(config, 0) != 0) {
			config = "../../res/server/config_defaults.json";
			if (access_t(config, 0) != 0) {
				return;
			}
		}
	}

	FILE* f; size_t len; char* data;
	cJSON* json;

	f = fopen_t(config, "rb");

	if (f == NULL) {
		//printf("Error Open File: [%s]\n", filename);
		return;
	}

	fseek_t(f, 0, SEEK_END);
	len = ftell_t(f);
	fseek_t(f, 0, SEEK_SET);
	data = (char*)malloc(len + 1);
	fread(data, 1, len, f);
	fclose(f);

	json = cJSON_Parse(data);
	if (!json) {
		//printf("Error before: [%s]\n", cJSON_GetErrorPtr());	
		free(data);
		return;
	}

	cJSON* logJson = cJSON_GetObjectItem(json, "docke_config");
	if (logJson) {

		cJSON* item = logJson->child;
		while ((item != NULL) && (item->string != NULL))
		{
			if (strcmp(item->string, "dockerSize") == 0 && pDocksHandle->dockerSize == 1) {
				pDocksHandle->dockerSize = (int)cJSON_GetNumberValue(item);
			}
			else if (strcmp(item->string, "scriptPath") == 0) {
				sdsfree(pDocksHandle->scriptPath);
				pDocksHandle->scriptPath = sdsnew(cJSON_GetStringValue(item));
			}
			else if (strcmp(item->string, "assetsPath") == 0 && sdslen(pDocksHandle->assetsPath) == 0) {
				sdsfree(pDocksHandle->assetsPath);
				pDocksHandle->assetsPath = sdsnew(cJSON_GetStringValue(item));
			}
			else if (strcmp(item->string, "entryfile") == 0 && sdslen(pDocksHandle->assetsPath) == 0) {
				sdsfree(pDocksHandle->entryfile);
				pDocksHandle->entryfile = sdsnew(cJSON_GetStringValue(item));
			}
			else if (strcmp(item->string, "entryfunction") == 0 && sdslen(pDocksHandle->assetsPath) == 0) {
				sdsfree(pDocksHandle->entryfunction);
				pDocksHandle->entryfunction = sdsnew(cJSON_GetStringValue(item));
			}
			item = item->next;
		}
	}

	cJSON_Delete(json);
	free(data);
}

static void PacketFree(void* ptr) {
	sdsfree(ptr);
}

void* DockerRun(void* pVoid) {

	PDockerHandle pDockerHandle = pVoid;

	LVMSetGlobleLightUserdata(pDockerHandle->LVMHandle, "dockerHandle", pVoid);
	LVMSetGlobleInt(pDockerHandle->LVMHandle, "dockerid", pDockerHandle->id);

	//调用脚本的初始化
	LVMCallFunction(pDockerHandle->LVMHandle, pDocksHandle->entryfile, pDocksHandle->entryfunction, 0, 0);

	return NULL;
}

void DocksCreate(unsigned int ip, unsigned char uportOffset, unsigned short uport, const char* assetsPath, unsigned short dockerSize, int nodetype) {

	if (pDocksHandle) return;

	if (ip == 0) {
		n_error("DocksCreate udp_ip or udp_port is empty! ip:%i port:%i", ip, uportOffset);
		return;
	}

	pDocksHandle = malloc(sizeof(DocksHandle));
	pDocksHandle->dockerSize = dockerSize;
	pDocksHandle->scriptPath = sdsnew("./lua/");
	pDocksHandle->assetsPath = sdsnew(assetsPath);
	pDocksHandle->nodetype = nodetype;
	pDocksHandle->ip = ip;
	pDocksHandle->uportOffset = uportOffset;
	pDocksHandle->uport = uport;
	pDocksHandle->entryfile = sdsnew("dockerrun");
	pDocksHandle->entryfunction = sdsnew("main");

	doJsonParseFile(NULL, pDocksHandle);

	//因为使用的是list所以0开头
	for (unsigned int i = 0; i < pDocksHandle->dockerSize; i++) {
		PDockerHandle pDockerHandle = malloc(sizeof(DockerHandle));
		pDockerHandle->eQueue = EqCreate();
		pDockerHandle->LVMHandle = LVMCreate(pDocksHandle->scriptPath, pDocksHandle->assetsPath);
		pDockerHandle->id = i;

		pDockerHandle->id_allocate = dictCreate(DefaultUintPtr(), NULL);
		pDockerHandle->id_unallocate = dictCreate(DefaultUintPtr(), NULL);
		pDockerHandle->entityCount = 0;
		pthread_create(&pDockerHandle->pthreadHandle, NULL, DockerRun, pDockerHandle);

		pDocksHandle->listPDockerHandle[i] = pDockerHandle;
	}

	return ;
}

void DockerCancel() {
	for (unsigned int i = 0; i < pDocksHandle->dockerSize; i++) {
		PDockerHandle pDockerHandle = pDocksHandle->listPDockerHandle[i];

		PProtoHead pProtoHead = (PProtoHead)sdsnewlen(NULL, sizeof(ProtoHead));
		pProtoHead->len = 0;
		pProtoHead->proto = proto_ctr_cancel;

		EqPush(pDockerHandle->eQueue, pProtoHead);
		pthread_join(pDockerHandle->pthreadHandle, NULL);

		EqDestory(pDockerHandle->eQueue, PacketFree);

		free(pDockerHandle);
	}
}

void DocksDestory() {
	if (pDocksHandle) {
		DockerCancel(pDocksHandle);
		sdsfree(pDocksHandle->scriptPath);
		sdsfree(pDocksHandle->assetsPath);

		sdsfree(pDocksHandle->entryfile);
		sdsfree(pDocksHandle->entryfunction);

		pDocksHandle = 0;
	}
}

int DockerLoop(void* pVoid, lua_State* L, long long msec) {

	PDockerHandle pDockerHandle = pVoid;
	//EqWait(pDockerHandle->eQueue);

	unsigned long long mtiem = GetCurrentMilli();
	mtiem = mtiem + msec;
	long long secs = mtiem / 1000;
	long long nsecs = (mtiem % 1000) * (1000 * 1000);

	EqTimeWait(pDockerHandle->eQueue, secs, nsecs);

	//如果长度超过限制要释放掉队列
	size_t eventQueueLength;
	pDockerHandle->pBuf = EqPopWithLen(pDockerHandle->eQueue, &eventQueueLength);

	PProtoHead pProtoHead = (PProtoHead)pDockerHandle->pBuf;
	if (pProtoHead != 0) {
		//可以用table的方式来处理lua的不确定多返回值的情况ret={fun()}
		lua_pushnumber(L, pProtoHead->proto);
		if (pProtoHead->proto == proto_ctr_cancel) {
			//由脚本退出循环
			sdsfree(pDockerHandle->pBuf);
			return 1;
		}
		else if (pProtoHead->proto == proto_rpc_create) {
			PProtoRPCCreate pProtoRPCCreate = (PProtoRPCCreate)(pDockerHandle->pBuf);
			size_t len = pProtoHead->len - sizeof(ProtoRPCCreate);
			lua_pushlstring(L, pProtoRPCCreate->callArg, len);
			sdsfree(pDockerHandle->pBuf);
			return 2;
		}
		else if (pProtoHead->proto == proto_rpc_call) {

			PProtoRPC pProtoRPC = (PProtoRPC)(pDockerHandle->pBuf);
			luaL_u64pushnumber(L, pProtoRPC->id);
			//初始化参数
			size_t len = pProtoHead->len - sizeof(ProtoRPC);
			lua_pushlstring(L, pProtoRPC->callArg, len);
			sdsfree(pDockerHandle->pBuf);
			return 3;
		}
		else if (pProtoHead->proto == proto_run_lua) {
			PProtoRunLua pProtoRunLua = (PProtoRunLua)(pDockerHandle->pBuf);
			luaL_dostring(L, pProtoRunLua->luaString);
			sdsfree(pDockerHandle->pBuf);
		}
		else if (pProtoHead->proto == proto_route_call) {
			PProtoRoute pProtoRoute = (PProtoRoute)(pDockerHandle->pBuf);
			luaL_u64pushnumber(L, pProtoRoute->did);
			luaL_u64pushnumber(L, pProtoRoute->pid);
			//初始化参数
			size_t len = pProtoHead->len - sizeof(ProtoRoute);
			lua_pushlstring(L, pProtoRoute->callArg, len);
			sdsfree(pDockerHandle->pBuf);
			return 4;
		}
	}

	return 0;
}

unsigned int DockerSize() {
	return pDocksHandle->dockerSize;
}

void DockerPushMsg(unsigned int dockerId, unsigned char* b, unsigned short s) {
	if (dockerId < pDocksHandle->dockerSize) {
		sds msg = sdsnewlen(b, s);
		EqPush(pDocksHandle->listPDockerHandle[dockerId]->eQueue, msg);
	}	
}

void DockerRandomPushMsg(unsigned char* b, unsigned short s) {

	unsigned int dockerId = rand() % pDocksHandle->dockerSize;
	sds msg = sdsnewlen(b, s);
	EqPush(pDocksHandle->listPDockerHandle[dockerId]->eQueue, msg);
}


void DockerSend(void* pVoid, unsigned long long id, const char* pc, size_t s) {

	PDockerHandle pDockerHandle = pVoid;

	VPEID pEntityID = CreateEIDFromLongLong(id);
	unsigned int addr = GetAddrFromEID(pEntityID);
	unsigned char port =  GetPortFromEID(pEntityID);
	unsigned char docker = GetDockFromEID(pEntityID);
	DestoryEID(pEntityID);

	unsigned short len = sizeof(ProtoRPC) + s;
	PProtoHead pProtoHead = malloc(len);
	pProtoHead->len = len;
	pProtoHead->proto = proto_rpc_call;

	PProtoRPC pProtoRPC = (PProtoRPC)pProtoHead;
	pProtoRPC->id = id;

	memcpy(pProtoRPC->callArg, pc, s);

	if (pDocksHandle->ip == addr && pDocksHandle->uportOffset == port) {
		DockerPushMsg(docker, (unsigned char*)pProtoHead, len);
	}
	else {
		SendToNodeWithUINT(addr, pDocksHandle->uport + port, (unsigned char*)pProtoHead, len);
	}
	
	free(pProtoHead);
}

void DockerSendToClient(void* pVoid, unsigned long long did, unsigned long long pid, const char* pc, size_t s) {

	PDockerHandle pDockerHandle = pVoid;

	VPEID pEntityID = CreateEIDFromLongLong(pid);
	unsigned int addr = GetAddrFromEID(pEntityID);
	unsigned char port = GetPortFromEID(pEntityID);
	unsigned char docker = GetDockFromEID(pEntityID);
	DestoryEID(pEntityID);

	unsigned short len = sizeof(ProtoRoute) + s;
	PProtoHead pProtoHead = malloc(len);
	pProtoHead->len = len;
	pProtoHead->proto = proto_route_call;

	PProtoRoute pProtoRoute = (PProtoRoute)pProtoHead;
	pProtoRoute->did = did;
	pProtoRoute->pid = pid;

	memcpy(pProtoRoute->callArg, pc, s);

	if (pDocksHandle->ip == addr && pDocksHandle->uportOffset == port) {
		SendToEntity(pid, (unsigned char*)pProtoHead, len);
	}
	else {
		SendToNodeWithUINT(addr, pDocksHandle->uport + port, (unsigned char*)pProtoHead, len);
	}

	free(pProtoHead);
}

void DockerCopyRpcToClient(void* pVoid) {
	PDockerHandle pDockerHandle = pVoid;

	PProtoHead pProtoHead = (PProtoHead)pDockerHandle->pBuf;
	if (pProtoHead->proto == proto_rpc_call) {

		PProtoRPC pProtoRPC = (PProtoRPC)pDockerHandle->pBuf;

		DockerSendToClient(pVoid, pProtoRPC->id, pProtoRPC->id, pProtoRPC->callArg, pProtoRPC->protoHead.len - sizeof(ProtoRPC));
	}
	else {
		elog_error(ctg_script, "DockerCopyRpcToClient not find proto_rpc_call");
	}
}

//创建entity还要考虑负载均衡
void DockerCreateEntity(void* pVoid, int type, const char* c, size_t s) {

	PDockerHandle pDockerHandle = pVoid;

	unsigned short len = sizeof(ProtoRPCCreate) + s;
	PProtoHead pProtoHead = malloc(len);
	pProtoHead->len = len;
	pProtoHead->proto = proto_rpc_create;

	PProtoRPCCreate protoRPCCreate = (PProtoRPCCreate)pProtoHead;

	memcpy(protoRPCCreate->callArg, c, s);

	unsigned int ip = 0;
	unsigned short port = 0;

	if (DockerCurrent == type)
		DockerPushMsg(pDockerHandle->id, (unsigned char*)pProtoHead, len);
	else if(DockerRandom == type)
		DockerRandomPushMsg((unsigned char*)pProtoHead, len);
	else if(NodeInside == type)
	{
		GetUpdInside(&ip, &port);
	}
	else if (NodeOutside == type)
	{
		GetUpdOutside(&ip, &port);
	}
	else if (NodeRandom == type)
	{
		GetUpdFromAll(&ip, &port);
	}

	if (NodeInside == type || NodeOutside == type || NodeRandom == type) {

		if (htonl(pDocksHandle->ip) == ip && (pDocksHandle->uport + pDocksHandle->uportOffset)== port) {
			DockerRandomPushMsg((char*) c, s);
		}
		else {
			SendToNodeWithUINT(ip, port, (unsigned char*)pProtoHead, len);
		}
	}
	free(pProtoHead);
}