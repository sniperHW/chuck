
/*
*  一个简单的三级时间轮实现,无时间回绕问题
*/

#ifndef _CHK_TIMER_H
#define _CHK_TIMER_H

#include <stdint.h>
#include "util/chk_list.h"
#include "chk_ud.h"



typedef struct chk_timer     chk_timer;

typedef struct chk_timermgr  chk_timermgr;

typedef int32_t (*chk_timeout_cb)(uint64_t tick,chk_ud ud);

typedef void    (*chk_timer_ud_cleaner)(chk_ud*);


/**
 * 创建定时管理器
 */

chk_timermgr *chk_timermgr_new();

/**
 * 删除定时管理器
 */

void chk_timermgr_del(chk_timermgr*);

/**
 * 注册一个定时器 
 * @param m 定时管理器
 * @param ms 超时时间(毫秒)
 * @param cb 超时回调
 * @param ud 传递给cb的用户数据
 * @param now 当前时间	
 */

chk_timer *chk_timer_register(chk_timermgr *m,uint32_t ms,chk_timeout_cb cb,chk_ud ud,uint64_t now);

/**
 * 删除定时器
 * @param t 定时器
 */

void chk_timer_unregister(chk_timer *t);

/**
 * 设置用于清理ud的函数 
 * @param t 定时器
 * @param clearfn 当t被销毁时调用这个函数清理ud	
 */
 
void chk_timer_set_ud_cleaner(chk_timer *t,chk_timer_ud_cleaner clearfn);

/**
 * 驱动定时管理器 
 * @param m 定时管理器
 * @param now 当前时间	
 */

void chk_timer_tick(chk_timermgr *m,uint64_t now);

//just for test
uint64_t chk_timer_expire(chk_timer*);

uint32_t chk_timer_timeout(chk_timer*);

uint64_t chk_tmer_inctick(uint64_t tick); 


#endif
