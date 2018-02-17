#include "util/base64.h"

static int lua_encode(lua_State *L){
	size_t len;
	const unsigned char *input = (const unsigned char*)lua_tolstring(L,-1,&len);
	const char *output = b64_encode(input,len);
	lua_pushstring(L,b64_encode(input,len));
	free(output);	
	return 1;	
}

static int lua_decode(lua_State *L){
	size_t len1,len2;
	const char *input = (const char*)lua_tolstring(L,-1,&len1);
	const char *output = (const char*)b64_decode_ex(input,len1,&len2);
	lua_pushlstring(L,output,len2);
	free(output);
	return 1;	
}

static void register_base64(lua_State *L) {
	lua_newtable(L);
	SET_FUNCTION(L,"encode",lua_encode);
	SET_FUNCTION(L,"decode",lua_decode);
}
