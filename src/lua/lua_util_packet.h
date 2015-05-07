#ifndef _LUA_UTIL_PACKET_H
#define _LUA_UTIL_PACKET_H

#include "packet/wpacket.h"
#include "packet/rpacket.h"
#include "lua/lua_util.h"

//read/write lua table to rpacket/wpacket

const char*
lua_pack_table(wpacket *wpk,lua_State *L,int index);

int 
lua_unpack_table(rpacket *rpk,lua_State *L);

#endif