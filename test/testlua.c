#include <stdio.h>
#include "lua/chk_lua.h"

int main() {
	const char *error;
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);
	if (luaL_dofile(L,"test.lua")) {
		error = lua_tostring(L, -1);
		lua_pop(L,1);
		printf("%s\n",error);
		return 0;
	}

	//调用无参无返回
	if((error = chk_Lua_PCall(L,"fun0",NULL))) {
		printf("error on fun0:%s\n",error);
	}

	//1个参数，1返回
	const char *ret;
	if((error = chk_Lua_PCall(L,"fun1","s:s","hello",&ret))) {
		printf("error on fun1:%s\n",error);
	}else {
		printf("ret from fun1:%s\n",ret);
	}

	//返回并调用一个lua function引用
	chk_luaRef lFun;
	if((error = chk_Lua_PCall(L,"funf",":r",&lFun))) {
		printf("error on funf:%s\n",error);
	}else {
		chk_Lua_PCallRef(lFun,NULL);
	}		

	return 0;
}