typedef struct lua_timer lua_timer;

typedef struct lua_timermgr lua_timermgr;

static void timer_ud_cleaner(void *ud) {
	chk_luaRef *cb = (chk_luaRef*)ud;
	chk_luaRef_release(cb);
	free(cb);
}