#include "util/chk_timer_define.h"

#define TIMERMGR_METATABLE "lua_timermgr"

#define TIMER_METATABLE "lua_timer"

void chk_timermgr_init(chk_timermgr *);

void chk_timermgr_finalize(chk_timermgr *);

typedef struct {
	chk_timer *timer;
}lua_timer;

static void timer_ud_cleaner(chk_ud ud) {
	chk_luaRef_release(&ud.v.lr);
}

static int32_t lua_timeout_cb(uint64_t tick,chk_ud ud) {
	chk_luaRef cb = ud.v.lr;
	const char *error; 
	lua_Integer ret;
	if(NULL != (error = chk_Lua_PCallRef(cb,":i",&ret))) {
		CHK_SYSLOG(LOG_ERROR,"error on lua_timeout_cb %s",error);
		return -1;
	}
	return (int32_t)ret;	
}

#define lua_checktimermgr(L,I)	\
	(chk_timermgr*)luaL_checkudata(L,I,TIMERMGR_METATABLE)

#define lua_checktimer(L,I)	\
	(lua_timer*)luaL_checkudata(L,I,TIMER_METATABLE)

static int32_t lua_timer_gc(lua_State *L) {
	lua_timer *luatimer = lua_checktimer(L,1);
	if(luatimer->timer) {
		chk_timer_unregister(luatimer->timer);
	}
	return 0;
}

static int32_t lua_timermgr_gc(lua_State *L) {
	chk_timermgr *timermgr = lua_checktimermgr(L,1);
	chk_timermgr_finalize(timermgr);
	return 0;
}

static int32_t lua_new_timermgr(lua_State *L) {
	if(!LUA_NEWUSERDATA(L,chk_timermgr))
		return 0;
	luaL_getmetatable(L, TIMERMGR_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

static int32_t lua_timermgr_register(lua_State *L) {
	chk_luaRef    cb;
	uint32_t      ms;
	uint64_t      tick;
	lua_timer    *luatimer;
	chk_timermgr *timermgr = lua_checktimermgr(L,1);
	ms  = (uint32_t)luaL_optinteger(L,2,1);
	if(!lua_isfunction(L,3)) 
		return luaL_error(L,"argument 3 of event_loop_addtimer must be lua function"); 
	cb = chk_toluaRef(L,3);
	
	luatimer = LUA_NEWUSERDATA(L,lua_timer);
	if(!luatimer) {
		CHK_SYSLOG(LOG_ERROR,"LUA_NEWUSERDATA() failed");
		return 0;
	}
	tick = chk_accurate_tick64();
	luatimer->timer = chk_timer_register(timermgr,ms,lua_timeout_cb,chk_ud_make_lr(cb),tick);

	if(luatimer->timer)
		chk_timer_set_ud_cleaner(luatimer->timer,timer_ud_cleaner);
	else {
		chk_luaRef_release(&cb);
		CHK_SYSLOG(LOG_ERROR,"chk_loop_addtimer() failed");
		return 0;
	}
	luaL_getmetatable(L, TIMER_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

static int32_t lua_timermgr_tick(lua_State *L) {
	chk_timermgr *timermgr = lua_checktimermgr(L,1);
	chk_timer_tick(timermgr,chk_systick64());
	return 0;
}

static int32_t lua_unregister_timer(lua_State *L) {
	lua_timer *luatimer = lua_checktimer(L,1);
	if(luatimer->timer) {
		chk_timer_unregister(luatimer->timer);
		luatimer->timer = NULL;
	}
	return 0;
}


static void register_timer(lua_State *L) {
	luaL_Reg timermgr_mt[] = {
		{"__gc", lua_timermgr_gc},
		{NULL, NULL}
	};

	luaL_Reg timermgr_methods[] = {
		{"Tick",    lua_timermgr_tick},
		{"RegTimer",lua_timermgr_register},
		{NULL,     NULL}
	};

	luaL_Reg timer_mt[] = {
		{"__gc", lua_timer_gc},
		{NULL, NULL}
	};

	luaL_Reg timer_methods[] = {
		{"UnRegister",    lua_unregister_timer},
		{NULL,     NULL}
	};	

	luaL_newmetatable(L, TIMERMGR_METATABLE);
	luaL_setfuncs(L, timermgr_mt, 0);

	luaL_newlib(L, timermgr_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	luaL_newmetatable(L, TIMER_METATABLE);
	luaL_setfuncs(L, timer_mt, 0);

	luaL_newlib(L, timer_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);	

	lua_newtable(L);
	SET_FUNCTION(L,"New",lua_new_timermgr);
}

