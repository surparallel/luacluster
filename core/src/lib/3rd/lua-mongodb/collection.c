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

static int m_aggregate(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	bson_t *pipeline = castBSON(L, 2);
	bson_t *options = toBSON(L, 3);
	mongoc_read_prefs_t *prefs = toReadPrefs(L, 4);
	pushCursor(L, mongoc_collection_aggregate(collection, MONGOC_QUERY_NONE, pipeline, options, prefs), 1);
	return 1;
}

static int m_count(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	bson_t *query = castBSON(L, 2);
	bson_t *options = toBSON(L, 3);
	mongoc_read_prefs_t *prefs = toReadPrefs(L, 4);
	bson_error_t error;
	int64_t n = mongoc_collection_count_documents(collection, query, options, prefs, 0, &error);
	if (n == -1) return commandError(L, &error);
	pushInt64(L, n);
	return 1;
}

static int m_createBulkOperation(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	bson_t *options = toBSON(L, 2);
	pushBulkOperation(L, mongoc_collection_create_bulk_operation_with_opts(collection, options), 1);
	return 1;
}

static int m_drop(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	bson_t *options = toBSON(L, 2);
	bson_error_t error;
	return commandStatus(L, mongoc_collection_drop_with_opts(collection, options, &error), &error);
}

static int m_find(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	bson_t *query = castBSON(L, 2);
	bson_t *options = toBSON(L, 3);
	mongoc_read_prefs_t *prefs = toReadPrefs(L, 4);
	pushCursor(L, mongoc_collection_find_with_opts(collection, query, options, prefs), 1);
	return 1;
}

static int m_findAndModify(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	bson_t *query = castBSON(L, 2);
	bson_t *options = castBSON(L, 3);
	bson_t reply;
	bson_error_t error;
	int nres = 1;
	mongoc_find_and_modify_opts_t *opts = mongoc_find_and_modify_opts_new();
	mongoc_find_and_modify_opts_append(opts, options);
	if (!mongoc_collection_find_and_modify_with_opts(collection, query, opts, &reply, &error)) nres = commandError(L, &error);
	else pushBSONField(L, &reply, "value", false);
	mongoc_find_and_modify_opts_destroy(opts);
	bson_destroy(&reply);
	return nres;
}

static int m_findOne(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	bson_t *query = castBSON(L, 2);
	bson_t *options = toBSON(L, 3);
	mongoc_read_prefs_t *prefs = toReadPrefs(L, 4);
	bson_t opts;
	mongoc_cursor_t *cursor;
	int nres;
	bson_init(&opts);
	if (options) bson_copy_to_excluding_noinit(options, &opts, "limit", "singleBatch", (char *)0);
	BSON_APPEND_INT32(&opts, "limit", 1);
	BSON_APPEND_BOOL(&opts, "singleBatch", true);
	cursor = mongoc_collection_find_with_opts(collection, query, &opts, prefs);
	nres = iterateCursor(L, cursor, 0);
	mongoc_cursor_destroy(cursor);
	bson_destroy(&opts);
	return nres;
}

static int m_getName(lua_State *L) {
	lua_pushstring(L, mongoc_collection_get_name(checkCollection(L, 1)));
	return 1;
}

static int m_getReadPrefs(lua_State *L) {
	pushReadPrefs(L, mongoc_collection_get_read_prefs(checkCollection(L, 1)));
	return 1;
}

static int m_insert(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	bson_t *document = castBSON(L, 2);
	int flags = toInsertFlags(L, 3);
	bson_error_t error;
	return commandStatus(L, mongoc_collection_insert(collection, flags, document, 0, &error), &error);
}

static int m_insertMany(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	int i, n = lua_gettop(L) - 1;
	const bson_t *documents[LUA_MINSTACK];
	bson_error_t error;
	if (n > LUA_MINSTACK) return luaL_error(L, "too many documents");
	for (i = 0; i < n; ++i) documents[i] = castBSON(L, i + 2);
	return commandStatus(L, mongoc_collection_insert_many(collection, documents, n, 0, 0, &error), &error);
}

static int m_insertOne(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	bson_t *document = castBSON(L, 2);
	bson_t *options = toBSON(L, 3);
	bson_error_t error;
	return commandStatus(L, mongoc_collection_insert_one(collection, document, options, 0, &error), &error);
}

static int m_remove(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	bson_t *query = castBSON(L, 2);
	int flags = toRemoveFlags(L, 3);
	bson_error_t error;
	return commandStatus(L, mongoc_collection_remove(collection, flags, query, 0, &error), &error);
}

static int m_removeMany(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	bson_t *query = castBSON(L, 2);
	bson_t *options = toBSON(L, 3);
	bson_error_t error;
	return commandStatus(L, mongoc_collection_delete_many(collection, query, options, 0, &error), &error);
}

static int m_removeOne(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	bson_t *query = castBSON(L, 2);
	bson_t *options = toBSON(L, 3);
	bson_error_t error;
	return commandStatus(L, mongoc_collection_delete_one(collection, query, options, 0, &error), &error);
}

static int m_rename(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	const char *dbname = luaL_checkstring(L, 2);
	const char *collname = luaL_checkstring(L, 3);
	bool force = lua_toboolean(L, 4);
	bson_t *options = toBSON(L, 5);
	bson_error_t error;
	return commandStatus(L, mongoc_collection_rename_with_opts(collection, dbname, collname, force, options, &error), &error);
}

static int m_replaceOne(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	bson_t *query = castBSON(L, 2);
	bson_t *document = castBSON(L, 3);
	bson_t *options = toBSON(L, 4);
	bson_error_t error;
	return commandStatus(L, mongoc_collection_replace_one(collection, query, document, options, 0, &error), &error);
}

static int m_setReadPrefs(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	mongoc_read_prefs_t *prefs = checkReadPrefs(L, 2);
	mongoc_collection_set_read_prefs(collection, prefs);
	return 0;
}

static int m_update(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	bson_t *query = castBSON(L, 2);
	bson_t *document = castBSON(L, 3);
	int flags = toUpdateFlags(L, 4);
	bson_error_t error;
	return commandStatus(L, mongoc_collection_update(collection, flags, query, document, 0, &error), &error);
}

static int m_updateMany(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	bson_t *query = castBSON(L, 2);
	bson_t *document = castBSON(L, 3);
	bson_t *options = toBSON(L, 4);
	bson_error_t error;
	return commandStatus(L, mongoc_collection_update_many(collection, query, document, options, 0, &error), &error);
}

static int m_updateOne(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	bson_t *query = castBSON(L, 2);
	bson_t *document = castBSON(L, 3);
	bson_t *options = toBSON(L, 4);
	bson_error_t error;
	return commandStatus(L, mongoc_collection_update_one(collection, query, document, options, 0, &error), &error);
}

static int m__gc(lua_State *L) {
	mongoc_collection_t *collection = checkCollection(L, 1);
	if (getHandleMode(L, 1)) return 0; /* Reference handle */
	mongoc_collection_destroy(collection);
	unsetType(L);
	return 0;
}

static const luaL_Reg funcs[] = {
	{"aggregate", m_aggregate},
	{"count", m_count},
	{"createBulkOperation", m_createBulkOperation},
	{"drop", m_drop},
	{"find", m_find},
	{"findAndModify", m_findAndModify},
	{"findOne", m_findOne},
	{"getName", m_getName},
	{"getReadPrefs", m_getReadPrefs},
	{"insert", m_insert},
	{"insertMany", m_insertMany},
	{"insertOne", m_insertOne},
	{"remove", m_remove},
	{"removeMany", m_removeMany},
	{"removeOne", m_removeOne},
	{"rename", m_rename},
	{"replaceOne", m_replaceOne},
	{"setReadPrefs", m_setReadPrefs},
	{"update", m_update},
	{"updateMany", m_updateMany},
	{"updateOne", m_updateOne},
	{"__gc", m__gc},
	{0, 0}
};

void pushCollection(lua_State *L, mongoc_collection_t *collection, bool ref, int pidx) {
	pushHandle(L, collection, ref ? 1 : -1, pidx);
	setType(L, TYPE_COLLECTION, funcs);
}

mongoc_collection_t *checkCollection(lua_State *L, int idx) {
	return *(mongoc_collection_t **)luaL_checkudata(L, idx, TYPE_COLLECTION);
}
