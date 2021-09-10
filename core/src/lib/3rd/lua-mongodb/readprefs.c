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

static int m__gc(lua_State *L) {
	mongoc_read_prefs_destroy(checkReadPrefs(L, 1));
	unsetType(L);
	return 0;
}

static const luaL_Reg funcs[] = {
	{"__gc", m__gc},
	{0, 0}
};

static const char *const names[] = {
	"primary",
	"primaryPreferred",
	"secondary",
	"secondaryPreferred",
	"nearest",
0};

static const mongoc_read_mode_t modes[] = {
	MONGOC_READ_PRIMARY,
	MONGOC_READ_PRIMARY_PREFERRED,
	MONGOC_READ_SECONDARY,
	MONGOC_READ_SECONDARY_PREFERRED,
	MONGOC_READ_NEAREST,
};

static mongoc_read_mode_t checkMode(lua_State *L, int idx) {
	int i = luaL_checkoption(L, idx, 0, names);
	return modes[i];
}

static int64_t checkMaxStalenessSeconds(lua_State *L, int idx) {
	int64_t res = checkInt64(L, idx);
	luaL_argcheck(L, res == MONGOC_NO_MAX_STALENESS || res >= MONGOC_SMALLEST_MAX_STALENESS_SECONDS, idx, "invalid value");
	return res;
}

int newReadPrefs(lua_State *L) {
	mongoc_read_prefs_t *prefs = mongoc_read_prefs_new(checkMode(L, 1));
	if (!lua_isnoneornil(L, 2)) mongoc_read_prefs_set_tags(prefs, castBSON(L, 2));
	if (!lua_isnoneornil(L, 3)) mongoc_read_prefs_set_max_staleness_seconds(prefs, checkMaxStalenessSeconds(L, 3));
	if (!mongoc_read_prefs_is_valid(prefs)) {
		mongoc_read_prefs_destroy(prefs);
		return luaL_error(L, "invalid read preferences");
	}
	pushHandle(L, prefs, 0, 0);
	setType(L, TYPE_READPREFS, funcs);
	return 1;
}

void pushReadPrefs(lua_State *L, const mongoc_read_prefs_t *prefs) {
	pushHandle(L, mongoc_read_prefs_copy(prefs), 0, 0);
	setType(L, TYPE_READPREFS, funcs);
}

mongoc_read_prefs_t *checkReadPrefs(lua_State *L, int idx) {
	return *(mongoc_read_prefs_t **)luaL_checkudata(L, idx, TYPE_READPREFS);
}

mongoc_read_prefs_t *toReadPrefs(lua_State *L, int idx) {
	return luaL_opt(L, checkReadPrefs, idx, 0);
}
