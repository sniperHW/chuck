#ifndef _LUAPACKET_H
#define _LUAPACKET_H

#include "lua/lua_util.h"
#include "packet/packet.h"

//#define LUAPACKET_METATABLE "luapacket_metatable"

typedef struct{
	packet *_packet;
}luapacket;

luapacket*
lua_topacket(lua_State *L, int index);

void       
lua_pushpacket(lua_State *L,packet*);

void       
reg_luapacket(lua_State *L);

#endif
