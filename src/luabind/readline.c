#include "lua/chk_lua.h"
#include "readline/readline.h"
#include "readline/history.h"

int32_t chk_lua_readline(lua_State *L)
{
	const char *prompt = lua_tostring(L,1);
	if(!prompt) {
		return 0;
	}

	char *line = readline(prompt);

	if(NULL != line) {

		if(*line) {
			add_history(line);
		}
		lua_pushstring(L,line);
		free(line);
		return 1;
	}
	else {
		return 0;
	}

}

int32_t luaopen_readline(lua_State *L)
{
	//lua_newtable(L);
	//SET_FUNCTION(L,"Readline",chk_lua_readline);
	lua_pushcfunction(L,chk_lua_readline);
	return 1;
}
