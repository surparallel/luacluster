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

//Sudoku
#include "uv.h"
#include "plateform.h"
#include "dict.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "elog.h"
#include "sds.h"
#include "filesys.h"
#include "dicthelp.h"
#include "lvm.h"
#include "entityid.h"
#include "int64.h"
#include "timesys.h"
#include "adlist.h"
#include "dict.h"
#include "transform.h"
#include "quaternion.h"
#include "vector.h"
#include "vector2.h"
#include "dicthelp.h"
#include <math.h>
#include "lua_cmsgpack.h"
#include "docker.h"
#include "int64.h"
#include "sudoku.h"
#include "rtree.h"
#include "rtreehelp.h"
#include "3dmathapi.h"
#include "go.h"


typedef struct _Go {
	char* goMap;//二维数组标记某个坐标点是否被占用
	void* s;//PSudoku
}*PGo, Go;

int GoMove(void* pGo, unsigned long long id, struct Vector3 position,
	struct Vector3 rotation, float velocity, unsigned int stamp, unsigned int stampStop, unsigned int isGhost, int entityType, int moveType) {
	PGo g = (PGo)pGo;

	if (moveType == 0) {
		struct Vector3 nowPosition;
		CurrentPosition(&position, &rotation, velocity, stamp, stampStop, &nowPosition, stampStop);
		unsigned int x = (unsigned int)nowPosition.x;
		unsigned int z = (unsigned int)nowPosition.z;
		if (g->goMap[x * z] == 1)
			return 0;

		g->goMap[x * z] = 1;
		x = (unsigned int)position.x;
		z = (unsigned int)position.z;
		g->goMap[x * z] = 0;
	}

	SudokuMove(g->s, id, position, rotation, velocity, stamp, stampStop, isGhost, entityType);

	return 1;
}

static int luaB_Move(lua_State* L) {

	PGo pGo = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);

	//position
	struct Vector3 position = { 0 };
	position.x = luaL_checknumber(L, 3);
	position.y = luaL_checknumber(L, 4);
	position.z = luaL_checknumber(L, 5);

	//rotation
	struct Vector3 rotation = { 0 };
	rotation.x = luaL_checknumber(L, 6);
	rotation.y = luaL_checknumber(L, 7);
	rotation.z = luaL_checknumber(L, 8);

	//velocity 每秒移动的速度
	float velocity = luaL_checknumber(L, 9);

	//stamp 秒
	unsigned int stamp = luaL_checkinteger(L, 10);
	unsigned int stampStop = luaL_checkinteger(L, 11);
	unsigned int isGhost = luaL_checkinteger(L, 12);
	unsigned int entityType = luaL_checkinteger(L, 13);
	unsigned int moveType = luaL_checkinteger(L, 14);

	lua_pushinteger(L, GoMove(pGo, id, position, rotation, velocity, stamp, stampStop, isGhost, entityType, moveType));
	return 1;
}

static int luaB_Leave(lua_State* L) {

	PGo pGo = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);

	SudokuLeave(pGo->s, id);
	return 0;
}

static int luaB_Entry(lua_State* L) {

	PGo pGo = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);

	//position
	struct Vector3 position = { 0 };
	position.x = luaL_checknumber(L, 3);
	position.y = luaL_checknumber(L, 4);
	position.z = luaL_checknumber(L, 5);

	//rotation
	struct Vector3 rotation = { 0 };
	rotation.x = luaL_checknumber(L, 6);
	rotation.y = luaL_checknumber(L, 7);
	rotation.z = luaL_checknumber(L, 8);

	//velocity 每秒移动的速度
	float velocity = luaL_checknumber(L, 9);

	//stamp 秒
	unsigned int stamp = luaL_checkinteger(L, 10);
	unsigned int stampStop = luaL_checkinteger(L, 11);
	int isGhost = luaL_checkinteger(L, 12);
	int entityType = luaL_checkinteger(L, 13);
	
	//s_fun("sudoku(%U)::Entry %U ",s->spaceId, id);
	SudokuEntry(pGo->s, id, position, rotation, velocity, stamp, stampStop, isGhost, entityType);
	return 0;
}


//这里要发送广播
static int luaB_Update(lua_State* L) {

	PGo pGo = lua_touserdata(L, 1);
	unsigned int count = luaL_checkinteger(L, 2);
	float deltaTime = luaL_checknumber(L, 3);
	SudokuUpdate(pGo->s, count, deltaTime);
	return 0;
}

void* GoCreate(struct Vector3 gird, struct Vector3 begin, struct Vector3 end, int isBigWorld, unsigned long long spaceId, unsigned int outsideSec) {
	PGo pGo = calloc(1, sizeof(Go));
	if (pGo == NULL)return 0;

	size_t count = (unsigned int)(end.x - begin.x) * (unsigned int)(end.z - begin.z);
	pGo->goMap = calloc(1, count);

	pGo->s = SudokuCreate(gird, begin, end, isBigWorld, spaceId, outsideSec);
	return pGo;
}

static int luaB_Create(lua_State* L) {

	struct Vector3 gird;
	struct Vector3 begin;
	struct Vector3 end;
	gird.x = luaL_checknumber(L, 1);
	gird.z = luaL_checknumber(L, 2);
	begin.x = luaL_checknumber(L, 3);
	begin.z = luaL_checknumber(L, 4);
	end.x = luaL_checknumber(L, 5);
	end.z = luaL_checknumber(L, 6);
	int isBigWorld = luaL_checkint(L, 7);
	unsigned long long spaceId = luaL_tou64(L, 8);
	unsigned int outsideSec = luaL_checkinteger(L, 9);

	lua_pushlightuserdata(L, GoCreate(gird, begin, end, isBigWorld, spaceId, outsideSec));
	return 1;
}

void GoDestory(void* pGo) {
	PGo g = (PGo)pGo;
	SudokuDestory(g->s);
	free(g->goMap);
}

static int luaB_Destroy(lua_State* L) {

	PGo pGo = lua_touserdata(L, 1);
	SudokuDestory(pGo->s);
	return 1;
}

static int luaB_Insert(lua_State* L) {

	PGo pGo = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);

	double rect[4] = { 0 };
	rect[0] = luaL_checknumber(L, 3);
	rect[1] = luaL_checknumber(L, 4);
	rect[2] = luaL_checknumber(L, 5);
	rect[3] = luaL_checknumber(L, 6);

	SudokuInsert(pGo->s, id, rect);
	return 0;
}

void GoAlter(void* pGo, unsigned long long id, double rect[4]) {

	PGo g = pGo;
	if (1 == SudokuAlter(g->s, id, rect)) {
		free(g->goMap);

		size_t count = (unsigned int)(rect[2] - rect[0]) * (unsigned int)(rect[3] - rect[0]);
		g->goMap = calloc(1, count);
	}
}

static int luaB_Alter(lua_State* L) {

	PGo pGo = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);

	double rect[4] = { 0 };
	rect[0] = luaL_checknumber(L, 3);
	rect[1] = luaL_checknumber(L, 4);
	rect[2] = luaL_checknumber(L, 5);
	rect[3] = luaL_checknumber(L, 6);

	GoAlter(pGo, id, rect);
	return 0;
}

static int luaB_SetGhost(lua_State* L) {

	PGo pGo = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);

	SudokuSetGhost(pGo->s, id);
	return 0;
}

static const luaL_Reg go_funcs[] = {
	{"Create", luaB_Create},
	{"Destroy", luaB_Destroy},
	{"Update", luaB_Update},
	{"Move", luaB_Move},
	{"Leave", luaB_Leave},
	{"Entry", luaB_Entry},
	{"Insert", luaB_Insert},
	{"Alter", luaB_Alter},
	{"SetGhost", luaB_SetGhost},
	{NULL, NULL}
};

int LuaOpenGo(lua_State* L) {
	luaL_register(L, "goapi", go_funcs);
	return 1;
}