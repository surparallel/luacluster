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
#include "dict.h"
#include "adlist.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "elog.h"
#include "sds.h"
#include "filesys.h"
#include "dicthelp.h"
#include "equeue.h"
#include "lvm.h"
#include "docker.h"
#include "proto.h"
#include "entityid.h"
#include "int64.h"
#include "timesys.h"
#include "uvnetmng.h"

//因为网络层有entity和client的对应信息。
//所以如果entity是当前节点就直接发到网络层
//如果是其他节点就使用upd发送到其他节点的网络层处理
//客户端发送给服务，服务发送给客户端，其他节点发送给客户端都使用这个协议
static int luaB_SendToClient(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");

	//entity id
	unsigned long long did = luaL_tou64be(L, 1, 0, 0);
	unsigned long long pid = luaL_tou64be(L, 2, 0, 0);

	size_t ret;
	const char* pc = luaL_checklstring(L, 3, &ret);

	//根据pid的不同发送到不同的节点
	//pid在当前节点，使用tcp
	//pid在其他节点，使用udp
	DockerSendToClient(pVoid, did, pid, pc, ret);
	return 0;
}

static int luaB_CopyRpcToClient(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	unsigned long long did = luaL_tou64be(L, 1, 0, 0);

	DockerCopyRpcToClient(pVoid, did);
	return 0;
}

//发送给其他对象的调用消息
//1， 同一个节点不同线程。2，不同节点
//ProtoRPC 从脚本层传入的id必须是完整的。
//解析成ip和端口后调用udp协议发送
static int luaB_Send(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");

	//entity id
	unsigned long long id = luaL_tou64be(L, 1, 0, 0);
	size_t ret;
	const char* pc = luaL_checklstring(L, 2, &ret);

	DockerSend(id, pc, ret);
	return 0;
}

static int luaB_Loop(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	return DockerLoop(pVoid, L);
}

static int luaB_AllocateID(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");

	unsigned long long id = AllocateID(pVoid);
	luaL_u64pushnumber(L, id);
	return 1;
}

static int luaB_UnallocateID(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");

	unsigned long long id = luaL_tou64(L, 1);
	UnallocateID(pVoid, id);
	return 0;
}

//销毁很简单，只要在脚本层删除即可。
//entityproxy的销毁就麻烦了，如果在逻辑层不控制、
//会导致通信失败，这种失败在异步模式下是正常的。
//两个entity必然同通过某个方式建立联系，而这种方式是开发者定义的。
//哪么也就要通过某种定义的方式取消联系。
//在异步模式下通信失败超时可以通过插件的形式进行扩展。
//如果什么功能都可以通过插件的形式进行扩展。。。懒的去开发
//socket因为是动态载入的lib,所以会和其他静态载入的lib冲突
//把所有库整合在一起就好了。
static int luaB_CreateEntity(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");

	unsigned int type = luaL_checkinteger(L, 1);
	size_t ret;
	const char* pc = luaL_checklstring(L, 2, &ret);
	DockerCreateEntity(pVoid, type, pc, ret);
	return 0;
}

static int luaB_BindNet(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");

	unsigned long long entityId = luaL_tou64be(L, 1, 0, 0);
	unsigned int clientId = luaL_checkinteger(L, 2);

	unsigned int len = sizeof(ProtoNetBind);
	PProtoHead pProtoHead = malloc(len);
	pProtoHead->len = len;
	pProtoHead->proto = proto_net_bind;

	PProtoNetBind pProtoNetBind = (PProtoNetBind)pProtoHead;
	pProtoNetBind->entityId = entityId;
	pProtoNetBind->clientId = clientId;

	idl32 id;
	id.u = clientId;

	MngSendToClient(id.id.work, (const char*)pProtoHead, len);
	free(pProtoHead);
	return 0;
}

static int luaB_DestoryNet(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");

	unsigned int clientId = luaL_checkinteger(L, 1);

	unsigned int len = sizeof(ProtoNetDestory);
	PProtoHead pProtoHead = malloc(len);
	pProtoHead->len = len;
	pProtoHead->proto = proto_net_destory;

	PProtoNetDestory pProtoNetDestory = (PProtoNetDestory)pProtoHead;
	pProtoNetDestory->clientId = clientId;

	idl32 id;
	id.u = clientId;

	MngSendToClient(id.id.work, (const char*)pProtoHead, len);
	free(pProtoHead);
	return 0;
}

static int luaB_GetCurrentMilli(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");

	luaL_u64pushnumber(L, GetCurrentMilli());
	return 1;
}


static int luaB_GetTick(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");

	luaL_u64pushnumber(L, GetTick());
	return 1;
}

static int luaB_GetDockerID(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");

	lua_pushinteger(L, GetDockerID(pVoid));
	return 1;
}

static int luaB_Script(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	char* ip = (unsigned char*)luaL_checkstring(L, 1);
	short port = luaL_checkinteger(L, 2);
	int id = luaL_checkinteger(L, 3);
	size_t len;
	char* script = (unsigned char*)luaL_checklstring(L, 4, &len);

	DockerRunScript(ip, port, id, script, len);
	return 0;
}

static int luaB_GetEntityCount(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");

	lua_pushinteger(L, GetEntityCount(pVoid));
	return 1;
}

static int luaB_SendWithList(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");

	//entity id
	unsigned long long id = luaL_tou64be(L, 1, 0, 0);
	size_t ret;
	const char* pc = luaL_checklstring(L, 2, &ret);
	list** list = lua_touserdata(L, 3);

	DockerSendWithList(id, pc, ret, list);
	return 0;
}

static int luaB_DockerCreateMsgList(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");

	list** ret = DockerCreateMsgList();
	lua_pushlightuserdata(L, ret);
	return 1;
}

static int luaB_DockerDestoryMsgList(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	list** list = lua_touserdata(L, 1);
	DockerDestoryMsgList(list);
	return 0;
}

static int luaB_DockerPushAllMsgList(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	list** list = lua_touserdata(L, 1);
	DockerPushAllMsgList(list);
	return 0;
}

static const luaL_Reg docker_funcs[] = {
	{"GetCurrentMilli", luaB_GetCurrentMilli},
	{"BindNet", luaB_BindNet},
	{"DestoryNet", luaB_DestoryNet},
	{"CreateEntity", luaB_CreateEntity},
	{"AllocateID", luaB_AllocateID},
	{"UnallocateID", luaB_UnallocateID},
	{"SendToClient", luaB_SendToClient},
	{"CopyRpcToClient", luaB_CopyRpcToClient},
	{"Send", luaB_Send},
	{"GetDockerID", luaB_GetDockerID},
	{"Script", luaB_Script},
	{"GetEntityCount", luaB_GetEntityCount},
	{"GetTick", luaB_GetTick},
	{"Loop", luaB_Loop},

	{"SendWithList", luaB_SendWithList},
	{"CreateMsgList", luaB_DockerCreateMsgList},
	{"DestoryMsgList", luaB_DockerDestoryMsgList},
	{"PushAllMsgList", luaB_DockerPushAllMsgList},
	{NULL, NULL}
};

int LuaOpenDocker(lua_State* L) {
	luaL_register(L, "docker", docker_funcs);
	return 1;
}


