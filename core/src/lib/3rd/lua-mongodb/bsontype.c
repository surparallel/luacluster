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

static int m_unpack(lua_State *L) {
	return lua_istable(L, 1) ? unpackParams(L, 1) : 0;
}

static int m__tostring(lua_State *L) {
	int i, n;
	lua_settop(L, 1);
	luaL_argcheck(L, lua_istable(L, 1) && luaL_getmetafield(L, 1, "__name"), 1, "invalid object");
	n = lua_rawlen(L, 1);
	if (!n) return 1; /* Type name with no arguments */
	luaL_checkstack(L, LUA_MINSTACK + n * 2, "too many parameters");
	lua_pushliteral(L, "(");
	for (i = 0; i < n; ++i) {
		if (i) lua_pushliteral(L, ", ");
		lua_rawgeti(L, 1, i + 1);
		if (luaL_callmeta(L, -1, "__tostring")) lua_replace(L, -2);
		if (lua_type(L, -1) != LUA_TSTRING) continue;
		lua_pushfstring(L, "\"%s\"", lua_tostring(L, -1));
		lua_replace(L, -2);
	}
	lua_pushliteral(L, ")");
	lua_concat(L, lua_gettop(L) - 1);
	return 1;
}

static const luaL_Reg funcs[] = {
	{"unpack", m_unpack},
	{"__tostring", m__tostring},
	{0, 0}
};

static void setBSONType(lua_State *L, const char *name, bson_type_t type) {
	if (newType(L, name, funcs)) {
		lua_pushinteger(L, type);
		lua_setfield(L, -2, "__type");
	}
	lua_setmetatable(L, -2);
}

int newBinary(lua_State *L) {
	luaL_checkstring(L, 1);
	luaL_optinteger(L, 2, 0);
	packParams(L, 2);
	setBSONType(L, TYPE_BINARY, BSON_TYPE_BINARY);
	return 1;
}

int newDateTime(lua_State *L) {
	checkInt64(L, 1);
	packParams(L, 1);
	setBSONType(L, TYPE_DATETIME, BSON_TYPE_DATE_TIME);
	return 1;
}

int newDecimal128(lua_State *L) {
	const char *str = luaL_checkstring(L, 1);
	bson_decimal128_t dec;
	luaL_argcheck(L, bson_decimal128_from_string(str, &dec), 1, "invalid format");
	packParams(L, 1);
	setBSONType(L, TYPE_DECIMAL128, BSON_TYPE_DECIMAL128);
	return 1;
}

int newDouble(lua_State *L) {
	luaL_checknumber(L, 1);
	packParams(L, 1);
	setBSONType(L, TYPE_DOUBLE, BSON_TYPE_DOUBLE);
	return 1;
}

int newInt32(lua_State *L) {
	checkInt32(L, 1);
	packParams(L, 1);
	setBSONType(L, TYPE_INT32, BSON_TYPE_INT32);
	return 1;
}

int newInt64(lua_State *L) {
	checkInt64(L, 1);
	packParams(L, 1);
	setBSONType(L, TYPE_INT64, BSON_TYPE_INT64);
	return 1;
}

int newJavascript(lua_State *L) {
	luaL_checkstring(L, 1);
	toBSON(L, 2);
	packParams(L, 2);
	setBSONType(L, TYPE_JAVASCRIPT, BSON_TYPE_CODE);
	return 1;
}

int newRegex(lua_State *L) {
	luaL_checkstring(L, 1);
	luaL_optstring(L, 2, 0);
	packParams(L, 2);
	setBSONType(L, TYPE_REGEX, BSON_TYPE_REGEX);
	return 1;
}

int newTimestamp(lua_State *L) {
	checkInt32(L, 1);
	checkInt32(L, 2);
	packParams(L, 2);
	setBSONType(L, TYPE_TIMESTAMP, BSON_TYPE_TIMESTAMP);
	return 1;
}

void pushMaxKey(lua_State *L) {
	lua_newtable(L);
	setBSONType(L, TYPE_MAXKEY, BSON_TYPE_MAXKEY);
}

void pushMinKey(lua_State *L) {
	lua_newtable(L);
	setBSONType(L, TYPE_MINKEY, BSON_TYPE_MINKEY);
}

void pushNull(lua_State *L) {
	lua_newtable(L);
	setBSONType(L, TYPE_NULL, BSON_TYPE_NULL);
}
