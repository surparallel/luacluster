#ifndef l_int64_h
#define l_int64_h


#include "lua.h"
#include "lauxlib.h"


/* Definition of (u)int64_t { */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <stdint.h>
#elif defined(_MSC_VER)
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
typedef long long int64_t;
typedef unsigned long long uint64_t;
#endif /* } */


int luaopen_int64 (lua_State *L);


/*
** no error => 0
** errcode = type tag + 2
** e.g.:
** LUA_TNONE (-1) => 1
** LUA_TTHREAD (8) => 10
*/
LUALIB_API void luaL_i64pushstrerror (lua_State *L, int errcode);
#define luaL_u64pushstrerror luaL_i64pushstrerror  /* XXX? */

LUALIB_API int64_t luaL_toi64be (lua_State *L, int idx, int base,
                                 int *errcode);
LUALIB_API uint64_t luaL_tou64be (lua_State *L, int idx, int base,
                                  int *errcode);

LUALIB_API int64_t luaL_toi64bx (lua_State *L, int idx, int base,
                                 int *isnum);
LUALIB_API uint64_t luaL_tou64bx (lua_State *L, int idx, int base,
                                  int *isnum);

#define luaL_toi64b(L,idx,base) luaL_toi64be(L, (idx), (base), NULL)
#define luaL_tou64b(L,idx,base) luaL_tou64be(L, (idx), (base), NULL)

#define luaL_toi64x(L,idx,isnum) luaL_toi64bx(L, (idx), 0, (isnum))
#define luaL_tou64x(L,idx,isnum) luaL_tou64bx(L, (idx), 0, (isnum))

#define luaL_toi64(L,idx) luaL_toi64be(L, (idx), 0, NULL)
#define luaL_tou64(L,idx) luaL_tou64be(L, (idx), 0, NULL)

LUALIB_API void luaL_i64pushstring (lua_State *L, int64_t n);
LUALIB_API void luaL_u64pushstring (lua_State *L, uint64_t n);
LUALIB_API void luaL_i64pushhex (lua_State *L, int64_t n);
LUALIB_API void luaL_u64pushhex (lua_State *L, uint64_t n);

/*
** {=================================================================
** Test whether types are at least 64 bits wide.
** ==================================================================
*/
LUALIB_API int luaL_integeris64 (lua_State *L);
LUALIB_API int luaL_numberis64 (lua_State *L);
LUALIB_API int luaL_pointeris64 (lua_State *L);

/* }================================================================= */

/*
** {=================================================================
** This is incredibly hacky and may not even work.
** Fit a 64-bit into the bit field of a double, if this Lua
**    defined lua_Number as double.
** !! UNDEFINED BEHAVIOR / IMPLEMENTATION-DEFINED / NOT GUARANTEED !!
** ==================================================================
*/
LUALIB_API void luaL_i64pushnumber (lua_State *L, int64_t n);
LUALIB_API void luaL_u64pushnumber (lua_State *L, uint64_t n);

/* }================================================================= */

/*
** {=================================================================
** Throw error if can't fit 64-bit int into light userdatum.
** ==> Throw error on 32-bit systems.
** ==================================================================
*/
LUALIB_API void luaL_i64pushaddress (lua_State *L, int64_t n);
LUALIB_API void luaL_u64pushaddress (lua_State *L, uint64_t n);

/* }================================================================= */


unsigned long long double2u64(double x);
long long double2i64(const void* data, double x);
double i642double(long long n);
double u642double(unsigned long long n);

#endif
