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
#include "timesys.h"

void CurrentPosition(
	struct Vector3* position, 
	struct Vector3* angles, 
	float velocity, 
	unsigned int stamp, 
	unsigned int stampStop, 
	struct Vector3* out,
	unsigned int current
) {

	if(current == 0)
		current = GetCurrentSec();

	if (stampStop && current >= stampStop)
		current = stampStop;

	struct Quaternion quaternion;
	quatEulerAngles(angles, &quaternion);
	quatMultVector(&quaternion, &gForward, out);

	vector3Scale(out, out, (current - stamp) * velocity);
	vector3Add(position, out, out);
}

static int luaB_Position(lua_State* L) {

	s_fun("math3d::Position");
	//position
	struct Vector3 position;
	position.x = luaL_checknumber(L, 1);
	position.y = luaL_checknumber(L, 2);
	position.z = luaL_checknumber(L, 3);

	//rotation
	struct Vector3 angles;
	angles.x = luaL_checknumber(L,4);
	angles.y = luaL_checknumber(L, 5);;
	angles.z = luaL_checknumber(L, 6);

	//velocity 每秒移动的速度
	float velocity = luaL_checknumber(L, 7);

	//stamp 秒
	unsigned int stamp = luaL_checkinteger(L, 8);
	unsigned int stampStop = luaL_checkinteger(L, 9);

	struct Vector3 out;
	CurrentPosition(&position, &angles, velocity, stamp, stampStop, &out, 0);

	lua_pushnumber(L, out.x);
	lua_pushnumber(L, out.y);
	lua_pushnumber(L, out.z);

	return 3;
}

static int luaB_Dist(lua_State* L) {

	s_fun("sudoku::Dist");
	struct Vector3 position1;
	position1.x = luaL_checknumber(L, 1);
	position1.y = luaL_checknumber(L, 2);;
	position1.z = luaL_checknumber(L, 3);

	//position
	struct Vector3 position;
	position.x = luaL_checknumber(L, 4);
	position.y = luaL_checknumber(L, 5);;
	position.z = luaL_checknumber(L, 6);

	lua_pushnumber(L, sqrtf(vector3DistSqrd(&position, &position)));
	return 1;
}

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

	float distance = sqrtf(vector3MagSqrd(&forward));
	struct Quaternion out;
	quatLookRotation(&forward, &out);
	quatToEulerAngles(&out, &euler);

	lua_pushnumber(L, distance);
	lua_pushnumber(L, euler.x);
	lua_pushnumber(L, euler.y);
	lua_pushnumber(L, euler.z);
	return 4;
}

static const luaL_Reg math3d_funcs[] = {
	{"Position", luaB_Position},
	{"Dist", luaB_Dist},
	{"LookVector", luaB_LookVector},
	{NULL, NULL}
};

int LuaOpenMath3d(lua_State* L) {
	luaL_register(L, "math3d", math3d_funcs);
	return 1;
}

