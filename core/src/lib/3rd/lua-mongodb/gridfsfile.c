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


static int fileError(lua_State *L, mongoc_gridfs_file_t *file) {
	bson_error_t error;
	lua_pushnil(L);
	if (!mongoc_gridfs_file_error(file, &error)) return 1; /* No actual error */
	lua_pushstring(L, error.message);
	return 2;
}

static int m_getAliases(lua_State *L) {
	pushBSON(L, mongoc_gridfs_file_get_aliases(checkGridFSFile(L, 1)), 0);
	return 1;
}

static int m_getChunkSize(lua_State *L) {
	pushInt32(L, mongoc_gridfs_file_get_chunk_size(checkGridFSFile(L, 1)));
	return 1;
}

static int m_getContentType(lua_State *L) {
	lua_pushstring(L, mongoc_gridfs_file_get_content_type(checkGridFSFile(L, 1)));
	return 1;
}

static int m_getFilename(lua_State *L) {
	lua_pushstring(L, mongoc_gridfs_file_get_filename(checkGridFSFile(L, 1)));
	return 1;
}

static int m_getId(lua_State *L) {
	pushBSONValue(L, mongoc_gridfs_file_get_id(checkGridFSFile(L, 1)));
	return 1;
}

static int m_getLength(lua_State *L) {
	pushInt64(L, mongoc_gridfs_file_get_length(checkGridFSFile(L, 1)));
	return 1;
}

static int m_getMD5(lua_State *L) {
	lua_pushstring(L, mongoc_gridfs_file_get_md5(checkGridFSFile(L, 1)));
	return 1;
}

static int m_getMetadata(lua_State *L) {
	pushBSON(L, mongoc_gridfs_file_get_metadata(checkGridFSFile(L, 1)), 0);
	return 1;
}

static int m_getUploadDate(lua_State *L) {
	pushInt64(L, mongoc_gridfs_file_get_upload_date(checkGridFSFile(L, 1)));
	return 1;
}

static int m_read(lua_State *L) {
	mongoc_gridfs_file_t *file = checkGridFSFile(L, 1);
	lua_Integer maxlen = luaL_checkinteger(L, 2);
	lua_Integer buflen = LUAL_BUFFERSIZE;
	luaL_Buffer b;
	mongoc_iovec_t iov;
	ssize_t n;
	luaL_argcheck(L, maxlen > 0, 2, "must be positive");
	luaL_buffinit(L, &b);
	for (;;) {
		if (buflen > maxlen) buflen = maxlen; /* Read no more than needed */
		iov.iov_len = buflen;
		iov.iov_base = luaL_prepbuffer(&b);
		n = mongoc_gridfs_file_readv(file, &iov, 1, buflen, 0);
		if (n <= 0) break;
		luaL_addsize(&b, n);
		maxlen -= n;
	}
	luaL_pushresult(&b);
	if (n == -1) return fileError(L, file);
	if (!n && !lua_rawlen(L, -1)) lua_pushnil(L); /* EOF */
	return 1;
}

static int m_remove(lua_State *L) {
	mongoc_gridfs_file_t *file = checkGridFSFile(L, 1);
	bson_error_t error;
	return commandStatus(L, mongoc_gridfs_file_remove(file, &error), &error);
}

static int m_save(lua_State *L) {
	mongoc_gridfs_file_t *file = checkGridFSFile(L, 1);
	if (!mongoc_gridfs_file_save(file)) return fileError(L, file);
	lua_pushboolean(L, 1);
	return 1;
}

static int m_seek(lua_State *L) {
	static const char *const whence[] = {"set", "cur", "end", 0};
	static const int modes[] = {SEEK_SET, SEEK_CUR, SEEK_END};
	mongoc_gridfs_file_t *file = checkGridFSFile(L, 1);
	int64_t offset = checkInt64(L, 2);
	int i = luaL_checkoption(L, 3, whence[0], whence);
	lua_pushboolean(L, !mongoc_gridfs_file_seek(file, offset, modes[i]));
	return 1;
}

static int m_setAliases(lua_State *L) {
	mongoc_gridfs_file_t *file = checkGridFSFile(L, 1);
	const bson_t *aliases = castBSON(L, 2);
	mongoc_gridfs_file_set_aliases(file, aliases);
	return 0;
}

static int m_setContentType(lua_State *L) {
	mongoc_gridfs_file_t *file = checkGridFSFile(L, 1);
	const char *contentType = luaL_checkstring(L, 2);
	mongoc_gridfs_file_set_content_type(file, contentType);
	return 0;
}

static int m_setFilename(lua_State *L) {
	mongoc_gridfs_file_t *file = checkGridFSFile(L, 1);
	const char *filename = luaL_checkstring(L, 2);
	mongoc_gridfs_file_set_filename(file, filename);
	return 0;
}

static int m_setId(lua_State *L) {
	mongoc_gridfs_file_t *file = checkGridFSFile(L, 1);
	bson_value_t value;
	bson_error_t error;
	bool status;
	toBSONValue(L, 2, &value);
	status = mongoc_gridfs_file_set_id(file, &value, &error);
	bson_value_destroy(&value);
	checkStatus(L, status, &error);
	return 0;
}

static int m_setMD5(lua_State *L) {
	mongoc_gridfs_file_t *file = checkGridFSFile(L, 1);
	const char *md5 = luaL_checkstring(L, 2);
	mongoc_gridfs_file_set_md5(file, md5);
	return 0;
}

static int m_setMetadata(lua_State *L) {
	mongoc_gridfs_file_t *file = checkGridFSFile(L, 1);
	const bson_t *metadata = castBSON(L, 2);
	mongoc_gridfs_file_set_metadata(file, metadata);
	return 0;
}

static int m_tell(lua_State *L) {
	pushInt64(L, mongoc_gridfs_file_tell(checkGridFSFile(L, 1)));
	return 1;
}

static int m_write(lua_State *L) {
	mongoc_gridfs_file_t *file = checkGridFSFile(L, 1);
	size_t len;
	const char *str = luaL_checklstring(L, 2, &len);
	mongoc_iovec_t iov;
	int64_t n;
	iov.iov_len = len;
	iov.iov_base = (char *)str;
	n = mongoc_gridfs_file_writev(file, &iov, 1, 0);
	if (n == -1) return fileError(L, file);
	pushInt64(L, n);
	return 1;
}

static int m__gc(lua_State *L) {
	mongoc_gridfs_file_destroy(checkGridFSFile(L, 1));
	unsetType(L);
	return 0;
}

static const luaL_Reg funcs[] = {
	{"getAliases", m_getAliases},
	{"getChunkSize", m_getChunkSize},
	{"getContentType", m_getContentType},
	{"getFilename", m_getFilename},
	{"getId", m_getId},
	{"getLength", m_getLength},
	{"getMD5", m_getMD5},
	{"getMetadata", m_getMetadata},
	{"getUploadDate", m_getUploadDate},
	{"read", m_read},
	{"remove", m_remove},
	{"save", m_save},
	{"seek", m_seek},
	{"setAliases", m_setAliases},
	{"setContentType", m_setContentType},
	{"setFilename", m_setFilename},
	{"setId", m_setId},
	{"setMD5", m_setMD5},
	{"setMetadata", m_setMetadata},
	{"tell", m_tell},
	{"write", m_write},
	{"__len", m_getLength},
	{"__gc", m__gc},
	{0, 0}
};

void pushGridFSFile(lua_State *L, mongoc_gridfs_file_t *file, int pidx) {
	pushHandle(L, file, -1, pidx);
	setType(L, TYPE_GRIDFSFILE, funcs);
}

mongoc_gridfs_file_t *checkGridFSFile(lua_State *L, int idx) {
	return *(mongoc_gridfs_file_t **)luaL_checkudata(L, idx, TYPE_GRIDFSFILE);
}
