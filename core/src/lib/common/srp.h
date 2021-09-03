/*
Provide a shared library for lua which contains the openssl srp6a implemention. And add the calculation of M1, M2 which openssl do not provide.

Please run test_srp.lua to see the password verification process between client and server.

Openresty is strongly recommended working with lua_srp6a, you will see how easily make a web server with openresty and how easily make a secure password verification with lua_srp6a.
*/

int luaopen_srp(lua_State* L);