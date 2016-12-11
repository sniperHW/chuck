#define REDIS_METATABLE "lua_redis"

typedef struct {
	chk_redisclient *client;
}lua_redis_client;

#define lua_checkredisclient(L,I)	\
	(lua_redis_client*)luaL_checkudata(L,I,REDIS_METATABLE)

static int32_t lua_redisclient_gc(lua_State *L) {
	lua_redis_client *client = lua_checkredisclient(L,1);
	if(client->client) {
		chk_redis_close(client->client,0);
		client->client = NULL;
	}
	return 0;
}

static void lua_redis_disconnect_cb(chk_redisclient *_,void *ud,int32_t err) {
	lua_redis_client *c = (lua_redis_client*)ud;
	c->client = NULL;	
}

typedef struct {
	void (*Push)(chk_luaPushFunctor *self,lua_State *L);
	chk_redisclient *c;
}luaRedisPusher;

static void PushRedis(chk_luaPushFunctor *_,lua_State *L) {
	luaRedisPusher *self = (luaRedisPusher*)_;
	if(self->c){
		lua_redis_client *luaclient = LUA_NEWUSERDATA(L,lua_redis_client);
		if(luaclient) {
			luaclient->client = self->c;
			chk_redis_set_disconnect_cb(self->c,lua_redis_disconnect_cb,(void*)luaclient);
			luaL_getmetatable(L, REDIS_METATABLE);
			lua_setmetatable(L, -2);
		}else {
			chk_redis_close(self->c,0);   
			lua_pushnil(L);
		}		
	}
	else {
		lua_pushnil(L);
	}
}

static void lua_redis_connect_cb(chk_redisclient *c,void *ud,int32_t err) {
	chk_luaRef *cb = (chk_luaRef*)ud;
	const char *error;
	luaRedisPusher pusher;
	if(c){
		pusher.c = c;
		pusher.Push = PushRedis;
		error = chk_Lua_PCallRef(*cb,"fi",(chk_luaPushFunctor*)&pusher,err);	
	}else error = chk_Lua_PCallRef(*cb,"pi",NULL,err);
	if(error) CHK_SYSLOG(LOG_ERROR,"error on lua_redis_connect_cb %s",error);				
	POOL_RELEASE_LUAREF(cb);
	//chk_luaRef_release(cb);
	//free(cb);
}

static int32_t lua_redis_close(lua_State *L) {
	lua_redis_client *c = lua_checkredisclient(L,1);
	int32_t err = (int32_t)luaL_optinteger(L,2,0);
	chk_redis_close(c->client,err);
	return 0;
}

static int32_t lua_redis_connect_ip4(lua_State *L) {
	chk_event_loop *event_loop;
	chk_luaRef     *cb;
	chk_sockaddr    server;
	const char     *ip;
	int16_t         port;
	if(!lua_isfunction(L,4)) 
		return luaL_error(L,"argument 4 of redis_connect_ip4 must be lua function");
	event_loop = lua_checkeventloop(L,1);
	ip = luaL_checkstring(L,2);
	port = (int16_t)luaL_checkinteger(L,3);
	
	if(0 != easy_sockaddr_ip4(&server,ip,port)) {
		lua_pushstring(L,"redis_connect_ip4 invaild address or port");
		return 1;
	}

	cb = POOL_NEW_LUAREF();

	if(!cb) {
		lua_pushstring(L,"new luaref failed");
		return 1;
	}

	*cb = chk_toluaRef(L,4);
	if(0 != chk_redis_connect(event_loop,&server,lua_redis_connect_cb,cb)){
		POOL_RELEASE_LUAREF(cb);
		lua_pushstring(L,"redis_connect_ip4 connect error");
		return 1;
	}
	return 0;
}

static inline void build_resultset(redisReply* reply,lua_State *L){
	int32_t i;
	if(reply->type == REDIS_REPLY_INTEGER){
		lua_pushinteger(L,reply->integer);
	}else if(reply->type == REDIS_REPLY_STRING ||
			 reply->type ==	REDIS_REPLY_STATUS){
		lua_pushstring(L,reply->str);
	}else if(reply->type == REDIS_REPLY_ARRAY){
		lua_newtable(L);
		for(i = 0; i < reply->elements; ++i){
			build_resultset(reply->element[i],L);
			lua_rawseti(L,-2,i+1);
		}
	}else{
		if(reply->type == REDIS_REPLY_ERROR)
			CHK_SYSLOG(LOG_ERROR,"REDIS_REPLY_ERROR [%s]",reply->str);
		lua_pushnil(L);
	}
}

typedef struct {
	void (*Push)(chk_luaPushFunctor *self,lua_State *L);
	redisReply    *reply;
}ReplyPusher;

static void PushReply(chk_luaPushFunctor *_,lua_State *L) {
	ReplyPusher *self = (ReplyPusher*)_;
	build_resultset(self->reply,L);
}

void lua_redis_reply_cb(chk_redisclient *_,redisReply *reply,void *ud) {
	chk_luaRef *cb = (chk_luaRef*)ud;
	const char *error = NULL;
	const char *redis_err_str = NULL;
	ReplyPusher pusher;
	if(!cb) return;
	if(reply){
		pusher.reply = reply;
		pusher.Push = PushReply;
		if(reply->type == REDIS_REPLY_ERROR) {
			redis_err_str = reply->str;
		}
		error = chk_Lua_PCallRef(*cb,"fs",(chk_luaPushFunctor*)&pusher,redis_err_str);	
	}
	else{
		error = chk_Lua_PCallRef(*cb,"p",NULL);
	}
	
	if(error) CHK_SYSLOG(LOG_ERROR,"error on redis_reply_cb %s",error);	
	POOL_RELEASE_LUAREF(cb);
}

static int32_t lua_redis_execute(lua_State *L) {
	chk_luaRef *cb = NULL;
	lua_redis_client *client = lua_checkredisclient(L,1);
	const char *cmd = lua_tostring(L,3); 

	if(!client) {
		lua_pushstring(L,"redis_client == NULL");		
		return 1;
	}

	if(!cmd) {
		lua_pushstring(L,"cmd == NULL");		
		return 1;		
	}

	if(lua_isfunction(L,2)) {
		cb = POOL_NEW_LUAREF();
		if(!cb) {
			lua_pushstring(L,"new luaref failed");
			return 1;
		}
		*cb = chk_toluaRef(L,2);
	}

	int32_t param_size = lua_gettop(L) - 3;
	
	if(0 != chk_redis_execute_lua(client->client,cmd,lua_redis_reply_cb,(void*)cb,L,4,param_size)) {
		POOL_RELEASE_LUAREF(cb);
		lua_pushstring(L,"redis_execute error");
		return 1;
	}

	return 0;
}

static void register_redis(lua_State *L) {
	luaL_Reg redis_mt[] = {
		{"__gc", lua_redisclient_gc},
		{NULL, NULL}
	};

	luaL_Reg redis_methods[] = {
		{"Execute",          lua_redis_execute},
		{"Close",            lua_redis_close},
		{NULL,     NULL}
	};

	luaL_newmetatable(L, REDIS_METATABLE);
	luaL_setfuncs(L, redis_mt, 0);

	luaL_newlib(L, redis_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);


	lua_newtable(L);
	SET_FUNCTION(L,"Connect_ip4",lua_redis_connect_ip4);
}
