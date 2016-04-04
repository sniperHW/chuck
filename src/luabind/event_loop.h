#include "event/chk_event_loop_define.h"

int32_t chk_loop_init(chk_event_loop *loop);

void chk_loop_finalize(chk_event_loop *loop);

#define EVENT_LOOP_METATABLE "lua_event_loop"

#define lua_checkeventloop(L,I)	\
	(chk_event_loop*)luaL_checkudata(L,I,EVENT_LOOP_METATABLE)

static int32_t lua_event_loop_gc(lua_State *L) {
	chk_event_loop *event_loop = lua_checkeventloop(L,1);
	chk_loop_finalize(event_loop);
	return 0;
}

static int32_t lua_new_event_loop(lua_State *L) {
	chk_event_loop tmp,*event_loop;
	if(0 != chk_loop_init(&tmp)) return 0;
	event_loop = (chk_event_loop*)lua_newuserdata(L, sizeof(*event_loop));
	if(!event_loop) {
		chk_loop_finalize(&tmp);
		return 0;
	}
	*event_loop = tmp;
	luaL_getmetatable(L, EVENT_LOOP_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

static int32_t lua_event_loop_run(lua_State *L) {
	chk_event_loop *event_loop;
	int32_t         ms,ret;
	event_loop = lua_checkeventloop(L,1);
	ms = (int32_t)luaL_optinteger(L,2,-1);
	if(ms == -1) ret = chk_loop_run(event_loop);
	else ret = chk_loop_run_once(event_loop,ms);
	if(ret != 0) {
		lua_pushinteger(L,ret);
		return 1;
	}
	return 0;
}

static int32_t lua_event_loop_end(lua_State *L) {
	chk_event_loop *event_loop = lua_checkeventloop(L,1);
	chk_loop_end(event_loop);
	return 0;
}

static int32_t lua_event_loop_addtimer(lua_State *L) {
	uint32_t       	ms;
	lua_timer      *luatimer;
	chk_luaRef      cb;
	chk_event_loop *event_loop = lua_checkeventloop(L,1);
	ms = (uint32_t)luaL_optinteger(L,2,1);
	if(!lua_isfunction(L,3)) 
		return luaL_error(L,"argument 3 of event_loop_addtimer must be lua function"); 
	cb = chk_toluaRef(L,3);
	luatimer = (lua_timer*)lua_newuserdata(L, sizeof(*luatimer));
	luatimer->cb = cb;
	luatimer->timer = chk_loop_addtimer(event_loop,ms,lua_timeout_cb,luatimer);
	if(luatimer->timer) chk_timer_set_ud_cleaner(luatimer->timer,timer_ud_cleaner);
	else chk_luaRef_release(&luatimer->cb);
	luaL_getmetatable(L, TIMER_METATABLE);
	lua_setmetatable(L, -2);	
	return 1;
}

static void register_event_loop(lua_State *L) {
	luaL_Reg event_loop_mt[] = {
		{"__gc", lua_event_loop_gc},
		{NULL, NULL}
	};

	luaL_Reg event_loop_methods[] = {
		{"Run",    lua_event_loop_run},
		{"End",    lua_event_loop_end},
		{"RegTimer",lua_event_loop_addtimer},
		{NULL,     NULL}
	};

	luaL_newmetatable(L, EVENT_LOOP_METATABLE);
	luaL_setfuncs(L, event_loop_mt, 0);

	luaL_newlib(L, event_loop_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	lua_newtable(L);
	SET_FUNCTION(L,"New",lua_new_event_loop);
}







