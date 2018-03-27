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
	chk_event_loop *event_loop;
	event_loop = LUA_NEWUSERDATA(L,chk_event_loop);
	if(!event_loop) {
		CHK_SYSLOG(LOG_ERROR,"LUA_NEWUSERDATA() failed");
		return 0;
	}
	if(chk_error_ok != chk_loop_init(event_loop)){
		CHK_SYSLOG(LOG_ERROR,"chk_loop_init() failed");
		return 0;
	}
	luaL_getmetatable(L, EVENT_LOOP_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

static int32_t lua_event_loop_run(lua_State *L) {
	chk_event_loop *event_loop;
	int32_t         ms,ret;
	event_loop = lua_checkeventloop(L,1);
	ms = (int32_t)luaL_optinteger(L,2,-1);
	if(ms == -1){
		ret = chk_loop_run(event_loop);
	}
	else { 
		ret = chk_loop_run_once(event_loop,ms);
	}

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


static int32_t _register_timer(lua_State *L,int type) {
	uint32_t       	ms;
	lua_timer      *luatimer;
	chk_event_loop *event_loop = lua_checkeventloop(L,1);
	ms = (uint32_t)luaL_optinteger(L,2,1);
	if(!lua_isfunction(L,3)) {
		CHK_SYSLOG(LOG_ERROR,"argument 3 of event_loop_addtimer must be lua function");		
		return luaL_error(L,"argument 3 of event_loop_addtimer must be lua function"); 
	}

	luatimer = LUA_NEWUSERDATA(L,lua_timer);
	if(!luatimer) {
		CHK_SYSLOG(LOG_ERROR,"LUA_NEWUSERDATA() failed");
		return 0;
	}
	if(NULL != (luatimer->timer = chk_loop_addtimer(event_loop,ms,lua_timeout_cb,chk_ud_make_void((void*)luatimer)))){
		luatimer->type = type;
		luatimer->cb = chk_toluaRef(L,3);
		luatimer->self = chk_toluaRef(L,-1);
		chk_timer_set_ud_cleaner(luatimer->timer,timer_ud_cleaner);
	} else {
		CHK_SYSLOG(LOG_ERROR,"chk_loop_addtimer() failed");
		return 0;
	}
	luaL_getmetatable(L, TIMER_METATABLE);
	lua_setmetatable(L, -2);	
	return 1;
}

static int32_t lua_event_loop_addtimer(lua_State *L) {
	return _register_timer(L,timer_repeat);
}

static int32_t lua_event_loop_oncetimer(lua_State *L) {
	return _register_timer(L,timer_once);
}

static int32_t lua_event_loop_set_idle(lua_State *L) {
	chk_luaRef      cb = {NULL,-1};
	chk_event_loop *event_loop = lua_checkeventloop(L,1);
	if(!lua_isfunction(L,2)) {
		CHK_SYSLOG(LOG_ERROR,"argument 2 of lua_event_loop_set_idle must be lua function");		
		return luaL_error(L,"argument 2 of lua_event_loop_set_idle must be lua function"); 
	}
	cb = chk_toluaRef(L,2);
	if(0 != chk_loop_set_idle_func_lua(event_loop,cb)) {
		lua_pushstring(L,"set_idle failed");
		return 1;
	}

	return 0;
}

static void signal_ud_dctor(chk_ud ud) {
	chk_luaRef_release(&ud.v.lr);
}

static void signal_callback(chk_ud ud) {
	chk_luaRef cb = ud.v.lr;
	const char   *error; 
	if(NULL != (error = chk_Lua_PCallRef(cb,":"))) {
		CHK_SYSLOG(LOG_ERROR,"error on signal_cb %s",error);
	}	
}

static int32_t lua_watch_signal(lua_State *L) {
	chk_event_loop *event_loop = lua_checkeventloop(L,1);
	int32_t signo = (int32_t)luaL_checkinteger(L,2);
	chk_luaRef cb = {0};

	if(!lua_isfunction(L,3))
		return luaL_error(L,"argument 3 must be lua function");
	cb = chk_toluaRef(L,3);
	if(chk_error_ok != chk_watch_signal(event_loop,signo,signal_callback,chk_ud_make_lr(cb),signal_ud_dctor)) {
		chk_luaRef_release(&cb);
		printf("call chk_watch_signal failed\n");
		return 0;
	}

	return 0; 
}

static int32_t lua_unwatch_signal(lua_State *L) {
	int32_t signo = (int32_t)luaL_checkinteger(L,2);
	chk_unwatch_signal(signo);	
	return 0;
}

static void call_closure(chk_ud closure) {
	const char   *error; 
	if(NULL != (error = chk_Lua_PCallRef(closure.v.lr,":"))) {
		CHK_SYSLOG(LOG_ERROR,"error on call_closure %s",error);
	}
}

static int32_t lua_post_closure(lua_State *L) {
	chk_event_loop *event_loop = lua_checkeventloop(L,1);
	if(!lua_isfunction(L,2)) {
		CHK_SYSLOG(LOG_ERROR,"argument 2 of lua_post_closure must be lua closure");		
		return luaL_error(L,"argument 2 of lua_post_closure must be lua closure"); 
	}
	chk_luaRef closure = chk_toluaRef(L,2);	
	return  chk_loop_post_closure(event_loop,call_closure,chk_ud_make_lr(closure));	
}

static void register_event_loop(lua_State *L) {
	luaL_Reg event_loop_mt[] = {
		{"__gc", lua_event_loop_gc},
		{NULL, NULL}
	};

	luaL_Reg event_loop_methods[] = {
		{"PostClosure",  lua_post_closure},
		{"WatchSignal",  lua_watch_signal},
		{"UnWatchSignal",lua_unwatch_signal},
		{"Run",    	     lua_event_loop_run},
		{"Stop",         lua_event_loop_end},
		{"AddTimer",     lua_event_loop_addtimer},
		{"AddTimerOnce", lua_event_loop_oncetimer},
		{"SetIdle",      lua_event_loop_set_idle},
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







