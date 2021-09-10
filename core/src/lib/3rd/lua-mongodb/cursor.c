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

static int iterator(lua_State *L) {
	return iterateCursor(L, checkCursor(L, 1), lua_upvalueindex(1));
}

static int m_iterator(lua_State *L) {
	checkCursor(L, 1);
	if (lua_isnoneornil(L, 2)) lua_pushvalue(L, lua_upvalueindex(1)); /* Default iterator */
	else {
		lua_pushvalue(L, 2);
		lua_pushcclosure(L, iterator, 1); /* Iterator with handler */
	}
	lua_pushvalue(L, 1); /* State */
	return 2;
}

static int m_more(lua_State *L) {
	lua_pushboolean(L, mongoc_cursor_more(checkCursor(L, 1)));
	return 1;
}

static int m_next(lua_State *L) {
	return iterateCursor(L, checkCursor(L, 1), 0);
}

static int m_value(lua_State *L) {
	return iterateCursor(L, checkCursor(L, 1), 2);
}

static int m__gc(lua_State *L) {
	mongoc_cursor_destroy(checkCursor(L, 1));
	unsetType(L);
	return 0;
}

static const luaL_Reg funcs[] = {
	{"more", m_more},
	{"next", m_next},
	{"value", m_value},
	{"__gc", m__gc},
	{0, 0}
};

void pushCursor(lua_State *L, mongoc_cursor_t *cursor, int pidx) {
	pushHandle(L, cursor, -1, pidx);
	if (newType(L, TYPE_CURSOR, funcs)) {
		lua_pushcfunction(L, iterator); /* Default iterator ... */
		lua_pushcclosure(L, m_iterator, 1); /* ... cached as upvalue 1 */
		lua_setfield(L, -2, "iterator");
	}
	lua_setmetatable(L, -2);
}

int iterateCursor(lua_State *L, mongoc_cursor_t *cursor, int hidx) {
	const bson_t *bson;
	bson_error_t error;
	if (mongoc_cursor_next(cursor, &bson)) {
		pushBSON(L, bson, hidx);
		return 1;
	}
	if (mongoc_cursor_error(cursor, &error)) {
		checkStatus(L, !hidx, &error); /* Throw exception if unpacking */
		return commandError(L, &error);
	}
	lua_pushnil(L);
	return 1;
}

mongoc_cursor_t *checkCursor(lua_State *L, int idx) {
	return *(mongoc_cursor_t **)luaL_checkudata(L, idx, TYPE_CURSOR);
}
