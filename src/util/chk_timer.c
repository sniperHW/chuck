#define _CORE_
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "util/chk_timer.h"
#include "util/chk_util.h"
#include "util/chk_timer_define.h"
#include "util/chk_time.h"
#include "../config.h"

enum{
	INCB      = 1,
	RELEASING = 1 << 1,
};

static uint16_t wheel_size[] = {
	1000,
	3600,
	24
};

#ifdef TEST_REWINDING    //测试时间回绕
# define REWINDING_TIME   (1000*3600*24)
# define CAL_EXPIRE(T,MS) (((T) + (MS)) % REWINDING_TIME)
# define INC_LASTTICK(T)  ((T) = ((T) + 1) % REWINDING_TIME)
#else
# define REWINDING_TIME   0xFFFFFFFFFFFFFFFF
# define CAL_EXPIRE(T,MS) ((T) + (MS))
# define INC_LASTTICK(T)  (++(T))
#endif

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

static pthread_key_t  timer_pool_key;//用于设置key_destructor以在线程结束时清理timer_pool
__thread chk_dlist   *timer_pool;

struct chk_timer {
	chk_dlist_entry      entry;
	chk_timeout_cb       cb;
	chk_timer_ud_cleaner cleaner;	
	uint32_t             timeout;
	uint64_t             expire;
	int32_t              status;
	chk_ud               ud;
};

static void key_destructor(void *_) {
	chk_timer *t;
	while((t = cast(chk_timer*,chk_dlist_pop(timer_pool))))
		free(t);
	free(timer_pool);
}

static inline chk_timer *get_free_timer() {
	chk_timer *t;
	int32_t    i;
	if(!timer_pool) {
		pthread_key_create(&timer_pool_key,key_destructor);
		timer_pool = malloc(sizeof(*timer_pool));
		if(!timer_pool) {
			return NULL;
		}
		chk_dlist_init(timer_pool);		
	}

	if(chk_dlist_empty(timer_pool)) {
		for(i = 0;i < INIT_FREE_TIMER_SIZE; ++i) {
			t = calloc(1,sizeof(*t));
			if(t) {
				chk_dlist_pushback(timer_pool,cast(chk_dlist_entry*,t));
			}
			else {
				break;
			}
		}		
	}

	t = cast(chk_timer*,chk_dlist_pop(timer_pool));
	if(t) {
		memset(t,0,sizeof(*t));
	}
	return t;
}

static inline void release_timer(chk_timer *t) {
	chk_dlist_pushback(timer_pool,cast(chk_dlist_entry*,t));
}

static wheel *wheel_new(uint8_t type) {
	wheel   *w;
	int16_t  size,i;
	if(type >  wheel_day) return NULL;
	w = calloc(1,sizeof(*w)*wheel_size[type]*sizeof(chk_dlist));
	if(!w) return NULL;	
	w->type = type;
	w->cur  = type == wheel_sec ? -1:0;
	size = cast(uint16_t,wheel_size[type]);
	for(i = 0; i < size; ++i) chk_dlist_init(&w->tlist[i]);
	return w;	
}

static inline uint64_t cal_remain(uint64_t now,uint64_t expire) {
	if(chk_unlikely(now > expire)) {
		//出现时间回绕
		return (REWINDING_TIME - now) + expire;
	} else return expire - now;
}

static inline void _reg(chk_timermgr *m,chk_timer *t,uint64_t tick) {
	uint32_t slot,wsize;
	wheel   *w;
	int8_t   wtype = wheel_sec;
	uint32_t remain = cast(uint32_t,cal_remain(tick,t->expire));
	do {
		w     = m->wheels[wtype];
		wsize = wheel_size[wtype];
		if(wtype == wheel_day || wsize >= remain) {
			slot = w->cur + remain;
			slot = slot >= wsize? slot-wsize:slot;
			chk_dlist_pushback(&w->tlist[slot],&t->entry);
			break;		
		}else {
			remain -= 1;
			remain /= wsize;
			wtype++;
		}		
	}while(1); 
}

static inline void _destroy_timer(chk_timer *t) {
	if(t->cleaner) t->cleaner(&t->ud);
	release_timer(t);
}

static void fire(chk_timermgr *m,wheel *w,uint64_t tick) {
	int32_t    ret;
	chk_timer *t;
	chk_dlist  tlist;	
	if(++(w->cur) == wheel_size[w->type]) w->cur = 0; 
	if(!chk_dlist_empty(&w->tlist[w->cur])) {
		chk_dlist_init(&tlist);
		chk_dlist_move(&tlist,&w->tlist[w->cur]);				
		if(w->type == wheel_sec) {		
			while((t = cast(chk_timer*,chk_dlist_pop(&tlist)))) {
				t->status |= INCB;
				assert(tick == t->expire);
				ret = t->cb(tick,t->ud);
				t->status ^= INCB;
				if(!(t->status & RELEASING) && ret >= 0) {
					if(ret > 0) t->timeout = ret;
					t->expire = CAL_EXPIRE(tick,t->timeout);
					_reg(m,t,tick);
				}else _destroy_timer(t);
			}		
		}else {		
			while((t = cast(chk_timer*,chk_dlist_pop(&tlist))))
				_reg(m,t,tick);
		}
	}
	if(w->cur + 1 == wheel_size[w->type] && w->type < wheel_day)
		fire(m,m->wheels[w->type+1],tick);
}

int32_t chk_timermgr_init(chk_timermgr *m) {
	int8_t i;
	m->ptrtick = NULL;
	for(i = 0; i <= wheel_day; ++i){
		m->wheels[i] = wheel_new(i);
		if(!m->wheels[i]) return -1;		
	}
	return 0;
}

void chk_timermgr_finalize(chk_timermgr *m) {
	int16_t     i,j,size;
	chk_dlist  *tlist;
	chk_timer  *t;
	for(i = 0; i <= wheel_day; ++i) {
		if(!m->wheels[i]) break;
		size = wheel_size[m->wheels[i]->type];
		for(j = 0; j < size; ++j) {
			tlist = &m->wheels[i]->tlist[j];
			while((t = cast(chk_timer*,chk_dlist_pop(tlist))))
				_destroy_timer(t);				
		}
		free(m->wheels[i]);
	}	
}

chk_timermgr *chk_timermgr_new() {
	chk_timermgr *m = calloc(1,sizeof(*m));
	if(!m) return NULL;
	if(0 != chk_timermgr_init(m)) {
		chk_timermgr_finalize(m);
		free(m);
		return NULL;
	}
	return m;
}

void chk_timer_tick(chk_timermgr *m,uint64_t now) {
	if(!m->ptrtick) return;//没有注册过定时器
	while(m->lasttick != now) {
		INC_LASTTICK(m->lasttick);
		fire(m,m->wheels[wheel_sec],m->lasttick);
	}
} 

chk_timer *chk_timer_register(chk_timermgr *m,uint32_t ms,chk_timeout_cb cb,chk_ud ud,uint64_t now) {
	chk_timer *t;
	if(!cb){
		return NULL;
	}
	t = get_free_timer();
	if(!t){
		return NULL;
	}
	t->timeout = ms > MAX_TIMEOUT ? MAX_TIMEOUT : (ms > 0 ? ms : 1);
	t->cb = cb;
	t->ud = ud;
	if(chk_unlikely(!m->ptrtick)){
		m->ptrtick  = &m->lasttick;
		m->lasttick = now;
	}
	t->expire = CAL_EXPIRE(now,t->timeout);
	_reg(m,t,m->lasttick);
	return t;
}

void chk_timer_unregister(chk_timer *t) {
	if(!t || t->status & RELEASING) return;
	t->status |= RELEASING;
	if(!(t->status & INCB)){
		chk_dlist_remove(cast(chk_dlist_entry*,t));
		_destroy_timer(t);
	}
}

void chk_timermgr_del(chk_timermgr *m) {
	chk_timermgr_finalize(m);
	free(m);	
}

void chk_timer_set_ud_cleaner(chk_timer *t,chk_timer_ud_cleaner ud_cleaner) {
	t->cleaner = ud_cleaner;
}

uint64_t chk_timer_expire(chk_timer *t) {
	return t->expire; 
}

uint32_t chk_timer_timeout(chk_timer *t) {
	return t->timeout;
}

uint64_t chk_tmer_inctick(uint64_t tick) {
#ifdef TEST_REWINDING
	return (tick + 1) % REWINDING_TIME;
#else
	return tick + 1;
#endif	

}
