/*
** Copyright (C) 2016-2021 Arseny Vakhrushev <arseny.vakhrushev@me.com>
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
** THE SOFTWARE.
*/

#include "common.h"

#if LUA_VERSION_NUM < 502
void *luaL_testudata(lua_State* L, int idx, const char *name) {
	void *obj = lua_touserdata(L, idx);
	if (!obj || !lua_getmetatable(L, idx)) return 0;
	luaL_getmetatable(L, name);
	if (!lua_rawequal(L, -1, -2)) obj = 0;
	lua_pop(L, 2);
	return obj;
}
#endif

static int m__tostring(lua_State *L) {
	lua_pushfstring(L, "%s: %p", typeName(L, 1), lua_topointer(L, 1));
	return 1;
}

bool newType(lua_State *L, const char *name, const luaL_Reg *funcs) {
	if (!luaL_newmetatable(L, name)) return false;
	lua_pushboolean(L, 0);
	lua_setfield(L, -2, "__metatable");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pushstring(L, name);
	lua_setfield(L, -2, "__name");
	lua_pushcfunction(L, m__tostring);
	lua_setfield(L, -2, "__tostring");
#if LUA_VERSION_NUM < 502
	luaL_register(L, 0, funcs);
#else
	luaL_setfuncs(L, funcs, 0);
#endif
	return true;
}

void setType(lua_State *L, const char *name, const luaL_Reg *funcs) {
	newType(L, name, funcs);
	lua_setmetatable(L, -2);
}

void unsetType(lua_State *L) {
	lua_pushnil(L);
	lua_setmetatable(L, -2);
}

const char *typeName(lua_State *L, int idx) {
	if (luaL_getmetafield(L, idx, "__name")) { /* Use object's typename */
		const char *name = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : 0;
		lua_pop(L, 1);
		if (name) return name; /* Valid until object is GC'ed */
	}
	return luaL_typename(L, idx);
}

int typeError(lua_State *L, int idx, const char *name) {
	return argError(L, idx, "%s expected, got %s", name, typeName(L, idx));
}

void pushHandle(lua_State *L, void *obj, int mode, int pidx) {
	check(L, obj);
	*(void **)lua_newuserdata(L, sizeof obj) = obj;
	if (mode < 0) lua_getuservalue(L, pidx); /* Inherit environment */
	else { /* New environment */
		lua_createtable(L, 3, 0);
		lua_pushvalue(L, -2);
		lua_rawseti(L, -2, 1); /* env[1]: handle */
		lua_pushinteger(L, mode);
		lua_rawseti(L, -2, 2); /* env[2]: mode */
		if (pidx) {
			lua_getuservalue(L, pidx);
			lua_rawseti(L, -2, 3); /* env[3]: parent environment */
		}
	}
	lua_setuservalue(L, -2);
}

int getHandleMode(lua_State *L, int idx) {
	int mode;
	lua_getuservalue(L, idx);
	lua_rawgeti(L, -1, 1); /* env[1]: handle */
	if (!lua_rawequal(L, -1, idx)) {
		lua_pop(L, 2);
		return 0;
	}
	lua_rawgeti(L, -2, 2); /* env[2]: mode */
	mode = lua_tointeger(L, -1);
	lua_pop(L, 3);
	return mode;
}

void packParams(lua_State *L, int n) {
	lua_settop(L, n);
	lua_createtable(L, n, 0);
	lua_insert(L, 1);
	while (n) lua_rawseti(L, 1, n--);
}

int unpackParams(lua_State *L, int idx) {
	int i = 0, n = lua_rawlen(L, idx);
	luaL_checkstack(L, LUA_MINSTACK + n, "too many parameters");
	while (i < n) lua_rawgeti(L, idx, ++i);
	return n;
}

void checkStatus(lua_State *L, bool status, const bson_error_t *error) {
	if (!status) luaL_error(L, "%s", error->message);
}

int commandError(lua_State *L, const bson_error_t *error) {
	lua_pushnil(L);
	if (!error->code) return 1; /* No actual error */
	lua_pushstring(L, error->message);
	return 2;
}

int commandStatus(lua_State *L, bool status, const bson_error_t *error) {
	if (!status) return commandError(L, error);
	lua_pushboolean(L, 1);
	return 1;
}

int commandReply(lua_State *L, bool status, bson_t *reply, const bson_error_t *error) {
	if (!status) {
		bson_destroy(reply);
		return commandError(L, error);
	}
	pushBSONWithSteal(L, reply);
	return 1;
}

int commandStrVec(lua_State *L, char **strv, const bson_error_t *error) {
	int i = 0;
	if (!strv) return commandError(L, error);
	lua_newtable(L);
	while (strv[i]) {
		lua_pushstring(L, strv[i]);
		lua_rawseti(L, -2, ++i);
	}
	bson_strfreev(strv);
	return 1;
}
