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
	rtree_free(pBigWorld->prtree);

	listSetFreeMethod(pBigWorld->spaceList, DestroyFun);
	listRelease(pBigWorld->spaceList);
	dictRelease(pBigWorld->spaceDict);
	free(pBigWorld);
	return 0;
}

//检查地图树，并重新定向到指定地图服务，可能指定到多个地图
static int luaB_Entry(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	PBigWorld pBigWorld = (PBigWorld)lua_touserdata(L, 1);

	unsigned long long entityid = luaL_tou64(L, 1);
	double pointx = luaL_checknumber(L, 2);
	double pointz = luaL_checknumber(L, 3);

	double rects[2 * 2];
	rects[0] = pointx;
	rects[1] = pointz;
	rects[2] = pointx;
	rects[3] = pointz;

	list* result = listCreate();
	rtree_search(pBigWorld->prtree, rects, MultipleIter, result);

	mp_buf* pmp_buf = mp_buf_new();
	mp_encode_bytes(pmp_buf, "OnEntrySpace", strlen("OnEntrySpace"));

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

static int luaB_OnFull(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	PBigWorld pBigWorld = (PBigWorld)lua_touserdata(L, 1);

	unsigned long long id = luaL_tou64(L, 1);

	dictEntry* entry = dictFind(pBigWorld->spaceDict, &id);
	if (entry != NULL) {
		PBigWorldSpace pBigWorldSpace = (PBigWorldSpace)dictGetVal(entry);
		if (pBigWorldSpace->adjust == 1) {
			n_error("OnFull Repeat submission %U", id);
		}
			
		pBigWorldSpace->adjust = 1;
		double linex = (pBigWorldSpace->end.x - pBigWorldSpace->begin.x) / 2;

		if (linex > pBigWorld->boundary.x) {
			pBigWorldSpace->tobegin.x = pBigWorldSpace->begin.x + linex;
		}
		else {
			n_error("Cannot split space again %U", id);
		}
	}

	return 0;
}

static int luaB_OnSpace(lua_State* L) {

	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	PBigWorld pBigWorld = (PBigWorld)lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 1);
	unsigned long long oid = luaL_tou64(L, 2);
	double beginx = luaL_checknumber(L, 3);
	double beginz = luaL_checknumber(L, 4);
	double endx = luaL_checknumber(L, 5);
	double endz = luaL_checknumber(L, 6);

	PBigWorldSpace pBigWorldSpace = calloc(1, sizeof(BigWorldSpace));
	pBigWorldSpace->begin.x = beginx;
	pBigWorldSpace->begin.z = beginz;
	pBigWorldSpace->end.x = endx;
	pBigWorldSpace->end.z = endz;
	pBigWorldSpace->id = id;

	mp_buf* pmp_buf2 = mp_buf_new();
	mp_encode_bytes(pmp_buf2, "OnAlterSpace", sizeof("OnAlterSpace"));
	mp_encode_array(pmp_buf2, 5);
	mp_encode_double(pmp_buf2, u642double(pBigWorldSpace->id));
	mp_encode_double(pmp_buf2, pBigWorldSpace->begin.x);
	mp_encode_double(pmp_buf2, pBigWorldSpace->begin.z);
	mp_encode_double(pmp_buf2, pBigWorldSpace->end.x);
	mp_encode_double(pmp_buf2, pBigWorldSpace->end.z);

	if (oid != 0) {
		dictEntry* entry = dictFind(pBigWorld->spaceDict, &oid);
		if (entry != NULL) {
			PBigWorldSpace pBigWorldSpace =(PBigWorldSpace)dictGetVal(entry);
			if (pBigWorldSpace->adjust = 1) {
				pBigWorldSpace->begin = pBigWorldSpace->tobegin;
				pBigWorldSpace->toend = pBigWorldSpace->toend;
				pBigWorldSpace->adjust = 0;

				double rects[2 * 2];
				rects[0] = pBigWorldSpace->tobegin.x - pBigWorld->boundary.x;
				rects[1] = pBigWorldSpace->tobegin.z - pBigWorld->boundary.z;
				rects[2] = pBigWorldSpace->toend.x + pBigWorld->boundary.x;
				rects[3] = pBigWorldSpace->toend.z + pBigWorld->boundary.z;
				rtree_delete(pBigWorld->prtree, rects, &oid);
				rtree_insert(pBigWorld->prtree, rects, &oid);

				mp_encode_array(pmp_buf2, 5);
				mp_encode_double(pmp_buf2, u642double(pBigWorldSpace->id));
				mp_encode_double(pmp_buf2, pBigWorldSpace->begin.x);
				mp_encode_double(pmp_buf2, pBigWorldSpace->begin.z);
				mp_encode_double(pmp_buf2, pBigWorldSpace->end.x);
				mp_encode_double(pmp_buf2, pBigWorldSpace->end.z);
			}
		}
	}

	//先删除旧的再插入新的。
	double rects[2 * 2];
	rects[0] = beginx - pBigWorld->boundary.x;
	rects[1] = beginz - pBigWorld->boundary.z;
	rects[2] = endx + pBigWorld->boundary.x;
	rects[3] = endz + pBigWorld->boundary.z;

	rtree_insert(pBigWorld->prtree, rects, &id);

	listIter* iter = listGetIterator(pBigWorld->spaceList, AL_START_HEAD);
	listNode* node;

	mp_buf* pmp_buf = mp_buf_new();
	mp_encode_bytes(pmp_buf, "OnInitSpace", sizeof("OnInitSpace"));
	mp_encode_array(pmp_buf, listLength(pBigWorld->spaceList));

	while ((node = listNext(iter)) != NULL) {
		PBigWorldSpace pBigWorldSpace = (PBigWorldSpace)listNodeValue(node);

		mp_encode_array(pmp_buf, 5);
		mp_encode_double(pmp_buf, u642double(pBigWorldSpace->id));
		mp_encode_double(pmp_buf, pBigWorldSpace->begin.x);
		mp_encode_double(pmp_buf, pBigWorldSpace->begin.z);
		mp_encode_double(pmp_buf, pBigWorldSpace->end.x);
		mp_encode_double(pmp_buf, pBigWorldSpace->end.z);

		DockerSend(pBigWorldSpace->id, pmp_buf2->b, pmp_buf2->len);
	}

	DockerSend(id, pmp_buf->b, pmp_buf->len);
	mp_buf_free(pmp_buf);
	mp_buf_free(pmp_buf2);

	listAddNodeHead(pBigWorld->spaceList, pBigWorldSpace);
	dictAddWithLonglong(pBigWorld->spaceDict, id, pBigWorldSpace);

	lua_pushnil(L);
	return 1;
}

static const luaL_Reg bigworld_funcs[] = {
	{"Create", luaB_Create},
	{"Destroy", luaB_Destroy},
	{"Entry", luaB_Entry},
	{"OnFull", luaB_OnFull},
	{"OnSpace", luaB_OnSpace },
	{NULL, NULL}
};

int LuaOpenBigworld(lua_State* L) {
	luaL_register(L, "bigworldapi", bigworld_funcs);
	return 2;
}


