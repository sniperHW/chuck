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

/*
chk_timer      *chk_loop_addtimer(chk_event_loop*,uint32_t timeout,chk_timeout_cb,void *ud);

int32_t         chk_loop_add_handle(chk_event_loop*,chk_handle*,chk_event_callback);
*/

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
	lua_event_loop *event_loop;
	int32_t         ms,ret;
}




