#ifndef _CHK_EVENT_LOOP_H
#define _CHK_EVENT_LOOP_H

#include <stdint.h>
#include "util/chk_timer.h"  
#include "util/chk_list.h"  
#include "event/chk_event.h"
#include    "chk_ud.h"

typedef struct {
    chk_dlist_entry entry;
    chk_ud          data;
    void (*func)(chk_ud);
}chk_clouser;
 
/**
 * 创建一个新的event_loop
 */

chk_event_loop *chk_loop_new();

/**
 * 销毁event_loop
 * @param loop event_loop
 */

void            chk_loop_del(chk_event_loop *loop);

/**
 * 运行event_loop,直到chk_loop_end调用
 * @param loop event_loop
 */

int32_t         chk_loop_run(chk_event_loop *loop);

/**
 * 运行event_loop一次
 * @param loop event_loop
 * @param ms 如果没有事件,调用最多休眠ms毫秒 
 */

int32_t         chk_loop_run_once(chk_event_loop *loop,uint32_t ms);

/**
 * 终止event_loop的运行,(终止之后如需重启再次调用run系列函数)
 * @param loop event_loop
 */

void            chk_loop_end(chk_event_loop *loop);

/**
 * 向event_loop注册一个定时器
 * @param loop event_loop
 * @param timeout 定时器超时时间(毫秒)
 * @param cb 定时器回调函数
 * @param ud 用户传递数据,调用cb时回传 
 */

chk_timer      *chk_loop_addtimer(chk_event_loop *loop,uint32_t timeout,chk_timeout_cb cb,chk_ud ud);

/**
 * 向event_loop注册一个handle
 * @param loop event_loop
 * @param handle 要注册的handle
 * @param cb handle有事件时的回调函数
 */

int32_t         chk_loop_add_handle(chk_event_loop *loop,chk_handle *handle,chk_event_callback cb);

/**
 * 移除handle与event_loop的关联(如果有),移除后handle将不再触发任何事件回调
 * @param handle 要移除的handle
 */

int32_t         chk_loop_remove_handle(chk_handle *handle);


int32_t         chk_loop_set_idle_func(chk_event_loop *loop,void (*idle_cb)());

int32_t         chk_loop_post_closure(chk_event_loop *loop,void (*func)(chk_ud),chk_ud ud);

#if CHUCK_LUA

#include "lua/chk_lua.h"

int32_t         chk_loop_set_idle_func_lua(chk_event_loop *loop,chk_luaRef idle_cb);

#endif

#endif