#include "engine/engine.h"

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

static int32_t lua_engine_del(lua_State *L){
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

#define SET_FUNCTION(L,NAME,FUNC) do{\
	lua_pushstring(L,NAME);\
	lua_pushcfunction(L,FUNC);\
	lua_settable(L, -3);\
}while(0)


void reg_luaengine(lua_State *L){
    luaL_Reg engine_mt[] = {
        {"__gc", lua_engine_del},
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

}