#ifndef _CHK_EXCEPT_H
#define _CHK_EXCEPT_H
#include <setjmp.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>  
#include "util/chk_list.h"
#include "util/chk_log.h"

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

#define LOG_STACK_SIZE 64

typedef struct chk_expn_frame      chk_expn_frame;         //异常帧

typedef struct chk_expn_thd        chk_expn_thd;           //线程异常记录


struct chk_expn_frame {
    chk_list_entry entry;
    sigjmp_buf     jumpbuffer;
    int8_t         is_process;    
    const char    *file;
    const char    *func;   
};

struct chk_expn_thd {
	chk_list    expstack;
    const char *exception;
    void       *addr;
    size_t      sz; 
    void       *bt[LOG_STACK_SIZE];          //用于记录异常时的调用栈信息  
};

//获得当前线程的chk_expn_thd
chk_expn_thd *chk_exp_get_thread_expn();

//日志记录调用栈
void chk_exp_log_call_stack(const char *discription);

//日志记录异常调用栈
void chk_exp_log_exption_stack();

void chk_exp_throw(const char *exp);

void chk_exp_rethrow(chk_expn_frame *frame);

static inline chk_list *chk_exp_stack() {
	return &chk_exp_get_thread_expn()->expstack;
}

static inline void chk_exp_push(chk_expn_frame *frame) {
	chk_list_pushfront(chk_exp_stack(),cast(chk_list_entry*,frame));
}

static inline chk_expn_frame *chk_exp_pop() {
    return cast(chk_expn_frame*,chk_list_pop(chk_exp_stack()));
}

static inline chk_expn_frame *chk_exp_top() {
    return cast(chk_expn_frame*,chk_list_begin(chk_exp_stack()));
}

#define TRY                                                         \
    do{                                                             \
    	chk_expn_frame  frame;                                      \
        chk_expn_thd   *expn_thd = chk_exp_get_thread_expn();       \
        frame.entry = (chk_list_entry){0};                          \
        frame.file = __FILE__;                                      \
        frame.func = __FUNCTION__;                                  \
        expn_thd->sz = 0;                                           \
        expn_thd->exception = NULL;                                 \
        frame.is_process = 1;                                       \
        chk_exp_push(&frame);                                       \
        int savesigs= SIGSEGV | SIGBUS | SIGFPE;                    \
    	if(sigsetjmp(frame.jumpbuffer,savesigs) == 0)
	
#define THROW(EXP)                                                  \
    chk_exp_throw(EXP)

#define CATCH(EXP)                                                  \
    else if(!frame.is_process &&                                    \
            0 == strcmp(frame.exception,EXP) &&                     \
            ({frame.is_process=1;frame.is_process;}))

#define CATCH_ALL                                                   \
    else if(!frame.is_process &&                                    \
            ({frame.is_process=1;frame.is_process;}))

#define ENDTRY                                                      \
        if(!frame.is_process) chk_exp_rethrow(&frame);              \
        else chk_exp_pop();                                         \
    }while(0);
 
/*
* 注意在try,catch块中不能直接用return退出当前函数,否则会调至异常
* 栈不能得到正确清理.必须使用RETURN
*/
#define RETURN                                                      \
  do{                                                               \
    chk_expn_frame *top;                                            \
    while((top = chk_exp_top())!=NULL) {                            \
        if(0 == strcmp(top->file,__FILE__) &&                       \
           0 == strcmp(top->func == __FUNCTION__)) {                \
            chk_exp_pop();                                          \
        }else break;                                                \
    };                                                              \
   }while(0);return			

#endif
