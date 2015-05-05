#include "engine/engine.h"
#include "util/log.h"

#define LUAENGINE_METATABLE "luaengine_metatable"


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


engine *lua_toengine(lua_State *L, int index){
	return (engine*)luaL_testudata(L, index, LUAENGINE_METATABLE);
}

static int32_t lua_engine_gc(lua_State *L){
	engine *e = lua_toengine(L,1);
	engine_del(e);
	return 0;
}

static int32_t lua_engine_run(lua_State *L){
	engine *e = lua_toengine(L,1);
	engine_run(e);
	return 0;
}


static int32_t lua_engine_stop(lua_State *L){
	engine *e = lua_toengine(L,1);
	engine_stop(e);
	return 0;
}

static int32_t lua_timer_callback(uint32_t v,uint64_t _,void *ud){
	luaRef *cb = (luaRef*)ud;
	int32_t ret = -1;
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
	return ret;
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
}

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
   	SET_FUNCTION(L,"RemTimer",lua_remove_timer);

}