#include <assert.h>
#include "thread/chk_thread.h"
#include "util/chk_log.h"

#define _CORE_

#include "event/chk_event_loop.h" 
#include "event/chk_event_loop_define.h"

static int32_t stopevent = 1;

#define READY_TO_HANDLE(ENTRY)                                              \
    (chk_handle*)(((char*)(ENTRY))-sizeof(chk_dlist_entry))


enum {
	INLOOP  =  1 << 1,
	CLOSING =  1 << 2,
};


static inline void chk_idle_finalize(chk_event_loop *e) {
	if(e->idle.idle_timer) {
		chk_timer_unregister(e->idle.idle_timer);
	}

#ifdef CHUCK_LUA
	if(e->idle.on_idle.L) {
		chk_luaRef_release(&e->idle.on_idle);
	}
#endif

}

static inline void chk_check_idle(chk_event_loop *e,uint64_t elapse) {

	if(e->idle.fire_tick == 0) {
		return;
	}

/*
*	当前实际时间与定时器到器时间相差小于1ms,表明系统没有太多事情需要干
*/	

#ifdef CHUCK_LUA

	if(e->idle.on_idle.L) {
		if(elapse < 1) {	
			const char   *error; 
			if(NULL != (error = chk_Lua_PCallRef(e->idle.on_idle,":"))) {
				CHK_SYSLOG(LOG_ERROR,"error on idle %s",error);
			}
		}
	}

#else

	if(e->idle.on_idle) {
		if(elapse < 1) {	
			e->idle.on_idle();
		}
	}

#endif

	e->idle.fire_tick = 0;

}


void chk_destroy_closure(chk_clouser *c) {
	#ifdef CHUCK_LUA
		if(c->data.v.lr.L) {
			chk_luaRef_release(&c->data.v.lr);
		}
	#endif
	free(c);
}

#ifdef _LINUX
#	include "chk_event_loop_epoll.h"
#elif  _MACH
#	include "chk_event_loop_kqueue.h"
#else
#	error "un support platform!"		
#endif

int32_t chk_loop_post_closure(chk_event_loop *loop,void (*func)(chk_ud),chk_ud ud) {
	if(!loop || !func) {
		return chk_error_invaild_argument;
	}
	chk_clouser *c = (chk_clouser*)calloc(1,sizeof(*c));
	c->data = ud;
	c->func = func;
	chk_list_pushback(&loop->closures,(chk_list_entry*)c);
	return 0;
}

int32_t chk_loop_run_once(chk_event_loop *e,uint32_t ms) {
	return _loop_run(e,ms,1);
}

int32_t chk_loop_add_handle(chk_event_loop *e,chk_handle *h,chk_event_callback cb) {
	if(h->loop) return chk_error_duplicate_add_handle;
	return h->handle_add(e,h,cb);
}

void chk_loop_end(chk_event_loop *e) {
	TEMP_FAILURE_RETRY(write(e->notifyfds[1],&stopevent,sizeof(stopevent)));
}

chk_event_loop *chk_loop_new() {
	chk_event_loop *ep = calloc(1,sizeof(*ep));
	if(!ep) return NULL;
	chk_list_init(&ep->closures);
	if(chk_error_ok != chk_loop_init(ep)) {
		CHK_SYSLOG(LOG_ERROR,"chk_loop_init() failed");
		free(ep);
		ep = NULL;
	}
	return ep;
}

void chk_loop_del(chk_event_loop *e) {
	if(e->threadid != chk_thread_current_tid()) {
		CHK_SYSLOG(LOG_ERROR,"e->threadid != chk_thread_current_tid()");
		return;
	}
	if(e->status & INLOOP)
		e->status |= CLOSING;
	else {
		chk_loop_finalize(e);
		chk_clouser *c;
		while((c = (chk_clouser*)chk_list_pop(&e->closures))) {
			c->func(c->data);
			chk_destroy_closure(c);
		}		
		free(e);
	}
}

int32_t chk_loop_run(chk_event_loop *e) {
	return _loop_run(e,0,0);
}

int32_t chk_loop_remove_handle(chk_handle *h) {
	return chk_unwatch_handle(h);
}


static int32_t on_idle_timer_timeout(uint64_t tick,chk_ud ud) {
	chk_event_loop *loop = (chk_event_loop*)ud.v.val;

#ifdef CHUCK_LUA
	if(!loop->idle.on_idle.L)
#else
	if(!loop->idle.on_idle)
#endif
	{	
		//on_idle被置空,取消定时器
		loop->idle.idle_timer = NULL;
		return -1;
	}
	else {
		/*
		 *  定时器的执行顺序是不确定的，所以这里不会立刻执行空闲判断，而是把预期的触发时间记录下来
		 *  在循环的最后再判定是否空闲 
		 *  
		*/
		loop->idle.fire_tick = chk_systick64();
		return 0;
	}
}

#ifdef CHUCK_LUA

int32_t chk_loop_set_idle_func_lua(chk_event_loop *loop,chk_luaRef idle_cb) {
	if(!loop) {
		return chk_error_invaild_argument;
	}

	loop->idle.on_idle = idle_cb;

	if(loop->idle.on_idle.L) {
		if(!loop->idle.idle_timer) {
			loop->idle.idle_timer = chk_loop_addtimer(loop,CHK_IDLE_TIMER_TIMEOUT,on_idle_timer_timeout,chk_ud_make_void(loop));
			if(!loop->idle.idle_timer) {
				chk_luaRef_release(&loop->idle.on_idle);
				return chk_error_add_timer;
			}
		}
	}
	return 0;
}

#else

int32_t chk_loop_set_idle_func(chk_event_loop *loop,void (*idle_cb)()) {
	if(!loop) {
		return chk_error_invaild_argument;
	}

	loop->idle.on_idle = idle_cb;

	if(loop->idle.on_idle) {
		if(!loop->idle.idle_timer) {
			loop->idle.idle_timer = chk_loop_addtimer(loop,CHK_IDLE_TIMER_TIMEOUT,on_idle_timer_timeout,chk_ud_make_void(loop));
			if(!loop->idle.idle_timer) {
				loop->idle.on_idle = NULL;
				return chk_error_add_timer;
			}
		}
	}
	return 0;
}

#endif
