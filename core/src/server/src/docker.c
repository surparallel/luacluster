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
#include "sdshelp.h"
#include "uvnetmng.h"
#include "uvnetudp.h"

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
	unsigned short entityCount;
	//entity的短id和长id的对应表
	dict* id_unallocate;
	dict* id_allocate;
	unsigned char* pBuf;//每轮消息缓存
	size_t eventQueueLength;

	dict* entitiesCache;
	size_t bufCur;

	list* bufList;

	unsigned int stat_send_count;//发送给客户端的封包总数
	unsigned int stat_packet_count;//其中封包打包的数量
	unsigned int stat_packet_client;//其中打包后发送的数量

	unsigned int bufCount;//buflist的处理次数
	unsigned int bufCountLast;
	unsigned long long bufCountStamp;
	unsigned long long bufListLen;

	unsigned stat_avg_queuelen;//过去10次心跳内队列的总长度
	unsigned stat_avg_queuecount;//统计次数，以获得平均长度

	unsigned long long max_congestion;
}*PDockerHandle, DockerHandle;

typedef struct _DocksHandle {
	PDockerHandle listPDockerHandle[MAX_DOCKER]; //每个线程的句柄
	unsigned short dockerSize;
	sds scriptPath;
	sds assetsPath;
	int	nodetype;

	sds entryFile;
	sds entryFunction;
	sds entryUpdate;
	sds entryProcess;

	size_t congestion;
	unsigned int cacheTime;
	unsigned int cacheForce;

	unsigned int updateCache;
	unsigned int updateScript;

	unsigned int popNodeSize;
	unsigned int packetLimit;
	unsigned int packetSize;
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
		return 0;
	}
	
	unsigned long long udpId = 0;
	GetRandomUdp(&udpId);
	idl64 rid;
	rid.u = udpId;
	if (rid.eid.port == 0)rid.eid.port = 1;

	idl64 eid;
	eid.eid.addr = rid.eid.addr;
	eid.eid.port = ~rid.eid.port;
	eid.eid.dock = pDockerHandle->id;
	eid.eid.id = id;
	unsigned long long retId = eid.u;
	return retId;
}

void UnallocateID(void* pVoid, unsigned long long id) {

	PDockerHandle pDockerHandle = pVoid;
	idl64 eid;
	eid.u = id;
	unsigned char port = ~eid.eid.port;
	unsigned int t_id = eid.eid.id;

	eid.eid.port= ~eid.eid.port;
	eid.eid.dock = 0;
	eid.eid.id = 0;

	if (!(IsNodeUdp(eid.u) && pDockerHandle->id == eid.eid.dock)) {
		n_error("UnallocateID::entity id is error! ip:%i port:%i dock:%i", eid.eid.addr, port, eid.eid.dock);
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
		printf("Error before: [%s]\n", cJSON_GetErrorPtr());	
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
			else if (strcmp(item->string, "entryFile") == 0 && sdslen(pDocksHandle->assetsPath) == 0) {
				sdsfree(pDocksHandle->entryFile);
				pDocksHandle->entryFile = sdsnew(cJSON_GetStringValue(item));
			}
			else if (strcmp(item->string, "entryFunction") == 0 && sdslen(pDocksHandle->assetsPath) == 0) {
				sdsfree(pDocksHandle->entryFunction);
				pDocksHandle->entryFunction = sdsnew(cJSON_GetStringValue(item));
			}
			else if (strcmp(item->string, "entryUpdate") == 0 && sdslen(pDocksHandle->assetsPath) == 0) {
				sdsfree(pDocksHandle->entryUpdate);
				pDocksHandle->entryUpdate = sdsnew(cJSON_GetStringValue(item));
			}
			else if (strcmp(item->string, "entryProcess") == 0 && sdslen(pDocksHandle->assetsPath) == 0) {
				sdsfree(pDocksHandle->entryProcess);
				pDocksHandle->entryProcess = sdsnew(cJSON_GetStringValue(item));
			}
			else if (strcmp(item->string, "bots") == 0) {
				global_bots = (int)cJSON_GetNumberValue(item);
			}
			else if (strcmp(item->string, "congestion") == 0) {
				pDocksHandle->congestion = (size_t)cJSON_GetNumberValue(item);
			}
			else if (strcmp(item->string, "cacheTime") == 0) {
				pDocksHandle->cacheTime = (size_t)cJSON_GetNumberValue(item);
			}
			else if (strcmp(item->string, "cacheForce") == 0) {
				pDocksHandle->cacheForce = (size_t)cJSON_GetNumberValue(item);
			}
			else if (strcmp(item->string, "updateCache") == 0) {
				pDocksHandle->updateCache = (size_t)cJSON_GetNumberValue(item);
			}
			else if (strcmp(item->string, "updateScript") == 0) {
				pDocksHandle->updateScript = (size_t)cJSON_GetNumberValue(item);
			}
			else if (strcmp(item->string, "popNodeSize") == 0) {
				pDocksHandle->popNodeSize = (size_t)cJSON_GetNumberValue(item);
			}
			else if (strcmp(item->string, "packetLimit") == 0) {
				pDocksHandle->packetLimit = (size_t)cJSON_GetNumberValue(item);
			}
			else if (strcmp(item->string, "packetSize") == 0) {
				pDocksHandle->packetSize = (size_t)cJSON_GetNumberValue(item);
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
	LVMSetGlobleInt(pDockerHandle->LVMHandle, "dockerID", pDockerHandle->id);
	LVMSetGlobleInt(pDockerHandle->LVMHandle, "bots", global_bots);

	//调用脚本的初始化
	LVMCallFunction(pDockerHandle->LVMHandle, pDocksHandle->entryFile, pDocksHandle->entryFunction);

	return;
}

void memFreeCallbackBuf(void* privdata, void* val) {
	DICT_NOTUSED(privdata);
	if(val)sdsfree(val);
}

static dictType LongLongDictTypeBuf = {
	longlongHashCallback,
	NULL,
	NULL,
	LonglongCompareCallback,
	memFreeCallback,
	memFreeCallbackBuf,
};

void* DefaultLonglongPtrBuf() {
	return &LongLongDictTypeBuf;
}

void DocksCreate(const char* assetsPath
	, unsigned short dockerSize
	, int nodetype
	, int bots) {

	if (dockerSize == 0) {
		n_error("DocksCreate::dockerSize is zero");

		return;
	}

	if (pDocksHandle) return;

	pDocksHandle = malloc(sizeof(DocksHandle));
	if (pDocksHandle == 0)return;
	pDocksHandle->dockerSize = dockerSize;
	pDocksHandle->scriptPath = sdsnew("./lua/");
	pDocksHandle->assetsPath = sdsnew(assetsPath);
	pDocksHandle->nodetype = nodetype;
	pDocksHandle->entryFile = sdsnew("dockerrun");
	pDocksHandle->entryFunction = sdsnew("main");
	pDocksHandle->entryUpdate = sdsnew("EntryUpdate");
	pDocksHandle->entryProcess = sdsnew("EntryProcess");
	pDocksHandle->congestion = 640625;
	pDocksHandle->cacheTime = 100;
	pDocksHandle->cacheForce = 0;
	pDocksHandle->updateCache = 100;
	pDocksHandle->updateScript = 1000;
	pDocksHandle->popNodeSize = 10000;
	pDocksHandle->packetLimit = 625;
	pDocksHandle->packetSize = 20 * 1024 * 1024;

	doJsonParseFile(NULL, pDocksHandle);
	
	global_bots = bots;

	//因为使用的是list所以0开头
	for (unsigned int i = 0; i < pDocksHandle->dockerSize; i++) {
		PDockerHandle pDockerHandle = malloc(sizeof(DockerHandle));
		if (pDockerHandle == 0)break;

		pDockerHandle->eQueue = EqCreate();
		pDockerHandle->LVMHandle = LVMCreate(pDocksHandle->scriptPath, pDocksHandle->assetsPath);
		pDockerHandle->id = i;

		pDockerHandle->id_allocate = dictCreate(DefaultUintPtr(), NULL);
		pDockerHandle->id_unallocate = dictCreate(DefaultUintPtr(), NULL);
		pDockerHandle->entityCount = 0;
		pDockerHandle->eventQueueLength = 0;
		pDockerHandle->pBuf = 0;
		pDockerHandle->bufCur = 0;
		
		pDockerHandle->entitiesCache = dictCreate(DefaultLonglongPtrBuf(), NULL);
		pDockerHandle->bufList = listCreate();

		pDockerHandle->stat_send_count = 0;
		pDockerHandle->stat_packet_count = 0;
		pDockerHandle->stat_packet_client = 0;

		pDockerHandle->stat_avg_queuelen = 0;
		pDockerHandle->stat_avg_queuecount = 0;
		pDockerHandle->max_congestion = 0;

		uv_thread_create(&pDockerHandle->pthreadHandle, DockerRun, pDockerHandle);

		pDocksHandle->listPDockerHandle[i] = pDockerHandle;
	}

	return ;
}

static void ListDestroyFun(void* value)
{
	sdsfree(value);
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

		dictRelease(pDockerHandle->entitiesCache);

		listSetFreeMethod(pDockerHandle->bufList, ListDestroyFun);
		listRelease(pDockerHandle->bufList);
		free(pDockerHandle);
	}
}

void DocksDestory() {
	if (pDocksHandle) {
		DockerCancel(pDocksHandle);
		sdsfree(pDocksHandle->scriptPath);
		sdsfree(pDocksHandle->assetsPath);

		sdsfree(pDocksHandle->entryFile);
		sdsfree(pDocksHandle->entryFunction);
		sdsfree(pDocksHandle->entryUpdate);
		sdsfree(pDocksHandle->entryProcess);

		pDocksHandle = 0;
	}
}

int DockerLoopProcessBuf(PDockerHandle pDockerHandle, lua_State* L, char* pBuf) {

	PProtoHead pProtoHead = (PProtoHead)pBuf;
	if (pProtoHead != 0) {
		//可以用table的方式来处理lua的不确定多返回值的情况ret={fun()}
		if (pProtoHead->proto == proto_packet) {
			PProtoRPC pProtoRPC = (PProtoRPC)(pBuf);
			//由脚本退出循环
			if (pDockerHandle->bufCur == 0) {

				//开始处理第一个
				PProtoHead pProtoHeadFirst = (PProtoHead)pProtoRPC->callArg;
				pDockerHandle->bufCur = sizeof(ProtoRPC) + pProtoHeadFirst->len;

				//也最后一个结束处理
				if (pDockerHandle->bufCur < pProtoHeadFirst->len) {
					n_error("DockerLoop::DockerProcessBuf::pProtoHeadFirst pDockerHandle->bufCur < pProtoHeadFirst->len %i %i", pDockerHandle->bufCur, pProtoHeadFirst->len);
					return 0;
				}

				if (pDockerHandle->bufCur > pProtoHead->len) {
					n_error("DockerLoop::DockerProcessBuf::pProtoHeadFirst pDockerHandle->bufCur > pProtoHead->len %i %i", pDockerHandle->bufCur, pProtoHead->len);
					return 0;
				}

				if (pProtoHeadFirst->proto != proto_route_call) {
					n_error("DockerLoop::DockerProcessBuf::pProtoHeadOther pProtoHeadOther->proto != proto_route_call");
					return 0;
				}

				if (pDockerHandle->bufCur == pProtoHead->len) {
					pDockerHandle->bufCur = 0;
				}
				//始终为1，结束后需要检查队列后退出
				pDockerHandle->eventQueueLength = 1;
				if (pProtoHeadFirst->proto == proto_route_call) {
					PProtoRoute pProtoRoute = (PProtoRoute)(pProtoHeadFirst);
					pProtoRoute->pid = pProtoRPC->id;
				}

				return DockerLoopProcessBuf(pDockerHandle, L, pProtoRPC->callArg);
			}
			else {
				PProtoHead pProtoHeadOther = (PProtoHead)(pBuf + pDockerHandle->bufCur);
				pDockerHandle->bufCur += pProtoHeadOther->len;

				//最后一个结束处理
				if (pDockerHandle->bufCur < pProtoHeadOther->len) {
					n_error("DockerLoop::DockerProcessBuf::pProtoHeadOther pDockerHandle->bufCur < pProtoHeadOther->len %i %i", pDockerHandle->bufCur, pProtoHead->len);
					return 0;
				}

				if (pDockerHandle->bufCur > pProtoHead->len) {
					n_error("DockerLoop::DockerProcessBuf::pProtoHeadOther pDockerHandle->bufCur > pProtoHead->len %i %i", pDockerHandle->bufCur, pProtoHead->len);
					return 0;
				}

				if (pProtoHeadOther->proto != proto_route_call) {
					n_error("DockerLoop::DockerProcessBuf::pProtoHeadOther pProtoHeadOther->proto != proto_route_call");
					return 0;
				}

				if (pDockerHandle->bufCur == pProtoHead->len) {
					pDockerHandle->bufCur = 0;
				}

				if (pProtoHeadOther->proto == proto_route_call) {
					PProtoRoute pProtoRoute = (PProtoRoute)(pProtoHeadOther);
					pProtoRoute->pid = pProtoRPC->id;
				}

				return DockerLoopProcessBuf(pDockerHandle, L, (char*)pProtoHeadOther);
			}
		}
		else if (pProtoHead->proto == proto_ctr_cancel) {
			//由脚本退出循环
			lua_newtable(L);
			lua_pushnumber(L, 1);
			lua_pushnumber(L, pProtoHead->proto);
			lua_settable(L, -3);
			return 1;
		}
		else if (pProtoHead->proto == proto_rpc_create) {

			PProtoRPCCreate pProtoRPCCreate = (PProtoRPCCreate)(pBuf);

			if (pProtoHead->len < sizeof(ProtoRPCCreate)) {
				elog_error(ctg_script, "DockerLoop::proto_rpc_create len error %i %i", pProtoHead->len, sizeof(ProtoRPCCreate));
				return 0;
			}

			lua_newtable(L);
			lua_pushnumber(L, 1);
			lua_pushnumber(L, pProtoHead->proto);
			lua_settable(L, -3);
			size_t len = pProtoHead->len - sizeof(ProtoRPCCreate);

			lua_pushnumber(L, 2);
			lua_pushlstring(L, pProtoRPCCreate->callArg, len);
			lua_settable(L, -3);
			return 1;
		}
		else if (pProtoHead->proto == proto_rpc_call) {

			if (pProtoHead->len < sizeof(ProtoRPC)) {
				elog_error(ctg_script, "DockerLoop::proto_rpc_call len error %i %i", pProtoHead->len, sizeof(ProtoRPC));
				return 0;
			}
			lua_newtable(L);
			lua_pushnumber(L, 1);
			lua_pushnumber(L, pProtoHead->proto);
			lua_settable(L, -3);

			PProtoRPC pProtoRPC = (PProtoRPC)(pBuf);

			lua_pushnumber(L, 2);
			luaL_u64pushnumber(L, pProtoRPC->id);
			lua_settable(L, -3);

			//初始化参数
			size_t len = pProtoHead->len - sizeof(ProtoRPC);
			lua_pushnumber(L, 3);
			lua_pushlstring(L, pProtoRPC->callArg, len);
			lua_settable(L, -3);
			return 1;
		}
		else if (pProtoHead->proto == proto_run_lua) {
			PProtoRunLua pProtoRunLua = (PProtoRunLua)(pBuf);
			luaL_dostring(L, pProtoRunLua->luaString);
			return 0;
		}
		else if (pProtoHead->proto == proto_route_call) {

			if (pProtoHead->len < sizeof(ProtoRoute)) {
				elog_error(ctg_script, "DockerLoop::proto_route_call len error %i %i", pProtoHead->len, sizeof(ProtoRoute));
				return 0;
			}

			lua_newtable(L);
			lua_pushnumber(L, 1);
			lua_pushnumber(L, pProtoHead->proto);
			lua_settable(L, -3);

			PProtoRoute pProtoRoute = (PProtoRoute)(pBuf);
			lua_pushnumber(L, 2);
			luaL_u64pushnumber(L, pProtoRoute->did);
			lua_settable(L, -3);

			lua_pushnumber(L, 3);
			luaL_u64pushnumber(L, pProtoRoute->pid);
			lua_settable(L, -3);

			size_t len = pProtoHead->len - sizeof(ProtoRoute);

			lua_pushnumber(L, 4);
			lua_pushlstring(L, pProtoRoute->callArg, len);
			lua_settable(L, -3);
			return 1;
		}
		else if (pProtoHead->proto == proto_rpc_destory) {
			lua_newtable(L);
			lua_pushnumber(L, 1);
			lua_pushnumber(L, pProtoHead->proto);
			lua_settable(L, -3);

			PProtoRPC pProtoRPC = (PProtoRPC)(pBuf);
			lua_pushnumber(L, 2);
			luaL_u64pushnumber(L, pProtoRPC->id);
			lua_settable(L, -3);
			return 1;
		}
	}
	return 0;
}

void DockerSendPacket(unsigned long long id, sds pBuf) {
	unsigned int s = sdslen(pBuf);
	unsigned int len = sizeof(ProtoRPC) + s;
	PProtoRPC pProtoRPC = malloc(len);
	if (pProtoRPC == NULL)return;
	pProtoRPC->protoHead.len = len;
	pProtoRPC->protoHead.proto = proto_packet;
	pProtoRPC->id = id;

	memcpy(pProtoRPC->callArg, pBuf, s);
	MngSendToEntity(id, (const char*)pProtoRPC, len);
	free(pProtoRPC);
}

void DockerCache(PDockerHandle pDockerHandle, unsigned long long stamp) {

	dictIterator* cacheIter = dictGetIterator(pDockerHandle->entitiesCache);
	dictEntry* cacheNode;
	while ((cacheNode = dictNext(cacheIter)) != NULL) {

		unsigned long long* key = dictGetKey(cacheNode);
		sds pBuf = dictGetVal(cacheNode);

		if (pBuf != 0) {
			//将拼包发送走
			DockerSendPacket(*key, pBuf);
			pDockerHandle->stat_packet_client ++;
		}
	}
	dictReleaseIterator(cacheIter);

	//释放上一次
	if (dictSize(pDockerHandle->entitiesCache) != 0)dictEmpty(pDockerHandle->entitiesCache, NULL);
}

int DockerLoop(void* pVoid, lua_State* L) {

	PDockerHandle pDockerHandle = pVoid;
	long long waittime = 0;
	unsigned long long beginstamp = GetTick();
	unsigned long long endstamp = beginstamp;
	unsigned long long updateStamp = beginstamp;
	unsigned long long count = 1;
	unsigned long long deltaTime = beginstamp - updateStamp;
	pDockerHandle->bufCount = 0;//buflist的处理次数
	pDockerHandle->bufCountLast = 0;
	pDockerHandle->bufCountStamp = beginstamp;
	pDockerHandle->bufListLen = 0;

	do {
		if (listLength(pDockerHandle->bufList) == 0 && pDockerHandle->eventQueueLength == 0 && waittime != 0) {

			if (dictSize(pDockerHandle->entitiesCache)) {
				DockerCache(pDockerHandle, beginstamp);

				n_stat("DockerLoop::stat(%i) stat_send_count %i/%i/%i", pDockerHandle->id, pDockerHandle->stat_send_count, pDockerHandle->stat_packet_count, pDockerHandle->stat_packet_client);
			}

			if (pDockerHandle->stat_send_count != 0) {
				pDockerHandle->stat_send_count = 0;
				pDockerHandle->stat_packet_count = 0;
				pDockerHandle->stat_packet_client = 0;
			}

			EqTimeWait(pDockerHandle->eQueue, waittime);
		}

		beginstamp = GetTick();

		deltaTime = beginstamp - updateStamp;
		if (updateStamp == 0 || deltaTime > pDocksHandle->updateScript) {

			int top = lua_gettop(L);
			lua_getglobal(L, pDocksHandle->entryUpdate);
			lua_pushnumber(L, (double)deltaTime);
			lua_pushnumber(L, (double)count);
			if (lua_pcall(L, 2, LUA_MULTRET, 0)) {
				n_error("LVMCallFunction.plua_pcall:%s lua:%s", pDocksHandle->entryUpdate, lua_tolstring(L, -1, NULL));
			}
			lua_settop(L, top);

			count = count + 1;
			updateStamp = beginstamp;

			if (deltaTime > pDocksHandle->updateScript * 2) {
				n_error("dockerrun(%i)::update's time is consumed: %i", pDockerHandle->id, deltaTime);
			}

			pDockerHandle->stat_avg_queuelen += pDockerHandle->eventQueueLength;
			pDockerHandle->stat_avg_queuecount += 1;

			if (pDockerHandle->stat_avg_queuecount >= 2000) {
				int avg = pDockerHandle->stat_avg_queuelen / pDockerHandle->stat_avg_queuecount;
				if(avg != 0) n_stat("dockerrun(%i)::update's avg eventQueueLength is: %i", pDockerHandle->id, avg);
				pDockerHandle->stat_avg_queuelen = 0;
				pDockerHandle->stat_avg_queuecount = 0;
			}
		}

		if (pDockerHandle->bufCur != 0) {
			//当前packet封包还没有处理完成
			int top = lua_gettop(L);
			lua_getglobal(L, pDocksHandle->entryProcess);
			if (DockerLoopProcessBuf(pDockerHandle, L, pDockerHandle->pBuf)) {
				if (lua_pcall(L, 1, LUA_MULTRET, 0)) {
					n_error("LVMCallFunction.plua_pcall:%s lua:%s", pDocksHandle->entryProcess, lua_tolstring(L, -1, NULL));
				}
			}
			lua_settop(L, top);
		}
		else {

			//如果加上最小间隔时间戳，可能会导致单个封包反应不即时的问题
			if (listLength(pDockerHandle->bufList) == 0) {

				if (dictSize(pDockerHandle->entitiesCache)) {
					DockerCache(pDockerHandle, beginstamp);

					n_stat("DockerLoop::stat(%i) stat_send_count %i/%i/%i", pDockerHandle->id, pDockerHandle->stat_send_count, pDockerHandle->stat_packet_count, pDockerHandle->stat_packet_client);
				}

				if (pDockerHandle->stat_send_count != 0) {
					pDockerHandle->stat_send_count = 0;
					pDockerHandle->stat_packet_count = 0;
					pDockerHandle->stat_packet_client = 0;
				}

				EqPopNodeWithLen(pDockerHandle->eQueue, pDocksHandle->popNodeSize, pDockerHandle->bufList, &pDockerHandle->eventQueueLength);
				
				//开始新的一轮
				pDockerHandle->bufCountStamp = beginstamp;
				pDockerHandle->bufListLen = listLength(pDockerHandle->bufList);
				if (pDockerHandle->bufListLen != 0) {
					pDockerHandle->bufCount += 1;
				}		
			}

			if (listLength(pDockerHandle->bufList) != 0) {
				sdsfree(pDockerHandle->pBuf);
				listNode* node = listLast(pDockerHandle->bufList);
				pDockerHandle->pBuf = listNodeValue(node);
				listDelNode(pDockerHandle->bufList, node);

				if (pDocksHandle->congestion && pDockerHandle->eventQueueLength > pDocksHandle->congestion && pDockerHandle->eventQueueLength > pDockerHandle->max_congestion) {
					n_error("PipeHandler::DockerLoop(%i) Massive packet congestion %i ", pDockerHandle->id, pDockerHandle->eventQueueLength);
					pDockerHandle->max_congestion = pDockerHandle->eventQueueLength;
				}
				else if (pDocksHandle->congestion && pDockerHandle->eventQueueLength < pDocksHandle->congestion && pDockerHandle->max_congestion != 0) {
					pDockerHandle->max_congestion = 0;
				}

				if (pDockerHandle->pBuf != 0 && strlen(pDockerHandle->pBuf) != 0) {
					int top = lua_gettop(L);
					lua_getglobal(L, pDocksHandle->entryProcess);
					if (DockerLoopProcessBuf(pDockerHandle, L, pDockerHandle->pBuf)) {
						if (lua_pcall(L, 1, LUA_MULTRET, 0)) {
							n_error("LVMCallFunction.plua_pcall:%s lua:%s", pDocksHandle->entryProcess, lua_tolstring(L, -1, NULL));
						}
					}
					lua_settop(L, top);
				}
			}
		}

		endstamp = GetTick();
		waittime = waittime - (endstamp - beginstamp);
		//本轮结束重置
		//最糟糕的情况是在每轮读秒要结束的时候触发了封包处理导致整体超时。
		if (waittime < 0) {
			waittime = pDocksHandle->updateCache;
		}
	} while (true);
}

unsigned int DockerSize() {
	return pDocksHandle->dockerSize;
}

list** DockerCreateMsgList() {
	list** retList = malloc(sizeof(list*) * pDocksHandle->dockerSize);
	if (retList == NULL) return NULL;
	for (int i = 0; i < pDocksHandle->dockerSize; i++) {
		retList[i] = listCreate();
	}

	return retList;
}

static void DestroyFun(void* value) {
	sdsfree(value);
}

void DockerDestoryMsgList(list* retList[]) {
	for (int i = 0; i < pDocksHandle->dockerSize; i++) {
		listSetFreeMethod(retList[i], DestroyFun);
		listRelease(retList[i]);
	}
	free(retList);
}

void DockerPushAllMsgList(list* retList[]) {
	for (int i = 0; i < pDocksHandle->dockerSize; i++) {
		EqPushList(pDocksHandle->listPDockerHandle[i]->eQueue, retList[i]);
	}
}

void DockerPushMsgList(list* retList[], unsigned int dockerId, unsigned char* b, unsigned int s) {
	if (dockerId < pDocksHandle->dockerSize) {
		sds msg = sdsnewlen(b, s);

		if (msg == NULL) {
			n_error("DockerPushMsgList sdsnewlen is null");
			return;
		}

		listNode* node = listCreateNode(msg);
		listAddNodeHeadForNode(retList[dockerId], node);
	}
}

void DockerPushMsg(unsigned int dockerId, unsigned char* b, unsigned int s) {
	if (dockerId < pDocksHandle->dockerSize) {
		sds msg = sdsnewlen(b, s);

		if (msg == NULL) {
			n_error("DockerPushMsg sdsnewlen is null");
			return;
		}

		listNode* node = listCreateNode(msg);
		EqPushNode(pDocksHandle->listPDockerHandle[dockerId]->eQueue, node);
	}	
}

unsigned int DockerRandomPushMsg(unsigned char* b, unsigned int s) {

	unsigned int dockerId = 0;

	//当线程数量大于等于2时，0线程不参与随机分配。
	if (pDocksHandle->dockerSize >= 2){
		dockerId = (rand() % (pDocksHandle->dockerSize - 1)) + 1;
	}else if (pDocksHandle->dockerSize == 1) {
		dockerId = 0;
	}

	sds msg = sdsnewlen(b, s);

	if (msg == NULL) {
		n_error("DockerRandomPushMsg sdsnewlen is null");
		return 0;
	}

	EqPush(pDocksHandle->listPDockerHandle[dockerId]->eQueue, msg);

	return dockerId;
}

void DockerRunScript(unsigned char*  ip, short port, int id, unsigned char* b, unsigned int s) {

	if (*ip == '\0' && id < 0) {
		for (unsigned int i = 0; i < pDocksHandle->dockerSize; i++) {
			PDockerHandle pDockerHandle = pDocksHandle->listPDockerHandle[i];

			unsigned int len = sizeof(PProtoRunLua) + s;
			PProtoHead pProtoHead = (PProtoHead)sdsnewlen(0, len);

			if (pProtoHead == NULL) {
				n_error("DockerRunScript sdsnewlen is null");
				return;
			}

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

		unsigned int len = sizeof(PProtoRunLua) + s;
		PProtoHead pProtoHead = (PProtoHead)sdsnewlen(0, len);

		if (pProtoHead == NULL) {
			n_error("DockerRunScript1 sdsnewlen is null");
			return;
		}

		pProtoHead->len = len;
		pProtoHead->proto = proto_run_lua;

		PProtoRunLua pProtoRunLua = (PProtoRunLua)pProtoHead;
		memcpy(pProtoRunLua->luaString, b, s);

		if (*ip == '\0') {
			EqPush(pDockerHandle->eQueue, pProtoHead);
		}
		else {
			UdpSendTo(ip, port, (unsigned char*)pProtoHead, len);
			sdsfree((sds)pProtoHead);
		}
	}
}

void DockerSendWithList(unsigned long long id, const char* pc, size_t s, list** list) {
	idl64 eid;
	eid.u = id;
	unsigned int addr = eid.eid.addr;
	unsigned char port = ~eid.eid.port;
	unsigned char docker = eid.eid.dock;

	unsigned int len = sizeof(ProtoRPC) + s;
	PProtoHead pProtoHead = malloc(len);
	if (pProtoHead == NULL)return;
	pProtoHead->len = len;
	pProtoHead->proto = proto_rpc_call;

	PProtoRPC pProtoRPC = (PProtoRPC)pProtoHead;
	pProtoRPC->id = id;

	memcpy(pProtoRPC->callArg, pc, s);

	eid.eid.port = ~eid.eid.port;
	eid.eid.dock = 0;
	eid.eid.id = 0;

	if (IsNodeUdp(eid.u) || (eid.eid.port == 1 && eid.eid.addr == 0)) {
		DockerPushMsgList(list, docker, (unsigned char*)pProtoHead, len);
	}
	else {
		UdpSendToWithUINT(addr, UdpBasePort() + port, (unsigned char*)pProtoHead, len);
	}

	free(pProtoHead);
}

void DockerSend(unsigned long long id, const char* pc, size_t s) {
	idl64 eid;
	eid.u = id;
	unsigned int addr = eid.eid.addr;
	unsigned char port = ~eid.eid.port;
	unsigned char docker = eid.eid.dock;

	unsigned int len = sizeof(ProtoRPC) + s;
	PProtoHead pProtoHead = malloc(len);
	if (pProtoHead == NULL)return;
	pProtoHead->len = len;
	pProtoHead->proto = proto_rpc_call;

	PProtoRPC pProtoRPC = (PProtoRPC)pProtoHead;
	pProtoRPC->id = id;

	memcpy(pProtoRPC->callArg, pc, s);

	eid.eid.port = ~eid.eid.port;
	eid.eid.dock = 0;
	eid.eid.id = 0;

	if (IsNodeUdp(eid.u) || (eid.eid.port == 1 && eid.eid.addr == 0)) {
		DockerPushMsg(docker, (unsigned char*)pProtoHead, len);
	}
	else {
		UdpSendToWithUINT(addr, UdpBasePort() + port, (unsigned char*)pProtoHead, len);
	}
	
	free(pProtoHead);
}

void DockerSendToClient(void* pVoid, unsigned long long did, unsigned long long pid, const char* pc, size_t s) {

	if (pVoid == 0)return;
	PDockerHandle pDockerHandle = pVoid;
	idl64 eid;
	eid.u = pid;
	unsigned int addr = eid.eid.addr;
	unsigned char port = ~eid.eid.port;
	unsigned char docker = eid.eid.dock;

	unsigned int len = sizeof(ProtoRoute) + s;
	PProtoHead pProtoHead = malloc(len);
	if (pProtoHead == 0)
		return;

	pDockerHandle->stat_send_count ++;
	pProtoHead->len = len;
	pProtoHead->proto = proto_route_call;

	PProtoRoute pProtoRoute = (PProtoRoute)pProtoHead;
	pProtoRoute->did = did;
	pProtoRoute->pid = pid;

	memcpy(pProtoRoute->callArg, pc, s);

	eid.eid.port = ~eid.eid.port;
	eid.eid.dock = 0;
	eid.eid.id = 0;

	if (IsNodeUdp(eid.u) || (eid.eid.port == 1 && eid.eid.addr == 0)) {
		if (pVoid == 0) {
			MngSendToEntity(pid, (unsigned char*)pProtoHead, len);
		}
		else {

			if (pDocksHandle->cacheForce) {
				dictEntry* entryCurrent = dictFind(pDockerHandle->entitiesCache, &pid);
				if (entryCurrent == 0) {
					sds pbuf = sdsnewlen(0, len);

					if (pbuf == NULL) {
						n_error("DockerSendToClient sdsnewlen is null");
						return;
					}

					memcpy(pbuf, pProtoHead, len);
					dictAddWithLonglong(pDockerHandle->entitiesCache, pid, pbuf);
				}
				else {
					sds pbuf = dictGetVal(entryCurrent);
					size_t l = sdslen(pbuf) + len;
					if (l > pDocksHandle->packetSize) {
						DockerSendPacket(pid, pbuf);
						sdsclear(pbuf);
					}
					if (sdsavail(pbuf) < len) {
						pbuf = sdsMakeRoomFor(pbuf, len);

						if (pbuf == NULL) {
							n_error("DockerSendToClient sdsMakeRoomFor is null");
							return;
						}

						dictSetVal(pDockerHandle->entitiesCache, entryCurrent, pbuf);
					}
					memcpy(pbuf + sdslen(pbuf), pProtoHead, len);
					sdsIncrLen(pbuf, (int)len);	
					pDockerHandle->stat_packet_count++;
				}
			}
			else {
				if (pDockerHandle->bufListLen < pDocksHandle->packetLimit) {
					MngSendToEntity(pid, (unsigned char*)pProtoHead, len);
				}
				else {
					dictEntry* entryCurrent = dictFind(pDockerHandle->entitiesCache, &pid);
					if (entryCurrent == 0) {
						sds pbuf = sdsnewlen(0, len);

						if (pbuf == NULL) {
							n_error("DockerSendToClient2 sdsnewlen is null");
							return;
						}

						memcpy(pbuf, pProtoHead, len);
						dictAddWithLonglong(pDockerHandle->entitiesCache, pid, pbuf);
					}
					else {
						sds pbuf = dictGetVal(entryCurrent);
						size_t l = sdslen(pbuf) + len;
						if (l > pDocksHandle->packetSize) {
							DockerSendPacket(pid, pbuf);
							sdsclear(pbuf);
						}
						if (sdsavail(pbuf) < len) {
							pbuf = sdsMakeRoomFor(pbuf, len);

							if (pbuf == NULL) {
								n_error("DockerSendToClient2 sdsMakeRoomFor is null");
								return;
							}

							dictSetVal(pDockerHandle->entitiesCache, entryCurrent, pbuf);
						}
						memcpy(pbuf + sdslen(pbuf), pProtoHead, len);
						sdsIncrLen(pbuf, (int)len);

						pDockerHandle->stat_packet_count++;
					}
				}
			}
		}
	}
	else {
		UdpSendToWithUINT(addr, UdpBasePort() + port, (unsigned char*)pProtoHead, len);
	}

	free(pProtoHead);
}

void DockerCopyRpcToClient(void* pVoid, unsigned long long did) {
	PDockerHandle pDockerHandle = pVoid;

	PProtoHead pProtoHead = (PProtoHead)pDockerHandle->pBuf;
	if (pProtoHead->proto == proto_rpc_call) {
		PProtoRPC pProtoRPC = (PProtoRPC)pDockerHandle->pBuf;
		if (did != 0){
			DockerSendToClient(pVoid, did, pProtoRPC->id, pProtoRPC->callArg, pProtoRPC->protoHead.len - sizeof(ProtoRPC));
		}
		else {
			DockerSendToClient(pVoid, pProtoRPC->id, pProtoRPC->id, pProtoRPC->callArg, pProtoRPC->protoHead.len - sizeof(ProtoRPC));
		}
	}
	else {
		elog_error(ctg_script, "DockerCopyRpcToClient not find proto_rpc_call");
	}
}

//创建entity还要考虑负载均衡
void DockerCreateEntity(void* pVoid, int type, const char* c, size_t s) {

	PDockerHandle pDockerHandle = pVoid;

	unsigned int len = sizeof(ProtoRPCCreate) + s;
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

		idl64 eid;
		eid.u = 0;
		eid.eid.addr = ip;
		eid.eid.port = port - UdpBasePort();

		if (IsNodeUdp(eid.u) || (eid.eid.port == 1 && eid.eid.addr == 0)) {
			DockerRandomPushMsg((unsigned char*)pProtoHead, len);
		}
		else {
			UdpSendToWithUINT(ip, port, (unsigned char*)pProtoHead, len);
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