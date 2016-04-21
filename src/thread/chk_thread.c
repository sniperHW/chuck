#include <stdlib.h>
#include <sys/types.h>
#include <sys/syscall.h>  
#include "thread/chk_thread.h"
#include "thread/chk_sync.h"

#ifdef _LINUX
# define gettid() syscall(__NR_gettid)  
#elif _MACH
# define gettid() getpid()
#else
# error "un support platform!"		
#endif

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

struct chk_thread {
	pthread_t       threadid;
    pid_t           tid;   
};

typedef struct {
	void           *arg;
	void           *(*routine)(void*);
	uint8_t         running;
	chk_mutex       mtx;
	chk_condition   cond;
	chk_thread     *t;
}argum;

__thread chk_thread *tthread = NULL;
__thread pid_t       tid = 0;

static void *start_routine(void *_) {
	void *ret;
	argum *starg = cast(argum*,_);
	void *arg = starg->arg;
	void*(*routine)(void*) = starg->routine;
	tthread = starg->t;
	tid = tthread->tid = gettid();
	chk_mutex_lock(&starg->mtx);
	if(!starg->running) {
		starg->running = 1;
		chk_mutex_unlock(&starg->mtx);
		chk_condition_signal(&starg->cond);
	}
	ret = routine(arg);
	return ret;
}

chk_thread *_thread_new() {
	chk_thread *t = calloc(1,sizeof(*t));
	return t;
}

void chk_thread_del(chk_thread *t) {
	free(t);
}

chk_thread *chk_thread_new(void *(*routine)(void*),void *ud) {
	pthread_attr_t   attr;
	argum 			 starg;
	chk_thread *     t;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);

	if(!tthread) {
		//主线程
		if(!(tthread = _thread_new())) return NULL;
		tthread->tid = gettid();
	}
	
	if(!(t = _thread_new())) return NULL;
	starg.routine = routine;
	starg.arg     = ud;
	starg.running = 0;
	starg.t       = t;
	chk_mutex_init(&starg.mtx);
	chk_condition_init(&starg.cond,&starg.mtx);
	pthread_create(&t->threadid,&attr,start_routine,&starg);

	chk_mutex_lock(&starg.mtx);
	while(!starg.running) {
		chk_condition_wait(&starg.cond);
	}
	chk_mutex_unlock(&starg.mtx);
	chk_condition_uninit(&starg.cond);		
	chk_mutex_uninit(&starg.mtx);		
	pthread_attr_destroy(&attr);
	return t;
}

void *chk_thread_join(chk_thread *t) {
	void *result    = NULL;	
	pthread_join(t->threadid,&result);
	return result;
}

pid_t chk_thread_tid(chk_thread *t) {
	return t->tid;
}

pid_t chk_thread_current_tid()
{
	if(!tid){
		tid = gettid();
	}
	return tid;
}
