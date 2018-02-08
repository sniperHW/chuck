#include "util/sha1.h"

static int lua_sha1(lua_State *L){
	size_t len;
	unsigned char output[20];
	const unsigned char *input = (const unsigned char*)lua_tolstring(L,-1,&len);
	sha1(input,len,output);
	lua_pushlstring(L,(const char*)&output,20);
	return 1;	
}

static void register_crypt(lua_State *L) {
	lua_newtable(L);
	SET_FUNCTION(L,"sha1",lua_sha1);
}