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
	chk_luaRef *cb = (chk_luaRef*)ud;
	const char *error;
	if(NULL != (error = chk_Lua_PCallRef(*cb,"i",err)))
		CHK_SYSLOG(LOG_ERROR,"error on redis_disconnect_cb %s",error);
	chk_luaRef_release(cb);
	free(cb);	
}

typedef struct {
	void (*Push)(chk_luaPushFunctor *self,lua_State *L);
	chk_redisclient *c;
}luaRedisPusher;

static void PushRedis(chk_luaPushFunctor *_,lua_State *L) {
	luaRedisPusher *self = (luaRedisPusher*)_;
	lua_redis_client *luaclient = (lua_redis_client*)lua_newuserdata(L, sizeof(*luaclient));
	luaclient->client = self->c;
	luaL_getmetatable(L, REDIS_METATABLE);
	lua_setmetatable(L, -2);
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
	chk_luaRef_release(cb);
	free(cb);
}


static int32_t lua_redis_set_disconnect_cb(lua_State *L) {
	chk_luaRef       *cb;
	lua_redis_client *c = lua_checkredisclient(L,1);
	if(!lua_isfunction(L,2)) 
		return luaL_error(L,"argument 2 of redis_set_disconnect_cb must be lua function");
	cb  = calloc(1,sizeof(*cb));
	*cb = chk_toluaRef(L,2);
	chk_redis_set_disconnect_cb(c->client,lua_redis_disconnect_cb,cb);
	return 0;
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

	cb  = calloc(1,sizeof(*cb));
	*cb = chk_toluaRef(L,4);
	if(0 != chk_redis_connect(event_loop,&server,lua_redis_connect_cb,cb)){
		chk_luaRef_release(cb);
		free(cb);
		lua_pushstring(L,"redis_connect_ip4 connect error");
		return 1;
	}
	return 0;
}

static inline void build_resultset(redisReply* reply,lua_State *L){
	int32_t i;
	if(reply->type == REDIS_REPLY_INTEGER){
		lua_pushinteger(L,reply->integer);
	}else if(reply->type == REDIS_REPLY_STRING){
		lua_pushstring(L,reply->str);
	}else if(reply->type == REDIS_REPLY_ARRAY){
		lua_newtable(L);
		for(i = 0; i < reply->elements; ++i){
			build_resultset(reply->element[i],L);
			lua_rawseti(L,-2,i+1);
		}
	}else lua_pushnil(L);
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
	ReplyPusher pusher;
	if(!cb) return;
	if(reply){
		pusher.reply = reply;
		pusher.Push = PushReply;
		error = chk_Lua_PCallRef(*cb,"f",(chk_luaPushFunctor*)&pusher);	
	}else error = chk_Lua_PCallRef(*cb,"p",NULL);
	if(error) CHK_SYSLOG(LOG_ERROR,"error on redis_reply_cb %s",error);	
	chk_luaRef_release(cb);
	free(cb);
}

static int32_t lua_redis_execute(lua_State *L) {
	chk_luaRef *cb = NULL;
	lua_redis_client *client = lua_checkredisclient(L,1);
	const char *query_str = luaL_optstring(L,2,"");
	if(lua_isfunction(L,3)) {
		cb = calloc(1,sizeof(*cb));
		*cb = chk_toluaRef(L,3);
	}
	if(0 != chk_redis_execute(client->client,query_str,lua_redis_reply_cb,cb)) {
		chk_luaRef_release(cb);
		free(cb);
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
		{"SetDisconnectedCb",lua_redis_set_disconnect_cb},
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
