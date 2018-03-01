#ifdef _CORE_

#include <fcntl.h>

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif


static int32_t _add_event(chk_event_loop *e,chk_handle *h,int32_t event) {
	struct kevent ke;
	EV_SET(&ke, h->fd, event, EV_ADD, 0, 0, h);
	if(0 != kevent(e->kfd, &ke, 1, NULL, 0, NULL)){
		CHK_SYSLOG(LOG_ERROR,"kevent() failed errno:%d",errno);
		return chk_error_kevent_add;
	}
	return chk_error_ok;	
}

static int32_t _enable_event(chk_event_loop *e,chk_handle *h,int32_t event) {
	struct kevent ke;
	EV_SET(&ke, h->fd, event,EV_ENABLE, 0, 0, h);
	if(0 != kevent(e->kfd, &ke, 1, NULL, 0, NULL)) {
		if(ENOENT == errno) {
			return _add_event(e,h,event); 
		}
		else {
			CHK_SYSLOG(LOG_ERROR,"kevent() failed errno:%d",errno);
			return chk_error_kevent_enable;
		}
	}
	return chk_error_ok;
}

static int32_t _disable_event(chk_event_loop *e,chk_handle *h,int32_t event) {
	struct kevent ke;
	EV_SET(&ke, h->fd, event,EV_DISABLE, 0, 0, h);
	if(0 != kevent(e->kfd, &ke, 1, NULL, 0, NULL)){
		CHK_SYSLOG(LOG_ERROR,"kevent() failed errno:%d",errno);
		return chk_error_kevent_disable;
	}
	return chk_error_ok;	
}

int32_t chk_watch_handle(chk_event_loop *e,chk_handle *h,int32_t events) {

	int32_t ret;
	struct kevent ke;
	if(h->loop) {
		return chk_error_duplicate_add_handle;
	}
	if(events & CHK_EVENT_READ) {
		if((ret = _add_event(e,h,EVFILT_READ)) != chk_error_ok){
			CHK_SYSLOG(LOG_ERROR,"_add_event() failed");
			return chk_error_kevent_add;
		}
	}
	if(events & CHK_EVENT_WRITE) {
		if((ret = _add_event(e,h,EVFILT_WRITE)) != chk_error_ok){
			CHK_SYSLOG(LOG_ERROR,"_add_event() failed");
			EV_SET(&ke, h->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
			kevent(e->kfd, &ke, 1, NULL, 0, NULL);		
			return chk_error_kevent_add;
		}
	}
	h->events = events;
	h->loop = e;
	chk_dlist_pushback(&e->handles,cast(chk_dlist_entry*,h));
	return chk_error_ok;	
}

int32_t chk_unwatch_handle(chk_handle *h) {
	struct kevent ke;
	chk_event_loop *e = h->loop;
	if(!e){
		return chk_error_no_event_loop;
	}
	EV_SET(&ke, h->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	kevent(e->kfd, &ke, 1, NULL, 0, NULL);
	EV_SET(&ke, h->fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	kevent(e->kfd, &ke, 1, NULL, 0, NULL);
	h->events = 0;
	h->loop = NULL;
	chk_dlist_remove(&h->ready_entry);
	chk_dlist_remove(&h->entry);
	return chk_error_ok;	
}

int32_t chk_events_enable(chk_handle *h,int32_t events) {
	int32_t ret;
	chk_event_loop *e = h->loop;	
	if(!e) return chk_error_no_event_loop;
	if(events & CHK_EVENT_READ) {
		if((ret = _enable_event(e,h,EVFILT_READ)) != chk_error_ok){
			CHK_SYSLOG(LOG_ERROR,"_enable_event() failed");			
			return chk_error_kevent_enable;
		}
		h->events |= CHK_EVENT_READ;
	}

	if(events & CHK_EVENT_WRITE) {
		if((ret = _enable_event(e,h,EVFILT_WRITE)) != chk_error_ok){
			CHK_SYSLOG(LOG_ERROR,"_enable_event() failed");			
			return chk_error_kevent_enable;
		}
		h->events |= CHK_EVENT_WRITE;		
	}	
	return chk_error_ok;
}

int32_t chk_events_disable(chk_handle *h,int32_t events) {
	int32_t ret;
	chk_event_loop *e = h->loop;
	if(!e) return chk_error_no_event_loop;
	if(events & CHK_EVENT_READ) {
		if((ret = _disable_event(e,h,EVFILT_READ)) != chk_error_ok){
			CHK_SYSLOG(LOG_ERROR,"_disable_event() failed");
			return chk_error_kevent_disable;
		}
		h->events &= (~CHK_EVENT_READ);
	}

	if(events & CHK_EVENT_WRITE) {
		if((ret = _disable_event(e,h,EVFILT_WRITE)) != chk_error_ok){
			CHK_SYSLOG(LOG_ERROR,"_disable_event() failed");			
			return chk_error_kevent_disable;
		}
		h->events &= (~CHK_EVENT_WRITE);		
	}	
	return chk_error_ok;
}

int32_t chk_is_read_enable(chk_handle*h) {
	return (h->events & CHK_EVENT_READ) > 0 ? 1:0;
}

int32_t chk_is_write_enable(chk_handle*h) {
	return (h->events & CHK_EVENT_WRITE) > 0 ? 1:0;
}

extern int32_t easy_noblock(int32_t fd,int32_t noblock); 

int32_t chk_loop_init(chk_event_loop *e) {
	struct kevent ke;	
	//different from epoll,The queue is not inherited by a child created with fork(2)
	int32_t kfd = kqueue();
	if(kfd < 0) {
		CHK_SYSLOG(LOG_ERROR,"kevent() failed errno:%d",errno);
		return chk_error_create_kqueue;
	}
	if(chk_error_ok != chk_create_notify_channel(e->notifyfds)) {
		CHK_SYSLOG(LOG_ERROR,"chk_create_notify_channel() failed");
		close(kfd);
		return chk_error_create_notify_channel;		
	}
	e->kfd = kfd;
	e->idle.fire_tick = 0;
	e->timermgr = NULL;
	e->maxevents = 64;
	e->events = calloc(1,(sizeof(*e->events)*e->maxevents));

	if(!e->events) {
		CHK_SYSLOG(LOG_ERROR,"create e->events failed,no memory");
		close(kfd);
		chk_close_notify_channel(e->notifyfds);
		return chk_error_no_memory;
	}
	
	EV_SET(&ke,e->notifyfds[0], EVFILT_READ, EV_ADD, 0, 0, (void*)(int64_t)e->notifyfds[0]);
	if(0 != kevent(kfd, &ke, 1, NULL, 0, NULL)){
		CHK_SYSLOG(LOG_ERROR,"kevent() failed errno:%d",errno);
		close(kfd);
		chk_close_notify_channel(e->notifyfds);
		free(e->events);
		return chk_error_create_notify_channel;	
	}
	e->threadid = chk_thread_current_tid();
	chk_dlist_init(&e->handles);			
	return chk_error_ok;
}

void chk_loop_finalize(chk_event_loop *e) {
	chk_handle *h;
	if(e->timermgr){
		chk_timermgr_del(e->timermgr);
	}
	while((h = cast(chk_handle*,chk_dlist_pop(&e->handles)))){
		h->on_events(h,CHK_EVENT_LOOPCLOSE);
		chk_unwatch_handle(h);
	}
	chk_close_notify_channel(e->notifyfds);
	free(e->events);
	chk_idle_finalize(e);
}

int32_t _loop_run(chk_event_loop *e,uint32_t ms,int once) {
	int32_t ret = chk_error_ok;
	int32_t i,nfds,ticktimer;
	chk_handle      *h;
	chk_dlist        ready_list;
	chk_dlist_entry *read_entry;
	struct timespec ts,*pts;
	uint64_t msec,t;
	chk_clouser     *c;	
	struct kevent   *tmp;
	do {
		ticktimer = 0;
		chk_dlist_init(&ready_list);
		if(once || chk_list_size(&e->closures) > 0){
			if(chk_list_size(&e->closures) > 0) {
				ts.tv_nsec = 0;
				ts.tv_sec  = 0;				
			} else {			
				msec = ms%1000;
				ts.tv_nsec = (msec*1000*1000);
				ts.tv_sec   = (ms/1000);
			}
			pts = &ts;
		}
		else {
			pts = NULL;
		}

		nfds = TEMP_FAILURE_RETRY(kevent(e->kfd, NULL, 0, e->events,e->maxevents,pts));
		t = chk_systick64();
		if(nfds > 0) {
			e->status |= INLOOP;
			for(i=0; i < nfds ; ++i) {
				struct kevent *event = &e->events[i];
				if(event->udata == (void*)(int64_t)e->notifyfds[0]) {
					int32_t _;
					while(TEMP_FAILURE_RETRY(read(e->notifyfds[0],&_,sizeof(_))) > 0);
					goto loopend;	
				}else if(event->udata == e->timermgr){
					ticktimer = 1;//优先处理其它事件,定时器事件最后处理
				}
				else {
					h = cast(chk_handle*,event->udata);
					h->active_evetns = 0;
					if(event->filter == EVFILT_READ){
						h->active_evetns |= CHK_EVENT_READ;
					}

					if(event->filter == EVFILT_WRITE){
						h->active_evetns |= CHK_EVENT_WRITE;
					}

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
			CHK_SYSLOG(LOG_ERROR,"kevent() failed errno:%d",errno);
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

	uint64_t tick;
	int32_t  flags =  EV_ADD | EV_ENABLE;
	struct kevent change;
	if(!e->timermgr){
		e->timermgr = chk_timermgr_new();
		if(!e->timermgr) {
			CHK_SYSLOG(LOG_ERROR,"call chk_timermgr_new() failed");
			return NULL;
		}
		EV_SET(&change, 1, EVFILT_TIMER,flags, NOTE_USECONDS, 1000, e->timermgr);
		struct timespec thetime;
  		bzero(&thetime,sizeof(thetime));
  		if(-1 == kevent(e->kfd, &change,1, NULL , 0, &thetime)){
  			CHK_SYSLOG(LOG_ERROR,"kevent timer failed");
  			chk_timermgr_del(e->timermgr);
  			e->timermgr = NULL;
  			return NULL;
  		}

	}
	tick = chk_accurate_tick64();
	return chk_timer_register(e->timermgr,timeout,cb,ud,tick); 
}


#endif
