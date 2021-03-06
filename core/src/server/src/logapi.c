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
#include "elog.h"
#include "sds.h"
#include "filesys.h"
#include "dicthelp.h"

static int luaB_error(lua_State* L) {
 	const char* str = luaL_checkstring(L, 1);
	elog_error(ctg_script, str);
	return 0;
}

static int luaB_warn(lua_State* L) {
	const char* str = luaL_checkstring(L, 1);
	elog_warn(ctg_script, str);
	return 0;
}

static int luaB_stat(lua_State* L) {
	const char* str = luaL_checkstring(L, 1);
	elog_stat(ctg_script, str);
	return 0;
}

static int luaB_fun(lua_State* L) {
	const char* str = luaL_checkstring(L, 1);
	elog_fun(ctg_script, str);
	return 0;
}

static int luaB_details(lua_State* L) {
	const char* str = luaL_checkstring(L, 1);
	elog_details(ctg_script, str);
	return 0;
}

static int luaB_user_error(lua_State* L) {
	const char* str = luaL_checkstring(L, 1);
	elog_error(ctg_user, str);
	return 0;
}

static int luaB_user_warn(lua_State* L) {
	const char* str = luaL_checkstring(L, 1);
	elog_warn(ctg_user, str);
	return 0;
}

static int luaB_user_stat(lua_State* L) {
	const char* str = luaL_checkstring(L, 1);
	elog_stat(ctg_user, str);
	return 0;
}

static int luaB_user_fun(lua_State* L) {
	const char* str = luaL_checkstring(L, 1);
	elog_fun(ctg_user, str);
	return 0;
}

static int luaB_user_details(lua_State* L) {
	const char* str = luaL_checkstring(L, 1);
	elog_details(ctg_user, str);
	return 0;
}

static int luaB_node_error(lua_State* L) {
	const char* str = luaL_checkstring(L, 1);
	elog_error(ctg_node, str);
	return 0;
}

static int luaB_node_warn(lua_State* L) {
	const char* str = luaL_checkstring(L, 1);
	elog_warn(ctg_node, str);
	return 0;
}

static int luaB_node_stat(lua_State* L) {
	const char* str = luaL_checkstring(L, 1);
	elog_stat(ctg_node, str);
	return 0;
}

static int luaB_node_fun(lua_State* L) {
	const char* str = luaL_checkstring(L, 1);
	elog_fun(ctg_node, str);
	return 0;
}

static int luaB_node_details(lua_State* L) {
	const char* str = luaL_checkstring(L, 1);
	elog_details(ctg_node, str);
	return 0;
}

static const luaL_Reg elog_funcs[] = {

  {"error", luaB_user_error},
  {"warn", luaB_user_warn},
  {"stat", luaB_user_stat},
  {"fun", luaB_user_fun},
  {"details", luaB_user_details},

  {"sys_error", luaB_error},
  {"sys_warn", luaB_warn},
  {"sys_stat", luaB_stat},
  {"sys_fun", luaB_fun},
  {"sys_details", luaB_details},

  {"node_error", luaB_node_error},
  {"node_warn", luaB_node_warn},
  {"node_stat", luaB_node_stat},
  {"node_fun", luaB_node_fun},
  {"node_details", luaB_node_details},
  
  {NULL, NULL}
};

int LuaOpenElog(lua_State* L) {
	luaL_register(L, "elog", elog_funcs);
	return 1;
}


