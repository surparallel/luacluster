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
#include "luaex.h"
#include "dockerapi.h"
#include "srp.h"
#include "bit.h"
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

void* LVMCreate(const char* scriptPath, const char* assetsPath) {

	PLVMHandle pLVMHandle = malloc(sizeof(LVMHandle));
	pLVMHandle->luaVM = luaL_newstate();
	pLVMHandle->scriptPath = sdsnew(scriptPath);

	luaL_openlibs(pLVMHandle->luaVM);
	luaopen_int64(pLVMHandle->luaVM);
	LuaOpenDocker(pLVMHandle->luaVM);
	LuaOpenSudoku(pLVMHandle->luaVM);
	LuaOpenBigworld(pLVMHandle->luaVM);
	luaopen_srp(pLVMHandle->luaVM);
	luaopen_cmsgpack(pLVMHandle->luaVM);
	luaopen_bit(pLVMHandle->luaVM);
	LuaOpenElog(pLVMHandle->luaVM);
	luaopen_mongo(pLVMHandle->luaVM);
	LuaOpenMath3d(pLVMHandle->luaVM);
	LuaAddPath(pLVMHandle->luaVM, "path", (char*)assetsPath);
	luaL_requiref(pLVMHandle->luaVM, "socket.core", luaopen_socket_core, 0);
	luaL_requiref(pLVMHandle->luaVM, "mime.core", luaopen_mime_core, 0);
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

int LVMCallFunction(void* pvLVMHandle, char* sdsFile, char* fun, const char* arg, size_t len) {

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
		elog(log_error, ctg_script, "plg_LvmCallFile.plua_pcall:%s lua:%s", allPath, lua_tolstring(pLVMHandle->luaVM, -1, NULL));
		lua_settop(pLVMHandle->luaVM, top);
		sdsfree(allPath);
		return 0;
	}

	sdsfree(allPath);

	//call fun
	lua_getfield(pLVMHandle->luaVM, LUA_GLOBALSINDEX, fun);

	int argCount = 0;
	//这里要压入参数
	if (arg != NULL)
		argCount = mp_c_unpack(pLVMHandle->luaVM, arg, len);

	if (lua_pcall(pLVMHandle->luaVM, argCount, LUA_MULTRET, 0)) {
		elog(log_error, ctg_script, "plg_LvmCallFile.plua_pcall:%s lua:%s", fun, lua_tolstring(pLVMHandle->luaVM, -1, NULL));
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
	return ret;
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
