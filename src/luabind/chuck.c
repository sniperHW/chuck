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

const char *stack_value_tostr(lua_State *L,int index) {
	static char buff[1024];
	int type = lua_type(L,index);
	switch(type) {
		case LUA_TNIL:{
			snprintf(buff,1023,"nil");
		}break;
		case LUA_TUSERDATA:{
			snprintf(buff,1023,"userdata:%p",lua_touserdata(L,index));
		}break;
		case LUA_TLIGHTUSERDATA:{
			snprintf(buff,1023,"lightuserdata:%p",lua_touserdata(L,index));
		}break;
		case LUA_TNUMBER:{
			lua_Number v = lua_tonumber(L,index);
			if(v != (lua_Integer)v){
				snprintf(buff,1023,"%f",v);
			} else {
				snprintf(buff,1023,"%lld",lua_tointeger(L,index));
			}
		}break;
		case LUA_TBOOLEAN:{
			lua_Integer b = lua_tointeger(L,index);
			snprintf(buff,1023,"%s",b == 1 ? "true":"false");
		}break;
		case LUA_TTABLE:{
			snprintf(buff,1023,"table:%p",lua_topointer(L,index));
		}break;
		case LUA_TTHREAD:{
			snprintf(buff,1023,"thread");
		}break;
		case LUA_TFUNCTION:{
			snprintf(buff,1023,"function");
		}break;
		case LUA_TSTRING:{
			snprintf(buff,1023,"%s",lua_tostring(L,index));
		}
		break;
		default:{
			snprintf(buff,1023,"unknow");
		}
	}

	buff[1023] = 0;
	return buff;
}

void show_stack(lua_State *L) {
	int top = lua_gettop(L);
	int i;
	for(i = top;i > 0; --i) {
		if(i == top) {
			printf("(stack index:%d) %s <- top\n",i,stack_value_tostr(L,i));
		} else {
			printf("(stack index:%d) %s\n",i,stack_value_tostr(L,i));			
		}
	}
}

#include "timer.h"
#include "event_loop.h"
#include "buffer.h"
#include "socket.h"
#include "redis.h"
#include "packet.h"
#include "log.h"
#include "ssl.h"
#include "time.h"
#include "base64.h"
#include "crypt.h"
#include "coroutine.h"


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
	SET_CONST(L,SIGTTIN);
	SET_CONST(L,SIGSYS);
	SET_CONST(L,SIGWINCH);
	SET_CONST(L,SIGIOT);
	SET_CONST(L,SIGIO);
}

int32_t luaopen_chuck(lua_State *L)
{
	signal(SIGPIPE,SIG_IGN);
	
	register_timer(L);
	lua_newtable(L);
	//REGISTER_MODULE(L,"timer",register_timer);
	REGISTER_MODULE(L,"event_loop",register_event_loop);
	REGISTER_MODULE(L,"socket",register_socket);
	REGISTER_MODULE(L,"redis",register_redis);
	REGISTER_MODULE(L,"buffer",register_buffer);
	REGISTER_MODULE(L,"packet",register_packet);		
	REGISTER_MODULE(L,"signal",register_signum);
	REGISTER_MODULE(L,"log",register_log);
	REGISTER_MODULE(L,"ssl",register_ssl);
	REGISTER_MODULE(L,"time",register_time);	
	REGISTER_MODULE(L,"base64",register_base64);
	REGISTER_MODULE(L,"crypt",register_crypt);
	REGISTER_MODULE(L,"coroutine",register_coroutine);
	return 1;
}