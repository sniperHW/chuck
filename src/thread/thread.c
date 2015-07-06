#include <stdlib.h>
#include <sys/types.h>
#include <sys/syscall.h>  
#include "thread.h"
#include "sync.h"

#define gettidv1() syscall(__NR_gettid)  
#define gettidv2() syscall(SYS_gettid)  

__thread pid_t tid = 0;

//extern void clear_thdmailbox();


static void child()
{
	tid = 0;
}

struct start_arg{
	void           *arg;
	void           *(*routine)(void*);
	uint8_t         running;
	mutex      	   *mtx;
	condition 	   *cond;
};


static void *start_routine(void *_)
{
	void *ret;
	struct start_arg *starg = cast(struct start_arg*,_);
	void *arg = starg->arg;
	void*(*routine)(void*) = starg->routine;
	mutex_lock(starg->mtx);
	if(!starg->running){
		starg->running = 1;
		mutex_unlock(starg->mtx);
		condition_signal(starg->cond);
	}
	ret = routine(arg);
	//clear_thdmailbox();
	return ret;
}


thread *thread_new(int32_t flag,void *(*routine)(void*),void *ud)
{
	pthread_attr_t attr;
	struct start_arg starg;
	thread *t;
	pthread_attr_init(&attr);
	if(flag | JOINABLE)
		pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
	else
		pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);	
	
	t = calloc(1,sizeof(*t));
	starg.routine = routine;
	starg.arg = ud;
	starg.running = 0;
	starg.mtx = mutex_new();
	starg.cond = condition_new(starg.mtx);
	pthread_create(&t->threadid,&attr,start_routine,&starg);

	mutex_lock(starg.mtx);
	while(!starg.running){
		condition_wait(starg.cond);
	}
	mutex_unlock(starg.mtx);
	condition_del(starg.cond);		
	mutex_del(starg.mtx);		
		
	pthread_attr_destroy(&attr);
	return t;
}

void *thread_join(thread *t)
{
	void *result = NULL;
	pthread_join(t->threadid,&result);
	return result;
}

void *thread_del(thread *t)
{
	void *result = NULL;
	pthread_join(t->threadid,&result);
	free(t);
	return result;	
}

pid_t thread_id()
{
	if(!tid){
		tid = gettidv1();
		pthread_atfork(NULL,NULL,child);
	}
	return tid;
}

#ifdef _CHUCKLUA
void *thread_routine(void *arg)
{
	const char *file = cast(const char *,arg);
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);
	if (luaL_dofile(L,file)) {
		const char * error = lua_tostring(L, -1);
		lua_pop(L,1);
		printf("%s\n",error);
	}
	return NULL;
}

int32_t lua_newcthread(lua_State *L)
{
	thread *t;
	if(!lua_isstring(L,1))
		return luaL_error(L,"invaild arg1");
	t = thread_new(JOINABLE,thread_routine,cast(void*,lua_tostring(L,1)));
	if(t)
		lua_pushlightuserdata(L,t);
	else
		lua_pushnil(L);
	return 1;
}

int32_t lua_threadjoin(lua_State *L)
{
	thread *t = lua_touserdata(L,1);
	if(t){
		thread_join(t);
		thread_del(t);
	}
	return 0;
}

#define SET_FUNCTION(L,NAME,FUNC) do{\
	lua_pushstring(L,NAME);\
	lua_pushcfunction(L,FUNC);\
	lua_settable(L, -3);\
}while(0)

void reg_luathread(lua_State *L)
{
	lua_newtable(L);
	SET_FUNCTION(L,"new",lua_newcthread);
	SET_FUNCTION(L,"join",lua_threadjoin);
}



#endif

