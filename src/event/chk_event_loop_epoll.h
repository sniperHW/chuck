#ifdef _CORE_

#include <sys/timerfd.h>

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

static int32_t events_mod(chk_handle *h,int32_t events);

int32_t chk_events_add(chk_event_loop *e,chk_handle *h,int32_t events) {
	//assert((events & EPOLLET) == 0);
	struct epoll_event ev = {0};
	ev.data.ptr = h;
	ev.events = events;
	errno = 0;
	if(!h->loop){
		if(0 != epoll_ctl(e->epfd,EPOLL_CTL_ADD,h->fd,&ev)) 
			return errno;
		h->events = events;
		h->loop = e;
		chk_dlist_pushback(&e->handles,cast(chk_dlist_entry*,h));
		return 0;
	}else
		return events_mod(h,events);
}


int32_t chk_events_remove(chk_handle *h) {
	struct epoll_event ev = {0};
	chk_event_loop *e = h->loop;
	errno = 0;
	if(0 != epoll_ctl(e->epfd,EPOLL_CTL_DEL,h->fd,&ev)) 
		return errno; 
	h->events = 0;
	h->loop = NULL;
	chk_dlist_remove(&h->ready_entry);
	chk_dlist_remove(&h->entry);
	return 0;	
}

int32_t events_mod(chk_handle *h,int32_t events) {
	//assert((events & EPOLLET) == 0);
	chk_event_loop *e = h->loop;	
	struct epoll_event ev = {0};
	ev.data.ptr = h;
	ev.events = events;
	errno = 0;
	if(0 != epoll_ctl(e->epfd,EPOLL_CTL_MOD,h->fd,&ev)) 
		return errno; 
	h->events = events;		
	return 0;	
}


int32_t chk_events_enable(chk_handle *h,int32_t events) {
	return events_mod(h,h->events | events);
}

int32_t chk_events_disable(chk_handle *h,int32_t events) {
	return events_mod(h,h->events & (~events));
}

int32_t chk_is_read_enable(chk_handle*h) {
	return h->events & EPOLLIN;
}

int32_t chk_is_write_enable(chk_handle*h) {
	return h->events & EPOLLOUT;
}


int32_t chk_loop_init(chk_event_loop *e) {
	assert(e);
	struct epoll_event ev = {0};
	int32_t epfd = epoll_create1(EPOLL_CLOEXEC);
	if(epfd < 0) return errno;
	int32_t tmp[2];
	if(pipe2(tmp,O_NONBLOCK|O_CLOEXEC) != 0) {
		close(epfd);
		return errno;
	}		
	e->epfd = epfd;
	e->tfd  = -1;
	e->maxevents = 64;
	e->events = calloc(1,(sizeof(*e->events)*e->maxevents));
	e->notifyfds[0] = tmp[0];
	e->notifyfds[1] = tmp[1];
	ev.data.fd = e->notifyfds[0];
	ev.events = EPOLLIN;
	if(0 != epoll_ctl(e->epfd,EPOLL_CTL_ADD,ev.data.fd,&ev)) {
		close(epfd);
		close(tmp[0]);
		close(tmp[1]);
		free(e->events);
		return errno;
	}
	e->threadid = chk_thread_id();
	chk_dlist_init(&e->handles);	
	return 0;
}

void chk_loop_finalize(chk_event_loop *e) {
	assert(e);
	chk_handle *h;
	if(e->tfd >= 0) {
		close(e->tfd);
		chk_timermgr_del(e->timermgr);
	}

	while((h = cast(chk_handle*,chk_dlist_pop(&e->handles)))){
		chk_events_remove(h);
		h->on_events(h,CHK_EVENT_ECLOSE);
	}

	close(e->epfd);
	close(e->notifyfds[0]);
	close(e->notifyfds[1]);
	free(e->events);
}

int32_t _loop_run(chk_event_loop *e,uint32_t ms,int once) {
	int32_t ret = 0;
	int32_t i,nfds,ticktimer;
	int64_t _;
	chk_handle      *h;
	chk_dlist        ready_list;
	chk_dlist_entry *read_entry;	
	do {
		ticktimer = 0;
		errno = 0;
		chk_dlist_init(&ready_list);
		nfds = TEMP_FAILURE_RETRY(epoll_wait(e->epfd,e->events,e->maxevents,once ? ms : -1));
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
			}
			if(ticktimer) chk_timer_tick(e->timermgr,chk_accurate_tick64());
			e->status ^= INLOOP;
			if(e->status & CLOSING) break;
			if(nfds == e->maxevents){
				free(e->events);
				e->maxevents <<= 2;
				e->events = calloc(1,sizeof(*e->events)*e->maxevents);
			}				
		}else if(nfds < 0) {
			ret = errno;
			break;
		}	
	}while(!once);

loopend:	
	if(e->status & CLOSING) {
		chk_loop_finalize(e);
	}	
	return ret;
}

chk_timer *chk_loop_addtimer(chk_event_loop *e,uint32_t timeout,chk_timeout_cb cb,void *ud) {
	struct  itimerspec spec;
    struct  timespec now;
    int64_t nosec;
	struct  epoll_event ev = {0};   
	if(e->tfd < 0) {
    	e->tfd = timerfd_create(CLOCK_MONOTONIC,TFD_CLOEXEC|TFD_NONBLOCK);
	    if(e->tfd < 0) return NULL;
	    clock_gettime(CLOCK_MONOTONIC, &now);      
    	nosec = (now.tv_sec)*1000*1000*1000 + now.tv_nsec + 1*1000*1000;
    	spec.it_value.tv_sec = nosec/(1000*1000*1000);
        spec.it_value.tv_nsec = nosec%(1000*1000*1000);
        spec.it_interval.tv_sec = 0;
        spec.it_interval.tv_nsec = 1*1000*1000;  
	    
	    if(0 != timerfd_settime(e->tfd,TFD_TIMER_ABSTIME,&spec,0)) {
	        close(e->tfd);
	        e->tfd = -1;
	        return NULL;		
		}
		ev.data.fd = e->tfd;
		ev.events  = EPOLLIN;
		if(0 != epoll_ctl(e->epfd,EPOLL_CTL_ADD,e->tfd,&ev)) {
	        close(e->tfd);
	        e->tfd = -1;
	        return NULL;				
		}
		e->timermgr = chk_timermgr_new();
	}
	return chk_timer_register(e->timermgr,timeout,cb,ud,chk_accurate_tick64()); 
}

#endif
