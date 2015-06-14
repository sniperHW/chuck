typedef struct engine{
	engine_head;
	int32_t kfd;
	struct kevent* events;
	int    maxevents;
	//for timer
   	struct kevent change;	
}engine;

int32_t 
event_add(engine *e,handle *h,
		  int32_t events)
{	
	struct kevent ke;
	EV_SET(&ke, h->fd, events, EV_ADD, 0, 0, h);
	errno = 0;
	if(0 != kevent(e->kfd, &ke, 1, NULL, 0, NULL))
		return -errno;
	
	if(events == EVFILT_READ)
		h->set_read = 1;
	else if(events == EVFILT_WRITE)
		h->set_write = 1;
	h->e = e;
	return 0;	
}

int32_t 
event_remove(handle *h)
{
	struct kevent ke;
	EV_SET(&ke, h->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	kevent(e->kfd, &ke, 1, NULL, 0, NULL);
	EV_SET(&ke, h->fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	kevent(e->kfd, &ke, 1, NULL, 0, NULL);
	h->events = 0;
	h->e = NULL;	
	return 0;	
}

int32_t 
event_enable(handle *h,int32_t events)
{
	struct kevent ke;
	EV_SET(&ke, h->fd, events,EV_ENABLE, 0, 0, h);
	errno = 0;
	if(0 != kevent(e->kfd, &ke, 1, NULL, 0, NULL))
		return -errno;
	if(events == EVFILT_READ)
		h->set_read = 1;
	else if(events == EVFILT_WRITE)
		h->set_write = 1;
	return 0;
}

int32_t 
event_disable(handle *h,int32_t events)
{
	struct kevent ke;
	EV_SET(&ke, h->fd, events,EV_DISABLE, 0, 0, h);
	errno = 0;
	if(0 != kevent(e->kfd, &ke, 1, NULL, 0, NULL))
		return -errno;

	if(events == EVFILT_READ)
		h->set_read = 0;
	else if(events == EVFILT_WRITE)
		h->set_write = 0;
	
	return 0;
}


engine *
engine_new()
{
	int32_t kfd = kqueue();
	if(kfd < 0) return NULL;
	fcntl (kfd, F_SETFD, FD_CLOEXEC);
	int32_t tmp[2];
	if(pipe2(tmp,O_NONBLOCK|O_CLOEXEC) != 0){
		close(kfd);
		return NULL;
	}		
	engine *kq = calloc(1,sizeof(*kq));
	kq->kfd = kfd;
	kq->maxevents = 64;
	kq->events = calloc(1,(sizeof(*kq->events)*kq->maxevents));
	kq->notifyfds[0] = tmp[0];
	kq->notifyfds[1] = tmp[1];
	struct kevent ke;
	EV_SET(&ke,tmp[0], EVFILT_READ, EV_ADD, 0, 0, tmp[0]);
	errno = 0;
	if(0 != kevent(kfd, &ke, 1, NULL, 0, NULL)){
		close(kfd);
		close(tmp[0]);
		close(tmp[1]);
		free(kq->events);
		free(kq);
		return NULL;		
	}	
	return (engine*)kq;
}


static inline void
_engine_del(engine *e)
{
	handle *h;
	if(e->tfd){
		engine_remove(cast(handle*,e->tfd));
		wheelmgr_del(e->timermgr);
	}
	while((h = cast(handle*,dlist_pop(&e->handles)))){
		event_remove(h);
		h->on_events(h,EVENT_ECLOSE);
	}
	close(e->kfd);
	close(e->notifyfds[0]);
	close(e->notifyfds[1]);
	free(e->events);
}

void 
engine_del(engine *e)
{
	assert(e->threadid == thread_id());
	if(e->status & INLOOP)
		e->status |= CLOSING;
	else{
		_engine_del(e);
		free(e);
	}
}

/*void
engine_del_lua(engine *e)
{
	assert(e->threadid == thread_id());
	if(e->status & INLOOP)
		e->status |= CLOSING;
	else{	
		_engine_del(e);
	}
}*/

int32_t 
engine_run(engine *e)
{
	int32_t ret = 0;
	for(;;){
		errno = 0;
		int32_t i;
		handle *h;
		int32_t nfds = TEMP_FAILURE_RETRY(kevent(e->kfd, &e->change,e->timerfd? 1 : 0, 
										  e->events,e->maxevents, NULL));	
		if(nfds > 0){
			e->status |= INLOOP;
			for(i=0; i < nfds ; ++i)
			{
				struct kevent *event = &e->events[i];
				if(event->udata == e->notifyfds[0]){
					int32_t _;
					while(TEMP_FAILURE_RETRY(read(e->notifyfds[0],&_,sizeof(_))) > 0);
					break;	
				}else{
					h = cast(handle*,event->udata);
					h->on_events(h,event->filter);;
				}
			}
			e->status ^= INLOOP;
			if(e->status & CLOSING)
				break;			
			if(nfds == e->maxevents){
				free(e->events);
				e->maxevents <<= 2;
				e->events = calloc(1,sizeof(*e->events)*e->maxevents);
			}				
		}else if(nfds < 0){
			ret = -errno;
			break;
		}	
	}
	if(e->status & CLOSING){
		ret = -EENGCLOSE;
		//if(e->status & LUAOBj)
		//	engine_del_lua(e);
		//else
		engine_del(e);
	}	
	return ret;
}

void 
engine_stop(engine *e)
{
	int32_t _;
	TEMP_FAILURE_RETRY(write(e->notifyfds[1],&_,sizeof(_)));
}

