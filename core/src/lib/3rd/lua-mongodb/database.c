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

static int m_addUser(lua_State *L) {
	mongoc_database_t *database = checkDatabase(L, 1);
	const char *username = luaL_checkstring(L, 2);
	const char *password = luaL_checkstring(L, 3);
	bson_t *roles = toBSON(L, 4);
	bson_t *extra = toBSON(L, 5);
	bson_error_t error;
	return commandStatus(L, mongoc_database_add_user(database, username, password, roles, extra, &error), &error);
}

static int m_createCollection(lua_State *L) {
	mongoc_database_t *database = checkDatabase(L, 1);
	const char *collname = luaL_checkstring(L, 2);
	bson_t *options = toBSON(L, 3);
	bson_error_t error;
	mongoc_collection_t *collection = mongoc_database_create_collection(database, collname, options, &error);
	if (!collection) return commandError(L, &error);
	pushCollection(L, collection, false, 1);
	return 1;
}

static int m_drop(lua_State *L) {
	mongoc_database_t *database = checkDatabase(L, 1);
	bson_t *options = toBSON(L, 2);
	bson_error_t error;
	return commandStatus(L, mongoc_database_drop_with_opts(database, options, &error), &error);
}

static int m_getCollection(lua_State *L) {
	mongoc_database_t *database = checkDatabase(L, 1);
	const char *collname = luaL_checkstring(L, 2);
	pushCollection(L, mongoc_database_get_collection(database, collname), false, 1);
	return 1;
}

static int m_getCollectionNames(lua_State *L) {
	mongoc_database_t *database = checkDatabase(L, 1);
	bson_t *options = toBSON(L, 2);
	bson_error_t error;
	return commandStrVec(L, mongoc_database_get_collection_names_with_opts(database, options, &error), &error);
}

static int m_getName(lua_State *L) {
	lua_pushstring(L, mongoc_database_get_name(checkDatabase(L, 1)));
	return 1;
}

static int m_getReadPrefs(lua_State *L) {
	pushReadPrefs(L, mongoc_database_get_read_prefs(checkDatabase(L, 1)));
	return 1;
}

static int m_hasCollection(lua_State *L) {
	mongoc_database_t *database = checkDatabase(L, 1);
	const char *collname = luaL_checkstring(L, 2);
	bson_error_t error;
	return commandStatus(L, mongoc_database_has_collection(database, collname, &error), &error);
}

static int m_removeAllUsers(lua_State *L) {
	mongoc_database_t *database = checkDatabase(L, 1);
	bson_error_t error;
	return commandStatus(L, mongoc_database_remove_all_users(database, &error), &error);
}

static int m_removeUser(lua_State *L) {
	mongoc_database_t *database = checkDatabase(L, 1);
	const char *username = luaL_checkstring(L, 2);
	bson_error_t error;
	return commandStatus(L, mongoc_database_remove_user(database, username, &error), &error);
}

static int m_setReadPrefs(lua_State *L) {
	mongoc_database_t *database = checkDatabase(L, 1);
	mongoc_read_prefs_t *prefs = checkReadPrefs(L, 2);
	mongoc_database_set_read_prefs(database, prefs);
	return 0;
}

static int m__gc(lua_State *L) {
	mongoc_database_destroy(checkDatabase(L, 1));
	unsetType(L);
	return 0;
}

static const luaL_Reg funcs[] = {
	{"addUser", m_addUser},
	{"createCollection", m_createCollection},
	{"drop", m_drop},
	{"getCollection", m_getCollection},
	{"getCollectionNames", m_getCollectionNames},
	{"getName", m_getName},
	{"getReadPrefs", m_getReadPrefs},
	{"hasCollection", m_hasCollection},
	{"removeAllUsers", m_removeAllUsers},
	{"removeUser", m_removeUser},
	{"setReadPrefs", m_setReadPrefs},
	{"__gc", m__gc},
	{0, 0}
};

void pushDatabase(lua_State *L, mongoc_database_t *database, int pidx) {
	pushHandle(L, database, -1, pidx);
	setType(L, TYPE_DATABASE, funcs);
}

mongoc_database_t *checkDatabase(lua_State *L, int idx) {
	return *(mongoc_database_t **)luaL_checkudata(L, idx, TYPE_DATABASE);
}
