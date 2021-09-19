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
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "elog.h"
#include "sds.h"
#include "filesys.h"
#include "dicthelp.h"
#include "quaternion.h"
#include "vector.h"

static int luaB_LookVector(lua_State* L) {

	struct Vector3 a;
	a.x = luaL_checknumber(L, 1);
	a.y = luaL_checknumber(L, 2);
	a.z = luaL_checknumber(L, 3);

	struct Vector3 b;
	b.x = luaL_checknumber(L, 4);
	b.y = luaL_checknumber(L, 5);
	b.z = luaL_checknumber(L, 6);

	struct Vector3 forward, euler;
	vector3Sub(&b, &a, &forward);
	struct Quaternion out;
	quatLookRotation(&forward, &out);
	quatToEulerAngles(&out, &euler);

	lua_pushnumber(L, euler.x);
	lua_pushnumber(L, euler.y);
	lua_pushnumber(L, euler.z);
	return 3;
}

static const luaL_Reg math3d_funcs[] = {
  {"LookVector", luaB_LookVector},
  {NULL, NULL}
};

int LuaOpenMath3d(lua_State* L) {
	luaL_register(L, "math3d", math3d_funcs);
	return 2;
}

