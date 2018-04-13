#define LOGFILE_METATABLE "lua_logfile"

typedef struct {
	chk_logfile *log;
}lua_log_file;

#define lua_checklogfile(L,I)	\
	(lua_log_file*)luaL_checkudata(L,I,LOGFILE_METATABLE)


static int32_t lua_create_logfile(lua_State *L) {
	const char *filename = luaL_checkstring(L,1);

	if(!filename) {
		CHK_SYSLOG(LOG_ERROR,"NULL == filename");		
		return luaL_error(L,"arg 1 of create_logfile is not a string");		
	}

	lua_log_file *lua_log = LUA_NEWUSERDATA(L,lua_log_file);

	if(!lua_log) {
		CHK_SYSLOG(LOG_ERROR,"newuserdata() failed");
		return 0;
	}

	lua_log->log = chk_create_logfile(filename);
	
	if(!lua_log->log) { 
		CHK_SYSLOG(LOG_ERROR,"chk_create_logfile() failed");		
		return 0;
	}

	luaL_setmetatable(L, LOGFILE_METATABLE);	
	return 1;	
}

static int32_t lua_logfile_gc(lua_State *L) {
	lua_log_file *logfile = lua_checklogfile(L,1);
	logfile->log = NULL;
	return 0;
}

static int32_t lua_write_log(lua_State *L) {
	int32_t size;
	char *buff;
	const char *str;	
	lua_log_file *logfile = lua_checklogfile(L,1);

	if(!logfile->log) {
		return 0;
	}

	int32_t loglev = luaL_checkinteger(L,2);
	if(loglev >= chk_current_loglev()) {
		str = luaL_checkstring(L,3);
		buff = calloc(CHK_MAX_LOG_SIZE,sizeof(char));

		if(!buff) {
			CHK_SYSLOG(LOG_ERROR,"calloc(CHK_MAX_LOG_SIZE) failed");			
			return 0;
		}

        size = chk_log_prefix(buff,loglev);
        snprintf(&buff[size],CHK_MAX_LOG_SIZE-size-1,"%s",str);	
		chk_log(logfile->log,loglev,buff);
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
		buff = calloc(CHK_MAX_LOG_SIZE,sizeof(char));
		
		if(!buff) {
			CHK_SYSLOG(LOG_ERROR,"calloc(CHK_MAX_LOG_SIZE) failed");			
			return 0;
		}

        size = chk_log_prefix(buff,loglev);
        snprintf(&buff[size],CHK_MAX_LOG_SIZE-size-1,"%s",str);	
		chk_syslog(loglev,buff);
	}	
	return 0;	
}

static int32_t lua_set_syslog_file_prefix(lua_State *L) {
	const char *prefix = luaL_checkstring(L,1);
	chk_set_syslog_file_prefix(prefix);
	return 0;
}

static int32_t lua_set_log_dir(lua_State *L) {
	const char *dir = luaL_checkstring(L,1);
	chk_set_log_dir(dir);
	return 0;
}

static int32_t lua_set_log_level(lua_State *L) {
	int32_t loglev = luaL_checkinteger(L,1);
	if(loglev >= LOG_TRACE && loglev <= LOG_CRITICAL) {
		chk_set_loglev(loglev);
		lua_pushboolean(L,1);
	} else {
		lua_pushboolean(L,0);
	}
	return 1;	
}

static int32_t lua_get_log_level(lua_State *L) {
	lua_pushinteger(L,chk_current_loglev());
	return 1;	
}

static void register_log(lua_State *L) {

	luaL_Reg logfile_mt[] = {
		{"__gc", lua_logfile_gc},
		{NULL, NULL}
	};

	luaL_Reg logfile_methods[] = {
		{"Log",    lua_write_log},
		{NULL,     NULL}
	};	

	luaL_newmetatable(L, LOGFILE_METATABLE);
	luaL_setfuncs(L, logfile_mt, 0);

	luaL_newlib(L, logfile_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	lua_newtable(L);
	SET_FUNCTION(L,"CreateLogfile",lua_create_logfile);
	SET_FUNCTION(L,"SysLog",lua_sys_log);
	SET_FUNCTION(L,"SetSysLogPrefix",lua_set_syslog_file_prefix);
	SET_FUNCTION(L,"SetLogDir",lua_set_log_dir);
	SET_FUNCTION(L,"SetLogLevel",lua_set_log_level);
	SET_FUNCTION(L,"GetLogLevel",lua_get_log_level);	
	lua_pushstring(L,"trace");lua_pushinteger(L,LOG_TRACE);lua_settable(L, -3);		
	lua_pushstring(L,"info");lua_pushinteger(L,LOG_INFO);lua_settable(L, -3);
	lua_pushstring(L,"debug");lua_pushinteger(L,LOG_DEBUG);lua_settable(L, -3);
	lua_pushstring(L,"warning");lua_pushinteger(L,LOG_WARN);lua_settable(L, -3);
	lua_pushstring(L,"error");lua_pushinteger(L,LOG_ERROR);lua_settable(L, -3);
	lua_pushstring(L,"critical");lua_pushinteger(L,LOG_CRITICAL);lua_settable(L, -3);			

}	