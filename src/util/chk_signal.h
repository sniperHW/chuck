#ifndef  _CHK_SIGNAL_H
#define _CHK_SIGNAL_H

#include "event/chk_event.h"
#include <signal.h>

typedef void (*signal_cb)(void *);

/*
 * 因为异常处理机制,SIGSEGV不允许被watch,watch SIGSEGV将返回失败
*/
int32_t chk_watch_signal(chk_event_loop *,int32_t signo,signal_cb,void *ud,void (*ud_dctor)(void*));

void    chk_unwatch_signal(int32_t signo);


#endif