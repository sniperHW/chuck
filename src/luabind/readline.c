#include "lua/chk_lua.h"

#include "linenoise.h"

void completion(const char *buf, linenoiseCompletions *lc) {
    
    switch(buf[0]){
    	case 'r':{
        	linenoiseAddCompletion(lc,"redis_execute");
        	linenoiseAddCompletion(lc,"result");
    	}
    	break;
    	case 'e':{
    		linenoiseAddCompletion(lc,"exit");  
    	}
    	break;
    	case 'p':{
      		linenoiseAddCompletion(lc,"print");   		
    	}
    	break;
    	default:{
    		return;
    	}

    }
}

int32_t chk_lua_readline(lua_State *L)
{
	const char *prompt = lua_tostring(L,1);
	if(!prompt) {
		return 0;
	}

	char *line = linenoise(prompt);

	if(NULL != line) {

		if(*line) {
			linenoiseHistoryAdd(line);
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
	//linenoiseHistoryLoad("history.txt");
    linenoiseSetCompletionCallback(completion);	
	lua_pushcfunction(L,chk_lua_readline);
	return 1;
}
