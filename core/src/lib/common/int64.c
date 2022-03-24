#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "int64.h"


/* 64-bit int printf format specifiers { */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <inttypes.h>
typedef long long strtoll_t;
typedef unsigned long long strtoull_t;
#elif defined(_MSC_VER)
#define __PRI_64 "I64"
#define strtoll _strtoi64
#define strtoull _strtoui64
typedef __int64 strtoll_t;
typedef unsigned __int64 strtoull_t;
#else
#define __PRI_64 "ll"
typedef long long strtoll_t;
typedef unsigned long long strtoull_t;
#endif

#ifndef PRId64
#define PRId64 __PRI_64 "d"
#endif
#ifndef PRIu64
#define PRIu64 __PRI_64 "u"
#endif
#ifndef PRIx64
#define PRIx64 __PRI_64 "x"
#endif  /* } */


#undef abs
#define abs(n) ((n) < 0 ? -(n) : (n))


#define MAX_DBL_INT 9007199254740992LL  /* 2**53 */

#define LUA_INTEGER_64 (sizeof(lua_Integer) >= sizeof(int64_t))
#define LUA_NUMBER_64 (sizeof(lua_Number) >= sizeof(int64_t))
#define LUA_POINTER_64 (sizeof(void *) >= sizeof(int64_t))


#define ITOA_BUFSIZ 32

#define SUFFIX_SIGNED  //"LL"
#define SUFFIX_UNSIGNED //"ULL"


#define LTYPE "INT64"


#define SUCCESS 1
#define FAILURE 0


typedef struct obj64 {
  int _unsigned;
  union {
    int64_t i;
    uint64_t u;
  } v;
} obj64;


typedef union dbl64 {
  volatile double d;
  volatile int64_t i;
  volatile uint64_t u;
} dbl64;


typedef struct str_info {
  const char *s;
  size_t l;
  int base;
} str_info;


typedef struct stack_info {
  lua_State *L;
  int idx;
} stack_info;


/*
** {=================================================================
** Lua 5.1 compatibility
** ==================================================================
*/

#if LUA_VERSION_NUM < 502  /* { */

/* lua-5.2.2/src/lua.h:88 */
#define LUA_NUMTAGS   9

/* lua-5.2.2/src/lua.h:184-196 */
#define LUA_OPADD 0
#define LUA_OPSUB 1
#define LUA_OPMUL 2
#define LUA_OPDIV 3
#define LUA_OPMOD 4
#define LUA_OPPOW 5
#define LUA_OPUNM 6

#define LUA_OPEQ  0
#define LUA_OPLT  1
#define LUA_OPLE  2


#endif 
#if LUA_VERSION_NUM < 501

/* lua-5.2.2/src/lauxlib.c:284-287 */
static void luaL_setmetatable(lua_State* L, const char* tname) {
    luaL_getmetatable(L, tname);
    lua_setmetatable(L, -2);
}

static lua_Integer lua_tointegerx(lua_State* L, int idx, int* isnum) {
    lua_Integer n = lua_tointeger(L, idx);
    if (isnum) *isnum = (n != 0 || lua_type(L, idx) == LUA_TNUMBER);
    return n;
}


/* lua-5.2.2/src/lauxlib.c:290-302 */
static void *luaL_testudata (lua_State *L, int ud, const char *tname) {
  void *p = lua_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (lua_getmetatable(L, ud)) {  /* does it have a metatable? */
      luaL_getmetatable(L, tname);  /* get correct metatable */
      if (!lua_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      lua_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}

#endif  /* } */

/* }================================================================= */


/*
** {=================================================================
** Test whether types are at least 64 bits wide.
** ==================================================================
*/


LUALIB_API int luaL_integeris64 (lua_State *L) {
  (void)L;
  return LUA_INTEGER_64;
}


LUALIB_API int luaL_numberis64 (lua_State *L) {
  (void)L;
  return LUA_NUMBER_64;
}


LUALIB_API int luaL_pointeris64 (lua_State *L) {
  (void)L;
  return LUA_POINTER_64;
}


/* }================================================================= */


/*
** {=================================================================
** Lua type => (u)int64_t
** ==================================================================
*/


#define ltag_to_ltoi_error(tt) ((tt) + 2)

#define LTOI_ERRMSG(type) "cannot convert " type " to int64"

static const char *ltoi_errmsg[] = {
  "ok",
  LTOI_ERRMSG("none"),
  LTOI_ERRMSG("nil"),
  LTOI_ERRMSG("boolean"),
  LTOI_ERRMSG("address"),  /* light userdata */
  LTOI_ERRMSG("number"),
  LTOI_ERRMSG("string"),
  LTOI_ERRMSG("table"),
  LTOI_ERRMSG("function"),
  "userdata is not an int64 boxed type object",
  LTOI_ERRMSG("thread"),
  "not a valid type"
};


LUALIB_API void luaL_i64pushstrerror (lua_State *L, int errcode) {
  if (errcode < 0 || errcode > ltag_to_ltoi_error(LUA_NUMTAGS))
    errcode = ltag_to_ltoi_error(LUA_NUMTAGS);
  lua_pushstring(L, ltoi_errmsg[errcode]);
}


static int str2i64 (const void *data, int64_t *n) {
  const str_info *info = data;
  char *endptr;
  strtoll_t x = strtoll(info->s, &endptr, info->base);
  if (info->s == endptr) return FAILURE;
  if (*endptr == 'L' || *endptr == 'l') {
    endptr++;
    if (*endptr == 'L' || *endptr == 'l') endptr++;
    else return FAILURE;
  }
  while (isspace((unsigned char)*endptr)) endptr++;
  if (endptr != info->s + info->l) return FAILURE;
  *n = (int64_t)x;
  return SUCCESS;
}


static int str2u64 (const void *data, uint64_t *n) {
  const str_info *info = data;
  char *endptr;
  strtoull_t x = strtoull(info->s, &endptr, info->base);
  if (info->s == endptr) return FAILURE;
  if (*endptr == 'U' || *endptr == 'u') endptr++;
  if (*endptr == 'L' || *endptr == 'l') {
    endptr++;
    if (*endptr == 'L' || *endptr == 'l') endptr++;
    else return FAILURE;
  }
  while (isspace((unsigned char)*endptr)) endptr++;
  if (endptr != info->s + info->l) return FAILURE;
  *n = (uint64_t)x;
  return SUCCESS;
}


static int num2i64 (const void *data, int64_t *n) {
  const stack_info *info = data;
  int64_t i = (int64_t)lua_tointeger(info->L, info->idx);
  lua_Number x = lua_tonumber(info->L, info->idx);
  if (isnormal(x) && i == x && (!LUA_NUMBER_64
      || (!LUA_INTEGER_64 || abs(i) <= MAX_DBL_INT))) {
    *n = i;
  }
  else if (LUA_NUMBER_64) {
    /* !!! XXX: BIT HACKS -- NOT GUARANTEED !!! */
    dbl64 d;
    d.d = x;
    *n = d.i;
  }
  else return FAILURE;
  return SUCCESS;
}


static int num2u64 (const void *data, uint64_t *n) {
  const stack_info *info = data;
  uint64_t i = (uint64_t)lua_tointeger(info->L, info->idx);
  lua_Number x = lua_tonumber(info->L, info->idx);
  if (isnormal(x) && (!LUA_NUMBER_64
      || (!LUA_INTEGER_64 || i <= MAX_DBL_INT))) {
    *n = i;
  }
  else if (LUA_NUMBER_64) {
    /* !!! XXX: BIT HACKS -- NOT GUARANTEED !!! */
    dbl64 d;
    d.d = x;
    *n = d.u;
  }
  else return FAILURE;
  return SUCCESS;
}


static int addr2i64 (const void *data, int64_t *n) {
  if (LUA_POINTER_64) {
    *n = (int64_t)data;
    return SUCCESS;
  }
  else return FAILURE;
}


static int addr2u64 (const void *data, uint64_t *n) {
  if (LUA_POINTER_64) {
    *n = (uint64_t)data;
    return SUCCESS;
  }
  else return FAILURE;
}


static int hvud2i64 (const void *data, int64_t *n) {
  const stack_info *info = data;
  const obj64 *o = luaL_testudata(info->L, info->idx, LTYPE);
  if (o) {
    *n = o->v.i;
    return SUCCESS;
  }
  else return FAILURE;
}


static int hvud2u64 (const void *data, uint64_t *n) {
  const stack_info *info = data;
  const obj64 *o = luaL_testudata(info->L, info->idx, LTYPE);
  if (o) {
    *n = o->v.u;
    return SUCCESS;
  }
  else return FAILURE;
}


LUALIB_API int64_t luaL_toi64be (lua_State *L, int idx, int base,
                                 int *errcode) {
  int64_t n;
  int success;
  const int tt = lua_type(L, idx);
  switch (tt) {
    case LUA_TNUMBER: {
      stack_info info = {L, idx};
      success = num2i64(&info, &n);
      break;
    }
    case LUA_TSTRING: {
      str_info info;
      info.s = lua_tolstring(L, idx, &info.l);
      info.base = base;
      success = str2i64(&info, &n);
      break;
    }
    case LUA_TLIGHTUSERDATA: {
      const void *ptr = lua_touserdata(L, idx);
      success = addr2i64(ptr, &n);
      break;
    }
    case LUA_TUSERDATA: {
      stack_info info = {L, idx};
      success = hvud2i64(&info, &n);
      break;
    }
    default: {
      success = 1;
      break;
    }
  }
  if (errcode) *errcode = success ? 0 : ltag_to_ltoi_error(tt);
  return success ? n : 0;
}


LUALIB_API uint64_t luaL_tou64be (lua_State *L, int idx, int base,
                                  int *errcode) {
  uint64_t n = 0;
  int success;
  const int tt = lua_type(L, idx);
  switch (tt) {
    case LUA_TNUMBER: {
      stack_info info = {L, idx};
      success = num2u64(&info, &n);
      break;
    }
    case LUA_TSTRING: {
      str_info info;
      info.s = lua_tolstring(L, idx, &info.l);
      info.base = base;
      success = str2u64(&info, &n);
      break;
    }
    case LUA_TLIGHTUSERDATA: {
      const void *ptr = lua_touserdata(L, idx);
      success = addr2u64(ptr, &n);
      break;
    }
    case LUA_TUSERDATA: {
      stack_info info = {L, idx};
      success = hvud2u64(&info, &n);
      break;
    }
    default: {
      success = 1;
      break;
    }
  }
  if (errcode) *errcode = success ? 0 : ltag_to_ltoi_error(tt);
  return success ? n : 0;
}


LUALIB_API int64_t luaL_toi64bx (lua_State *L, int idx, int base,
                                 int *isnum) {
  int errcode;
  int64_t n = luaL_toi64be(L, idx, base, &errcode);
  if (isnum) *isnum = (errcode == 0);
  return n;
}


LUALIB_API uint64_t luaL_tou64bx (lua_State *L, int idx, int base,
                                  int *isnum) {
  int errcode;
  uint64_t n = luaL_tou64be(L, idx, base, &errcode);
  if (isnum) *isnum = (errcode == 0);
  return n;
}


static void i64pushfstr (lua_State *L, void *n, const char *fmt,
                         int _unsigned) {
  char buf[ITOA_BUFSIZ];
  int64_t i;
  int offset = 0;
  if (strchr(fmt, 'x') && !_unsigned) {
    i = *(int64_t *)n;
    if (i < 0) {
      buf[offset++] = '-';
      i = -i;
      n = &i;
    }
  }
  sprintf(&buf[offset], fmt, _unsigned ? *(uint64_t *)n : *(int64_t *)n);
  lua_pushstring(L, buf);
}


LUALIB_API void luaL_i64pushstring (lua_State *L, int64_t n) {
  i64pushfstr(L, &n, "%"PRId64 SUFFIX_SIGNED, 0);
}


LUALIB_API void luaL_u64pushstring (lua_State *L, uint64_t n) {
  i64pushfstr(L, &n, "%"PRIu64 SUFFIX_UNSIGNED, 1);
}


LUALIB_API void luaL_i64pushhex (lua_State *L, int64_t n) {
  i64pushfstr(L, &n, "%#"PRIx64 SUFFIX_SIGNED, 0);
}


LUALIB_API void luaL_u64pushhex (lua_State *L, uint64_t n) {
  i64pushfstr(L, &n, "%#"PRIx64 SUFFIX_UNSIGNED, 1);
}


/* }================================================================= */

/*
** {=================================================================
** If the absolute value of the int fits in 2**53, this works fine.
**
** OTHERWISE:
** This is incredibly hacky and may not even work.
** Fit a 64-bit into the bit field of a double, if this Lua
**    defined lua_Number as double.
** !!! UNDEFINED BEHAVIOR / IMPLEMENTATION-DEFINED / NOT GUARANTEED !!!
** ==================================================================
*/


#define ERR_LNUM "cannot convert int64 to number"


LUALIB_API void luaL_i64pushnumber (lua_State *L, int64_t n) {
  if (LUA_NUMBER_64) {
    if (abs(n) <= MAX_DBL_INT) {
      if (LUA_INTEGER_64)
        lua_pushinteger(L, (lua_Integer)n);
      else
        lua_pushnumber(L, (lua_Number)n);
    }
    else {
      /* !!! XXX: BIT MAGIC / NOT GUARANTEED !!! */
      dbl64 x;
      x.i = n;
      lua_pushnumber(L, x.d);
    }
  }
  else luaL_error(L, ERR_LNUM);
}


LUALIB_API void luaL_u64pushnumber (lua_State *L, uint64_t n) {
  if (LUA_NUMBER_64) {
    if (n <= MAX_DBL_INT) {
      lua_pushnumber(L, (lua_Number)n);
    }
    else {
      /* !!! XXX: BIT MAGIC / NOT GUARANTEED !!! */
      dbl64 x;
      x.u = n;
      lua_pushnumber(L, x.d);
    }
  }
  else luaL_error(L, ERR_LNUM);
}


/* }================================================================= */

/*
** {=================================================================
** Throw error if can't fit 64-bit int into light userdatum.
** ==> Throw error on 32-bit systems.
** ==================================================================
*/


#define ERR_ADDR "cannot convert int64 to address"


LUALIB_API void luaL_i64pushaddress (lua_State *L, int64_t n) {
  if (LUA_POINTER_64)
    lua_pushlightuserdata(L, (void *)n);
  else luaL_error(L, ERR_ADDR);
}


LUALIB_API void luaL_u64pushaddress (lua_State *L, uint64_t n) {
  if (LUA_POINTER_64)
    lua_pushlightuserdata(L, (void *)n);
  else luaL_error(L, ERR_ADDR);
}


/* }================================================================= */

/*
** {=================================================================
** Lua interface to 64-bit types.
** ==================================================================
*/


#define toInt64(L) luaL_checkudata(L, 1, LTYPE)


static obj64 *obj64_pushraw (lua_State *L) {
  obj64 *o = lua_newuserdata(L, sizeof *o);
  luaL_setmetatable(L, LTYPE);
  return o;
}


static obj64 *obj64_pushint (lua_State *L, int64_t n) {
  obj64 *o = obj64_pushraw(L);
  o->_unsigned = 0;
  o->v.i = n;
  return o;
}


static obj64 *obj64_pushunsigned (lua_State *L, uint64_t n) {
  obj64 *o = obj64_pushraw(L);
  o->_unsigned = 1;
  o->v.u = n;
  return o;
}


static int obj64_unew (lua_State *L) {
  int base = luaL_optint(L, 2, 0);
  int errcode;
  uint64_t n = luaL_tou64be(L, 1, base, &errcode);
  if (errcode != 0) {
    lua_pushnil(L);
    luaL_i64pushstrerror(L, errcode);
    return 2;
  }
  obj64_pushunsigned(L, n);
  return 1;
}


static int obj64_inew (lua_State *L) {
  int base = luaL_optint(L, 2, 0);
  int errcode;
  int64_t n = luaL_tou64be(L, 1, base, &errcode);
  if (errcode != 0) {
    lua_pushnil(L);
    luaL_i64pushstrerror(L, errcode);
    return 2;
  }
  obj64_pushint(L, n);
  return 1;
}


// FIXME: remove module self at index 1
static int obj64_xnew (lua_State *L) {
  int isnum;
  int base = (int)lua_tointegerx(L, 2, &isnum);  /* zero by default */
  int top = isnum ? 3 : 2;
  lua_settop(L, top);
  lua_pushcfunction(L, lua_toboolean(L, top) ? obj64_unew : obj64_inew);
  lua_pushvalue(L, 1);
  lua_pushinteger(L, base);
  lua_call(L, 2, 1);
  return 1;
}


static int obj64_tostring (lua_State *L) {
  obj64 *o = toInt64(L);
  if (o->_unsigned)
    luaL_u64pushstring(L, o->v.u);
  else
    luaL_i64pushstring(L, o->v.i);
  return 1;
}


static int obj64_unsigned (lua_State *L) {
  obj64 *o = toInt64(L);
  lua_pushboolean(L, o->_unsigned);
  return 1;
}


static int obj64_toaddr (lua_State *L) {
  obj64 *o = toInt64(L);
  if (o->_unsigned)
    luaL_u64pushaddress(L, o->v.u);
  else
    luaL_i64pushaddress(L, o->v.i);
  return 1;
}


static int obj64_tonumber (lua_State *L) {
  obj64 *o = toInt64(L);
  if (o->_unsigned)
    luaL_u64pushnumber(L, o->v.u);
  else
    luaL_u64pushnumber(L, o->v.i);
  return 1;
}


static int obj64_tohex (lua_State *L) {
  obj64 *o = toInt64(L);
  if (o->_unsigned)
    luaL_u64pushhex(L, o->v.u);
  else
    luaL_i64pushhex(L, o->v.i);
  return 1;
}


static int i_compare (int op, int64_t a, int64_t b) {
  switch (op) {
    case LUA_OPEQ: return a == b;
    case LUA_OPLT: return a < b;
    case LUA_OPLE: return a <= b;
    default: return 0;
  }
}


static int u_compare (int op, uint64_t a, uint64_t b) {
  switch (op) {
    case LUA_OPEQ: return a == b;
    case LUA_OPLT: return a < b;
    case LUA_OPLE: return a <= b;
    default: return 0;
  }
}


static int obj64_compare (lua_State *L, int op) {
  obj64 *o;
  int other;
  int errcode;
  switch (lua_type(L, 1)) {
    case LUA_TUSERDATA: {
      other = 2;
      o = luaL_checkudata(L, 1, LTYPE);
    }
    default: {
      other = 1;
      o = luaL_checkudata(L, 2, LTYPE);
    }
  }
  if (o->_unsigned) {
    uint64_t u = luaL_tou64be(L, other, 0, &errcode);
    if (errcode == 0) {
      uint64_t a, b;
      if (other == 2) { a = o->v.u; b = u; }
      else { b = o->v.u; a = u; }
      lua_pushboolean(L, u_compare(op, a, b));
      return 1;
    }
  }
  else {
    int64_t i = luaL_toi64be(L, other, 0, &errcode);
    if (errcode == 0) {
      int64_t a, b;
      if (other == 2) { a = o->v.i; b = i; }
      else { b = o->v.i; a = i; }
      lua_pushboolean(L, i_compare(op, a, b));
      return 1;
    }
  }
  lua_pushnil(L);
  luaL_i64pushstrerror(L, errcode);
  return 2;
}


static int obj64_eq (lua_State *L) {
  return obj64_compare(L, LUA_OPEQ);
}


static int obj64_lt (lua_State *L) {
  return obj64_compare(L, LUA_OPLT);
}


static int obj64_le (lua_State *L) {
  return obj64_compare(L, LUA_OPLE);
}


static int64_t i_arith (int op, int64_t a, int64_t b) {
  switch (op) {
    case LUA_OPADD: return a + b;
    case LUA_OPSUB: return a - b;
    case LUA_OPMUL: return a * b;
    case LUA_OPDIV: return a / b;
    case LUA_OPMOD: return a % b;
    case LUA_OPPOW: {
      if (b < 0) return 0;
      else if (b == 0) return 1;
      else if (b == 1) return a;
      if (a == 2) {
        if (b >= 0)
          return (int64_t)(1 << b);
        else
          return 1 >> -b;
      }
      else {
        /* exponentiation by squaring */
        /* http://stackoverflow.com/a/101613 */
        int64_t r = 1;
        while (b) {
          if (b & 1) r *= a;
          b >>= 1;
          a *= a;
        }
        return r;
      }
    }
    default: return 0;
  }
}


static uint64_t u_arith (int op, uint64_t a, uint64_t b) {
  switch (op) {
    case LUA_OPADD: return a + b;
    case LUA_OPSUB: return a - b;
    case LUA_OPMUL: return a * b;
    case LUA_OPDIV: return a / b;
    case LUA_OPMOD: return a % b;
    case LUA_OPPOW: {
      if (b == 0) return 1;
      else if (b == 1) return a;
      if (a == 2) return (int64_t)(1 << b);
      else {
        /* exponentiation by squaring */
        /* http://stackoverflow.com/a/101613 */
        uint64_t r = 1;
        while (b) {
          if (b & 1) r *= a;
          b >>= 1;
          a *= a;
        }
        return r;
      }
    }
    default: return 0;
  }
}


static int obj64_arith (lua_State *L, int op) {
  obj64 *o;
  int errcode;
  o = toInt64(L);
  if (o->_unsigned) {
    uint64_t u = luaL_tou64be(L, 2, 0, &errcode);
    if (errcode == 0) {
      obj64_pushunsigned(L, u_arith(op, o->v.u, u));
      return 1;
    }
  }
  else {
    int64_t i = luaL_toi64be(L, 2, 0, &errcode);
    if (errcode == 0) {
      obj64_pushint(L, i_arith(op, o->v.i, i));
      return 1;
    }
  }
  lua_pushnil(L);
  luaL_i64pushstrerror(L, errcode);
  return 2;
}


static int obj64_add (lua_State *L) {
  return obj64_arith(L, LUA_OPADD);
}


static int obj64_sub (lua_State *L) {
  return obj64_arith(L, LUA_OPSUB);
}


static int obj64_mul (lua_State *L) {
  return obj64_arith(L, LUA_OPMUL);
}


static int obj64_div (lua_State *L) {
  return obj64_arith(L, LUA_OPDIV);
}


static int obj64_mod (lua_State *L) {
  return obj64_arith(L, LUA_OPMOD);
}


static int obj64_pow (lua_State *L) {
  return obj64_arith(L, LUA_OPPOW);
}


static int obj64_unm (lua_State *L) {
  obj64 *o = toInt64(L);
  int64_t n = o->_unsigned ? (int64_t)o->v.u : o->v.i;
  obj64_pushint(L, -n);
  return 1;
}


static const luaL_Reg lib[] = {
  {"new_signed", obj64_inew},
  {"new_unsigned", obj64_unew},
  {"__call", obj64_xnew},
  {NULL, NULL}
};


static const luaL_Reg meta[] = {
  {"__tostring", obj64_tostring},
  {"__eq", obj64_eq},
  {"__lt", obj64_lt},
  {"__le", obj64_le},
  {"__add", obj64_add},
  {"__sub", obj64_sub},
  {"__mul", obj64_mul},
  {"__div", obj64_div},
  {"__mod", obj64_mod},
  {"__pow", obj64_pow},
  {"__unm", obj64_unm},
  {"unsigned", obj64_unsigned},
  {"tohex", obj64_tohex},
  {"tonumber", obj64_tonumber},
  {"toaddress", obj64_toaddr},
  {NULL, NULL}
};


#if LUA_VERSION_NUM == 502
#define luaL_register(L,n,l) { luaL_newlib(L,l);}
#endif

int luaopen_int64 (lua_State *L) {
  luaL_newmetatable(L, LTYPE);

#if LUA_VERSION_NUM > 501 && !defined(LUA_COMPAT_MODULE)
  luaL_setfuncs(L, meta, 0);
#else
  luaL_openlib(L, NULL, meta, 0);
#endif

  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");

#if LUA_VERSION_NUM > 501 && !defined(LUA_COMPAT_MODULE)
  luaL_setfuncs(L, lib, 0);
#else
  luaL_openlib(L, "int64", lib, 0);
#endif

  lua_pushvalue(L, -1);
  lua_setmetatable(L, -2);
  return 1;
}


/* }================================================================= */


unsigned long long double2u64(double x) {

    unsigned long long i = (unsigned long long)x;
    if (isnormal(x) && i <= MAX_DBL_INT) {
        return i;
    }
    else {
        /* !!! XXX: BIT HACKS -- NOT GUARANTEED !!! */
        dbl64 d;
        d.d = x;
        return d.u;
    }
}

long long double2i64(const void* data, double x) {

    long long i = (long long)x;
    if (isnormal(x) && i == x && abs(i) <= MAX_DBL_INT) {
        return i;
    }
    else {
        /* !!! XXX: BIT HACKS -- NOT GUARANTEED !!! */
        dbl64 d;
        d.d = x;
        return d.i;
    }
}

 double i642double(long long n) {
    if (abs(n) <= MAX_DBL_INT) {
       return (double)n;
    }
    else {
        /* !!! XXX: BIT MAGIC / NOT GUARANTEED !!! */
        dbl64 x;
        x.i = n;
        return x.d;
    }
}

 double u642double(unsigned long long n) {
    if (n <= MAX_DBL_INT) {
        return (double)n;
    }
    else {
        /* !!! XXX: BIT MAGIC / NOT GUARANTEED !!! */
        dbl64 x;
        x.u = n;
        return x.d;

    }
}