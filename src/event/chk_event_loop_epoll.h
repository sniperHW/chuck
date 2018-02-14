#ifdef _CORE_

#include <sys/timerfd.h>
#include "util/chk_util.h"

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

static int32_t events_mod(chk_handle *h,int32_t events);

int32_t chk_watch_handle(chk_event_loop *e,chk_handle *h,int32_t events) {
	struct epoll_event ev = {0};
	ev.data.ptr = h;
	ev.events = events;
	if(h->loop) {
		return chk_error_duplicate_add_handle;
	}
	if(0 != epoll_ctl(e->epfd,EPOLL_CTL_ADD,h->fd,&ev)){ 
		CHK_SYSLOG(LOG_ERROR,"epoll_ctl() failed errno:%d",errno);
		return chk_error_epoll_add;
	}
	h->events = events;
	h->loop = e;
	chk_dlist_pushback(&e->handles,cast(chk_dlist_entry*,h));
	return chk_error_ok;
}


int32_t chk_unwatch_handle(chk_handle *h) {
	struct epoll_event ev = {0};
	chk_event_loop *e = h->loop;
	if(!e) {
		return chk_error_no_event_loop;
	}
	if(0 != epoll_ctl(e->epfd,EPOLL_CTL_DEL,h->fd,&ev)){ 
		CHK_SYSLOG(LOG_ERROR,"epoll_ctl() failed errno:%d",errno);
		return chk_error_epoll_del; 
	}
	h->events = 0;
	h->loop = NULL;
	chk_dlist_remove(&h->ready_entry);
	chk_dlist_remove(&h->entry);
	return chk_error_ok;	
}

int32_t events_mod(chk_handle *h,int32_t events) {
	chk_event_loop *e = h->loop;	
	struct epoll_event ev = {0};
	if(!e) {
		CHK_SYSLOG(LOG_DEBUG,"NULL == h->loop");
		return chk_error_no_event_loop;
	}
	ev.data.ptr = h;
	ev.events = events;
	if(0 != epoll_ctl(e->epfd,EPOLL_CTL_MOD,h->fd,&ev)){ 
		CHK_SYSLOG(LOG_ERROR,"epoll_ctl() failed errno:%d",errno);
		return chk_error_epoll_mod; 
	}
	h->events = events;		
	return chk_error_ok;	
}


int32_t chk_events_enable(chk_handle *h,int32_t events) {
	return events_mod(h,h->events | events);
}

int32_t chk_events_disable(chk_handle *h,int32_t events) {
	return events_mod(h,h->events & (~events));
}

int32_t chk_is_read_enable(chk_handle*h) {
	return (h->events & EPOLLIN) > 0 ? 1:0;
}

int32_t chk_is_write_enable(chk_handle*h) {
	return (h->events & EPOLLOUT) > 0 ? 1:0;
}


int32_t chk_loop_init(chk_event_loop *e) {
	assert(e);
	struct epoll_event ev = {0};
	int32_t epfd = epoll_create1(EPOLL_CLOEXEC);
	if(epfd < 0) {
		CHK_SYSLOG(LOG_ERROR,"epoll_create1() failed,errno:%d",errno);
		return chk_error_create_epoll;
	}

	if(chk_error_ok != chk_create_notify_channel(e->notifyfds)) {
		CHK_SYSLOG(LOG_ERROR,"chk_create_notify_channel() failed");
		close(epfd);
		return chk_error_create_notify_channel;		
	}		
	e->epfd = epfd;
	e->idle.fire_tick = 0;
	e->tfd  = -1;
	e->maxevents = 64;
	e->events = calloc(1,(sizeof(*e->events)*e->maxevents));
	if(!e->events) {
		CHK_SYSLOG(LOG_ERROR,"create e->events failed,no memory");		
		close(epfd);
		chk_close_notify_channel(e->notifyfds);
		return chk_error_no_memory;		
	}
	e->timermgr = NULL;
	ev.data.fd = e->notifyfds[0];
	ev.events = EPOLLIN;
	if(0 != epoll_ctl(e->epfd,EPOLL_CTL_ADD,ev.data.fd,&ev)) {
		CHK_SYSLOG(LOG_ERROR,"epoll_ctl() failed errno:%d",errno);
		close(epfd);
		chk_close_notify_channel(e->notifyfds);
		free(e->events);
		return chk_error_epoll_add;
	}
	e->threadid = chk_thread_current_tid();
	chk_dlist_init(&e->handles);	
	return chk_error_ok;
}

void chk_loop_finalize(chk_event_loop *e) {
	assert(e);
	chk_handle *h;
	if(e->tfd >= 0) {
		close(e->tfd);
		chk_timermgr_del(e->timermgr);
	}

	while((h = cast(chk_handle*,chk_dlist_pop(&e->handles)))){
		h->on_events(h,CHK_EVENT_LOOPCLOSE);
		chk_unwatch_handle(h);
	}
	e->tfd  = -1;
	close(e->epfd);
	chk_close_notify_channel(e->notifyfds);
	free(e->events);
	chk_idle_finalize(e);
}

int32_t _loop_run(chk_event_loop *e,uint32_t ms,int once) {
	int32_t ret = chk_error_ok;
	int32_t i,nfds,ticktimer;
	int64_t _;
	uint64_t t;
	chk_handle         *h;
	chk_dlist           ready_list;
	chk_dlist_entry    *read_entry;
	chk_clouser        *c;
	struct epoll_event *tmp;	
	do {
		ticktimer = 0;
		chk_dlist_init(&ready_list);
		if(chk_list_size(&e->closures) > 0) {
			ms = 0;
		}
		nfds = TEMP_FAILURE_RETRY(epoll_wait(e->epfd,e->events,e->maxevents,once ? ms : -1));
		t = chk_systick64();
		if(nfds > 0) {
			e->status |= INLOOP;
			for(i=0; i < nfds ; ++i) {
				struct epoll_event *event = &e->events[i];
				if(event->data.fd == e->notifyfds[0]) {
					int32_t _;
					while(TEMP_FAILURE_RETRY(read(e->notifyfds[0],&_,sizeof(_))) > 0);
					goto loopend;	
				}else if(event->data.fd == e->tfd) {
					TEMP_FAILURE_RETRY(read(e->tfd,&_,sizeof(_))); 
					ticktimer = 1;//优先处理其它事件,定时器事件最后处理
				}else {
					h = cast(chk_handle*,event->data.ptr);
					h->active_evetns = event->events;
					chk_dlist_pushback(&ready_list,&h->ready_entry);
				}
			}
			while((read_entry = chk_dlist_pop(&ready_list))) {
				h = READY_TO_HANDLE(read_entry);
				h->on_events(h,h->active_evetns);
				//这里之后不能访问h,因为h在on_events中可能被释放
			}
			if(ticktimer) chk_timer_tick(e->timermgr,chk_accurate_tick64());
			e->status ^= INLOOP;
			if(e->status & CLOSING) break;
			if(nfds == e->maxevents){
				e->maxevents <<= 2;
				tmp = realloc(e->events,sizeof(*e->events)*e->maxevents);
				if(NULL == tmp) {
					CHK_SYSLOG(LOG_ERROR,"realloc() failed");
					ret = chk_error_no_memory;
					break;
				}
				e->events = tmp;
			}				
		}else if(nfds < 0) {
			CHK_SYSLOG(LOG_ERROR,"epoll_wait() failed errno:%d",errno);
			ret = chk_error_loop_run;
			break;
		}
		int cc = 0;
		while((c = (chk_clouser*)chk_list_pop(&e->closures))) {
			c->func(c->data);
			chk_destroy_closure(c);
			if(++cc > 1024) {
				break;
			}
		}		
		chk_check_idle(e,chk_systick64() - t);	
	}while(!once);

loopend:	
	if(e->status & CLOSING) {
		chk_loop_finalize(e);
	}	
	return ret;
}

chk_timer *chk_loop_addtimer(chk_event_loop *e,uint32_t timeout,chk_timeout_cb cb,chk_ud ud) {
	struct   itimerspec spec;
    uint64_t tick;
	struct   epoll_event ev = {0};   
	if(e->tfd < 0) {
    	e->tfd = timerfd_create(CLOCK_MONOTONIC,TFD_CLOEXEC|TFD_NONBLOCK);
	    if(e->tfd < 0) {
	    	CHK_SYSLOG(LOG_ERROR,"timerfd_create failed errno:%d",errno);
	    	return NULL;
	    }
    	spec.it_value.tv_sec = 0;
        spec.it_value.tv_nsec = 1*1000*1000;
        spec.it_interval.tv_sec = 0;
        spec.it_interval.tv_nsec = 1*1000*1000;  
	    
	    if(0 != timerfd_settime(e->tfd,0,&spec,0)) {
	    	CHK_SYSLOG(LOG_ERROR,"timerfd_settime failed errno:%d",errno);
	        close(e->tfd);
	        e->tfd = -1;
	        return NULL;		
		}
		ev.data.fd = e->tfd;
		ev.events  = EPOLLIN;
		if(0 != epoll_ctl(e->epfd,EPOLL_CTL_ADD,e->tfd,&ev)) {
			CHK_SYSLOG(LOG_ERROR,"epoll_ctl failed errno:%d",errno);			
	        close(e->tfd);
	        e->tfd = -1;
	        return NULL;				
		}
		e->timermgr = chk_timermgr_new();

		if(!e->timermgr) {
			CHK_SYSLOG(LOG_ERROR,"call chk_timermgr_new() failed");			
	        close(e->tfd);
	        e->tfd = -1;			
			return NULL;
		}
	}
	tick = chk_accurate_tick64();
	return chk_timer_register(e->timermgr,timeout,cb,ud,tick); 
}

#endif
