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

#include "uv.h"
#include "plateform.h"
#include "adlist.h"
#include "dict.h"
#include "dicthelp.h"
#include "cJSON.h"
#include "equeue.h"
#include "sds.h"
#include "lua.h"
#include "lauxlib.h"
#include "lvm.h"
#include "filesys.h"
#include "dockerapi.h"
#include "proto.h"
#include "elog.h"
#include "entityid.h"
#include "redishelp.h"
#include "timesys.h"
#include "int64.h"
#include "uvnet.h"
#include "sdshelp.h"

#define MAX_DOCKER 255

static int global_bots = 0;

enum entity {
	DockerCurrent = 0, //当前ddocker
	DockerRandom,//当前节点的随机ddocker
	NodeInside,//任意内部节点
	NodeOutside,//任意有对外部通信节点
	NodeRandom,//随机节点
	DockerZero//放入第零个节点，这个节点不会出现在DockerRandom中
};

//消息句柄是静态全局变量
typedef struct _DockerHandle {
	uv_thread_t pthreadHandle;
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
	size_t eventQueueLength;
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
	EID eID;
	CreateEID(&eID, pDocksHandle->ip, pDocksHandle->uportOffset, pDockerHandle->id, id);
	unsigned long long retId = GetEID(&eID);

	return retId;
}

void UnallocateID(void* pVoid, unsigned long long id) {

	PDockerHandle pDockerHandle = pVoid;
	EID eID;
	CreateEIDFromLongLong(id, &eID);
	unsigned int t_id = GetIDFromEID(&eID);
	if (!(pDocksHandle->ip == GetAddrFromEID(&eID) && pDocksHandle->uportOffset == GetPortFromEID(&eID) && pDockerHandle->id == GetDockFromEID(&eID))) {
		n_error("UnallocateID::entity id is error! ip:%i port:%i dock:%i", GetAddrFromEID(&eID), GetPortFromEID(&eID), GetDockFromEID(&eID));
		return;
	}

	dictEntry* entry = dictFind(pDockerHandle->id_allocate, &t_id);
	if (entry != NULL) {
		dictDelete(pDockerHandle->id_allocate, &t_id);
		dictAddWithUint(pDockerHandle->id_unallocate, t_id, 0);
	}
}

static void doJsonParseFile(char* config, PDocksHandle pDocksHandle)
{
	if (config == NULL) {
		config = getenv("GrypaniaAssetsPath");
		if (config == 0 || access_t(config, 0) != 0) {
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
		printf("Error Open File: [%s]\n", config);
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
			else if (strcmp(item->string, "bots") == 0) {
				global_bots = (int)cJSON_GetNumberValue(item);
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

void DockerRun(void* pVoid) {

	PDockerHandle pDockerHandle = pVoid;

	srand(pDockerHandle->id);

	LVMSetGlobleLightUserdata(pDockerHandle->LVMHandle, "dockerHandle", pVoid);
	LVMSetGlobleInt(pDockerHandle->LVMHandle, "dockerid", pDockerHandle->id);
	LVMSetGlobleInt(pDockerHandle->LVMHandle, "bots", global_bots);

	//调用脚本的初始化
	LVMCallFunction(pDockerHandle->LVMHandle, pDocksHandle->entryfile, pDocksHandle->entryfunction);

	return;
}

void DocksCreate(unsigned int ip, unsigned char uportOffset, unsigned short uport
	, const char* assetsPath
	, unsigned short dockerSize
	, int nodetype
	, int bots) {

	if (pDocksHandle) return;

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
	
	global_bots = bots;

	//因为使用的是list所以0开头
	for (unsigned int i = 0; i < pDocksHandle->dockerSize; i++) {
		PDockerHandle pDockerHandle = malloc(sizeof(DockerHandle));
		pDockerHandle->eQueue = EqCreate();
		pDockerHandle->LVMHandle = LVMCreate(pDocksHandle->scriptPath, pDocksHandle->assetsPath);
		pDockerHandle->id = i;

		pDockerHandle->id_allocate = dictCreate(DefaultUintPtr(), NULL);
		pDockerHandle->id_unallocate = dictCreate(DefaultUintPtr(), NULL);
		pDockerHandle->entityCount = 0;
		pDockerHandle->eventQueueLength = 0;
		pDockerHandle->pBuf = 0;

		uv_thread_create(&pDockerHandle->pthreadHandle, DockerRun, pDockerHandle);

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
		uv_thread_join(&pDockerHandle->pthreadHandle);

		EqDestory(pDockerHandle->eQueue, PacketFree);
		if (pDockerHandle->pBuf != 0)
			sdsfree(pDockerHandle->pBuf);
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
	if (pDockerHandle->eventQueueLength == 0 && msec != 0) {
		EqTimeWait(pDockerHandle->eQueue, msec);
	}

	sdsfree(pDockerHandle->pBuf);
	//如果长度超过限制要释放掉队列
	pDockerHandle->pBuf = EqPopWithLen(pDockerHandle->eQueue, &pDockerHandle->eventQueueLength);
	PProtoHead pProtoHead = (PProtoHead)pDockerHandle->pBuf;
	if (pProtoHead != 0) {
		//可以用table的方式来处理lua的不确定多返回值的情况ret={fun()}
		lua_pushnumber(L, pProtoHead->proto);
		if (pProtoHead->proto == proto_ctr_cancel) {
			//由脚本退出循环
			return 1;
		}
		else if (pProtoHead->proto == proto_rpc_create) {
			PProtoRPCCreate pProtoRPCCreate = (PProtoRPCCreate)(pDockerHandle->pBuf);

			if (pProtoHead->len < sizeof(ProtoRPCCreate)) {
				elog_error(ctg_script, "DockerLoop::proto_rpc_create len error %i %i", pProtoHead->len, sizeof(ProtoRPCCreate));
				return 0;
			}
			size_t len = pProtoHead->len - sizeof(ProtoRPCCreate);
			lua_pushlstring(L, pProtoRPCCreate->callArg, len);
			return 2;
		}
		else if (pProtoHead->proto == proto_rpc_call) {

			PProtoRPC pProtoRPC = (PProtoRPC)(pDockerHandle->pBuf);
			luaL_u64pushnumber(L, pProtoRPC->id);
			//初始化参数

			if (pProtoHead->len < sizeof(ProtoRPC)) {
				elog_error(ctg_script, "DockerLoop::proto_rpc_call len error %i %i", pProtoHead->len, sizeof(ProtoRPC));
				return 0;
			}
			size_t len = pProtoHead->len - sizeof(ProtoRPC);
			lua_pushlstring(L, pProtoRPC->callArg, len);
			return 3;
		}
		else if (pProtoHead->proto == proto_run_lua) {
			PProtoRunLua pProtoRunLua = (PProtoRunLua)(pDockerHandle->pBuf);
			luaL_dostring(L, pProtoRunLua->luaString);
		}
		else if (pProtoHead->proto == proto_route_call) {
			PProtoRoute pProtoRoute = (PProtoRoute)(pDockerHandle->pBuf);
			luaL_u64pushnumber(L, pProtoRoute->did);
			luaL_u64pushnumber(L, pProtoRoute->pid);

			if (pProtoHead->len < sizeof(ProtoRoute)) {
				elog_error(ctg_script, "DockerLoop::proto_route_call len error %i %i", pProtoHead->len, sizeof(ProtoRoute));
				return 0;
			}
			size_t len = pProtoHead->len - sizeof(ProtoRoute);
			lua_pushlstring(L, pProtoRoute->callArg, len);
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

unsigned int DockerRandomPushMsg(unsigned char* b, unsigned short s) {

	unsigned int dockerId = 0;

	//当线程分配大于等于4时，0线程才不参与随机分配。
	if (pDocksHandle->dockerSize  >= 4){
		dockerId = (rand() % (pDocksHandle->dockerSize - 1)) + 1;
	}
	else {
		dockerId = rand() % pDocksHandle->dockerSize;
	}

	sds msg = sdsnewlen(b, s);
	EqPush(pDocksHandle->listPDockerHandle[dockerId]->eQueue, msg);

	return dockerId;
}

void DockerRunScript(unsigned char*  ip, short port, int id, unsigned char* b, unsigned short s) {

	if (*ip == '\0' && id < 0) {
		for (unsigned int i = 0; i < pDocksHandle->dockerSize; i++) {
			PDockerHandle pDockerHandle = pDocksHandle->listPDockerHandle[i];

			unsigned short len = sizeof(PProtoRunLua) + s;
			PProtoHead pProtoHead = (PProtoHead)sdsnewlen(0, len);
			pProtoHead->len = len;
			pProtoHead->proto = proto_run_lua;

			PProtoRunLua pProtoRunLua = (PProtoRunLua)pProtoHead;
			pProtoRunLua->dockerid = id;
			memcpy(pProtoRunLua->luaString, b, s);
			EqPush(pDockerHandle->eQueue, pProtoHead);
		}
	}
	else {
		PDockerHandle pDockerHandle = pDocksHandle->listPDockerHandle[id];

		unsigned short len = sizeof(PProtoRunLua) + s;
		PProtoHead pProtoHead = (PProtoHead)sdsnewlen(0, len);
		pProtoHead->len = len;
		pProtoHead->proto = proto_run_lua;

		PProtoRunLua pProtoRunLua = (PProtoRunLua)pProtoHead;
		memcpy(pProtoRunLua->luaString, b, s);

		if (*ip == '\0') {
			EqPush(pDockerHandle->eQueue, pProtoHead);
		}
		else {
			NetSendToNode(ip, port, (unsigned char*)pProtoHead, len);
			sdsfree((sds)pProtoHead);
		}
	}
}

void DockerSend(unsigned long long id, const char* pc, size_t s) {

	//n_fun("docker::DockerSend");
	EID eID;
	CreateEIDFromLongLong(id, &eID);
	unsigned int addr = GetAddrFromEID(&eID);
	unsigned char port =  GetPortFromEID(&eID);
	unsigned char docker = GetDockFromEID(&eID);

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
		NetSendToNodeWithUINT(addr, pDocksHandle->uport + port, (unsigned char*)pProtoHead, len);
	}
	
	free(pProtoHead);
}

void DockerSendToClient(void* pVoid, unsigned long long did, unsigned long long pid, const char* pc, size_t s) {

	PDockerHandle pDockerHandle = pVoid;
	EID eID;
	CreateEIDFromLongLong(pid, &eID);
	unsigned int addr = GetAddrFromEID(&eID);
	unsigned char port = GetPortFromEID(&eID);
	unsigned char docker = GetDockFromEID(&eID);

	unsigned short len = sizeof(ProtoRoute) + s;
	PProtoHead pProtoHead = malloc(len);
	pProtoHead->len = len;
	pProtoHead->proto = proto_route_call;

	PProtoRoute pProtoRoute = (PProtoRoute)pProtoHead;
	pProtoRoute->did = did;
	pProtoRoute->pid = pid;

	memcpy(pProtoRoute->callArg, pc, s);

	if (pDocksHandle->ip == addr && pDocksHandle->uportOffset == port) {
		NetSendToEntity(pid, (unsigned char*)pProtoHead, len);
	}
	else {
		NetSendToNodeWithUINT(addr, pDocksHandle->uport + port, (unsigned char*)pProtoHead, len);
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

	if (DockerZero == type)
		DockerPushMsg(0, (unsigned char*)pProtoHead, len);
	else if (DockerCurrent == type)
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

		if (pDocksHandle->ip == ip && (pDocksHandle->uport + pDocksHandle->uportOffset)== port) {
			DockerRandomPushMsg((unsigned char*)pProtoHead, len);
		}
		else {
			NetSendToNodeWithUINT(ip, port, (unsigned char*)pProtoHead, len);
		}
	}
	free(pProtoHead);
}

unsigned int GetDockerID(void* pVoid) {
	PDockerHandle pDockerHandle = pVoid;
	return pDockerHandle->id;
}

unsigned int GetEntityCount(void* pVoid) {
	PDockerHandle pDockerHandle = pVoid;
	return pDockerHandle->entityCount;
}