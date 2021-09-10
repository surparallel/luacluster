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

#define MAXSTACK 1000 /* Arbitrary stack size limit to check for recursion */

static int m_append(lua_State *L) {
	bson_t *bson = checkBSON(L, 1);
	size_t klen;
	const char *key = luaL_checklstring(L, 2, &klen);
	bson_value_t value;
	toBSONValue(L, 3, &value);
	bson_append_value(bson, key, klen, &value);
	bson_value_destroy(&value);
	return 0;
}

static int m_concat(lua_State *L) {
	bson_t *bson = checkBSON(L, 1);
	bson_t *value = castBSON(L, 2);
	luaL_argcheck(L, value != bson, 2, "invalid value");
	bson_concat(bson, value);
	return 0;
}

static int m_data(lua_State *L) {
	bson_t *bson = checkBSON(L, 1);
	lua_pushlstring(L, (const char *)bson_get_data(bson), bson->len);
	return 1;
}

static int m_find(lua_State *L) {
	bson_t *bson = checkBSON(L, 1);
	const char *key = luaL_checkstring(L, 2);
	pushBSONField(L, bson, key, true);
	return 1;
}

static int m_value(lua_State *L) {
	pushBSON(L, checkBSON(L, 1), 2);
	return 1;
}

static int m__tostring(lua_State *L) {
	size_t len;
	char *str = bson_as_json(checkBSON(L, 1), &len);
	lua_pushlstring(L, str, len);
	bson_free(str);
	return 1;
}

static int m__len(lua_State *L) {
	pushInt32(L, checkBSON(L, 1)->len);
	return 1;
}

static int m__eq(lua_State *L) {
	lua_pushboolean(L, bson_equal(checkBSON(L, 1), checkBSON(L, 2)));
	return 1;
}

static int m__gc(lua_State *L) {
	bson_destroy(checkBSON(L, 1));
	unsetType(L);
	return 0;
}

static const luaL_Reg funcs[] = {
	{"append", m_append},
	{"concat", m_concat},
	{"data", m_data},
	{"find", m_find},
	{"value", m_value},
	{"__tostring", m__tostring},
	{"__len", m__len},
	{"__eq", m__eq},
	{"__gc", m__gc},
	{0, 0}
};

static bool error(lua_State *L, int *nerr, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
#if LUA_VERSION_NUM >= 504
	luaL_checkstack(L, 1, 0);
#endif
	lua_pushvfstring(L, fmt, ap);
	va_end(ap);
	lua_insert(L, -(++(*nerr)));
	return false;
}

static bool isInteger(lua_State *L, int idx, lua_Integer *val) {
	lua_Integer i;
#if LUA_VERSION_NUM < 503
	lua_Number n;
	if (!lua_isnumber(L, idx)) return false;
	n = lua_tonumber(L, idx);
	i = (lua_Integer)n;
	if (i != n) return false;
#else
	int res;
	i = lua_tointegerx(L, idx, &res);
	if (!res) return false;
#endif
	*val = i;
	return true;
}

#define isInt32(i) ((i) >= INT32_MIN && (i) <= INT32_MAX)

static bool isArray(const bson_t *bson) {
	bson_iter_t iter;
	return bson_iter_init(&iter, bson) && bson_iter_next(&iter) && !strcmp(bson_iter_key(&iter), "0");
}

static bool isBSON(const char *str, size_t len) {
	uint32_t pfx;
	if (len < 5 || len > INT_MAX || str[len - 1]) return false;
	memcpy (&pfx, str, 4);
	return len == (size_t)BSON_UINT32_FROM_LE(pfx);
}

static bool initBSON(const char *str, size_t len, bson_t *bson, bson_error_t *error) {
	if (!isBSON(str, len)) return bson_init_from_json(bson, str, len, error);
	bson_init(bson);
	memcpy(bson_reserve_buffer(bson, len), str, len);
	if (bson_validate_with_error(bson, BSON_VALIDATE_NONE, error)) return true;
	bson_destroy(bson);
	return false;
}

static bool appendBSONType(lua_State *L, bson_type_t type, int idx, int *nerr, bson_t *bson, const char *key, size_t klen) {
	int top = lua_gettop(L);
	unpackParams(L, idx);
	switch (type) {
		case BSON_TYPE_INT32:
			bson_append_int32(bson, key, klen, toInt32(L, top + 1));
			break;
		case BSON_TYPE_INT64:
			bson_append_int64(bson, key, klen, toInt64(L, top + 1));
			break;
		case BSON_TYPE_DOUBLE:
			bson_append_double(bson, key, klen, lua_tonumber(L, top + 1));
			break;
		case BSON_TYPE_DECIMAL128: {
			const char *str = lua_tostring(L, top + 1);
			bson_decimal128_t dec;
			if (!str || !bson_decimal128_from_string(str, &dec)) goto error;
			bson_append_decimal128(bson, key, klen, &dec);
			break;
		}
		case BSON_TYPE_BINARY: {
			size_t len;
			const char *str = lua_tolstring(L, top + 1, &len);
			if (!str) goto error;
			bson_append_binary(bson, key, klen, lua_tointeger(L, top + 2), (const uint8_t *)str, len);
			break;
		}
		case BSON_TYPE_DATE_TIME:
			bson_append_date_time(bson, key, klen, toInt64(L, top + 1));
			break;
		case BSON_TYPE_REGEX:
			bson_append_regex(bson, key, klen, lua_tostring(L, top + 1), lua_tostring(L, top + 2));
			break;
		case BSON_TYPE_TIMESTAMP:
			bson_append_timestamp(bson, key, klen, toInt32(L, top + 1), toInt32(L, top + 2));
			break;
		case BSON_TYPE_CODE: {
			const char *code = lua_tostring(L, top + 1);
			bson_t *scope = testBSON(L, top + 2);
			if (!code) goto error;
			if (scope) bson_append_code_with_scope(bson, key, klen, code, scope);
			else bson_append_code(bson, key, klen, code);
			break;
		}
		case BSON_TYPE_MAXKEY:
			bson_append_maxkey(bson, key, klen);
			break;
		case BSON_TYPE_MINKEY:
			bson_append_minkey(bson, key, klen);
			break;
		case BSON_TYPE_NULL:
			bson_append_null(bson, key, klen);
			break;
		default:
		error:
			return error(L, nerr, "invalid parameters for BSON type %d", type);
	}
	lua_settop(L, top);
	return true;
}

static void toBSONType(lua_State *L, bson_type_t type, int idx, bson_value_t *val) {
	int top = lua_gettop(L);
	unpackParams(L, idx);
	switch (val->value_type = type) {
		case BSON_TYPE_INT32:
			val->value.v_int32 = toInt32(L, top + 1);
			break;
		case BSON_TYPE_INT64:
			val->value.v_int64 = toInt64(L, top + 1);
			break;
		case BSON_TYPE_DOUBLE:
			val->value.v_double = lua_tonumber(L, top + 1);
			break;
		case BSON_TYPE_DECIMAL128: {
			const char *str = lua_tostring(L, top + 1);
			if (!str || !bson_decimal128_from_string(str, &val->value.v_decimal128)) goto error;
			break;
		}
		case BSON_TYPE_BINARY: {
			size_t len;
			const char *str = lua_tolstring(L, top + 1, &len);
			if (!str) goto error;
			memcpy(
				val->value.v_binary.data = bson_malloc(len), str,
				val->value.v_binary.data_len = len);
			val->value.v_binary.subtype = lua_tointeger(L, top + 2);
			break;
		}
		case BSON_TYPE_DATE_TIME:
			val->value.v_datetime = toInt64(L, top + 1);
			break;
		case BSON_TYPE_REGEX:
			val->value.v_regex.regex = bson_strdup(lua_tostring(L, top + 1));
			val->value.v_regex.options = bson_strdup(lua_tostring(L, top + 2));
			break;
		case BSON_TYPE_TIMESTAMP:
			val->value.v_timestamp.timestamp = toInt32(L, top + 1);
			val->value.v_timestamp.increment = toInt32(L, top + 2);
			break;
		case BSON_TYPE_CODE: {
			size_t len;
			const char *code = lua_tolstring(L, top + 1, &len);
			bson_t *scope = testBSON(L, top + 2);
			if (!code) goto error;
			val->value.v_code.code_len = len;
			val->value.v_code.code = bson_strndup(code, len);
			if (!scope) break;
			val->value_type = BSON_TYPE_CODEWSCOPE;
			memcpy(
				val->value.v_codewscope.scope_data = bson_malloc(scope->len), bson_get_data(scope),
				val->value.v_codewscope.scope_len = scope->len);
			break;
		}
		case BSON_TYPE_MAXKEY:
		case BSON_TYPE_MINKEY:
		case BSON_TYPE_NULL:
			break;
		default:
		error:
			argError(L, idx, "invalid parameters for BSON type %d", type);
	}
	lua_settop(L, top);
}

static lua_Integer getArrayLength(lua_State *L, int idx) {
	lua_Integer len = -1;
	lua_getfield(L, idx, "__array");
	if (lua_toboolean(L, -1)) {
		if (!isInteger(L, -1, &len)) len = lua_rawlen(L, idx);
		if (len < 0) len = 0;
	}
	lua_pop(L, 1);
	return len;
}

static bool appendTable(lua_State *L, int idx, int ridx, int *nerr, bson_t *bson, lua_Integer len);

static bool appendValue(lua_State *L, int idx, int ridx, int *nerr, bson_t *bson, const char *key, size_t klen) {
	if (luaL_getmetafield(L, idx, "__toBSON")) { /* Transform value */
		lua_pushvalue(L, idx);
		if (lua_pcall(L, 1, 1, 0)) return error(L, nerr, "%s", lua_isstring(L, -1) ? lua_tostring(L, -1) : "(error object is not a string)");
		lua_replace(L, idx);
	}
	switch (lua_type(L, idx)) {
		case LUA_TNIL:
			bson_append_null(bson, key, klen);
			break;
		case LUA_TBOOLEAN:
			bson_append_bool(bson, key, klen, lua_toboolean(L, idx));
			break;
		case LUA_TNUMBER: {
			lua_Integer i;
			if (isInteger(L, idx, &i)) {
				if (isInt32(i)) bson_append_int32(bson, key, klen, i);
				else bson_append_int64(bson, key, klen, i);
				break;
			}
			bson_append_double(bson, key, klen, lua_tonumber(L, idx));
			break;
		}
		case LUA_TSTRING: {
			size_t len;
			const char *str = lua_tolstring(L, idx, &len);
			bson_append_utf8(bson, key, klen, str, len);
			break;
		}
		case LUA_TTABLE: {
			lua_Integer len;
			bson_t doc;
			if (luaL_getmetafield(L, idx, "__type")) {
				if (!appendBSONType(L, lua_tointeger(L, -1), idx, nerr, bson, key, klen)) return false;
				lua_pop(L, 1);
				break;
			}
			len = getArrayLength(L, idx);
			if (len != -1) {
				bson_append_array_begin(bson, key, klen, &doc);
				if (!appendTable(L, idx, ridx, nerr, &doc, len)) return false;
				bson_append_array_end(bson, &doc);
			} else {
				bson_append_document_begin(bson, key, klen, &doc);
				if (!appendTable(L, idx, ridx, nerr, &doc, len)) return false;
				bson_append_document_end(bson, &doc);
			}
			break;
		}
		case LUA_TUSERDATA: {
			bson_t *doc;
			bson_oid_t *oid;
			if ((doc = testBSON(L, idx))) {
				if (isArray(doc)) bson_append_array(bson, key, klen, doc);
				else bson_append_document(bson, key, klen, doc);
				break;
			}
			if ((oid = testObjectID(L, idx))) {
				bson_append_oid(bson, key, klen, oid);
				break;
			}
		} /* Fall through */
		default:
			return error(L, nerr, "%s unexpected", typeName(L, idx));
	}
	return true;
}

static bool appendTable(lua_State *L, int idx, int ridx, int *nerr, bson_t *bson, lua_Integer len) {
	const char *key;
	size_t klen;
	int top = lua_gettop(L);
	if (top >= MAXSTACK) return error(L, nerr, "recursion detected");
	if (lua_getmetatable(L, idx)) return error(L, nerr, "table with metatable unexpected");
	lua_pushvalue(L, idx);
	lua_rawget(L, ridx);
	if (lua_toboolean(L, -1)) return error(L, nerr, "circular reference detected");
	lua_pushvalue(L, idx);
	lua_pushboolean(L, 1);
	lua_rawset(L, ridx);
	lua_settop(L, top);
	luaL_checkstack(L, LUA_MINSTACK, "too many nested values");
	if (len != -1) { /* As array */
		char buf[64];
		lua_Integer i;
		for (i = 0; i < len; ++i) {
			lua_rawgeti(L, idx, i + 1);
			klen = bson_uint32_to_string(i, &key, buf, sizeof buf);
			if (!appendValue(L, top + 1, ridx, nerr, bson, key, klen)) return error(L, nerr, "[%d] => ", i + 1);
			lua_pop(L, 1);
		}
	} else { /* As document */
		for (lua_pushnil(L); lua_next(L, idx); lua_pop(L, 1)) {
			if (lua_type(L, top + 1) != LUA_TSTRING) return error(L, nerr, "string index expected, got %s", typeName(L, top + 1));
			key = lua_tolstring(L, top + 1, &klen);
			if (!appendValue(L, top + 2, ridx, nerr, bson, key, klen)) return error(L, nerr, "[\"%s\"] => ", key);
		}
	}
	lua_pushvalue(L, idx);
	lua_pushnil(L);
	lua_rawset(L, ridx);
	return true;
}

static void unpackTable(lua_State *L, bson_iter_t *iter, int hidx, bool array);

static void unpackValue(lua_State *L, bson_iter_t *iter, int hidx) {
	switch (bson_iter_type(iter)) {
		case BSON_TYPE_BOOL:
			lua_pushboolean(L, bson_iter_bool(iter));
			break;
		case BSON_TYPE_INT32:
			pushInt32(L, bson_iter_int32(iter));
			break;
		case BSON_TYPE_INT64:
			pushInt64(L, bson_iter_int64(iter));
			break;
		case BSON_TYPE_DOUBLE:
			lua_pushnumber(L, bson_iter_double(iter));
			break;
		case BSON_TYPE_UTF8: {
			uint32_t len;
			const char *str = bson_iter_utf8(iter, &len);
			lua_pushlstring(L, str, len);
			break;
		}
		case BSON_TYPE_DOCUMENT:
		case BSON_TYPE_ARRAY: {
			bson_iter_t tmp;
			check(L, bson_iter_recurse(iter, &tmp));
			unpackTable(L, &tmp, hidx, BSON_ITER_HOLDS_ARRAY(iter));
			break;
		}
		case BSON_TYPE_OID:
			pushObjectID(L, bson_iter_oid(iter));
			break;
		case BSON_TYPE_DECIMAL128: {
			bson_decimal128_t dec;
			char buf[BSON_DECIMAL128_STRING];
			check(L, bson_iter_decimal128(iter, &dec));
			bson_decimal128_to_string(&dec, buf);
			lua_rawgetp(L, LUA_REGISTRYINDEX, &NEW_DECIMAL128);
			lua_pushstring(L, buf);
			lua_call(L, 1, 1);
			break;
		}
		case BSON_TYPE_BINARY: {
			bson_subtype_t subtype;
			uint32_t len;
			const uint8_t *buf;
			bson_iter_binary(iter, &subtype, &len, &buf);
			lua_rawgetp(L, LUA_REGISTRYINDEX, &NEW_BINARY);
			lua_pushlstring(L, (const char *)buf, len);
			lua_pushinteger(L, subtype);
			lua_call(L, 2, 1);
			break;
		}
		case BSON_TYPE_DATE_TIME:
			lua_rawgetp(L, LUA_REGISTRYINDEX, &NEW_DATETIME);
			pushInt64(L, bson_iter_date_time(iter));
			lua_call(L, 1, 1);
			break;
		case BSON_TYPE_CODE: {
			uint32_t len;
			const char *code = bson_iter_code(iter, &len);
			lua_rawgetp(L, LUA_REGISTRYINDEX, &NEW_JAVASCRIPT);
			lua_pushlstring(L, code, len);
			lua_call(L, 1, 1);
			break;
		}
		case BSON_TYPE_CODEWSCOPE: {
			bson_t scope;
			uint32_t clen, slen;
			const uint8_t *buf;
			const char *code = bson_iter_codewscope(iter, &clen, &slen, &buf);
			check(L, bson_init_static(&scope, buf, slen));
			lua_rawgetp(L, LUA_REGISTRYINDEX, &NEW_JAVASCRIPT);
			lua_pushlstring(L, code, clen);
			pushBSON(L, &scope, 0);
			lua_call(L, 2, 1);
			break;
		}
		case BSON_TYPE_REGEX: {
			const char *options;
			lua_rawgetp(L, LUA_REGISTRYINDEX, &NEW_REGEX);
			lua_pushstring(L, bson_iter_regex(iter, &options));
			lua_pushstring(L, options);
			lua_call(L, 2, 1);
			break;
		}
		case BSON_TYPE_TIMESTAMP: {
			uint32_t t, i;
			bson_iter_timestamp(iter, &t, &i);
			lua_rawgetp(L, LUA_REGISTRYINDEX, &NEW_TIMESTAMP);
			pushInt32(L, t);
			pushInt32(L, i);
			lua_call(L, 2, 1);
			break;
		}
		case BSON_TYPE_MAXKEY:
			lua_rawgetp(L, LUA_REGISTRYINDEX, &GLOBAL_MAXKEY);
			break;
		case BSON_TYPE_MINKEY:
			lua_rawgetp(L, LUA_REGISTRYINDEX, &GLOBAL_MINKEY);
			break;
		case BSON_TYPE_NULL:
			lua_rawgetp(L, LUA_REGISTRYINDEX, &GLOBAL_NULL);
			break;
		default:
			lua_pushnil(L);
			break;
	}
}

static void unpackTable(lua_State *L, bson_iter_t *iter, int hidx, bool array) {
	lua_Integer len = 0;
	lua_newtable(L);
	luaL_checkstack(L, LUA_MINSTACK, "too many nested values");
	while (bson_iter_next(iter)) {
		if (array) lua_pushinteger(L, ++len);
		else lua_pushlstring(L, bson_iter_key(iter), bson_iter_key_len(iter));
		unpackValue(L, iter, hidx);
		lua_rawset(L, -3);
	}
	if (array) {
		lua_pushinteger(L, len);
		lua_setfield(L, -2, "__array");
	}
	if (lua_isnil(L, hidx)) return; /* No handler */
	lua_pushvalue(L, hidx);
	lua_insert(L, -2);
	lua_call(L, 1, 1); /* Transform value */
}

int newBSON(lua_State *L) {
	castBSON(L, 1);
	return 1;
}

void pushBSON(lua_State *L, const bson_t *bson, int hidx) {
	if (!bson) { /* Return nil */
		lua_pushnil(L);
	} else if (!hidx) { /* Copy object */
		bson_copy_to(bson, lua_newuserdata(L, sizeof *bson));
		setType(L, TYPE_BSON, funcs);
	} else { /* Unpack value */
		bson_iter_t iter;
		check(L, bson_iter_init(&iter, bson));
		lua_pushvalue(L, hidx); /* Ensure handler index is valid */
		unpackTable(L, &iter, lua_gettop(L), isArray(bson));
		lua_replace(L, -2);
	}
}

void pushBSONWithSteal(lua_State *L, bson_t *bson) {
	bson_steal(lua_newuserdata(L, sizeof *bson), bson);
	setType(L, TYPE_BSON, funcs);
}

void pushBSONValue(lua_State *L, const bson_value_t *val) {
	bson_t bson;
	switch (val->value_type) {
		case BSON_TYPE_BOOL:
			lua_pushboolean(L, val->value.v_bool);
			break;
		case BSON_TYPE_INT32:
			pushInt32(L, val->value.v_int32);
			break;
		case BSON_TYPE_INT64:
			pushInt64(L, val->value.v_int64);
			break;
		case BSON_TYPE_DOUBLE:
			lua_pushnumber(L, val->value.v_double);
			break;
		case BSON_TYPE_UTF8:
			lua_pushlstring(L, val->value.v_utf8.str, val->value.v_utf8.len);
			break;
		case BSON_TYPE_DOCUMENT:
		case BSON_TYPE_ARRAY:
			check(L, bson_init_static(&bson, val->value.v_doc.data, val->value.v_doc.data_len));
			pushBSON(L, &bson, 0);
			break;
		case BSON_TYPE_OID:
			pushObjectID(L, &val->value.v_oid);
			break;
		case BSON_TYPE_DECIMAL128: {
			char buf[BSON_DECIMAL128_STRING];
			bson_decimal128_to_string(&val->value.v_decimal128, buf);
			lua_rawgetp(L, LUA_REGISTRYINDEX, &NEW_DECIMAL128);
			lua_pushstring(L, buf);
			lua_call(L, 1, 1);
			break;
		}
		case BSON_TYPE_BINARY:
			lua_rawgetp(L, LUA_REGISTRYINDEX, &NEW_BINARY);
			lua_pushlstring(L, (const char *)val->value.v_binary.data, val->value.v_binary.data_len);
			lua_pushinteger(L, val->value.v_binary.subtype);
			lua_call(L, 2, 1);
			break;
		case BSON_TYPE_DATE_TIME:
			lua_rawgetp(L, LUA_REGISTRYINDEX, &NEW_DATETIME);
			pushInt64(L, val->value.v_datetime);
			lua_call(L, 1, 1);
			break;
		case BSON_TYPE_CODE:
			lua_rawgetp(L, LUA_REGISTRYINDEX, &NEW_JAVASCRIPT);
			lua_pushlstring(L, val->value.v_code.code, val->value.v_code.code_len);
			lua_call(L, 1, 1);
			break;
		case BSON_TYPE_CODEWSCOPE:
			check(L, bson_init_static(&bson, val->value.v_codewscope.scope_data, val->value.v_codewscope.scope_len));
			lua_rawgetp(L, LUA_REGISTRYINDEX, &NEW_JAVASCRIPT);
			lua_pushlstring(L, val->value.v_codewscope.code, val->value.v_codewscope.code_len);
			pushBSON(L, &bson, 0);
			lua_call(L, 2, 1);
			break;
		case BSON_TYPE_REGEX:
			lua_rawgetp(L, LUA_REGISTRYINDEX, &NEW_REGEX);
			lua_pushstring(L, val->value.v_regex.regex);
			lua_pushstring(L, val->value.v_regex.options);
			lua_call(L, 2, 1);
			break;
		case BSON_TYPE_TIMESTAMP:
			lua_rawgetp(L, LUA_REGISTRYINDEX, &NEW_TIMESTAMP);
			pushInt32(L, val->value.v_timestamp.timestamp);
			pushInt32(L, val->value.v_timestamp.increment);
			lua_call(L, 2, 1);
			break;
		case BSON_TYPE_MAXKEY:
			lua_rawgetp(L, LUA_REGISTRYINDEX, &GLOBAL_MAXKEY);
			break;
		case BSON_TYPE_MINKEY:
			lua_rawgetp(L, LUA_REGISTRYINDEX, &GLOBAL_MINKEY);
			break;
		case BSON_TYPE_NULL:
			lua_rawgetp(L, LUA_REGISTRYINDEX, &GLOBAL_NULL);
			break;
		default:
			lua_pushnil(L);
			break;
	}
}

void pushBSONField(lua_State *L, const bson_t *bson, const char *key, bool any) {
	bson_iter_t iter, tmp;
	check(L, bson_iter_init(&iter, bson));
	if (bson_iter_find_descendant(&iter, key, &tmp) && /* If subdocument (or at least something) is found */
		(any || BSON_ITER_HOLDS_DOCUMENT(&tmp) || BSON_ITER_HOLDS_ARRAY(&tmp))) {
		pushBSONValue(L, bson_iter_value(&tmp)); /* Return as value */
	} else { /* Return nil */
		lua_pushnil(L);
	}
}

bson_t *checkBSON(lua_State *L, int idx) {
	return luaL_checkudata(L, idx, TYPE_BSON);
}

bson_t *testBSON(lua_State *L, int idx) {
	return luaL_testudata(L, idx, TYPE_BSON);
}

bson_t *castBSON(lua_State *L, int idx) {
	bson_t *bson;
	size_t len;
	const char *str = lua_tolstring(L, idx, &len);
	if (str) { /* From string */
		bson_error_t error;
		checkStatus(L, initBSON(str, len, bson = lua_newuserdata(L, sizeof *bson), &error), &error);
	} else { /* From value */
		int nerr = 0;
		if (luaL_callmeta(L, idx, "__toBSON")) lua_replace(L, idx); /* Transform value */
		if ((bson = testBSON(L, idx))) return bson; /* Nothing to do */
		if (!lua_istable(L, idx)) typeError(L, idx, "string, table or " TYPE_BSON);
		bson_init(bson = lua_newuserdata(L, sizeof *bson));
		lua_newtable(L);
		if (!appendTable(L, idx, lua_gettop(L), &nerr, bson, getArrayLength(L, idx))) {
			bson_destroy(bson);
			lua_concat(L, nerr);
			luaL_argerror(L, idx, lua_tostring(L, -1));
		}
		lua_pop(L, 1);
	}
	setType(L, TYPE_BSON, funcs);
	lua_replace(L, idx);
	return bson;
}

bson_t *toBSON(lua_State *L, int idx) {
	return luaL_opt(L, castBSON, idx, 0);
}

void toBSONValue(lua_State *L, int idx, bson_value_t *val) {
	luaL_checkany(L, idx);
	if (luaL_callmeta(L, idx, "__toBSON")) lua_replace(L, idx); /* Transform value */
	switch (lua_type(L, idx)) {
		case LUA_TNIL:
			val->value_type = BSON_TYPE_NULL;
			break;
		case LUA_TBOOLEAN:
			val->value_type = BSON_TYPE_BOOL;
			val->value.v_bool = lua_toboolean(L, idx);
			break;
		case LUA_TNUMBER: {
			lua_Integer i;
			if (isInteger(L, idx, &i)) {
				if (isInt32(i)) {
					val->value_type = BSON_TYPE_INT32;
					val->value.v_int32 = i;
				} else {
					val->value_type = BSON_TYPE_INT64;
					val->value.v_int64 = i;
				}
			} else {
				val->value_type = BSON_TYPE_DOUBLE;
				val->value.v_double = lua_tonumber(L, idx);
			}
			break;
		}
		case LUA_TSTRING: {
			size_t len;
			const char *str = lua_tolstring(L, idx, &len);
			val->value_type = BSON_TYPE_UTF8;
			val->value.v_utf8.len = len;
			val->value.v_utf8.str = bson_strndup(str, len);
			break;
		}
		case LUA_TTABLE: {
			lua_Integer len;
			bson_t bson;
			int nerr = 0;
			if (luaL_getmetafield(L, idx, "__type")) {
				toBSONType(L, lua_tointeger(L, -1), idx, val);
				lua_pop(L, 1);
				break;
			}
			len = getArrayLength(L, idx);
			bson_init(&bson);
			lua_newtable(L);
			if (!appendTable(L, idx, lua_gettop(L), &nerr, &bson, len)) {
				bson_destroy(&bson);
				lua_concat(L, nerr);
				luaL_argerror(L, idx, lua_tostring(L, -1));
			}
			lua_pop(L, 1);
			val->value_type = len != -1 ? BSON_TYPE_ARRAY : BSON_TYPE_DOCUMENT;
			val->value.v_doc.data = bson_destroy_with_steal(&bson, true, &val->value.v_doc.data_len);
			break;
		}
		case LUA_TUSERDATA: {
			bson_t *bson;
			bson_oid_t *oid;
			if ((bson = testBSON(L, idx))) {
				val->value_type = isArray(bson) ? BSON_TYPE_ARRAY : BSON_TYPE_DOCUMENT;
				memcpy(
					val->value.v_doc.data = bson_malloc(bson->len), bson_get_data(bson),
					val->value.v_doc.data_len = bson->len);
				break;
			}
			if ((oid = testObjectID(L, idx))) {
				val->value_type = BSON_TYPE_OID;
				bson_oid_copy(oid, &val->value.v_oid);
				break;
			}
		} /* Fall through */
		default:
			argError(L, idx, "%s unexpected", typeName(L, idx));
	}
}
