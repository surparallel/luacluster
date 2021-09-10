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
#include "lua-mongodb.h"

static int f_type(lua_State *L) {
	luaL_checkany(L, 1);
	lua_pushstring(L, typeName(L, 1));
	return 1;
}

static const luaL_Reg funcs[] = {
	{"type", f_type},
	{"Binary", newBinary},
	{"BSON", newBSON},
	{"Client", newClient},
	{"DateTime", newDateTime},
	{"Decimal128", newDecimal128},
	{"Double", newDouble},
	{"Int32", newInt32},
	{"Int64", newInt64},
	{"Javascript", newJavascript},
	{"ObjectID", newObjectID},
	{"ReadPrefs", newReadPrefs},
	{"Regex", newRegex},
	{"Timestamp", newTimestamp},
	{0, 0}
};

char NEW_BINARY, NEW_DATETIME, NEW_DECIMAL128, NEW_JAVASCRIPT, NEW_REGEX, NEW_TIMESTAMP;
char GLOBAL_MAXKEY, GLOBAL_MINKEY, GLOBAL_NULL;

int luaopen_mongo(lua_State *L) {
#if LUA_VERSION_NUM < 502
	luaL_register(L, "mongo", funcs);
#else
	luaL_newlib(L, funcs);
#endif
	lua_pushliteral(L, MODNAME);
	lua_setfield(L, -2, "_NAME");
	lua_pushliteral(L, VERSION);
	lua_setfield(L, -2, "_VERSION");

	/* Cache BSON type constructors for quick access */
	lua_getfield(L, -1, "Binary");
	lua_rawsetp(L, LUA_REGISTRYINDEX, &NEW_BINARY);
	lua_getfield(L, -1, "DateTime");
	lua_rawsetp(L, LUA_REGISTRYINDEX, &NEW_DATETIME);
	lua_getfield(L, -1, "Decimal128");
	lua_rawsetp(L, LUA_REGISTRYINDEX, &NEW_DECIMAL128);
	lua_getfield(L, -1, "Javascript");
	lua_rawsetp(L, LUA_REGISTRYINDEX, &NEW_JAVASCRIPT);
	lua_getfield(L, -1, "Regex");
	lua_rawsetp(L, LUA_REGISTRYINDEX, &NEW_REGEX);
	lua_getfield(L, -1, "Timestamp");
	lua_rawsetp(L, LUA_REGISTRYINDEX, &NEW_TIMESTAMP);

	/* Create BSON type singletons */
	pushMaxKey(L);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "MaxKey");
	lua_rawsetp(L, LUA_REGISTRYINDEX, &GLOBAL_MAXKEY);
	pushMinKey(L);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "MinKey");
	lua_rawsetp(L, LUA_REGISTRYINDEX, &GLOBAL_MINKEY);
	pushNull(L);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "Null");
	lua_rawsetp(L, LUA_REGISTRYINDEX, &GLOBAL_NULL);

	mongoc_init();
	return 1;
}
