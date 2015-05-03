#include "lua/lua_util.h"

		
int main(int argc,char **argv){
	if(argc < 2){
		printf("usage testlua luafile\n");
		return 0;
	}
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);
	if (luaL_dofile(L,argv[1])) {
		const char * error = lua_tostring(L, -1);
		lua_pop(L,1);
		printf("%s\n",error);
	}
	const char *err;
	printf("------makeLuaObjByStr---------\n\n");
	//dynamic build a lua table and return
	luaRef tabRef = makeLuaObjByStr(L,"return {a=100}");
	if(tabRef.L){
		int a = 0;
		err = LuaRef_Get(tabRef,"si","a",&a);
		if(err) printf("%s\n",err);
		else{
			printf("a == %d\n",a);	
		}
		release_luaRef(&tabRef);
	}


	printf("------call 0 arg---------\n\n");
	err = LuaCall(L,"fun0",NULL);
	if(err) printf("error on fun0:%s\n",err);
	

	printf("------call 2 arg(integer,str),1 ret(integer)---------\n\n");

	unsigned int iret1;
	err = LuaCall(L,"fun1","is:i",0xfffffffff,"hello",&iret1);
	if(err) printf("error on fun1:%s\n",err);
	else printf("ret1=%u\n",iret1);
	
	printf("------call 2 arg(str),1 ret(str)---------\n\n");
	char *sret1;
	err = LuaCall(L,"fun2","ss:s","hello","kenny",&sret1);
	if(err) printf("error on fun2:%s\n",err);
	else printf("sret1=%s\n",sret1);
	
	printf("------call 3 arg(str,str,lua str),2 ret(lua str)---------\n\n");
	char  *Sret1,*Sret2;
	size_t Lret1,Lret2;
	err = LuaCall(L,"fun3","ssS:SS","1","2","3",sizeof("3"),&Sret1,&Lret1,&Sret2,&Lret2);
	if(err) printf("error on fun3:%s\n",err); 
	else{
		printf("%s %ld\n",Sret1,Lret1);
		printf("%s %ld\n",Sret2,Lret2);		
	}
	
	printf("------call 0 arg, 1 ret(luaobj)---------\n\n");
	luaRef funRef;
	err = LuaCall(L,"fun5",":r",&funRef);
	if(err) printf("error on fun5:%s\n",err);
	LuaCallRefFunc(funRef,NULL);	
	
	
	printf("------call lua obj function---------\n\n");	
	int num;
	err = LuaCall(L,"fun6",":ri",&tabRef,&num);
	if(err) printf("error on fun6:%s\n",err);
	printf("%d\n",num);
	err = LuaCallTabFuncS(tabRef,"hello",NULL);
	if(err) printf("%s\n",err);
	
	printf("------set and get luaobj field---------\n\n");	
	err = LuaRef_Set(tabRef,"siss","f1",99,"f2","hello haha");
	if(err) printf("%s\n",err);
	else{
		int f1;
		const char *f2;
		err = LuaRef_Get(tabRef,"siss","f1",&f1,"f2",&f2);
		if(err) printf("%s\n",err);
		else printf("get %d,%s\n",f1,f2);
	}

	err = LuaRef_Set(tabRef,"sp","f1",NULL);
	if(err) printf("%s\n",err);
	else{
		err = LuaCall(L,"fun7",NULL);
		if(err) printf("error on fun7:%s\n",err);		
	}
	
	printf("------iterate luaobj-------------\n\n");
	lua_getglobal(L,"ttab4");
	tabRef = toluaRef(L,-1);
	LuaTabForEach(tabRef){
		const char *key = lua_tostring(L,EnumKey);
		const char *val = lua_tostring(L,EnumVal);
		printf("%s,%s\n",key,val);
	}

	printf("end\n");	
	return 0;	
}
