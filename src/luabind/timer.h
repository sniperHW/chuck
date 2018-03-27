#include "util/chk_timer_define.h"

#define TIMER_METATABLE "lua_timer"

enum {
	timer_once   = 1,
	timer_repeat = 2,
};

typedef struct {
	chk_timer *timer;
	chk_luaRef cb;
	chk_luaRef self;
	int        type;
}lua_timer;

static void timer_ud_cleaner(chk_ud *ud) {
	lua_timer *luatimer = (lua_timer*)ud->v.val;
	luatimer->timer = NULL;
	chk_luaRef_release(&luatimer->cb);
	chk_luaRef_release(&luatimer->self);
}

static int32_t lua_timeout_cb(uint64_t tick,chk_ud ud) {
	lua_timer *luatimer = (lua_timer*)ud.v.val;
	const char *error;
	lua_Integer ret;
	if(NULL != (error = chk_Lua_PCallRef(luatimer->cb,":i",&ret))) {
		CHK_SYSLOG(LOG_ERROR,"error on lua_timeout_cb %s",error);
		return -1;
	}
	if(luatimer->type == timer_repeat) {
		return ret;
	}else {
		return -1;
	}
}


#define lua_checktimer(L,I)	\
	(lua_timer*)luaL_checkudata(L,I,TIMER_METATABLE)

static int32_t lua_timer_gc(lua_State *L) {
	lua_timer *luatimer = lua_checktimer(L,1);
	if(luatimer->timer) {
		chk_timer *t = luatimer->timer;
		luatimer->timer = NULL;
		chk_timer_unregister(t);
	}
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
	luaL_Reg timer_mt[] = {
		{"__gc", lua_timer_gc},
		{NULL, NULL}
	};

	luaL_Reg timer_methods[] = {
		{"UnRegister",    lua_unregister_timer},
		{NULL,     NULL}
	};	

	luaL_newmetatable(L, TIMER_METATABLE);
	luaL_setfuncs(L, timer_mt, 0);

	luaL_newlib(L, timer_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);	
}

