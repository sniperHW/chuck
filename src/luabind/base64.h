#include "util/base64.h"

/*
static int lua_encode(lua_State *L){
	size_t len;
	const unsigned char *input = (const unsigned char*)lua_tolstring(L,-1,&len);
	const char *output = b64_encode(input,len);
	lua_pushlstring(L,output,strlen(output));
	free((void*)output);	
	return 1;	
}

static int lua_decode(lua_State *L){
	size_t len1,len2;
	const char *input = (const char*)lua_tolstring(L,-1,&len1);
	const char *output = (const char*)b64_decode_ex(input,len1,&len2);
	lua_pushlstring(L,output,len2);
	free((void*)output);
	return 1;	
}
*/
int lua_f_base64_encode(lua_State *L)
{
    const unsigned char *src = NULL;
    size_t slen = 0;

    if(lua_isnil(L, 1)) {
        src = (const unsigned char *) "";

    } else {
        src = (const unsigned char *) luaL_checklstring(L, 1, &slen);
    }

    unsigned char *end = malloc(base64_encoded_length(slen));
    int nlen = base64_encode(end, src, slen);
    lua_pushlstring(L, (char *) end, nlen);
    free(end);
    return 1;
}

int lua_f_base64_decode(lua_State *L)
{
    const unsigned char *src = NULL;
    size_t slen = 0;

    if(lua_isnil(L, 1)) {
        src = (const unsigned char *) "";

    } else {
        src = (unsigned char *) luaL_checklstring(L, 1, &slen);
    }

    unsigned char *end = malloc(base64_decoded_length(slen));
    int nlen = base64_decode(end, src, slen);
    lua_pushlstring(L, (char *) end, nlen);
    free(end);
    return 1;
}

int lua_f_base64_encode_url(lua_State *L)
{
    const unsigned char *src = NULL;
    size_t slen = 0;

    if(lua_isnil(L, 1)) {
        src = (const unsigned char *) "";

    } else {
        src = (const unsigned char *) luaL_checklstring(L, 1, &slen);
    }

    unsigned char *end = malloc(base64_encoded_length(slen));
    int nlen = base64_encode_url(end, src, slen);
    lua_pushlstring(L, (char *) end, nlen);
    free(end);
    return 1;
}

int lua_f_base64_decode_url(lua_State *L)
{
    const unsigned char *src = NULL;
    size_t slen = 0;

    if(lua_isnil(L, 1)) {
        src = (const unsigned char *) "";

    } else {
        src = (unsigned char *) luaL_checklstring(L, 1, &slen);
    }

    unsigned char *end = malloc(base64_decoded_length(slen));
    int nlen = base64_decode_url(end, src, slen);
    lua_pushlstring(L, (char *) end, nlen);
    free(end);
    return 1;
}

static void register_base64(lua_State *L) {
	lua_newtable(L);
	SET_FUNCTION(L,"encode",lua_f_base64_encode);
	SET_FUNCTION(L,"decode",lua_f_base64_decode);
}
