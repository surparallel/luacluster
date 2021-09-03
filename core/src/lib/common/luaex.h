/*
** $Id: lua.h,v 1.218.1.7 2012/01/13 20:36:20 roberto Exp $
** Lua - An Extensible Extension Language
** Lua.org, PUC-Rio, Brazil (http://www.lua.org)
** See Copyright Notice at the end of this file
*/

#include "lua.h"

LUALIB_API int (luaL_getsubtable)(lua_State* L, int idx, const char* fname);

LUALIB_API void (luaL_requiref)(lua_State* L, const char* modname,
    lua_CFunction openf, int glb);

LUALIB_API int luaB_loadfile(lua_State* L);

LUALIB_API int loader_Lua(lua_State* L);

LUALIB_API void LuaAddPath(lua_State* ls, char* name, char* value);