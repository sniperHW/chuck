#include "packet/luapacket.h"
#include "engine/engine.h"
#include "socket/wrap/connection.h"
#include "socket/socket_helper.h"
#include "socket/connector.h"
#include "socket/acceptor.h"

int32_t luaopen_chuck(lua_State *L){
	
	lua_newtable(L);
	
	reg_luaconnection(L);
	reg_luaconnector(L);
	reg_luaacceptor(L);
	reg_luaengine(L);

	lua_pushstring(L,"packet");
	reg_luapacket(L);
	lua_settable(L,-3);
			
	lua_pushstring(L,"socket_helper");
	reg_luasocket_helper(L);
	lua_settable(L,-3);
		
	return 1;
}