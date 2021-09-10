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

static int m_command(lua_State *L) {
	mongoc_client_t *client = checkClient(L, 1);
	const char *dbname = luaL_checkstring(L, 2);
	bson_t *command = castBSON(L, 3);
	bson_t *options = toBSON(L, 4);
	mongoc_read_prefs_t *prefs = toReadPrefs(L, 5);
	bson_t reply;
	bson_error_t error;
	bool status = mongoc_client_command_with_opts(client, dbname, command, prefs, options, &reply, &error);
	if (!bson_has_field(&reply, "cursor")) return commandReply(L, status, &reply, &error);
	pushCursor(L, mongoc_cursor_new_from_command_reply_with_opts(client, &reply, options), 1);
	return 1;
}

static int m_getCollection(lua_State *L) {
	mongoc_client_t *client = checkClient(L, 1);
	const char *dbname = luaL_checkstring(L, 2);
	const char *collname = luaL_checkstring(L, 3);
	pushCollection(L, mongoc_client_get_collection(client, dbname, collname), false, 1);
	return 1;
}

static int m_getDatabase(lua_State *L) {
	mongoc_client_t *client = checkClient(L, 1);
	const char *dbname = luaL_checkstring(L, 2);
	pushDatabase(L, mongoc_client_get_database(client, dbname), 1);
	return 1;
}

static int m_getDatabaseNames(lua_State *L) {
	mongoc_client_t *client = checkClient(L, 1);
	bson_t *options = toBSON(L, 2);
	bson_error_t error;
	return commandStrVec(L, mongoc_client_get_database_names_with_opts(client, options, &error), &error);
}

static int m_getDefaultDatabase(lua_State *L) {
	mongoc_database_t *database = mongoc_client_get_default_database(checkClient(L, 1));
	luaL_argcheck(L, database, 1, "default database is not configured");
	pushDatabase(L, database, 1);
	return 1;
}

static int m_getGridFS(lua_State *L) {
	mongoc_client_t *client = checkClient(L, 1);
	const char *dbname = luaL_checkstring(L, 2);
	const char *prefix = lua_tostring(L, 3);
	bson_error_t error;
	mongoc_gridfs_t *gridfs = mongoc_client_get_gridfs(client, dbname, prefix, &error);
	if (!gridfs) return commandError(L, &error);
	pushGridFS(L, gridfs, 1);
	return 1;
}

static int m_getReadPrefs(lua_State *L) {
	pushReadPrefs(L, mongoc_client_get_read_prefs(checkClient(L, 1)));
	return 1;
}
static int m_setReadPrefs(lua_State *L) {
	mongoc_client_t *client = checkClient(L, 1);
	mongoc_read_prefs_t *prefs = checkReadPrefs(L, 2);
	mongoc_client_set_read_prefs(client, prefs);
	return 0;
}

static int m__gc(lua_State *L) {
	mongoc_client_destroy(checkClient(L, 1));
	unsetType(L);
	return 0;
}

static const luaL_Reg funcs[] = {
	{"command", m_command},
	{"getCollection", m_getCollection},
	{"getDatabase", m_getDatabase},
	{"getDatabaseNames", m_getDatabaseNames},
	{"getDefaultDatabase", m_getDefaultDatabase},
	{"getGridFS", m_getGridFS},
	{"getReadPrefs", m_getReadPrefs},
	{"setReadPrefs", m_setReadPrefs},
	{"__gc", m__gc},
	{0, 0}
};

int newClient(lua_State *L) {
	mongoc_client_t *client = mongoc_client_new(luaL_checkstring(L, 1));
	luaL_argcheck(L, client, 1, "invalid format");
	pushHandle(L, client, 0, 0);
	setType(L, TYPE_CLIENT, funcs);
	return 1;
}

mongoc_client_t *checkClient(lua_State *L, int idx) {
	return *(mongoc_client_t **)luaL_checkudata(L, idx, TYPE_CLIENT);
}
