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

static int m_iterator(lua_State *L) {
	checkGridFSFileList(L, 1);
	lua_pushvalue(L, lua_upvalueindex(1)); /* Iterator */
	lua_pushvalue(L, 1); /* State */
	return 2;
}

static int m_next(lua_State *L) {
	mongoc_gridfs_file_list_t *list = checkGridFSFileList(L, 1);
	mongoc_gridfs_file_t *file = mongoc_gridfs_file_list_next(list);
	bson_error_t error;
	if (file) {
		pushGridFSFile(L, file, 1);
		return 1;
	}
	if (mongoc_gridfs_file_list_error(list, &error)) {
		checkStatus(L, !lua_toboolean(L, lua_upvalueindex(1)), &error); /* Throw exception in iterator mode */
		return commandError(L, &error);
	}
	lua_pushnil(L);
	return 1;
}

static int m__gc(lua_State *L) {
	mongoc_gridfs_file_list_destroy(checkGridFSFileList(L, 1));
	unsetType(L);
	return 0;
}

static const luaL_Reg funcs[] = {
	{"next", m_next},
	{"__gc", m__gc},
	{0, 0}
};

void pushGridFSFileList(lua_State *L, mongoc_gridfs_file_list_t *list, int pidx) {
	pushHandle(L, list, -1, pidx);
	if (newType(L, TYPE_GRIDFSFILELIST, funcs)) {
		lua_pushboolean(L, 1); /* Iterator mode on */
		lua_pushcclosure(L, m_next, 1); /* Iterator ... */
		lua_pushcclosure(L, m_iterator, 1); /* ... cached as upvalue 1 */
		lua_setfield(L, -2, "iterator");
	}
	lua_setmetatable(L, -2);
}

mongoc_gridfs_file_list_t *checkGridFSFileList(lua_State *L, int idx) {
	return *(mongoc_gridfs_file_list_t **)luaL_checkudata(L, idx, TYPE_GRIDFSFILELIST);
}
