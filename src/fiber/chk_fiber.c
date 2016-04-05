#ifdef _MACH
#define _XOPEN_SOURCE
#endif
#include <stdlib.h>
#include <pthread.h>
#include <ucontext.h>
#include <assert.h>
#include "util/chk_list.h"
#include "util/chk_time.h"
#include "util/chk_util.h"
#include "util/chk_timer.h"
#include "../config.h"
#include "chk_fiber.h"

//fiber status
enum {
	start = 1,
	ready,
	running,
	blocking,
	sleeping,
	yield,
	dead,
};

struct chk_fiber {
   chk_dlist_entry    dlist_entry;
   chk_refobj         refobj;	   
   ucontext_t         ucontext;		
   uint8_t            status;
   void   			 *stack;
   void 			 *param;
   chk_timer         *timer;   	   
   void (*main_fun)(void*);
};

typedef struct {
	chk_dlist         ready_list;
	uint32_t          stacksize;
	uint32_t          activecount;
	chk_timermgr     *timermgr;
	struct chk_fiber *current;
	struct chk_fiber *fiber_scheduler;
}*fiber_scheduler_t;

static __thread fiber_scheduler_t t_scheduler = NULL;

static void fiber_main_function(void *arg);

static inline void fiber_switch(struct chk_fiber* from,struct chk_fiber* to) {
	if(to->status == start){
		getcontext(&(to->ucontext));			
		to->ucontext.uc_stack.ss_sp = to->stack;
		to->ucontext.uc_stack.ss_size = t_scheduler->stacksize;
		to->ucontext.uc_link = NULL;
		makecontext(&(to->ucontext),(void(*)())fiber_main_function,1,to);
	}
	to->status = running;
    swapcontext(&(from->ucontext),&(to->ucontext));
}

static void fiber_main_function(void *arg) {
	struct chk_fiber* fiber = (struct chk_fiber*)arg;
	void *param = fiber->param;
	fiber->param = NULL;
	fiber->main_fun(param);
	fiber->status = dead;
	--t_scheduler->activecount;
	fiber_switch(fiber,t_scheduler->fiber_scheduler);
}


static void fiber_destroy(void *_u) {
	struct chk_fiber* fiber = (struct chk_fiber*)((char*)_u -  sizeof(chk_dlist_entry));
	chk_timer_unregister(fiber->timer);
	fiber->timer = NULL;	
	free(fiber->stack);
	free(fiber);	
}


static struct chk_fiber* fiber_create(void*stack,void(*fun)(void*),void *param) {
	struct chk_fiber *fiber = (struct chk_fiber*)calloc(1,sizeof(*fiber));
	chk_refobj_init(&fiber->refobj,fiber_destroy);	
	return fiber;
}

static int add2ready(struct chk_fiber* fiber) {
	if(fiber->status == start || fiber->status == blocking || 
	   fiber->status == sleeping || fiber->status == yield) {
		chk_timer_unregister(fiber->timer);
		fiber->timer = NULL;

		if(0 == chk_dlist_pushback(&(t_scheduler->ready_list),(chk_dlist_entry*)fiber)) {
			if(fiber->status != start){
				fiber->status = ready;
			}
			++t_scheduler->activecount;
			return 0;
		}
	}

	return -1;
}


static int32_t timeout_callback(uint64_t tick,void*ud) {
	struct chk_fiber *fiber = (struct chk_fiber*)ud;
	add2ready(fiber);
	return -1;
}

int chk_fiber_sheduler_init(uint32_t stack_size) {
	if(t_scheduler) return -1;
	t_scheduler = calloc(1,sizeof(*t_scheduler));
	stack_size = chk_get_pow2(stack_size);
	if(stack_size < MIN_FIBER_STACK_SIZE) stack_size = MIN_FIBER_STACK_SIZE;
	t_scheduler->stacksize = stack_size;
	t_scheduler->fiber_scheduler = fiber_create(NULL,NULL,NULL);
	t_scheduler->timermgr = chk_timermgr_new();
	chk_dlist_init(&(t_scheduler->ready_list));
	return 0;
}

int chk_fiber_sheduler_clear() {
	if(t_scheduler && t_scheduler->current == NULL) {
		struct chk_fiber *next;
		while((next = (struct chk_fiber *)chk_dlist_pop(&t_scheduler->ready_list))){
			chk_refobj_release(&next->refobj);
		};			
		chk_refobj_release(&t_scheduler->fiber_scheduler->refobj);
		free(t_scheduler);
		t_scheduler = NULL;
		return 0;
	}
	return -1;
}

chk_fiber_t chk_fiber_spawn(void(*fun)(void*),void *param){
	chk_fiber_t uident;
	bzero(&uident,sizeof(uident));
	if(!t_scheduler || !fun) return uident;
	void *stack = calloc(1,t_scheduler->stacksize);
	if(!stack) return uident;
	struct chk_fiber *fiber = fiber_create(stack,fun,param);
	if(!fiber) return uident;
	fiber->status = start;
	add2ready(fiber);
	return chk_get_refhandle(&fiber->refobj);
}

int chk_fiber_schedule() {
	
	if(!t_scheduler) return -1;

	chk_dlist tmp;
	chk_dlist_init(&tmp);
	struct chk_fiber *next;
	while((next = (struct chk_fiber *)chk_dlist_pop(&t_scheduler->ready_list))){
		t_scheduler->current = next;
		fiber_switch(t_scheduler->fiber_scheduler,next);
		t_scheduler->current = NULL;
		if(next->status == dead){
			chk_refobj_release(&next->refobj);
		}else if(next->status == yield){
			chk_dlist_pushback(&tmp,(chk_dlist_entry*)next);
		}
	};
	//process timeout
	chk_timer_tick(t_scheduler->timermgr,chk_systick64());

	while((next = (struct chk_fiber*)chk_dlist_pop(&tmp))){
		add2ready(next);
	}

	return t_scheduler->activecount;
}

int chk_fiber_yield(){

	struct chk_fiber *current = NULL;

	if(!t_scheduler || !t_scheduler->current)
		return -1;
	
	current = t_scheduler->current;

	current->status = yield;
	--t_scheduler->activecount;
	current->param = NULL;
	fiber_switch(current,t_scheduler->fiber_scheduler);
	return 0;
}

int chk_fiber_block(uint32_t ms,void **param) {
	
	struct chk_fiber *current = NULL;

	if(!t_scheduler || !t_scheduler->current)
		return -1;
	
	if(ms) {
		current = t_scheduler->current;
		chk_timer_unregister(current->timer);
		current->timer = chk_timer_register(t_scheduler->timermgr,ms,timeout_callback,current,chk_systick64());		
	}

	if(current->status != sleeping) current->status = blocking;
	--t_scheduler->activecount;
	if(param) *param = current->param;
	current->param = NULL;
	fiber_switch(current,t_scheduler->fiber_scheduler);
	return 0;		
}

int chk_fiber_sleep(uint32_t ms,void **param) {
	if(ms == 0) return chk_fiber_yield();
	t_scheduler->current->status = sleeping;
	return chk_fiber_block(ms,param);		
}  

chk_fiber_t chk_current_fiber() {
	chk_fiber_t uident;
	bzero(&uident,sizeof(uident));
	if(!t_scheduler) return uident;
	return chk_get_refhandle(&t_scheduler->current->refobj);
}

static inline struct chk_fiber *cast2fiber(chk_fiber_t f) {
	struct chk_fiber *fiber = (struct chk_fiber*)chk_cast2refobj(f);
	if(!fiber) return NULL;
	return (struct chk_fiber*)((char*)fiber  - sizeof(chk_dlist_entry));
}

int chk_fiber_wakeup(chk_fiber_t f,void **param) {
	struct chk_fiber *fiber; 
	int ret = -1;
	if(!t_scheduler) return ret;
	fiber = cast2fiber(f);
	if(!fiber) return ret;
	if(fiber->status == blocking || fiber->status == sleeping) {
		add2ready(fiber);
		fiber->param = param;
		ret = 0;
	}
	chk_refobj_release(&fiber->refobj);
	return ret;
} 

