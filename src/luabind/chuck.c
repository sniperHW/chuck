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

#define LUA_NEWUSERDATA(L,TYPE)   					({\
	TYPE *ret = lua_newuserdata(L, sizeof(TYPE));	  \
	if(ret) memset(ret,0,sizeof(TYPE));				  \
	ret;})

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

void register_signum(lua_State *L) {
	lua_newtable(L);
	SET_CONST(L,SIGINT);
	SET_CONST(L,SIGHUP);
	SET_CONST(L,SIGQUIT);
	SET_CONST(L,SIGILL);
	SET_CONST(L,SIGTRAP);
	SET_CONST(L,SIGABRT);
	SET_CONST(L,SIGBUS);
	SET_CONST(L,SIGFPE);
	SET_CONST(L,SIGKILL);
	SET_CONST(L,SIGUSR1);
	SET_CONST(L,SIGSEGV);
	SET_CONST(L,SIGUSR2);
	SET_CONST(L,SIGPIPE);
	SET_CONST(L,SIGALRM);
	SET_CONST(L,SIGTERM);
	SET_CONST(L,SIGCHLD);
	SET_CONST(L,SIGCONT);
	SET_CONST(L,SIGSTOP);
	SET_CONST(L,SIGTSTP);
	SET_CONST(L,SIGTTIN);
	SET_CONST(L,SIGTTOU);
	SET_CONST(L,SIGURG);
	SET_CONST(L,SIGXCPU);
	SET_CONST(L,SIGXFSZ);
	SET_CONST(L,SIGVTALRM);
	SET_CONST(L,SIGPOLL);
	SET_CONST(L,SIGTTIN);
	SET_CONST(L,SIGSYS);
	SET_CONST(L,SIGSTKFLT);
	SET_CONST(L,SIGWINCH);
	SET_CONST(L,SIGPWR);
	SET_CONST(L,SIGRTMIN-SIGRTMAX);
	SET_CONST(L,SIGIOT);
	SET_CONST(L,SIGIO);
	SET_CONST(L,SIGUNUSED);
}

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
	REGISTER_MODULE(L,"signal",register_signum);	
	return 1;
}