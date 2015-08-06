
/*
*  一个简单的三级时间轮实现,无时间回绕问题
*/

#ifndef _CHK_TIMER_H
#define _CHK_TIMER_H

#include <stdint.h>

#define MAX_TIMEOUT (1000*3600*24-1)

typedef struct chk_timer     chk_timer;

typedef struct chk_timermgr  chk_timermgr;

typedef int32_t (*chk_timeout_cb)(uint64_t tick,void*ud);

typedef void    (*chk_timer_ud_cleaner)(void*);

chk_timermgr *chk_timermgr_new();

void          chk_timermgr_del(chk_timermgr*);

chk_timer    *chk_timer_register(chk_timermgr*,uint32_t ms,chk_timeout_cb,void *ud,uint64_t now);

void 		  chk_timer_unregister(chk_timer*);

//设置ud清理函数
void          chk_timer_set_ud_cleaner(chk_timer*,chk_timer_ud_cleaner);

void 		  chk_timer_tick(chk_timermgr*,uint64_t now);

//just for test
uint64_t      chk_timer_expire(chk_timer*);

uint32_t      chk_timer_timeout(chk_timer*);

uint64_t      chk_tmer_inctick(uint64_t tick); 


#endif