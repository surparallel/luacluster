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

static void setFileOpts(lua_State *L, int idx, mongoc_gridfs_file_opt_t *opts) {
	int top = lua_gettop(L);
	memset(opts, 0, sizeof *opts);
	if (lua_isnoneornil(L, idx)) return;
	if (!lua_istable(L, idx)) typeError(L, idx, "table");
	/* Optional arguments are left on the stack to remain valid */
	lua_getfield(L, idx, "aliases");
	opts->aliases = toBSON(L, ++top);
	lua_getfield(L, idx, "chunkSize");
	opts->chunk_size = luaL_optinteger(L, ++top, 0);
	lua_getfield(L, idx, "contentType");
	opts->content_type = luaL_optstring(L, ++top, 0);
	lua_getfield(L, idx, "filename");
	opts->filename = luaL_optstring(L, ++top, 0);
	lua_getfield(L, idx, "md5");
	opts->md5 = luaL_optstring(L, ++top, 0);
	lua_getfield(L, idx, "metadata");
	opts->metadata = toBSON(L, ++top);
}

static int m_createFile(lua_State *L) {
	mongoc_gridfs_t *gridfs = checkGridFS(L, 1);
	mongoc_gridfs_file_opt_t opts;
	setFileOpts(L, 2, &opts);
	pushGridFSFile(L, mongoc_gridfs_create_file(gridfs, &opts), 1);
	return 1;
}

static int m_createFileFrom(lua_State *L) {
	mongoc_gridfs_t *gridfs = checkGridFS(L, 1);
	const char *filename = luaL_checkstring(L, 2);
	mongoc_gridfs_file_opt_t opts;
	mongoc_stream_t *stream;
	mongoc_gridfs_file_t *file;
	setFileOpts(L, 3, &opts);
	stream = mongoc_stream_file_new_for_path(filename, O_RDONLY, 0);
	if (!stream) goto error;
	file = mongoc_gridfs_create_file_from_stream(gridfs, stream, &opts);
	if (!file) goto error;
	pushGridFSFile(L, file, 1);
	return 1;
error:
	lua_pushnil(L);
	lua_pushfstring(L, "%s: %s", filename, strerror(errno));
	return 2;
}

static int m_drop(lua_State *L) {
	mongoc_gridfs_t *gridfs = checkGridFS(L, 1);
	bson_error_t error;
	return commandStatus(L, mongoc_gridfs_drop(gridfs, &error), &error);
}

static int m_find(lua_State *L) {
	mongoc_gridfs_t *gridfs = checkGridFS(L, 1);
	bson_t *query = castBSON(L, 2);
	bson_t *options = toBSON(L, 3);
	pushGridFSFileList(L, mongoc_gridfs_find_with_opts(gridfs, query, options), 1);
	return 1;
}

static int m_findOne(lua_State *L) {
	mongoc_gridfs_t *gridfs = checkGridFS(L, 1);
	bson_t *query = castBSON(L, 2);
	bson_t *options = toBSON(L, 3);
	bson_error_t error;
	mongoc_gridfs_file_t *file = mongoc_gridfs_find_one_with_opts(gridfs, query, options, &error);
	if (!file) return commandError(L, &error);
	pushGridFSFile(L, file, 1);
	return 1;
}

static int m_findOneByFilename(lua_State *L) {
	mongoc_gridfs_t *gridfs = checkGridFS(L, 1);
	const char *filename = luaL_checkstring(L, 2);
	bson_error_t error;
	mongoc_gridfs_file_t *file = mongoc_gridfs_find_one_by_filename(gridfs, filename, &error);
	if (!file) return commandError(L, &error);
	pushGridFSFile(L, file, 1);
	return 1;
}

static int m_getChunks(lua_State *L) {
	pushCollection(L, mongoc_gridfs_get_chunks(checkGridFS(L, 1)), true, 1);
	return 1;
}

static int m_getFiles(lua_State *L) {
	pushCollection(L, mongoc_gridfs_get_files(checkGridFS(L, 1)), true, 1);
	return 1;
}

static int m_removeByFilename(lua_State *L) {
	mongoc_gridfs_t *gridfs = checkGridFS(L, 1);
	const char *filename = luaL_checkstring(L, 2);
	bson_error_t error;
	return commandStatus(L, mongoc_gridfs_remove_by_filename(gridfs, filename, &error), &error);
}

static int m__gc(lua_State *L) {
	mongoc_gridfs_destroy(checkGridFS(L, 1));
	unsetType(L);
	return 0;
}

static const luaL_Reg funcs[] = {
	{"createFile", m_createFile},
	{"createFileFrom", m_createFileFrom},
	{"drop", m_drop},
	{"find", m_find},
	{"findOne", m_findOne},
	{"findOneByFilename", m_findOneByFilename},
	{"getChunks", m_getChunks},
	{"getFiles", m_getFiles},
	{"removeByFilename", m_removeByFilename},
	{"__gc", m__gc},
	{0, 0}
};

void pushGridFS(lua_State *L, mongoc_gridfs_t *gridfs, int pidx) {
	pushHandle(L, gridfs, 0, pidx); /* New environment due to dependants */
	setType(L, TYPE_GRIDFS, funcs);
}

mongoc_gridfs_t *checkGridFS(lua_State *L, int idx) {
	return *(mongoc_gridfs_t **)luaL_checkudata(L, idx, TYPE_GRIDFS);
}
