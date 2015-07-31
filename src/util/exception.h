#ifndef _CHK_EXCEPT_H
#define _CHK_EXCEPT_H
#include <setjmp.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>  
#include "util/list.h"
#include "util/log.h"

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

typedef struct chk_expn_frame      chk_expn_frame;         //异常帧
typedef struct chk_expn_thd        chk_expn_thd;           //线程异常记录


struct chk_expn_frame {
    listnode       entry;
    sigjmp_buf     jumpbuffer;
    int8_t         is_process;
    const char    *exception;     
    const char    *file;
    const char    *func;   
};

struct chk_expn_thd {
    list  expstack;  
};

//获得当前线程的chk_expn_thd
chk_expn_thd *chk_exp_get_thread_expn();

//日志记录调用栈
void chk_exp_log_stack();

void chk_exp_throw(const char *exp);

void chk_exp_rethrow(chk_expn_frame *frame);

static inline list *chk_exp_stack() {
    return &chk_exp_get_thread_expn()->expstack;
}

static inline void chk_exp_push(chk_expn_frame *frame) {
    list_pushfront(chk_exp_stack(),cast(listnode*,frame));
}

static inline chk_expn_frame *chk_exp_pop() {
    return cast(chk_expn_frame*,list_pop(chk_exp_stack()));
}

static inline chk_expn_frame *chk_exp_top() {
    return cast(chk_expn_frame*,list_begin(chk_exp_stack()));
}

#define TRY                                                         \
    do{                                                             \
        chk_expn_frame  frame;                                      \
        frame.entry = (listnode){0};                                \
        frame.file = __FILE__;                                      \
        frame.func = __FUNCTION__;                                  \
        frame.exception = "";                                       \
        frame.is_process = 1;                                       \
        chk_exp_push(&frame);                                       \
        int savesigs= SIGSEGV | SIGBUS | SIGFPE;                    \
        if(sigsetjmp(frame.jumpbuffer,savesigs) == 0)
    
#define THROW(EXP)                                                  \
    chk_exp_throw(EXP,__FILE__,__FUNCTION__)

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
