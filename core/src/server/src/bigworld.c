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

typedef struct _BitWorld
{
	struct rtree grtree;

}*PBitWorld, BitWorld;

static int luaB_Create(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	void* s = malloc(sizeof(BitWorld));
	lua_pushlightuserdata(L, s);
	return 1;
}

static int luaB_Destroy(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	PBitWorld s = lua_touserdata(L, 1);

	free(s);
	return 1;
}

static int luaB_Entry(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	PBitWorld s = (PBitWorld)lua_touserdata(L, 1);

	return 1;
}


static const luaL_Reg bigworld_funcs[] = {
	{"Create", luaB_Create},
	{"Destroy", luaB_Destroy},
	{"Entry", luaB_Entry},
	{NULL, NULL}
};

int LuaOpenBigworld(lua_State* L) {
	luaL_register(L, "bigworld", bigworld_funcs);
	return 2;
}


