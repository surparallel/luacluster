/*
** $Id: lua.h,v 1.218.1.7 2012/01/13 20:36:20 roberto Exp $
** Lua - An Extensible Extension Language
** Lua.org, PUC-Rio, Brazil (http://www.lua.org)
** See Copyright Notice at the end of this file
*/

#include "lua.h"

#include "lstate.h"
#include "lauxlib.h"
#include "llimits.h"
#include "luaex.h"
#include <string.h>
#include "sds.h"

/* test for pseudo index */
#define ispseudo(i)		((i) <= LUA_REGISTRYINDEX)

/*
** convert an acceptable stack index into an absolute index
*/
LUA_API int lua_absindex(lua_State* L, int idx) {
    return (idx > 0 || ispseudo(idx))
        ? idx
        : cast_int(L->top - L->ci->func) + idx;
}

/*
** ensure that stack[idx][fname] has a table and push that table
** into the stack
*/
LUALIB_API int luaL_getsubtable(lua_State* L, int idx, const char* fname) {

    lua_getfield(L, idx, fname);  /* get _LOADED[libname] */
    if (lua_istable(L, -1))  /* found */
        return 1;  /* table already there */
    else {
        lua_pop(L, 1);  /* remove previous result */
        idx = lua_absindex(L, idx);
        lua_newtable(L);
        lua_pushvalue(L, -1);  /* copy to be left at top */
        lua_setfield(L, idx, fname);  /* assign new table to field */
        return 0;  /* false, because did not find table there */
    }
}

static int load_aux(lua_State* L, int status) {
    if (status == 0)  /* OK? */
        return 1;
    else {
        lua_pushnil(L);
        lua_insert(L, -2);  /* put before error message */
        return 2;  /* return nil plus error message */
    }
}

LUALIB_API int luaB_loadfile(lua_State* L) {
    const char* fname = luaL_optstring(L, 1, NULL);
    return load_aux(L, luaL_loadfile(L, fname));
}

static const char* pushnexttemplate(lua_State* L, const char* path) {
    const char* l;
    while (*path == *LUA_PATHSEP) path++;  /* skip separators */
    if (*path == '\0') return NULL;  /* no more templates */
    l = strchr(path, *LUA_PATHSEP);  /* find next separator */
    if (l == NULL) l = path + strlen(path);
    lua_pushlstring(L, path, l - path);  /* template */
    return l;
}

static int readable(const char* filename) {
    FILE* f = fopen(filename, "r");  /* try to open file */
    if (f == NULL) return 0;  /* open failed */
    fclose(f);
    return 1;
}

static const char* findfile(lua_State* L, const char* name,
    const char* pname) {
    const char* path;
    name = luaL_gsub(L, name, ".", LUA_DIRSEP);
    lua_getfield(L, LUA_ENVIRONINDEX, pname);
    path = lua_tostring(L, -1);
    if (path == NULL)
        luaL_error(L, LUA_QL("package.%s") " must be a string", pname);
    lua_pushliteral(L, "");  /* error accumulator */
    while ((path = pushnexttemplate(L, path)) != NULL) {
        const char* filename;
        filename = luaL_gsub(L, lua_tostring(L, -1), LUA_PATH_MARK, name);
        lua_remove(L, -2);  /* remove path template */
        if (readable(filename))  /* does file exist and is readable? */
            return filename;  /* return that file name */
        lua_pushfstring(L, "\n\tno file " LUA_QS, filename);
        lua_remove(L, -2);  /* remove file name */
        lua_concat(L, 2);  /* add entry to possible error message */
    }
    return NULL;  /* not found */
}

static void loaderror(lua_State* L, const char* filename) {
    luaL_error(L, "error loading module " LUA_QS " from file " LUA_QS ":\n\t%s",
        lua_tostring(L, 1), filename, lua_tostring(L, -1));
}

LUALIB_API int loader_Lua(lua_State* L) {
    const char* filename;
    const char* name = luaL_checkstring(L, 1);
    filename = findfile(L, name, "path");
    if (filename == NULL) return 1;  /* library not found in this path */
    if (luaL_loadfile(L, filename) != 0)
        loaderror(L, filename);
    return 1;  /* library loaded successfully */
}

/*
** Stripped-down 'require': After checking "loaded" table, calls 'openf'
** to open a module, registers the result in 'package.loaded' table and,
** if 'glb' is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/
LUALIB_API void luaL_requiref(lua_State* L, const char* modname,
    lua_CFunction openf, int glb) {
    luaL_getsubtable(L, LUA_REGISTRYINDEX, "_LOADED");
    lua_getfield(L, -1, modname);  /* LOADED[modname] */
    if (!lua_toboolean(L, -1)) {  /* package not already loaded? */
        lua_pop(L, 1);  /* remove field */
        lua_pushcfunction(L, openf);
        lua_pushstring(L, modname);  /* argument to open function */
        lua_call(L, 1, 1);  /* call 'openf' to open module */
        lua_pushvalue(L, -1);  /* make copy of module (call result) */
        lua_setfield(L, -3, modname);  /* LOADED[modname] = module */
    }
    lua_remove(L, -2);  /* remove LOADED table */
    if (glb) {
        lua_pushvalue(L, -1);  /* copy of module */
        lua_setglobal(L, modname);  /* _G[modname] = module */
    }
}


/*
LuaAddPath(m_ls->GetCState(), "path", ".\\lual\\?.lua");
LuaAddPath(m_ls->GetCState(), "cpath", ".\\lual\\?.dll");
*/
LUALIB_API void LuaAddPath(lua_State* ls, char* name, char* value)
{
    sds v;
    lua_getglobal(ls, "package");
    lua_getfield(ls, -1, name);
    v = sdsnew(lua_tostring(ls, -1));
    v = sdscat(v, ";");
    v = sdscat(v, value);
    lua_pushstring(ls, v);
    lua_setfield(ls, -3, name);
    lua_pop(ls, 2);

    sdsfree(v);
}