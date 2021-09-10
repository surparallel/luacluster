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

static int m_execute(lua_State *L) {
	mongoc_bulk_operation_t *bulk = checkBulkOperation(L, 1);
	bson_t reply;
	bson_error_t error;
	return commandReply(L, mongoc_bulk_operation_execute(bulk, &reply, &error), &reply, &error);
}

static int m_insert(lua_State *L) {
	mongoc_bulk_operation_t *bulk = checkBulkOperation(L, 1);
	bson_t *document = castBSON(L, 2);
	bson_t *options = toBSON(L, 3);
	bson_error_t error;
	checkStatus(L, mongoc_bulk_operation_insert_with_opts(bulk, document, options, &error), &error);
	return 0;
}

static int m_removeMany(lua_State *L) {
	mongoc_bulk_operation_t *bulk = checkBulkOperation(L, 1);
	bson_t *query = castBSON(L, 2);
	bson_t *options = toBSON(L, 3);
	bson_error_t error;
	checkStatus(L, mongoc_bulk_operation_remove_many_with_opts(bulk, query, options, &error), &error);
	return 0;
}

static int m_removeOne(lua_State *L) {
	mongoc_bulk_operation_t *bulk = checkBulkOperation(L, 1);
	bson_t *query = castBSON(L, 2);
	bson_t *options = toBSON(L, 3);
	bson_error_t error;
	checkStatus(L, mongoc_bulk_operation_remove_one_with_opts(bulk, query, options, &error), &error);
	return 0;
}

static int m_replaceOne(lua_State *L) {
	mongoc_bulk_operation_t *bulk = checkBulkOperation(L, 1);
	bson_t *query = castBSON(L, 2);
	bson_t *document = castBSON(L, 3);
	bson_t *options = toBSON(L, 4);
	bson_error_t error;
	checkStatus(L, mongoc_bulk_operation_replace_one_with_opts(bulk, query, document, options, &error), &error);
	return 0;
}

static int m_updateMany(lua_State *L) {
	mongoc_bulk_operation_t *bulk = checkBulkOperation(L, 1);
	bson_t *query = castBSON(L, 2);
	bson_t *document = castBSON(L, 3);
	bson_t *options = toBSON(L, 4);
	bson_error_t error;
	checkStatus(L, mongoc_bulk_operation_update_many_with_opts(bulk, query, document, options, &error), &error);
	return 0;
}

static int m_updateOne(lua_State *L) {
	mongoc_bulk_operation_t *bulk = checkBulkOperation(L, 1);
	bson_t *query = castBSON(L, 2);
	bson_t *document = castBSON(L, 3);
	bson_t *options = toBSON(L, 4);
	bson_error_t error;
	checkStatus(L, mongoc_bulk_operation_update_one_with_opts(bulk, query, document, options, &error), &error);
	return 0;
}

static int m__gc(lua_State *L) {
	mongoc_bulk_operation_destroy(checkBulkOperation(L, 1));
	unsetType(L);
	return 0;
}

static const luaL_Reg funcs[] = {
	{"execute", m_execute},
	{"insert", m_insert},
	{"removeMany", m_removeMany},
	{"removeOne", m_removeOne},
	{"replaceOne", m_replaceOne},
	{"updateMany", m_updateMany},
	{"updateOne", m_updateOne},
	{"__gc", m__gc},
	{0, 0}
};

void pushBulkOperation(lua_State *L, mongoc_bulk_operation_t *bulk, int pidx) {
	pushHandle(L, bulk, -1, pidx);
	setType(L, TYPE_BULKOPERATION, funcs);
}

mongoc_bulk_operation_t *checkBulkOperation(lua_State *L, int idx) {
	return *(mongoc_bulk_operation_t **)luaL_checkudata(L, idx, TYPE_BULKOPERATION);
}
