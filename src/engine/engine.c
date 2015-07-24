#define __CORE__
#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <unistd.h>
#include <assert.h>
#include "engine/engine.h"
#include "util/log.h"
#include "util/time.h"
#include "thread/thread.h"

#ifdef _CHUCKLUA
#define LUAENGINE_METATABLE "luaengine_metatable"
static void engine_del_lua(engine *e);
#endif

extern int32_t pipe2(int pipefd[2], int flags);

enum{
	INLOOP  =  1 << 1,
	CLOSING =  1 << 2,
};

int32_t engine_add(engine *e,handle *h,generic_callback callback){
	return h->imp_engine_add(e,h,callback);
}

int32_t engine_remove(handle *h){
	if(!h->e) return -ENOASSENG;
	return event_remove(h);
}


#ifdef _LINUX

#include "engine_imp_epoll.h"

#elif _BSD

#include "engine_imp_kqueue.h"

#else

#error "un support platform!"		

#endif

int32_t engine_runonce(engine *e,int32_t timeout)
{
	return _engine_run(e,timeout);
}

int32_t engine_run(engine *e)
{
	return _engine_run(e,-1);
}

void engine_stop(engine *e)
{
	int32_t _;
	TEMP_FAILURE_RETRY(write(e->notifyfds[1],&_,sizeof(_)));
}

timer *engine_regtimer(
		engine *e,uint32_t timeout,
		int32_t(*cb)(uint32_t,uint64_t,void*),
		void *ud)
{
	if(!e->timermgr) engine_init_timer(e);
	return wheelmgr_register(e->timermgr,timeout,cb,ud,systick64());
}


#ifdef _CHUCKLUA

static int32_t lua_engine_new(lua_State *L)
{
	engine *ep = cast(engine*,lua_newuserdata(L, sizeof(*ep)));
	memset(ep,0,sizeof(*ep));
	if(0 != engine_init(ep)){
		free(ep);
		lua_pushnil(L);
		return 1;
	}	
	luaL_getmetatable(L, LUAENGINE_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}


static void engine_del_lua(engine *e)
{
	assert(e->threadid == thread_id());
	if(e->status & INLOOP)
		e->status |= CLOSING;
	else{	
		_engine_del(e);
	}
}


engine *lua_toengine(lua_State *L, int index){
	return cast(engine*,luaL_testudata(L, index, LUAENGINE_METATABLE));
}

static int32_t lua_engine_gc(lua_State *L){
	engine *e = lua_toengine(L,1);
	engine_del_lua(e);
	return 0;
}

static int32_t lua_engine_run(lua_State *L){
	engine *e    = lua_toengine(L,1);
	if(lua_isnumber(L,2)){
		uint32_t ms = cast(uint32_t,lua_tointeger(L,2));
		lua_pushinteger(L,engine_runonce(e,ms));
	}
	else
		lua_pushinteger(L,engine_run(e));
	return 1;
}


static int32_t lua_engine_stop(lua_State *L){
	engine *e = lua_toengine(L,1);
	engine_stop(e);
	return 0;
}

void lua_timer_new(lua_State *L,wheelmgr *m,uint32_t timeout,luaRef *cb);

static int32_t lua_engine_reg_timer(lua_State *L){
	engine    *e 	  = lua_toengine(L,1);
	uint32_t  timeout = cast(uint32_t,lua_tointeger(L,2));
	luaRef    cb      = toluaRef(L,3);
	timeout = timeout > MAX_TIMEOUT ? MAX_TIMEOUT : timeout;
	if(!e->timermgr) engine_init_timer(e);	
	lua_timer_new(L,e->timermgr,timeout,&cb);
	return 1; 
}

/*static int32_t lua_timer_callback(uint32_t v,uint64_t _,void *ud){
	luaRef *cb      =  cast(luaRef*,ud);
	lua_Integer ret =  -1;
	const char *error; 
	if(v == TEVENT_TIMEOUT){
		if((error = LuaCallRefFunc(*cb,":i",&ret))){
			SYS_LOG(LOG_ERROR,"error on lua_timer_callback:%s\n",error);	
		}
	}
	if(ret == -1){
		release_luaRef(cb);
		free(cb);
	}
	return cast(int32_t,ret);
}

static int32_t lua_engine_reg_timer(lua_State *L){
	engine  *e = lua_toengine(L,1);
	luaRef  *cb;
	timer   *t;
	uint32_t timeout;
	if(LUA_TNUMBER != lua_type(L,2))
		return luaL_error(L,"arg2 should be number");
	if(LUA_TFUNCTION != lua_type(L,3))
		return luaL_error(L,"arg3 should be function");
	timeout = lua_tonumber(L,2);
	if(timeout > MAX_TIMEOUT) timeout = MAX_TIMEOUT;
	cb = calloc(1,sizeof(*cb));
	*cb = toluaRef(L,3);
	t = engine_regtimer(e,timeout,lua_timer_callback,cb);
	if(!t){
		release_luaRef(cb);
		free(cb);		
		lua_pushnil(L);
	}else{
		lua_pushlightuserdata(L,t);
	}
	return 1;
}

static int32_t lua_remove_timer(lua_State *L){
	timer *t = lua_touserdata(L,1);
	unregister_timer(t);
	return 0;
}*/

#define SET_FUNCTION(L,NAME,FUNC) do{\
	lua_pushstring(L,NAME);\
	lua_pushcfunction(L,FUNC);\
	lua_settable(L, -3);\
}while(0)


void reg_luaengine(lua_State *L){
	luaL_Reg engine_mt[] = {
		{"__gc", lua_engine_gc},
		{NULL, NULL}
	};

	luaL_Reg engine_methods[] = {
		{"Run",    lua_engine_run},
		{"Stop",   lua_engine_stop},
		{NULL,     NULL}
	};

	luaL_newmetatable(L, LUAENGINE_METATABLE);
	luaL_setfuncs(L, engine_mt, 0);

	luaL_newlib(L, engine_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	SET_FUNCTION(L,"engine",lua_engine_new);
	SET_FUNCTION(L,"RegTimer",lua_engine_reg_timer);
	//SET_FUNCTION(L,"RemTimer",lua_remove_timer);
}

#else

engine *engine_new()
{
	engine *ep = calloc(1,sizeof(*ep));
	if(0 != engine_init(ep)){
		free(ep);
		ep = NULL;
	}
	return ep;
}

void engine_del(engine *e)
{
	assert(e->threadid == thread_id());
	if(e->status & INLOOP)
		e->status |= CLOSING;
	else{
		_engine_del(e);
		free(e);
	}
}

#endif