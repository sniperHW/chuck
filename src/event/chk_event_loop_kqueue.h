#ifdef _CORE_

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif


static int32_t _add_event(chk_event_loop *e,chk_handle *h,int32_t event) {
	struct kevent ke;
	EV_SET(&ke, h->fd, event, EV_ADD, 0, 0, h);
	errno = 0;
	if(0 != kevent(e->kfd, &ke, 1, NULL, 0, NULL))
		return -errno;
	return 0;	
}

static int32_t _enable_event(chk_event_loop *e,chk_handle *h,int32_t event) {
	struct kevent ke;
	EV_SET(&ke, h->fd, event,EV_ENABLE, 0, 0, h);
	errno = 0;
	if(0 != kevent(e->kfd, &ke, 1, NULL, 0, NULL) && errno == ENOENT){
		return _add_event(e,h,event);
	}
	return -errno;
}

static int32_t _disable_event(chk_event_loop *e,chk_handle *h,int32_t event) {
	struct kevent ke;
	EV_SET(&ke, h->fd, event,EV_DISABLE, 0, 0, h);
	errno = 0;
	if(0 != kevent(e->kfd, &ke, 1, NULL, 0, NULL))
		return -errno;
	return 0;	
}

int32_t chk_events_add(chk_event_loop *e,chk_handle *h,int32_t events) {

	int32_t ret;
	if(events & CHK_EVENT_READ) {
		if((ret = _add_event(e,h,EVFILT_READ)) != 0){
			return ret;
		}
	}

	if(events & CHK_EVENT_WRITE) {
		if((ret = _add_event(e,h,EVFILT_WRITE)) != 0){
			return ret;
		}
	}
	
	h->events = events;
	h->loop = e;
	chk_dlist_pushback(&e->handles,cast(chk_dlist_entry*,h));
	return 0;	
}

int32_t chk_events_remove(chk_handle *h) {
	struct kevent ke;
	chk_event_loop *e = h->loop;
	EV_SET(&ke, h->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	kevent(e->kfd, &ke, 1, NULL, 0, NULL);
	EV_SET(&ke, h->fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	kevent(e->kfd, &ke, 1, NULL, 0, NULL);
	h->events = 0;
	h->loop = NULL;
	chk_dlist_remove(&h->ready_entry);
	chk_dlist_remove(&h->entry);
	return 0;	
}

int32_t chk_events_enable(chk_handle *h,int32_t events) {
	int32_t ret;
	chk_event_loop *e = h->loop;	
	if(events & CHK_EVENT_READ) {
		if((ret = _enable_event(e,h,EVFILT_READ)) != 0){
			return ret;
		}
		h->events |= CHK_EVENT_READ;
	}

	if(events & CHK_EVENT_WRITE) {
		if((ret = _enable_event(e,h,EVFILT_WRITE)) != 0){
			return ret;
		}
		h->events |= CHK_EVENT_WRITE;		
	}	
	return 0;
}

int32_t chk_events_disable(chk_handle *h,int32_t events) {
	int32_t ret;
	chk_event_loop *e = h->loop;	
	if(events & CHK_EVENT_READ) {
		if((ret = _disable_event(e,h,EVFILT_READ)) != 0){
			return ret;
		}
		h->events &= (~CHK_EVENT_READ);
	}

	if(events & CHK_EVENT_WRITE) {
		if((ret = _disable_event(e,h,EVFILT_WRITE)) != 0){
			return ret;
		}
		h->events &= (~CHK_EVENT_WRITE);		
	}	
	return 0;
}

int32_t chk_is_read_enable(chk_handle*h) {
	return h->events & CHK_EVENT_READ;
}

int32_t chk_is_write_enable(chk_handle*h) {
	return h->events & CHK_EVENT_WRITE;
}

extern int32_t easy_noblock(int32_t fd,int32_t noblock); 

int32_t chk_loop_init(chk_event_loop *e) {

	int32_t kfd = kqueue();
	if(kfd < 0) return errno;
	fcntl (kfd, F_SETFD, FD_CLOEXEC);
	int32_t tmp[2];
	//if(pipe2(tmp,O_NONBLOCK|O_CLOEXEC) != 0){
	//	close(kfd);
	//	return errno;
	//}

	if(pipe(tmp) != 0){
		close(kfd);
		return errno;
	}	

	easy_noblock(tmp[0],1);		
	easy_noblock(tmp[1],1);

	e->kfd = kfd;
	e->tfd  = -1;
	e->maxevents = 64;
	e->events = calloc(1,(sizeof(*e->events)*e->maxevents));
	e->notifyfds[0] = tmp[0];
	e->notifyfds[1] = tmp[1];
	e->timermgr = NULL;
	struct kevent ke;
	EV_SET(&ke,tmp[0], EVFILT_READ, EV_ADD, 0, 0, (void*)(int64_t)tmp[0]);
	errno = 0;
	if(0 != kevent(kfd, &ke, 1, NULL, 0, NULL)){
		close(kfd);
		close(tmp[0]);
		close(tmp[1]);
		free(e->events);
		free(e);
		return -1;	
	}
	e->threadid = chk_thread_id();
	chk_dlist_init(&e->handles);			
	return 0;
}

void chk_loop_finalize(chk_event_loop *e) {

	chk_handle *h;
	if(e->tfd){
		chk_timermgr_del(e->timermgr);
	}
	while((h = cast(chk_handle*,chk_dlist_pop(&e->handles)))){
		chk_events_remove(h);
		h->on_events(h,CHK_EVENT_ECLOSE);
	}
	e->tfd  = -1;	
	close(e->notifyfds[0]);
	close(e->notifyfds[1]);
	free(e->events);
}

int32_t _loop_run(chk_event_loop *e,uint32_t ms,int once) {
	int32_t ret = 0;
	int32_t i,nfds,ticktimer;
	chk_handle      *h;
	chk_dlist        ready_list;
	chk_dlist_entry *read_entry;
	struct timespec ts,*pts;
	uint64_t msec;
	if(once){
		msec = ms%1000;
		ts.tv_nsec = (msec*1000*1000);
		ts.tv_sec   = (ms/1000);
		pts = &ts;
	}
	else {
		pts = NULL;
	}

	do {
		ticktimer = 0;
		errno = 0;
		chk_dlist_init(&ready_list);
		nfds = TEMP_FAILURE_RETRY(kevent(e->kfd, &e->change,e->tfd >=0 ? 1 : 0, e->events,e->maxevents,pts));
		if(nfds > 0) {
			e->status |= INLOOP;
			for(i=0; i < nfds ; ++i) {
				struct kevent *event = &e->events[i];
				if(event->udata == (void*)(int64_t)e->notifyfds[0]) {
					int32_t _;
					while(TEMP_FAILURE_RETRY(read(e->notifyfds[0],&_,sizeof(_))) > 0);
					goto loopend;	
				}else if(event->udata == e->timermgr) {
					ticktimer = 1;//优先处理其它事件,定时器事件最后处理
				}else {
					h = cast(chk_handle*,event->udata);
					
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
				h->active_evetns = 0;
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

	if(!e->timermgr){
		e->tfd      = e->notifyfds[0];
		e->timermgr = chk_timermgr_new();
		EV_SET(&e->change, e->tfd, EVFILT_TIMER, EV_ADD | EV_ENABLE, NOTE_USECONDS, 1000, e->timermgr);
	}
	return chk_timer_register(e->timermgr,timeout,cb,ud,chk_accurate_tick64()); 
}


#endif