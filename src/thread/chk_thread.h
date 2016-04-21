#ifndef _THREAD_H
#define _THREAD_H
#include <pthread.h>
#include <stdint.h> 
#include "thread/chk_sync.h"

typedef struct chk_thread chk_thread;

//创建线程,当新线程开始运行后函数才返回
chk_thread  *chk_thread_new(void *(*routine)(void*),void *ud);

void        *chk_thread_join(chk_thread*);

void         chk_thread_del(chk_thread*);

//获取某个线程的tid
pid_t        chk_thread_tid(chk_thread*);

//获取当前线程的tid
pid_t        chk_thread_current_tid();

#endif
