

int32_t lua_systick(lua_State *L) {
	lua_pushinteger(L,chk_systick());
	return 1;
}

int32_t lua_unixtime(lua_State *L) {
	lua_pushinteger(L,time(NULL));
	return 1;
}

static void register_time(lua_State *L) {
	lua_newtable(L);
	SET_FUNCTION(L,"systick",lua_systick);
	SET_FUNCTION(L,"unixtime",lua_unixtime);
}