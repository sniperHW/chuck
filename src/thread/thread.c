#define __CORE__
#include <stdlib.h>
#include <sys/types.h>
#include <sys/syscall.h> 
#include "engine/engine.h" 
#include "thread.h"
#include "sync.h"

#ifdef _LINUX
#define gettid() syscall(__NR_gettid)  
#elif _BSD
#define gettid() getpid()
#else
#error "un support platform!"		
#endif



extern int32_t pipe2(int pipefd[2], int flags);
#define MAX_QUENESIZE 4096  

__thread pid_t tid = 0;


typedef struct{
	handle      base;
	list        global_queue;
	list        private_queue;
	int32_t     notifyfd;	
	void 	    (*onmail)(mail *_mail);
	mutex      *mtx;
	condition  *cond;
	int16_t     wait;       
}tmailbox_;

typedef struct
{
	REFOBJ;
	pthread_t  threadid;
	tmailbox_ *mailbox;
    int32_t    joinable;
    pid_t      tid;
    volatile int8_t destroy;     
}thread;

__thread thread   *tthread = NULL;
__thread thread_t  thread_handle;

static inline thread *cast2tthread(thread_t t)
{
	return cast(thread*,cast2refobj(cast(refhandle,t)));
}

#ifdef _CHUCKLUA

static  int32_t   lock = 0;
static  list      all_threads;
#define LOCK() while (__sync_lock_test_and_set(&lock,1)) {}
#define UNLOCK() __sync_lock_release(&lock);

typedef struct{
	listnode node;
	thread_t thd;
}stThd;

static inline void __insert(thread_t thd){
	stThd *node = calloc(1,sizeof(*node));
	node->thd = thd;
	LOCK();
	list_pushback(&all_threads,cast(listnode*,node));
	UNLOCK();
}

static inline void __remove(thread_t thd){
	stThd *node;
	size_t size,i;
	LOCK();
	size = list_size(&all_threads);
	for(i = 0; i < size; ++i){
		node = cast(stThd*,list_pop(&all_threads));
		if(node->thd.identity == thd.identity){
			free(node);
			break;
		}else{
			list_pushback(&all_threads,cast(listnode*,node));
		}
	}
	UNLOCK();
}

static inline thread_t __findbytid(pid_t tid){
	thread_t handle = (thread_t){.identity=0,.ptr=NULL};
	thread  *t;
	stThd   *node;
	size_t   size,i;
	LOCK();
	size = list_size(&all_threads);
	for(i = 0; i < size; ++i){
		node = cast(stThd*,list_pop(&all_threads));
		t = cast2tthread(node->thd);
		if(!t){
			free(node);
		}else{
			list_pushback(&all_threads,cast(listnode*,node));
			if(t->tid != tid){
				refobj_dec(cast(refobj*,t));
			}else{
				handle = node->thd;
				break;
			}
		}
	}
	UNLOCK();
	return handle;	
}

#endif

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
	thread         *t;
};

static void *start_routine(void *_)
{
	void *ret;
	struct start_arg *starg = cast(struct start_arg*,_);
	void *arg = starg->arg;
	void*(*routine)(void*) = starg->routine;
	tthread = starg->t;
	thread_handle = cast(thread_t,get_refhandle(cast(refobj*,tthread)));
	tthread->tid = gettid();
	mutex_lock(starg->mtx);
	if(!starg->running){
		starg->running = 1;
		mutex_unlock(starg->mtx);
		condition_signal(starg->cond);
	}
	ret = routine(arg);
	tthread->destroy = 1;
	return ret;
}

static void thread_dctor(void *ptr)
{
	mail* mail_;	
	tmailbox_* mailbox = cast(thread*,ptr)->mailbox;
	while((mail_ = (mail*)list_pop(&mailbox->private_queue)))
		mail_del(mail_);	
	while((mail_ = (mail*)list_pop(&mailbox->global_queue)))
		mail_del(mail_);				
	close(((handle*)mailbox)->fd);
	close(mailbox->notifyfd);
	mutex_del(mailbox->mtx);
	condition_del(mailbox->cond);		
	free(mailbox);
}

#define MAX_EVENTS 512

static inline mail* getmail()
{
	uint32_t   try_;
	tmailbox_ *mailbox_ = tthread->mailbox;
	if(!list_size(&mailbox_->private_queue)){
		mutex_lock(mailbox_->mtx);
		if(!list_size(&mailbox_->global_queue)){
			try_ = 16;
			do{
				mutex_unlock(mailbox_->mtx);
				SLEEPMS(0);
				--try_;
				mutex_lock(mailbox_->mtx);
			}while(try_ > 0 &&!list_size(&mailbox_->global_queue));
			if(try_ == 0){
				TEMP_FAILURE_RETRY(read(cast(handle*,mailbox_)->fd,&try_,sizeof(try_)));
				mailbox_->wait = 1;
				mutex_unlock(mailbox_->mtx);
				return NULL;
			}
		}
		list_pushlist(&mailbox_->private_queue,&mailbox_->global_queue);
		mutex_unlock(mailbox_->mtx);
		condition_broadcast(mailbox_->cond);
	}
	return cast(mail*,list_pop(&mailbox_->private_queue));
}


static void on_events(handle *h,int32_t events)
{
	mail   *mail_;
	int32_t n = MAX_EVENTS;
	tmailbox_ *mailbox_ = tthread->mailbox;
	if(events == EENGCLOSE)
		return;	
	while((mail_ = getmail()) != NULL){
		mailbox_->onmail(mail_);
		mail_del(mail_);
		if(--n == 0) break;
	}
}

thread *_thread_new(){
	thread *t;
	int32_t tmp[2];
	if(pipe2(tmp,O_NONBLOCK|O_CLOEXEC) != 0){
		return NULL;
	}	
	t  = calloc(1,sizeof(*t));
	t->mailbox = calloc(1,sizeof(*t->mailbox));
	cast(handle*,t->mailbox)->fd = tmp[0];
	t->mailbox->notifyfd = tmp[1];
	t->mailbox->mtx = mutex_new();
	t->mailbox->wait = 1;
	t->mailbox->cond = condition_new(t->mailbox->mtx);
	refobj_init(cast(refobj*,t),thread_dctor);
	return t;
}


thread_t thread_new(void *(*routine)(void*),void *ud)
{
	pthread_attr_t attr;
	struct start_arg starg;
	thread *t;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);


	if(!tthread){
		//主线程
		tthread = _thread_new();
		if(!tthread) return (thread_t){.ptr=NULL,.identity=0};
		thread_handle = cast(thread_t,get_refhandle(cast(refobj*,tthread)));
		tthread->tid   = gettid();
#ifdef _CHUCKLUA		
		__insert(thread_handle);
#endif
	}
	
	t = _thread_new();
	if(!t) return (thread_t){.ptr=NULL,.identity=0};
	starg.routine = routine;
	starg.arg = ud;
	starg.running = 0;
	starg.mtx = mutex_new();
	starg.cond = condition_new(starg.mtx);
	starg.t = t;
	pthread_create(&t->threadid,&attr,start_routine,&starg);

	mutex_lock(starg.mtx);
	while(!starg.running){
		condition_wait(starg.cond);
	}
	mutex_unlock(starg.mtx);
	condition_del(starg.cond);		
	mutex_del(starg.mtx);		
		
	pthread_attr_destroy(&attr);
	return cast(thread_t,get_refhandle(cast(refobj*,t)));
}

void *thread_join(thread_t t)
{
	thread   *thread_   = cast2tthread(t);	
	void *result = NULL;
	if(!thread_){
		assert(0);
		return NULL;
	}
	pthread_join(thread_->threadid,&result);
	refobj_dec(cast(refobj*,thread_));//对应cast2tthread的引用增加
	refobj_dec(cast(refobj*,thread_));//对应_thread_new初始化时的1次计数
	return result;
}


pid_t thread_tid(thread_t t)
{
	pid_t    tid;
	thread   *thread_   = cast2tthread(t);	
	if(!thread_){
		return 0;
	}
	tid = thread_->tid;
	refobj_dec(cast(refobj*,thread_));
	return tid;
}

pid_t thread_id()
{
	if(!tid){
		tid = gettid();
		pthread_atfork(NULL,NULL,child);
	}
	return tid;
}

static inline int32_t notify(tmailbox_ *mailbox)
{
	int32_t  ret;
	uint64_t _ = 1;
	for(;;){
		errno = 0;
		ret = TEMP_FAILURE_RETRY(write(mailbox->notifyfd,&_,sizeof(_)));
		if(!(ret < 0 && errno == EAGAIN))
			break;
	}
	return ret > 0 ? 0:-1;
}

int32_t  thread_sendmail(thread_t t,mail *mail_)
{
	
	int32_t    ret = 0;
	thread    *target_thread   = cast2tthread(t);
	tmailbox_ *target_mailbox;  
	if(!target_thread) return -ETMCLOSE;
	target_mailbox = target_thread->mailbox;
	mail_->sender  = thread_handle;

	if(target_thread == tthread){
		do{
			if(target_mailbox->wait){
				mutex_lock(target_mailbox->mtx);
				if(target_mailbox->wait && (ret = notify(target_mailbox)) == 0)
					target_mailbox->wait = 0;
				mutex_unlock(target_mailbox->mtx);
				if(ret != 0) break;
			}
			list_pushback(&target_mailbox->private_queue,cast(listnode*,mail_));
		}while(0);	
	}else{
		mutex_lock(target_mailbox->mtx);
		while(list_size(&target_mailbox->global_queue) > MAX_QUENESIZE && !target_thread->destroy)
			condition_wait(target_mailbox->cond);
		if(target_thread->destroy)
			ret = -ETMCLOSE;
		else{
			if(target_mailbox->wait && (ret = notify(target_mailbox)) == 0)
				target_mailbox->wait = 0;
			if(ret == 0)
				list_pushback(&target_mailbox->global_queue,cast(listnode*,mail_));			
		}
		mutex_unlock(target_mailbox->mtx);
	}
	refobj_dec(cast(refobj*,target_thread));//cast会调用refobj_inc,所以这里要调用refobj_dec
	return ret;
}

int32_t  thread_process_mail(engine *e,void (*onmail)(mail *_mail))
{
	if(!tthread){
		//主线程
		tthread = _thread_new();
		if(!tthread) assert(0);
		thread_handle = cast(thread_t,get_refhandle(cast(refobj*,tthread)));
		tthread->tid   = gettid();
		__insert(thread_handle);
	}
	if(cast(handle*,tthread->mailbox)->e)
		return -EASSENG;
	cast(handle*,tthread->mailbox)->on_events = on_events;
	tthread->mailbox->onmail = onmail;
	if(event_add(e,cast(handle*,tthread->mailbox),EVENT_READ) != 0){
		return -EENASSERR;	
	}
	return 0;
}


#ifdef _CHUCKLUA

#include "lua/val.h"

typedef struct{
	mail     base;
	TValue  *v;      
}lua_mail;

typedef struct{
	luaPushFunctor base;
	TValue        *v;
}stPushMail;

typedef struct{
	luaPushFunctor base;
	thread_t       sender;
}stPushCT;


void lua_mail_dcotr(void *_)
{
	lua_mail *m = cast(lua_mail*,_);
	TVal_release(m->v);
}

void lua_pushcthread(lua_State *L,thread_t *t)
{
	lua_newtable(L);
	lua_pushnumber(L,t->identity);
	lua_rawseti(L,-2,1);
	lua_pushlightuserdata(L,t->ptr);
	lua_rawseti(L,-2,2);	
}

static void PushMail(lua_State *L,luaPushFunctor *_)
{
	stPushMail *self = (stPushMail*)_;
	if(self->v)
		pushVal(L,self->v);
	else
		lua_pushnil(L);
}

static void PushCT(lua_State *L,luaPushFunctor *_)
{
	stPushCT *self = (stPushCT*)_;
	lua_pushcthread(L,&self->sender);
}


__thread luaRef mailcb = (luaRef){.L=NULL,.rindex=LUA_REFNIL};


void lua_onmail(mail *_){
	const char *error;
	lua_mail *mail_ = cast(lua_mail*,_);
	stPushCT 	st1;
	stPushMail  st2;	
	st1.sender 	  = _->sender;
	st1.base.Push = PushCT;		
	st2.v = mail_->v;
	st2.base.Push = PushMail;
	if((error = LuaCallRefFunc(mailcb,"ff",&st1,&st2)))
		SYS_LOG(LOG_ERROR,"error on [%s:%d]:%s\n",__FILE__,__LINE__,error);
}

thread_t lua_tocthread(lua_State *L,int32_t i)
{
	thread_t t = (thread_t){.identity=0,.ptr = NULL};
	uint64_t identity;
	void    *ptr;
	if(lua_istable(L,i)){
		lua_rawgeti(L,i,1);
		identity = lua_tonumber(L,-1);
		lua_pop(L,1);
		lua_rawgeti(L,i,2);
		ptr = lua_touserdata(L,-1);
		lua_pop(L,1);
		t = (thread_t){.identity=identity,.ptr = ptr};
	}
	return t;		
}

int32_t lua_sendmail(lua_State *L)
{
	thread_t  t = lua_tocthread(L,1);
	lua_mail *m;
	if(lua_istable(L,2)){
		m    = calloc(1,sizeof(*m));
		cast(mail*,m)->dctor = lua_mail_dcotr;
		m->v = toVal(L,2);
		if(0 == thread_sendmail(t,cast(mail*,m))){
			lua_pushboolean(L,1);
			return 1;
		}
		mail_del(cast(mail*,m));
	}
	return 0;
}

int32_t lua_process_mail(lua_State *L)
{
	engine *e = lua_toengine(L,1);
	mailcb    = toluaRef(L,2);
	if(mailcb.L && 0 == thread_process_mail(e,lua_onmail))
	{
		lua_pushboolean(L,1);
		return 1;
	}
	return 0;
}

void *thread_routine(void *arg)
{
	__insert(thread_handle);
	const char *file = cast(const char *,arg);
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);
	if (luaL_dofile(L,file)) {
		const char * error = lua_tostring(L, -1);
		lua_pop(L,1);
		printf("%s\n",error);
	}
	if(mailcb.L) release_luaRef(&mailcb);
	__remove(thread_handle);
	return NULL;
}

int32_t lua_newcthread(lua_State *L)
{
	thread_t t;
	if(!lua_isstring(L,1))
		return luaL_error(L,"invaild arg1");
	t = thread_new(thread_routine,cast(void*,lua_tostring(L,1)));
	if(is_vaild_refhandle(&t))
		lua_pushcthread(L,&t);
	else
		lua_pushnil(L);
	return 1;
}

int32_t lua_threadjoin(lua_State *L)
{
	thread_t t = lua_tocthread(L,1);
	thread_join(t);
	return 0;
}

int32_t lua_tid(lua_State *L)
{
	thread_t t = lua_tocthread(L,1);
	lua_pushinteger(L,thread_tid(t));
	return 1;
}

int32_t lua_selftid(lua_State *L)
{
	lua_pushinteger(L,thread_id());
	return 1;
}

//向除自己以外的所有线程广播消息
int32_t lua_broadcast(lua_State *L)
{
	lua_mail *m;
	stThd *node;
	size_t size,i;	
	if(lua_istable(L,1)){
		LOCK();
		size = list_size(&all_threads);
		for(i = 0; i < size; ++i){
			node = cast(stThd*,list_pop(&all_threads));
			if(thread_handle.ptr != node->thd.ptr){			
				m    = calloc(1,sizeof(*m));
				cast(mail*,m)->dctor = lua_mail_dcotr;
				m->v = toVal(L,1);
				if(0 != thread_sendmail(node->thd,cast(mail*,m)))
					mail_del(cast(mail*,m));
			}
			list_pushback(&all_threads,cast(listnode*,node));
		}
		UNLOCK();
	}	
	return 0;
}

int32_t lua_findbytid(lua_State *L)
{
	pid_t tid       = lua_tointeger(L,1);
	thread_t handle = __findbytid(tid);
	if(is_vaild_refhandle(cast(refhandle*,&handle))){
		lua_pushcthread(L,&handle);
		return 1;
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
	SET_FUNCTION(L,"broadcast",lua_broadcast);
	SET_FUNCTION(L,"findbytid",lua_findbytid);
	SET_FUNCTION(L,"selftid",lua_selftid);
	SET_FUNCTION(L,"tid",lua_tid);
	SET_FUNCTION(L,"new",lua_newcthread);
	SET_FUNCTION(L,"join",lua_threadjoin);
	SET_FUNCTION(L,"sendmail",lua_sendmail);
	SET_FUNCTION(L,"process_mail",lua_process_mail);
}

#endif

