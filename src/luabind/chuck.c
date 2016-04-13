#define _CORE_
#include "../chuck.h"

#define SET_CONST(L,N) do{\
		lua_pushstring(L, #N);\
		lua_pushinteger(L, N);\
		lua_settable(L, -3);\
}while(0)

#define SET_FUNCTION(L,NAME,FUNC) do{\
	lua_pushstring(L,NAME);\
	lua_pushcfunction(L,FUNC);\
	lua_settable(L, -3);\
}while(0)

#include "timer.h"
#include "event_loop.h"
#include "buffer.h"
#include "socket.h"
#include "redis.h"
#include "packet.h"
#include "http.h"


#define REGISTER_MODULE(L,N,F) do {	\
	lua_pushstring(L,N);			\
	F(L);							\
	lua_settable(L,-3);				\
}while(0)

int32_t luaopen_chuck(lua_State *L)
{
	signal(SIGPIPE,SIG_IGN);
	lua_newtable(L);
	REGISTER_MODULE(L,"timer",register_timer);
	REGISTER_MODULE(L,"event_loop",register_event_loop);
	REGISTER_MODULE(L,"socket",register_socket);
	REGISTER_MODULE(L,"redis",register_redis);
	REGISTER_MODULE(L,"buffer",register_buffer);
	REGISTER_MODULE(L,"packet",register_packet);
	REGISTER_MODULE(L,"http",register_http);		
	
	return 1;
}