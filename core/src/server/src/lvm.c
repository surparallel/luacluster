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
#include "lua_cmsgpack.h"
#include "dockerapi.h"
#include "srp.h"
#include "logapi.h"
#include "luasocket.h"
#include "mime.h"
#include "int64.h"
#include "sudoku.h"
#include "bigworld.h"
#include "lua-mongodb.h"
#include "3dmathapi.h"

typedef struct _LVMHandle
{
	lua_State* luaVM;//lua handle
	sds scriptPath;
	int luaHot;
}*PLVMHandle, LVMHandle;

void LuaAddPath(lua_State* ls, char* name, char* value)
{
	sds v;
	lua_getglobal(ls, "package");
	lua_getfield(ls, -1, name);
	v = sdsnew(lua_tostring(ls, -1));
	v = sdscat(v, ";");
	v = sdscat(v, value);
	lua_pushstring(ls, v);
	lua_setfield(ls, -3, name);
	lua_pop(ls, 2);

	sdsfree(v);
}

#if LUA_VERSION_NUM > 501 && !defined(LUA_COMPAT_MODULE)

#else
LUALIB_API void luaL_requiref(lua_State* L, const char* modname,
	lua_CFunction openf, int glb){
	openf(L);
}
#endif

void* LVMCreate(const char* scriptPath, const char* assetsPath) {

	PLVMHandle pLVMHandle = malloc(sizeof(LVMHandle));
	pLVMHandle->luaVM = luaL_newstate();
	pLVMHandle->scriptPath = sdsnew(scriptPath);

	luaL_openlibs(pLVMHandle->luaVM);

	luaL_requiref(pLVMHandle->luaVM, "int64", luaopen_int64, 1);
	luaL_requiref(pLVMHandle->luaVM, "docker", LuaOpenDocker, 1);
	luaL_requiref(pLVMHandle->luaVM, "sudokuapi", LuaOpenSudoku, 1);
	luaL_requiref(pLVMHandle->luaVM, "bigworldapi", LuaOpenBigworld, 1);
	luaL_requiref(pLVMHandle->luaVM, "srp", luaopen_srp, 1);
	luaL_requiref(pLVMHandle->luaVM, "cmsgpack", luaopen_cmsgpack, 1);
	luaL_requiref(pLVMHandle->luaVM, "elog", LuaOpenElog, 1);
	luaL_requiref(pLVMHandle->luaVM, "mongo", luaopen_mongo, 1);
	luaL_requiref(pLVMHandle->luaVM, "math3d", LuaOpenMath3d, 1);
	luaL_requiref(pLVMHandle->luaVM, "socket.core", luaopen_socket_core, 1);
	luaL_requiref(pLVMHandle->luaVM, "mime.core", luaopen_mime_core, 1);
	
	if(sdslen((sds)assetsPath) != 0)
		LuaAddPath(pLVMHandle->luaVM, "path", (char*)assetsPath);
	LuaAddPath(pLVMHandle->luaVM, "path", (char*)"./lua/?.lua;");

	return pLVMHandle;
}

void LVMDestory(void* pvlVMHandle) {
	PLVMHandle pLVMHandle = pvlVMHandle;
	if (pLVMHandle == 0) {
		return;
	}
	
	sdsfree(pLVMHandle->scriptPath);
	lua_close(pLVMHandle->luaVM);
	free(pLVMHandle);
}

int LVMCallFunction(void* pvLVMHandle, char* sdsFile, char* fun) {

	PLVMHandle pLVMHandle = pvLVMHandle;
	int top = lua_gettop(pLVMHandle->luaVM);
	sds allPath = sdscatfmt(sdsempty(), "%s%s.lua", pLVMHandle->scriptPath, sdsFile);

	if (luaL_loadfile(pLVMHandle->luaVM, allPath)) {
		elog(log_error, ctg_script, "LVMCallFile.pluaL_loadfilex:%s", lua_tolstring(pLVMHandle->luaVM, -1, NULL));
		lua_settop(pLVMHandle->luaVM, top);
		sdsfree(allPath);
		return 0;
	}

	//load fun
	if (lua_pcall(pLVMHandle->luaVM, 0, LUA_MULTRET, 0)) {
		elog(log_error, ctg_script, "LVMCallFunction.plua_pcall:%s lua:%s", allPath, lua_tolstring(pLVMHandle->luaVM, -1, NULL));
		lua_settop(pLVMHandle->luaVM, top);
		sdsfree(allPath);
		return 0;
	}

	sdsfree(allPath);

	//call fun
	lua_getglobal(pLVMHandle->luaVM, fun);
	if (lua_pcall(pLVMHandle->luaVM, 0, LUA_MULTRET, 0)) {
		elog(log_error, ctg_script, "LVMCallFunction.plua_pcall:%s lua:%s", fun, lua_tolstring(pLVMHandle->luaVM, -1, NULL));
		lua_settop(pLVMHandle->luaVM, 0);
		return 0;
	}

	double ret = 1;
	if (lua_isnumber(pLVMHandle->luaVM, -1)) {
		ret = lua_tonumber(pLVMHandle->luaVM, -1);
	}

	//如果函数不返回呢？函数返回其他类型参数呢？
	//clear lua --lua_pop(L,1) lua_settop(L, -(n)-1)
	lua_settop(pLVMHandle->luaVM, 0);
	return (int)ret;
}

void LVMSetGlobleInt(void* pvLVMHandle, const char* name, int id) {
	PLVMHandle pLVMHandle = pvLVMHandle;

	lua_pushinteger(pLVMHandle->luaVM, id);
	lua_setglobal(pLVMHandle->luaVM, name);
}

void LVMSetGlobleLightUserdata(void* pvLVMHandle, const char* name, void* pVoid) {
	PLVMHandle pLVMHandle = pvLVMHandle;

	lua_pushlightuserdata(pLVMHandle->luaVM, pVoid);
	lua_setglobal(pLVMHandle->luaVM, name);
}

void* LVMGetGlobleLightUserdata(lua_State* L, const char* name) {

	void* ret;
	lua_getglobal(L, name);
	ret = lua_touserdata(L, -1);
	lua_pop(L, 1);
	return ret;
}
