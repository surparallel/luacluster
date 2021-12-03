/* bigworld.c - Seamless big world manager
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

//Salute the bigworld

#include "plateform.h"
#include "dict.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "elog.h"
#include "sds.h"
#include "filesys.h"
#include "dicthelp.h"
#include "equeue.h"
#include "lvm.h"
#include "proto.h"
#include "entityid.h"
#include "int64.h"
#include "timesys.h"
#include "adlist.h"
#include "dict.h"
#include "transform.h"
#include "quaternion.h"
#include "vector.h"
#include "rtree.h"
#include "docker.h"
#include "lua_cmsgpack.h"
#include "rtreehelp.h"

typedef struct _BigWorldSpace{
	struct Vector3 begin, end;//每个空间原始的坐标点
	unsigned long long id;

	unsigned int adjust;//进入调整状态
	struct Vector3 tobegin, toend;//要调整的坐标
}*PBigWorldSpace, BigWorldSpace;

//控制地图的分割服务
typedef struct _BigWorld
{
	struct Vector3 boundary;
	struct Vector3 begin, end;//地图原点和结束点，和unity一样是从左下角开始到右上角
	
	list* spaceList;//空间的链接
	dict* spaceDict;//空间id的索引
	struct rtree* prtree;
}*PBigWorld, BigWorld;


static int luaB_Create(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	PBigWorld pBigWorld = (PBigWorld)malloc(sizeof(BigWorld));

	struct Vector3 boundary;
	struct Vector3 begin;
	struct Vector3 end;
	boundary.x = luaL_checknumber(L, 1);
	boundary.z = luaL_checknumber(L, 2);
	begin.x = luaL_checknumber(L, 3);
	begin.z = luaL_checknumber(L, 4);
	end.x = luaL_checknumber(L, 5);
	end.z = luaL_checknumber(L, 6);

	s_fun("Bigworld::Create");

	if ((unsigned int)(end.x - begin.x) % (unsigned int)(boundary.x) != 0 || (unsigned int)(end.z - begin.z) % (unsigned int)(boundary.z) != 0) {
		s_error("bigworld::Create The boundary length is not an integer multiple of the gired");
		lua_pushnil(L);
		return 1;
	}

	pBigWorld->boundary = boundary;
	pBigWorld->begin = begin;
	pBigWorld->end = end;
	pBigWorld->prtree = rtree_new(sizeof(unsigned long long), 2);

	pBigWorld->spaceList = listCreate();
	pBigWorld->spaceDict = dictCreate(DefaultUintPtr(), NULL);
	
	lua_pushlightuserdata(L, pBigWorld);
	return 1;
}

static void DestroyFun(void* value) {
	free(value);
}

static int luaB_Destroy(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	PBigWorld pBigWorld = lua_touserdata(L, 1);

	s_fun("Bigworld::Destroy");

	rtree_free(pBigWorld->prtree);

	listSetFreeMethod(pBigWorld->spaceList, DestroyFun);
	listRelease(pBigWorld->spaceList);
	dictRelease(pBigWorld->spaceDict);
	free(pBigWorld);
	return 0;
}

//检查地图树，并重新定向到指定地图服务，可能指定到多个地图
static int luaB_BigWorldEntry(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	PBigWorld pBigWorld = (PBigWorld)lua_touserdata(L, 1);

	unsigned long long entityid = luaL_tou64(L, 2);

	s_fun("Bigworld::Entry %U", entityid);

	double pointx = luaL_checknumber(L, 3);
	double pointz = luaL_checknumber(L, 4);

	double rects[2 * 2];
	rects[0] = pointx;
	rects[1] = pointz;
	rects[2] = pointx;
	rects[3] = pointz;

	list* result = listCreate();
	rtree_search(pBigWorld->prtree, rects, MultipleIter, result);

	mp_buf* pmp_buf = mp_buf_new();
	MP_ENCODE_CONST(pmp_buf, "OnRedirectToSpace");
	mp_encode_array(pmp_buf, listLength(result));
	listIter* iter = listGetIterator(result, AL_START_HEAD);
	listNode* node;
	while ((node = listNext(iter)) != NULL) {
		struct Iter* iter = (struct Iter*)listNodeValue(node);
		mp_encode_double(pmp_buf, u642double(*(unsigned long long*)iter->item));
	}

	DockerSend(entityid, pmp_buf->b, pmp_buf->len);
	mp_buf_free(pmp_buf);

	listSetFreeMethod(result, DestroyFun);
	listRelease(result);
	return 1;
}

static int luaB_SpaceFull(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	PBigWorld pBigWorld = (PBigWorld)lua_touserdata(L, 1);

	unsigned long long id = luaL_tou64(L, 2);

	s_fun("Bigworld::SpaceFull %U", id);

	dictEntry* entry = dictFind(pBigWorld->spaceDict, &id);
	if (entry == NULL) {

		lua_pushnil(L);
		return 1;
	}

	PBigWorldSpace pBigWorldSpace = (PBigWorldSpace)dictGetVal(entry);
	if (pBigWorldSpace->adjust == 1) {
		s_error("SpaceFull Repeat submission %U", id);
		lua_pushnil(L);
		return 1;
	}
			
	pBigWorldSpace->adjust = 1;
	double linex = (pBigWorldSpace->end.x - pBigWorldSpace->begin.x) / 2;

	if (linex > pBigWorld->boundary.x) {
		pBigWorldSpace->toend.x = pBigWorldSpace->end.x - linex;
		pBigWorldSpace->toend.z = pBigWorldSpace->end.z;
		pBigWorldSpace->tobegin = pBigWorldSpace->begin;

		lua_pushinteger(L, pBigWorldSpace->adjust);
		lua_pushnumber(L, pBigWorldSpace->toend.x - pBigWorld->boundary.x);
		lua_pushnumber(L, pBigWorldSpace->begin.z - pBigWorld->boundary.x);
		lua_pushnumber(L, pBigWorldSpace->end.x + pBigWorld->boundary.z);
		lua_pushnumber(L, pBigWorldSpace->end.z + pBigWorld->boundary.z);
		return 5;
	}
	else {
		s_error("Cannot split space for %U", id);
		lua_pushnil(L);
		return 1;
	}
}

//空间创建成功开始分片
static int luaB_OnSpace(lua_State* L) {

	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	PBigWorld pBigWorld = (PBigWorld)lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);
	unsigned long long oid = luaL_tou64(L, 3);
	double beginx = luaL_checknumber(L, 4);
	double beginz = luaL_checknumber(L, 5);
	double endx = luaL_checknumber(L, 6);
	double endz = luaL_checknumber(L, 7);

	s_fun("Bigworld::OnSpace id:%U oid:%U", id, oid);

	PBigWorldSpace pBigWorldSpace = calloc(1, sizeof(BigWorldSpace));
	pBigWorldSpace->begin.x = beginx + pBigWorld->boundary.x;
	pBigWorldSpace->begin.z = beginz + pBigWorld->boundary.z;
	pBigWorldSpace->end.x = endx - pBigWorld->boundary.x;
	pBigWorldSpace->end.z = endz - pBigWorld->boundary.z;
	pBigWorldSpace->id = id;

	mp_buf* pmp_buf2 = mp_buf_new();
	MP_ENCODE_CONST(pmp_buf2, "OnAlterSpace");
	mp_encode_array(pmp_buf2, 5);
	mp_encode_double(pmp_buf2, u642double(pBigWorldSpace->id));
	mp_encode_double(pmp_buf2, pBigWorldSpace->begin.x - pBigWorld->boundary.x);
	mp_encode_double(pmp_buf2, pBigWorldSpace->begin.z - pBigWorld->boundary.z);
	mp_encode_double(pmp_buf2, pBigWorldSpace->end.x + pBigWorld->boundary.x);
	mp_encode_double(pmp_buf2, pBigWorldSpace->end.z + pBigWorld->boundary.z);

	if (oid != 0) {
		dictEntry* entry = dictFind(pBigWorld->spaceDict, &oid);
		if (entry != NULL) {
			PBigWorldSpace pBigWorldSpace =(PBigWorldSpace)dictGetVal(entry);
			if (pBigWorldSpace->adjust = 1) {
				pBigWorldSpace->begin = pBigWorldSpace->tobegin;
				pBigWorldSpace->end = pBigWorldSpace->toend;
				pBigWorldSpace->adjust = 0;

				double rects[2 * 2];
				rects[0] = pBigWorldSpace->begin.x - pBigWorld->boundary.x;
				rects[1] = pBigWorldSpace->begin.z - pBigWorld->boundary.z;
				rects[2] = pBigWorldSpace->end.x + pBigWorld->boundary.x;
				rects[3] = pBigWorldSpace->end.z + pBigWorld->boundary.z;
				rtree_delete(pBigWorld->prtree, rects, &oid);
				rtree_insert(pBigWorld->prtree, rects, &oid);

				mp_encode_array(pmp_buf2, 5);
				mp_encode_double(pmp_buf2, u642double(pBigWorldSpace->id));
				mp_encode_double(pmp_buf2, pBigWorldSpace->begin.x - pBigWorld->boundary.x);
				mp_encode_double(pmp_buf2, pBigWorldSpace->begin.z - pBigWorld->boundary.z);
				mp_encode_double(pmp_buf2, pBigWorldSpace->end.x + pBigWorld->boundary.x);
				mp_encode_double(pmp_buf2, pBigWorldSpace->end.z + pBigWorld->boundary.z);
			}
		}
	}

	//先删除旧的再插入新的。
	double rects[2 * 2];
	rects[0] = pBigWorldSpace->begin.x - pBigWorld->boundary.x;
	rects[1] = pBigWorldSpace->begin.z - pBigWorld->boundary.z;
	rects[2] = pBigWorldSpace->end.x + pBigWorld->boundary.x;
	rects[3] = pBigWorldSpace->end.z + pBigWorld->boundary.z;

	rtree_insert(pBigWorld->prtree, rects, &id);

	if (listLength(pBigWorld->spaceList) > 0) {
		listIter* iter = listGetIterator(pBigWorld->spaceList, AL_START_HEAD);
		listNode* node;

		mp_buf* pmp_buf = mp_buf_new();
		MP_ENCODE_CONST(pmp_buf, "OnInitSpace");
		mp_encode_array(pmp_buf, listLength(pBigWorld->spaceList));

		while ((node = listNext(iter)) != NULL) {
			PBigWorldSpace pBigWorldSpace = (PBigWorldSpace)listNodeValue(node);

			mp_encode_array(pmp_buf, 5);
			mp_encode_double(pmp_buf, u642double(pBigWorldSpace->id));
			mp_encode_double(pmp_buf, pBigWorldSpace->begin.x - pBigWorld->boundary.x);
			mp_encode_double(pmp_buf, pBigWorldSpace->begin.z - pBigWorld->boundary.z);
			mp_encode_double(pmp_buf, pBigWorldSpace->end.x + pBigWorld->boundary.x);
			mp_encode_double(pmp_buf, pBigWorldSpace->end.z + pBigWorld->boundary.z);

			DockerSend(pBigWorldSpace->id, pmp_buf2->b, pmp_buf2->len);
		}

		DockerSend(id, pmp_buf->b, pmp_buf->len);
		mp_buf_free(pmp_buf);
		
	}
	mp_buf_free(pmp_buf2);
	listAddNodeHead(pBigWorld->spaceList, pBigWorldSpace);
	dictAddWithLonglong(pBigWorld->spaceDict, id, pBigWorldSpace);

	lua_pushnil(L);
	return 1;
}

static const luaL_Reg bigworld_funcs[] = {
	{"Create", luaB_Create},
	{"Destroy", luaB_Destroy},
	{"Entry", luaB_BigWorldEntry},
	{"SpaceFull", luaB_SpaceFull},
	{"OnSpace", luaB_OnSpace },
	{NULL, NULL}
};

int LuaOpenBigworld(lua_State* L) {
	luaL_register(L, "bigworldapi", bigworld_funcs);
	return 1;
}


