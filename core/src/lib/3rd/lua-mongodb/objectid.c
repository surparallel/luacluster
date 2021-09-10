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

static int m_data(lua_State *L) {
	bson_oid_t *oid = checkObjectID(L, 1);
	lua_pushlstring(L, (const char *)oid->bytes, sizeof oid->bytes);
	return 1;
}

static int m_hash(lua_State *L) {
	pushInt32(L, bson_oid_hash(checkObjectID(L, 1)));
	return 1;
}

static int m__tostring(lua_State *L) {
	char buf[25];
	bson_oid_to_string(checkObjectID(L, 1), buf);
	lua_pushstring(L, buf);
	return 1;
}

static int m__eq(lua_State *L) {
	lua_pushboolean(L, bson_oid_equal(checkObjectID(L, 1), checkObjectID(L, 2)));
	return 1;
}

static const luaL_Reg funcs[] = {
	{"data", m_data},
	{"hash", m_hash},
	{"__tostring", m__tostring},
	{"__eq", m__eq},
	{0, 0}
};

int newObjectID(lua_State *L) {
	size_t len;
	const char *str = lua_tolstring(L, 1, &len);
	if (str) {
		luaL_argcheck(L, bson_oid_is_valid(str, len), 1, "invalid format");
		bson_oid_init_from_string(lua_newuserdata(L, sizeof(bson_oid_t)), str);
	} else {
		if (!lua_isnoneornil(L, 1)) return typeError(L, 1, "string");
		bson_oid_init(lua_newuserdata(L, sizeof(bson_oid_t)), 0);
	}
	setType(L, TYPE_OBJECTID, funcs);
	return 1;
}

void pushObjectID(lua_State *L, const bson_oid_t *oid) {
	bson_oid_copy(oid, lua_newuserdata(L, sizeof *oid));
	setType(L, TYPE_OBJECTID, funcs);
}

bson_oid_t *checkObjectID(lua_State *L, int idx) {
	return luaL_checkudata(L, idx, TYPE_OBJECTID);
}

bson_oid_t *testObjectID(lua_State *L, int idx) {
	return luaL_testudata(L, idx, TYPE_OBJECTID);
}
