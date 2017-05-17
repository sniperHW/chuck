#ifndef  _CHK_SIGNAL_H
#define _CHK_SIGNAL_H

#include "event/chk_event.h"
#include "chk_ud.h"
#include <signal.h>

typedef void (*signal_cb)(chk_ud);

/*
 * 因为异常处理机制,SIGSEGV不允许被watch,watch SIGSEGV将返回失败
*/
int32_t chk_watch_signal(chk_event_loop *,int32_t signo,signal_cb,chk_ud ud,void (*ud_dctor)(chk_ud));

void    chk_unwatch_signal(int32_t signo);


#endif