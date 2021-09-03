// gcc -fPIC -O3 -Wall -I/usr/local/ssl/include -I/usr/local/openresty/luajit/include/luajit-2.1/ -c srp.c  
// gcc -shared -fPIC srp.o /usr/local/openresty/luajit/lib/libluajit-5.1.so /usr/local/ssl/lib/libcrypto.a -o srp.so

#include <stdlib.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include <openssl/srp.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
# include <openssl/sha.h>

#include "srp.h"

static int srp_get_default_gN(lua_State* L)
{
    SRP_gN* GN = NULL;
    char N_num_bits[5] = {0};
    if(lua_gettop(L) >= 1 && lua_isstring(L, 1))
    {
        strncpy(N_num_bits, lua_tostring(L, 1), sizeof(N_num_bits));
        GN = SRP_get_default_gN(N_num_bits);
    }
    else
    {
        GN = SRP_get_default_gN(NULL);
    }

    if(GN == NULL)
    {
        return luaL_error(L, "get_default_gN failed");
    }

    char* strN = BN_bn2hex(GN->N);
    if(strN == NULL)
    {
        return luaL_error(L, "get_default_gN BN_bin2hex N failed");
    }

    lua_pushnumber(L, (lua_Number)BN_get_word(GN->g));
    lua_pushstring(L, strN);
    OPENSSL_free(strN);
    return 2;
};

static int srp_RAND_pseudo_bytes(lua_State* L)
{
    unsigned char rand_tmp[4096] = {0};
    int rand_size = 32;
    if(lua_gettop(L) >= 1 && lua_isnumber(L, 1))
    {
        rand_size = (int)lua_tonumber(L, 1);
        rand_size = rand_size > sizeof(rand_tmp) ? sizeof(rand_tmp) : rand_size;
    }

    RAND_pseudo_bytes(rand_tmp, rand_size);
    BIGNUM* rd = BN_bin2bn(rand_tmp, rand_size, NULL);
    if(rd == NULL)
    {
        return luaL_error(L, "RAND_pseudo_bytes BN_bin2bn rd failed");
    }

    char* strrd = BN_bn2hex(rd);
    if(strrd == NULL)
    {
        BN_free(rd);
        return luaL_error(L, "RAND_pseudo_bytes BN_bin2hex rd failed");
    }

    lua_pushstring(L, strrd);
    OPENSSL_free(strrd);
    BN_free(rd);
    return 1;
};

static int srp_Verify_mod_N(lua_State* L)
{
    if(lua_gettop(L) < 2)
    {
        return luaL_error(L, "Verify_mod_N require the variable to verify and N");
    }

    int ret = 1;
    BIGNUM* B = NULL;
    BIGNUM* N = NULL;

    if(!lua_isstring(L, 1) || !BN_hex2bn(&B, lua_tostring(L, 1)))
    {
        ret = luaL_error(L, "Verify_mod_N invalid verify variable");
        goto err;
    }

    if(!lua_isstring(L, 2) || !BN_hex2bn(&N, lua_tostring(L, 2)))
    {
        ret = luaL_error(L, "Verify_mod_N invalid N");
        goto err;
    }

    int result = SRP_Verify_B_mod_N(B, N);
    lua_pushnumber(L, result);

err:
    BN_free(B);
    BN_free(N);
    return ret;
};

static int srp_create_verifier(lua_State* L)
{
    if(lua_gettop(L) < 4)
    {
        return luaL_error(L, "create_verifier require username, passwd, N, g");
    }

    int ret = 1;
    char username[4096] = {0};
    char passwd[4096] = {0};
    BIGNUM* s = NULL;
    BIGNUM* N = NULL;
    BIGNUM* g = BN_new();
    BIGNUM* v = NULL;
    char* strv = NULL;

    if(!lua_isstring(L, 1))
    {
        ret = luaL_error(L, "create_verifier invalid username");
        goto err;
    }

    strncpy(username, lua_tostring(L, 1), sizeof(username));

    if(!lua_isstring(L, 2))
    {
        ret = luaL_error(L, "create_verifier invalid passwd");
        goto err;
    }

    strncpy(passwd, lua_tostring(L, 2), sizeof(passwd));

    if(!lua_isstring(L, 3) || !BN_hex2bn(&s, lua_tostring(L, 3)))
    {
        ret = luaL_error(L, "create_verifier invalid s");
        goto err;
    }

    if(!lua_isstring(L, 4) || !BN_hex2bn(&N, lua_tostring(L, 4)))
    {
        ret = luaL_error(L, "create_verifier invalid N");
        goto err;
    }

    if(!lua_isnumber(L, 5) || !BN_set_word(g, (unsigned long long)lua_tonumber(L, 5)))
    {
        ret = luaL_error(L, "create_verifier invalid g");
        goto err;
    }

    if(!SRP_create_verifier_BN(username, passwd, &s, &v, N, g)) 
    {
        ret = luaL_error(L, "create_verifier failed");
        goto err;
    }

    strv = BN_bn2hex(v);
    if(strv == NULL)
    {
        ret = luaL_error(L, "create_verifier BN_bin2hex v failed");
        goto err;
    }

    lua_pushstring(L, strv);

err:
    BN_free(s);
    BN_free(N);
    BN_free(g);
    BN_free(v);
    OPENSSL_free(strv);
    return ret;
};


static int srp_Calc_A(lua_State* L)
{
    if(lua_gettop(L) < 3)
    {
        return luaL_error(L, "Calc_A require a, N, g");
    }

    int ret = 1;
    BIGNUM* a = NULL;
    BIGNUM* N = NULL;
    BIGNUM* g = BN_new();
    BIGNUM* A = NULL;
    char* strA = NULL;

    if(!lua_isstring(L, 1) || !BN_hex2bn(&a, lua_tostring(L, 1)))
    {
        ret = luaL_error(L, "Calc_A invalid a");
        goto err;
    }

    if(!lua_isstring(L, 2) || !BN_hex2bn(&N, lua_tostring(L, 2)))
    {
        ret = luaL_error(L, "Calc_A invalid N");
        goto err;
    }

    if(!lua_isnumber(L, 3) || !BN_set_word(g, (unsigned long long)lua_tonumber(L, 3)))
    {
        ret = luaL_error(L, "Calc_A invalid g");
        goto err;
    }

    A = SRP_Calc_A(a, N, g);
    if(A == NULL)
    {
        ret = luaL_error(L, "Calc_A SRP_Calc_A failed");
        goto err;
    }

    strA = BN_bn2hex(A);
    if(strA == NULL)
    {
        ret = luaL_error(L, "Calc_A BN_bin2hex A failed");
        goto err;
    }

    lua_pushstring(L, strA);

err:
    BN_free(a);
    BN_free(N);
    BN_free(g);
    BN_free(A);
    OPENSSL_free(strA);
    return ret;
}

static int srp_Calc_B(lua_State* L)
{
    if(lua_gettop(L) < 4)
    {
        return luaL_error(L, "Calc_B require b, N, g, v");
    }

    int ret = 1;
    BIGNUM* b = NULL;
    BIGNUM* N = NULL;
    BIGNUM* g = BN_new();
    BIGNUM* v = NULL;
    BIGNUM* B = NULL;
    char* strB = NULL;

    if(!lua_isstring(L, 1) || !BN_hex2bn(&b, lua_tostring(L, 1)))
    {
        ret = luaL_error(L, "Calc_B invalid b");
        goto err;
    }

    if(!lua_isstring(L, 2) || !BN_hex2bn(&N, lua_tostring(L, 2)))
    {
        ret = luaL_error(L, "Calc_B invalid N");
        goto err;
    }

    if(!lua_isnumber(L, 3) || !BN_set_word(g, (unsigned long long)lua_tonumber(L, 3)))
    {
        ret = luaL_error(L, "Calc_B invalid g");
        goto err;
    }

    if(!lua_isstring(L, 4) || !BN_hex2bn(&v, lua_tostring(L, 4)))
    {
        ret = luaL_error(L, "Calc_B invalid v");
        goto err;
    }

    B = SRP_Calc_B(b, N, g, v);
    if(B == NULL)
    {
        ret = luaL_error(L, "Calc_B SRP_Calc_B failed");
        goto err;
    }

    strB = BN_bn2hex(B);
    if(strB == NULL)
    {
        ret = luaL_error(L, "Calc_B BN_bin2hex B failed");
        goto err;
    }

    lua_pushstring(L, strB);

err:
    BN_free(b);
    BN_free(N);
    BN_free(g);
    BN_free(v);
    BN_free(B);
    OPENSSL_free(strB);
    return ret;
};

static int srp_Calc_client_key(lua_State* L)
{
    if(lua_gettop(L) < 8)
    {
        return luaL_error(L, "Calc_client_key require A, B, N, s, username, passwd, g, a");
    }

    int ret = 1;
    BIGNUM* A = NULL;
    BIGNUM* B = NULL;
    BIGNUM* N = NULL;
    BIGNUM* s = NULL;
    BIGNUM* g = BN_new();
    BIGNUM* a = NULL;
    BIGNUM* u = NULL;
    BIGNUM* x = NULL;
    BIGNUM* K = NULL;
    char username[4096] = {0};
    char passwd[4096] = {0};
    char* strK = NULL;

    if(!lua_isstring(L, 1) || !BN_hex2bn(&A, lua_tostring(L, 1)))
    {
        ret = luaL_error(L, "Calc_client_key invalid A");
        goto err;
    }

    if(!lua_isstring(L, 2) || !BN_hex2bn(&B, lua_tostring(L, 2)))
    {
        ret = luaL_error(L, "Calc_client_key invalid B");
        goto err;
    }

    if(!lua_isstring(L, 3) || !BN_hex2bn(&N, lua_tostring(L, 3)))
    {
        ret = luaL_error(L, "Calc_client_key invalid N");
        goto err;
    }

    if(!lua_isstring(L, 4) || !BN_hex2bn(&s, lua_tostring(L, 4)))
    {
        ret = luaL_error(L, "Calc_client_key invalid s");
        goto err;
    }

    if(!lua_isstring(L, 5))
    {
        ret = luaL_error(L, "Calc_client_key invalid username");
        goto err;
    }

    strncpy(username, lua_tostring(L, 5), sizeof(username));

    if(!lua_isstring(L, 6))
    {
        ret = luaL_error(L, "Calc_client_key invalid passwd");
        goto err;
    }

    strncpy(passwd, lua_tostring(L, 6), sizeof(passwd));

    if(!lua_isnumber(L, 7) || !BN_set_word(g, (unsigned long long)lua_tonumber(L, 7)))
    {
        ret = luaL_error(L, "Calc_client_key invalid g");
        goto err;
    }

    if(!lua_isstring(L, 8) || !BN_hex2bn(&a, lua_tostring(L, 8)))
    {
        ret = luaL_error(L, "Calc_client_key invalid a");
        goto err;
    }

    /* calculate u */
    u = SRP_Calc_u(A, B, N);
    if(u == NULL)
    {
        ret = luaL_error(L, "Calc_client_key SRP_Calc_u failed");
        goto err;
    }

    /* calculate x */
    x = SRP_Calc_x(s, username, passwd);
    if(x == NULL)
    {
        ret = luaL_error(L, "Calc_client_key SRP_Calc_x failed");
        goto err;
    }

    K = SRP_Calc_client_key(N, B, g, x, a, u);
    if(K == NULL)
    {
        ret = luaL_error(L, "Calc_client_key SRP_Calc_client_key failed");
        goto err;
    }

    strK = BN_bn2hex(K);
    if(strK == NULL)
    {
        ret = luaL_error(L, "Calc_client_key BN_bin2hex K failed");
        goto err;
    }

    lua_pushstring(L, strK);

err:
    BN_free(A);
    BN_free(B);
    BN_free(N);
    BN_free(s);
    BN_free(g);
    BN_free(a);
    BN_free(u);
    BN_free(x);
    BN_free(K);
    OPENSSL_free(strK);
    return ret;
};

static int srp_Calc_server_key(lua_State* L)
{
    if(lua_gettop(L) < 5)
    {
        return luaL_error(L, "Calc_server_key require A, B, N, v, b");
    }

    int ret = 1;
    BIGNUM* A = NULL;
    BIGNUM* B = NULL;
    BIGNUM* v = NULL;
    BIGNUM* b = NULL;
    BIGNUM* N = NULL;
    BIGNUM* u = NULL;
    BIGNUM* K = NULL;
    char* strK = NULL;

    if(!lua_isstring(L, 1) || !BN_hex2bn(&A, lua_tostring(L, 1)))
    {
        ret = luaL_error(L, "Calc_server_key invalid A");
        goto err;
    }

    if(!lua_isstring(L, 2) || !BN_hex2bn(&B, lua_tostring(L, 2)))
    {
        ret = luaL_error(L, "Calc_server_key invalid B");
        goto err;
    }

    if(!lua_isstring(L, 3) || !BN_hex2bn(&N, lua_tostring(L, 3)))
    {
        ret = luaL_error(L, "Calc_server_key invalid N");
        goto err;
    }
	
    if(!lua_isstring(L, 4) || !BN_hex2bn(&v, lua_tostring(L, 4)))
    {
        ret = luaL_error(L, "Calc_server_key invalid v");
        goto err;
    }

    if(!lua_isstring(L, 5) || !BN_hex2bn(&b, lua_tostring(L, 5)))
    {
        ret = luaL_error(L, "Calc_server_key invalid b");
        goto err;
    }

    /* calculate u */
    u = SRP_Calc_u(A, B, N);
    if(u == NULL)
    {
        ret = luaL_error(L, "Calc_server_key SRP_Calc_u failed");
        goto err;
    }

    K = SRP_Calc_server_key(A, v, u, b, N);
    if(K == NULL)
    {
        ret = luaL_error(L, "Calc_server_key SRP_Calc_server_key failed");
        goto err;
    }

    strK = BN_bn2hex(K);
    if(strK == NULL)
    {
        ret = luaL_error(L, "Calc_server_key BN_bin2hex K failed");
        goto err;
    }

    lua_pushstring(L, strK);

err:
    BN_free(A);
    BN_free(B);
    BN_free(v);
    BN_free(b);
    BN_free(N);
    BN_free(u);
    BN_free(K);
    OPENSSL_free(strK);
    return ret;
};

BIGNUM* SRP_Calc_M1(BIGNUM* N, BIGNUM* g, const char* username, BIGNUM* s, BIGNUM* A, BIGNUM* B, BIGNUM* K)
{
    /* H[H(N) XOR H(g) | H(username) | s | A | B | K] */
    unsigned char* tmp = NULL;
    unsigned char dig[SHA_DIGEST_LENGTH];
    unsigned char digg[SHA_DIGEST_LENGTH];
    unsigned char digu[SHA_DIGEST_LENGTH];
    EVP_MD_CTX ctxt;

    if((tmp = OPENSSL_malloc(BN_num_bytes(N))) == NULL)
    {
        return NULL;
    }

    // H(N)
    EVP_MD_CTX_init(&ctxt);
    EVP_DigestInit_ex(&ctxt, EVP_sha1(), NULL);
    BN_bn2bin(N, tmp);
    EVP_DigestUpdate(&ctxt, tmp, BN_num_bytes(N));
    EVP_DigestFinal_ex(&ctxt, dig, NULL);

    // H(g)
    EVP_DigestInit_ex(&ctxt, EVP_sha1(), NULL);
    BN_bn2bin(g, tmp);
    EVP_DigestUpdate(&ctxt, tmp, BN_num_bytes(g));
    EVP_DigestFinal_ex(&ctxt, digg, NULL);

    // H(N) ^ H(g)
    int i = 0;
    for(; i < SHA_DIGEST_LENGTH; ++i)
    {
        dig[i] ^= digg[i];
    }

    // H(username)
    EVP_DigestInit_ex(&ctxt, EVP_sha1(), NULL);
    EVP_DigestUpdate(&ctxt, username, strlen(username));
    EVP_DigestFinal_ex(&ctxt, digu, NULL);

    EVP_DigestInit_ex(&ctxt, EVP_sha1(), NULL);
    EVP_DigestUpdate(&ctxt, dig, sizeof(dig));
    EVP_DigestUpdate(&ctxt, digu, sizeof(digu));
    BN_bn2bin(s, tmp);
    EVP_DigestUpdate(&ctxt, tmp, BN_num_bytes(s));
    BN_bn2bin(A, tmp);
    EVP_DigestUpdate(&ctxt, tmp, BN_num_bytes(A));
    BN_bn2bin(B, tmp);
    EVP_DigestUpdate(&ctxt, tmp, BN_num_bytes(B));
    BN_bn2bin(K, tmp);
    EVP_DigestUpdate(&ctxt, tmp, BN_num_bytes(K));
    EVP_DigestFinal_ex(&ctxt, dig, NULL);
    EVP_MD_CTX_cleanup(&ctxt);

    OPENSSL_free(tmp);
    return BN_bin2bn(dig, sizeof(dig), NULL);
};

static int srp_Calc_M1(lua_State* L)
{
    /* BIGNUM* N, BIGNUM* g, const char* username, BIGNUM* s, BIGNUM* A, BIGNUM* B, BIGNUM* K */
    if(lua_gettop(L) < 7)
    {
        return luaL_error(L, "Calc_M1 require N, g, username, s, A, B, K");
    }

    int ret = 1;
    BIGNUM* M1 = NULL;
    BIGNUM* N = NULL;
    BIGNUM* g = BN_new();
    char username[4096] = {0};
    BIGNUM* s = NULL;
    BIGNUM* A = NULL;
    BIGNUM* B = NULL;
    BIGNUM* K = NULL;
    char* strM1 = NULL;

    if(!lua_isstring(L, 1) || !BN_hex2bn(&N, lua_tostring(L, 1)))
    {
        ret = luaL_error(L, "Calc_M1 invalid N");
        goto err;
    }

    if(!lua_isnumber(L, 2) || !BN_set_word(g, (unsigned long long)lua_tonumber(L, 2)))
    {
        ret = luaL_error(L, "Calc_M1 invalid g");
        goto err;
    }

    if(!lua_isstring(L, 3))
    {
        ret = luaL_error(L, "Calc_M1 invalid username");
        goto err;
    }
    
    strncpy(username, lua_tostring(L, 3), sizeof(username));

    if(!lua_isstring(L, 4) || !BN_hex2bn(&s, lua_tostring(L, 4)))
    {
        ret = luaL_error(L, "Calc_M1 invalid s");
        goto err;
    }

    if(!lua_isstring(L, 5) || !BN_hex2bn(&A, lua_tostring(L, 5)))
    {
        ret = luaL_error(L, "Calc_M1 invalid A");
        goto err;
    }

    if(!lua_isstring(L, 6) || !BN_hex2bn(&B, lua_tostring(L, 6)))
    {
        ret = luaL_error(L, "Calc_M1 invalid B");
        goto err;
    }

    if(!lua_isstring(L, 7) || !BN_hex2bn(&K, lua_tostring(L, 7)))
    {
        ret = luaL_error(L, "Calc_M1 invalid K");
        goto err;
    }

    if((M1 = SRP_Calc_M1(N, g, username, s, A, B, K)) == NULL)
    {
        ret = luaL_error(L, "Calc_M1 SRP_Calc_M1 failed");
        goto err;
    }

    strM1 = BN_bn2hex(M1);
    if(strM1 == NULL)
    {
        ret = luaL_error(L, "Calc_M1 BN_bin2hex M1 failed");
        goto err;
    }

    lua_pushstring(L, strM1);

err:
    BN_free(M1);
    BN_free(N);
    BN_free(g);
    BN_free(s);
    BN_free(A);
    BN_free(B);
    BN_free(K);
    OPENSSL_free(strM1);
    return ret;
};

BIGNUM* SRP_Calc_M2(BIGNUM* A, BIGNUM* M1, BIGNUM* K)
{
    /* H(A | M1 | K) */
    unsigned char* tmp = NULL;
    unsigned char dig[SHA_DIGEST_LENGTH];
    EVP_MD_CTX ctxt;

    if((tmp = OPENSSL_malloc(BN_num_bytes(K))) == NULL)
    {
        return NULL;
    }

    EVP_MD_CTX_init(&ctxt);
    EVP_DigestInit_ex(&ctxt, EVP_sha1(), NULL);
    BN_bn2bin(A, tmp);
    EVP_DigestUpdate(&ctxt, tmp, BN_num_bytes(A));
    BN_bn2bin(M1, tmp);
    EVP_DigestUpdate(&ctxt, tmp, BN_num_bytes(M1));
    BN_bn2bin(K, tmp);
    EVP_DigestUpdate(&ctxt, tmp, BN_num_bytes(K));
    EVP_DigestFinal_ex(&ctxt, dig, NULL);
    EVP_MD_CTX_cleanup(&ctxt);

    OPENSSL_free(tmp);
    return BN_bin2bn(dig, sizeof(dig), NULL);
};

static int srp_Calc_M2(lua_State* L)
{
    /* BIGNUM* A, BIGNUM* M1, BIGNUM* K */
    if(lua_gettop(L) < 3)
    {
        return luaL_error(L, "Calc_M2 require A, M1, K");
    }

    int ret = 1;
    BIGNUM* M2 = NULL;
    BIGNUM* A = NULL;
    BIGNUM* M1 = NULL;
    BIGNUM* K = NULL;
    char* strM2 = NULL;

    if(!lua_isstring(L, 1) || !BN_hex2bn(&A, lua_tostring(L, 1)))
    {
        ret = luaL_error(L, "Calc_M2 invalid A");
        goto err;
    }

    if(!lua_isstring(L, 2) || !BN_hex2bn(&M1, lua_tostring(L, 2)))
    {
        ret = luaL_error(L, "Calc_M2 invalid M1");
        goto err;
    }

    if(!lua_isstring(L, 3) || !BN_hex2bn(&K, lua_tostring(L, 3)))
    {
        ret = luaL_error(L, "Calc_M2 invalid K");
        goto err;
    }

    if((M2 = SRP_Calc_M2(A, M1, K)) == NULL)
    {
        ret = luaL_error(L, "Calc_M2 SRP_Calc_M2 failed");
        goto err;
    }

    strM2 = BN_bn2hex(M2);
    if(strM2 == NULL)
    {
        ret = luaL_error(L, "Calc_M2 BN_bin2hex M2 failed");
        goto err;
    }

    lua_pushstring(L, strM2);

err:
    BN_free(M2);
    BN_free(A);
    BN_free(M1);
    BN_free(K);
    OPENSSL_free(strM2);
    return ret;
};

static const luaL_Reg srp_lib[] = {
    { "get_default_gN", srp_get_default_gN },
    { "RAND_pseudo_bytes", srp_RAND_pseudo_bytes },
    { "Verify_mod_N", srp_Verify_mod_N },
    { "create_verifier", srp_create_verifier },
    { "Calc_A", srp_Calc_A },
    { "Calc_B", srp_Calc_B },
    { "Calc_client_key", srp_Calc_client_key },
    { "Calc_server_key", srp_Calc_server_key },
    { "Calc_M1", srp_Calc_M1 },
    { "Calc_M2", srp_Calc_M2 },
    { NULL, NULL }
};

int luaopen_srp(lua_State *L)
{
    luaL_register(L, "srp", srp_lib);
    return 1;
};


/* lua test
local srp = require ("srp")

local username = "username"
local password = "123456"
print("Client ============== > Server:\n"
    .. "username = " .. username .. "\n")

local g, N = srp.get_default_gN("2048");
local s = srp.RAND_pseudo_bytes(16)
local v = srp.create_verifier(username, password, s, N, g)
print("v = " .. v);
print("Server ============== > Client:\n"
    .. "N = " .. N .. "\n"
    .. "g = " .. g .. "\n"
    .. "s = " .. s .. "\n")

local a = srp.RAND_pseudo_bytes(32)
print("a = " .. a)
local A = srp.Calc_A(a, N, g)
print("Client ============== > Server:\n"
    .. "A = " .. A .. "\n")

print("Server verify A mod N: " .. srp.Verify_mod_N(A, N))
local b = srp.RAND_pseudo_bytes(32)
--print("b: " .. b)
local B = srp.Calc_B(b, N, g, v)
print("Server ============== > Client:\n"
    .. "B = " .. B .. "\n")

print("Client verify B mod N: " .. srp.Verify_mod_N(B, N))
local Kclient = srp.Calc_client_key(A, B, N, s, username, password, g, a)
print("Kclient = " .. Kclient);
local M1 = srp.Calc_M1(N, g, username, s, A, B, Kclient);
print("Client ============== > Server:\n"
    .. "M1 = " .. M1 .. "\n")

print("Server verify M1 match")
local Kserver = srp.Calc_server_key(A, B, N, v, b);
print("Kserver = " .. Kserver);
local M2 = srp.Calc_M2(A, M1, Kserver)
print("Server ============== > Client:\n"
    .. "M2 = " .. M2 .. "\n")

print("Client verify M2 match \n")

*/

/*
local aes = require "resty.aes"

local function from_hex(hex)
    return string.gsub(hex, "%x%x", function(c) return string.char(tonumber(c, 16)) end)
end

local key = from_hex("933BE86463227AEEB040902D2A68B2899D2B36A24EE539A8EFA89C1240875F9E705CDA349A5025B678E0A4C3BF78409CD37A3A98487964EA4C675F9ADA00F88CF040EBD331A5264860720D6D8D26BF86D35509D714CF1FA640F62E32D801B29F6CE68638705F5F18B79299FAC0E566BA0E704483F9AC3DF6AA6F2F292F689CA3826A71A84698861EC8AE05C6A5DCD40E4F0ED01AB214413AAE23D3A34C56DA014BC9585574A92A11AF4201B79833C7C783BC5E7D713F5BA222FBDF6506FF1DCCBA736E06BDDA17C9A7631BABDBA1A92115B252B950998A623D5F2A1ADB324782D48FCC151CD0045219A75FAE8F18FA8ABF37977541C64CB00A6000F755106218")
key = string.sub(key, 1, 32)
local iv = from_hex("bfd3814678afe0036efa67ca8da44e2e")
local aes_256_cbc_with_iv = aes:new(key, nil, aes.cipher(256, "cbc"), {iv = iv})

-- AES 128 CBC with IV and no SALT
local encrypted = aes_256_cbc_with_iv:encrypt([[{"I":"username","q":1,"clt":{"p":"wxapp","v":10000}}]])
print(ngx.encode_base64(encrypted))
print(aes_256_cbc_with_iv:decrypt(encrypted))


encrypted = "AehgVsne741NRvGGRrrIcktjZtf52/0gFOAlWkSLLAoz8X2XpGhJ1Ccez0e4YA78ZsYHrflvUoRp6odSSgTmnpqVFvbQNqhzl9KJRw52FyQW84AEEUfMK3xIEZ16Lc5D"
encrypted = ngx.decode_base64(encrypted)
print("------------------")
print(aes_256_cbc_with_iv:decrypt(encrypted))


*/

