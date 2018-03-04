#define REDIS_METATABLE "lua_redis"

typedef struct {
	chk_redisclient *client;
	chk_luaRef       on_connection_loss;
}lua_redis_client;

#define lua_checkredisclient(L,I)	\
	(lua_redis_client*)luaL_checkudata(L,I,REDIS_METATABLE)

static int32_t lua_redisclient_gc(lua_State *L) {
	printf("lua_redisclient_gc\n");
	lua_redis_client *client = lua_checkredisclient(L,1);
	if(client->client) {
		chk_redis_close(client->client);
		client->client = NULL;
	}
	if(client->on_connection_loss.L) {
		chk_luaRef_release(&client->on_connection_loss);
	}	
	return 0;
}

static void lua_redis_disconnect_cb(chk_redisclient *_,chk_ud ud,int32_t err) {
	lua_redis_client *c = (lua_redis_client*)ud.v.val;
	c->client = NULL;
	if(c->on_connection_loss.L) {
		const char *error = chk_Lua_PCallRef(c->on_connection_loss,"");
		if(error) {
			CHK_SYSLOG(LOG_ERROR,"error on on_connection_loss %s",error);
		}
	}			
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
			chk_redis_set_disconnect_cb(self->c,lua_redis_disconnect_cb,chk_ud_make_void(luaclient));
			luaL_setmetatable(L, REDIS_METATABLE);
		}else {
			chk_redis_close(self->c);   
			lua_pushnil(L);
		}		
	}
	else {
		lua_pushnil(L);
	}
}

static void lua_redis_connect_cb(chk_redisclient *c,chk_ud ud,int32_t err) {
	chk_luaRef cb = ud.v.lr;
	const char *error;
	luaRedisPusher pusher;
	if(c){
		pusher.c = c;
		pusher.Push = PushRedis;
		error = chk_Lua_PCallRef(cb,"f",(chk_luaPushFunctor*)&pusher);	
	}else{
		error = chk_Lua_PCallRef(cb,"pi",NULL,err);
	}
	if(error) CHK_SYSLOG(LOG_ERROR,"error on lua_redis_connect_cb %s",error);				
	chk_luaRef_release(&cb);
}

static int32_t lua_redis_close(lua_State *L) {
	lua_redis_client *c = lua_checkredisclient(L,1);	
	if(c->client) {
		chk_redis_close(c->client);
		c->client = NULL;
	}
	return 0;
}

static int32_t lua_redis_connect_ip4(lua_State *L) {
	chk_event_loop *event_loop;
	chk_luaRef      cb;
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

	cb = chk_toluaRef(L,4);
	if(0 != chk_redis_connect(event_loop,&server,lua_redis_connect_cb,chk_ud_make_lr(cb))){
		chk_luaRef_release(&cb);
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

void lua_redis_reply_cb(chk_redisclient *_,redisReply *reply,chk_ud ud) {
	chk_luaRef  cb = ud.v.lr;
	const char *error = NULL;
	const char *redis_err_str = NULL;
	ReplyPusher pusher;
	if(!cb.L) return;
	if(reply){
		pusher.reply = reply;
		pusher.Push = PushReply;
		if(reply->type == REDIS_REPLY_ERROR) {
			redis_err_str = reply->str;
		}
		error = chk_Lua_PCallRef(cb,"fs",(chk_luaPushFunctor*)&pusher,redis_err_str);	
	}
	else{
		error = chk_Lua_PCallRef(cb,"ps",NULL,"unknow error");
	}
	
	if(error) CHK_SYSLOG(LOG_ERROR,"error on redis_reply_cb %s",error);	
	chk_luaRef_release(&cb);
}

typedef int32_t (*redis_execute_lua)(chk_redisclient*,const char *cmd,chk_redis_reply_cb cb,chk_ud ud,lua_State *L,int32_t start_idx,int32_t param_size);


static int32_t _lua_redis_execute(redis_execute_lua fn,lua_State *L) {
	chk_luaRef cb = {0};
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

	if(!client->client) {
		lua_pushstring(L,"connection loss");
		return 1;
	}

	if(lua_isfunction(L,2)) {
		cb = chk_toluaRef(L,2);
	}

	int32_t param_size = lua_gettop(L) - 3;
	
	if(0 != fn(client->client,cmd,lua_redis_reply_cb,chk_ud_make_lr(cb),L,4,param_size)) {
		chk_luaRef_release(&cb);
		lua_pushstring(L,"redis_execute error");
		return 1;
	}

	return 0;
}

static int32_t lua_redis_execute(lua_State *L) {
	return _lua_redis_execute(chk_redis_execute_lua,L);
}

static int32_t lua_redis_on_connection_loss(lua_State *L) {
	lua_redis_client *client = lua_checkredisclient(L,1);
	if(!client) {
		lua_pushstring(L,"invaild client");
		return 1;
	}

	if(!lua_isfunction(L,2)) {
		lua_pushstring(L,"cb should be a function");
		return 1;
	}

	client->on_connection_loss = chk_toluaRef(L,2);

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
		{"OnConnectionLoss", lua_redis_on_connection_loss},
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
