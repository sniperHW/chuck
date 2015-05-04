#include <stdlib.h>
#include <sys/types.h>
#include <sys/syscall.h>  
#include "thread.h"
#include "sync.h"

#define gettidv1() syscall(__NR_gettid)  
#define gettidv2() syscall(SYS_gettid)  

__thread pid_t tid = 0;


static void child(){
	tid = 0;
}

struct start_arg{
	void           *arg;
	void           *(*routine)(void*);
	uint8_t         running;
	mutex      	   *mtx;
	condition 	   *cond;
};


static void *start_routine(void *_){
	struct start_arg *starg = (struct start_arg*)_;
	void *arg = starg->arg;
	void*(*routine)(void*) = starg->routine;
	mutex_lock(starg->mtx);
	if(!starg->running){
		starg->running = 1;
		mutex_unlock(starg->mtx);
		condition_signal(starg->cond);
	}
	return routine(arg);
}


thread *thread_new(int32_t flag,void *(*routine)(void*),void *ud){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	if(flag | JOINABLE)
		pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
	else
		pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);	
	thread *t = calloc(1,sizeof(*t));
	if(flag | WAITRUN){
		struct start_arg starg;

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
		
	}else{
		pthread_create(&t->threadid,&attr,routine,ud);
	}
	pthread_attr_destroy(&attr);
	return t;
}

void* thread_join(thread *t)
{
	void *result = NULL;
	pthread_join(t->threadid,&result);
	return result;
}

void* thread_del(thread *t)
{
	void *result = NULL;
	pthread_join(t->threadid,&result);
	free(t);
	return result;	
}

pid_t   thread_id(){
	if(!tid){
		tid = gettidv1();
		pthread_atfork(NULL,NULL,child);
	}
	return tid;
}

