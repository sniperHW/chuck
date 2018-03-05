#include <openssl/ssl.h>
#include <openssl/err.h>

int32_t lua_SSL_CTX_new(lua_State *L) {

	const char *method = luaL_checkstring(L,1);

	lua_SSL_CTX *ctx = LUA_NEWUSERDATA(L,lua_SSL_CTX);

	if(0 == strcmp(method,"SSLv23_server_method")){
		ctx->ctx = SSL_CTX_new(SSLv23_server_method());
	}
	//to do:other method
	else {
		lua_pushnil(L);
		lua_pushstring(L,"un support method");
		return 2;
	}

	if(!ctx->ctx) {
		lua_pushnil(L);
		lua_pushstring(L,"SSL_CTX_new faile");
		return 2;		
	}	

	luaL_setmetatable(L, SSL_CTX_METATABLE);

	return 1;
}

int32_t lua_SSL_CTX_GC(lua_State *L) {
	lua_SSL_CTX *ctx = lua_check_ssl_ctx(L,1);
	if(ctx->ctx) {
		SSL_CTX_free(ctx->ctx);
		ctx->ctx = NULL;
	}
	return 0;
}

int32_t lua_SSL_CTX_use_certificate_file(lua_State *L) {
	
	lua_SSL_CTX *ctx = lua_check_ssl_ctx(L,1);

	if(!ctx->ctx) {
		lua_pushstring(L,"invaild ssl_ctx");		
		return 1;
	}

	const char *certificate = lua_tostring(L,2);

	if(!certificate) {
		lua_pushstring(L,"arg 2 must be string");
		return 1;
	}

    if (SSL_CTX_use_certificate_file(ctx->ctx,certificate, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stdout);
        lua_pushstring(L,"SSL_CTX_use_certificate_file failed");
        return 1;
    }

    return 0;
}

int32_t lua_SSL_CTX_use_PrivateKey_file(lua_State *L) {

	lua_SSL_CTX *ctx = lua_check_ssl_ctx(L,1);

	if(!ctx->ctx) {
		lua_pushstring(L,"invaild ssl_ctx");		
		return 1;
	}

	const char *privatekey = lua_tostring(L,2);


	if(!privatekey) {
		lua_pushstring(L,"arg must be string");
		return 1;
	}

    if (SSL_CTX_use_PrivateKey_file(ctx->ctx, privatekey, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stdout);
        lua_pushstring(L,"SSL_CTX_use_PrivateKey_file failed"); 
        return 1;
    }

    return 0;		
}

int32_t lua_SSL_CTX_check_private_key(lua_State *L) {
	lua_SSL_CTX *ctx = lua_check_ssl_ctx(L,1);

	if(!ctx->ctx) {
		lua_pushstring(L,"invaild ssl_ctx");		
		return 1;
	}

    if(!SSL_CTX_check_private_key(ctx->ctx)) {
        ERR_print_errors_fp(stdout);
        lua_pushstring(L,"SSL_CTX_check_private_key failed");        
        return 1;
    }
    return 0;			
} 


//chk_stream_socket *s,SSL_CTX *ctx

int32_t lua_ssl_connect(lua_State *L) {
	lua_stream_socket *s = lua_checkstreamsocket(L,1);
	if(!s->socket){
		luaL_error(L,"invaild lua_stream_socket");
	}
	if(0 == chk_ssl_connect(s->socket)) {
		return 0;
	}
	else {
		lua_pushstring(L,"chk_ssl_connect failed"); 
		return 1;
	}
}

int32_t lua_ssl_accept(lua_State *L) {
	lua_stream_socket *s = lua_checkstreamsocket(L,1);
	if(!s->socket){
		return luaL_error(L,"invaild lua_stream_socket");
	}
	lua_SSL_CTX *ctx = lua_check_ssl_ctx(L,2);
	if(!ctx->ctx) {
		lua_pushstring(L,"invaild ssl_ctx");		
		return 1;
	}

	if(0 == chk_ssl_accept(s->socket,ctx->ctx)) {
		return 0;
	}
	else {
		lua_pushstring(L,"chk_ssl_accept failed"); 
		return 1;
	}			
}

static void register_ssl(lua_State *L) {

	luaL_newmetatable(L, SSL_CTX_METATABLE);
	lua_pop(L,1);
	lua_newtable(L);
	SET_FUNCTION(L,"SSL_CTX_new",lua_SSL_CTX_new);
	SET_FUNCTION(L,"SSL_CTX_use_certificate_file",lua_SSL_CTX_use_certificate_file);
	SET_FUNCTION(L,"SSL_CTX_use_PrivateKey_file",lua_SSL_CTX_use_PrivateKey_file);	
	SET_FUNCTION(L,"SSL_CTX_check_private_key",lua_SSL_CTX_check_private_key);		
	SET_FUNCTION(L,"SSL_connect",lua_ssl_connect);
	SET_FUNCTION(L,"SSL_accept",lua_ssl_accept);		
}