#define LOGFILE_METATABLE "lua_logfile"

#define lua_checklogfile(L,I)	\
	(chk_logfile*)luaL_checkudata(L,I,LOGFILE_METATABLE)


static int32_t lua_create_logfile(lua_State *L) {
	const char *filename = luaL_checkstring(L,1);
	chk_logfile *logfile = chk_create_logfile(filename);
	lua_pushlightuserdata(L,logfile);
	luaL_getmetatable(L, LOGFILE_METATABLE);
	lua_setmetatable(L, -2);	
	return 1;	
}

static int32_t lua_write_log(lua_State *L) {
	int32_t size;
	char *buff;
	const char *str;	
	chk_logfile *logfile = lua_checklogfile(L,1);
	int32_t loglev = luaL_checkinteger(L,2);
	if(loglev >= chk_current_loglev()) {
		str = luaL_checkstring(L,3);
		buff = malloc(CHK_MAX_LOG_SIZE);
        size = chk_log_prefix(buff,loglev);
        snprintf(&buff[size],CHK_MAX_LOG_SIZE-size,"%s",str);	
		chk_log(logfile,loglev,buff);
	}
	return 0;
}

static int32_t lua_sys_log(lua_State *L) {
	int32_t size;
	char *buff;
	const char *str;		
	int32_t loglev = luaL_checkinteger(L,1);
	if(loglev >= chk_current_loglev()) {
		str = luaL_checkstring(L,2);
		buff = malloc(CHK_MAX_LOG_SIZE);
        size = chk_log_prefix(buff,loglev);
        snprintf(&buff[size],CHK_MAX_LOG_SIZE-size,"%s",str);	
		chk_syslog(loglev,buff);
	}	
	return 0;	
}

static void register_log(lua_State *L) {
	luaL_Reg logfile_methods[] = {
		{"Log",    lua_write_log},
		{NULL,     NULL}
	};	

	luaL_newmetatable(L, LOGFILE_METATABLE);

	luaL_newlib(L, logfile_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	lua_newtable(L);
	SET_FUNCTION(L,"CreateLogfile",lua_create_logfile);
	SET_FUNCTION(L,"SysLog",lua_sys_log);
	lua_pushstring(L,"info");lua_pushinteger(L,LOG_INFO);lua_settable(L, -3);
	lua_pushstring(L,"debug");lua_pushinteger(L,LOG_DEBUG);lua_settable(L, -3);
	lua_pushstring(L,"warning");lua_pushinteger(L,LOG_WARN);lua_settable(L, -3);
	lua_pushstring(L,"error");lua_pushinteger(L,LOG_ERROR);lua_settable(L, -3);
	lua_pushstring(L,"critical");lua_pushinteger(L,LOG_CRITICAL);lua_settable(L, -3);			

}	