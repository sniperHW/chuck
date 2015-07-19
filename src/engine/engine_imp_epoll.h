#include "timerfd.h"

typedef struct engine{
	engine_head;
	timerfd   *tfd;
	int32_t    epfd;
	struct     epoll_event* events;
	int32_t    maxevents;
}engine;


int32_t event_mod(handle *h,int32_t events);

int32_t event_add(engine *e,handle *h,int32_t events)
{
	assert((events & EPOLLET) == 0);
	struct epoll_event ev = {0};
	ev.data.ptr = h;
	ev.events = events;
	errno = 0;
	if(!h->e){
		if(0 != epoll_ctl(e->epfd,EPOLL_CTL_ADD,h->fd,&ev)) 
			return -errno;
		h->events = events;
		h->e = e;
		dlist_pushback(&e->handles,(dlistnode*)h);
		return 0;
	}else
		return event_mod(h,events);
}


int32_t event_remove(handle *h)
{
	struct epoll_event ev = {0};
	errno = 0;
	engine *e = h->e;
	if(0 != epoll_ctl(e->epfd,EPOLL_CTL_DEL,h->fd,&ev)) 
		return -errno; 
	h->events = 0;
	h->e = NULL;
	dlist_remove(cast(dlistnode*,h));
	return 0;	
}

int32_t event_mod(handle *h,int32_t events)
{
	assert((events & EPOLLET) == 0);
	engine *e = h->e;	
	struct epoll_event ev = {0};
	ev.data.ptr = h;
	ev.events = events;
	errno = 0;
	if(0 != epoll_ctl(e->epfd,EPOLL_CTL_MOD,h->fd,&ev)) 
		return -errno; 
	h->events = events;		
	return 0;	
}


int32_t event_enable(handle *h,int32_t events)
{
	return event_mod(h,h->events | events);
}

int32_t event_disable(handle *h,int32_t events)
{
	return event_mod(h,h->events & (~events));
}

void timerfd_callback(void *ud)
{
	wheelmgr *mgr = (wheelmgr*)ud;
	wheelmgr_tick(mgr,systick64());
}

timer *engine_regtimer(
		engine *e,uint32_t timeout,
		int32_t(*cb)(uint32_t,uint64_t,void*),
		void *ud)
{
	if(!e->tfd){
		e->timermgr = wheelmgr_new();
		e->tfd      = timerfd_new(1,e->timermgr);
		engine_associate(e,e->tfd,timerfd_callback);
	}
	return wheelmgr_register(e->timermgr,timeout,cb,ud,systick64());
}


static int32_t engine_init(engine *e)
{
	int32_t epfd = epoll_create1(EPOLL_CLOEXEC);
	if(epfd < 0) return -1;
	int32_t tmp[2];
	if(pipe2(tmp,O_NONBLOCK|O_CLOEXEC) != 0){
		close(epfd);
		return -1;
	}		
	e->epfd = epfd;
	e->maxevents = 64;
	e->events = calloc(1,(sizeof(*e->events)*e->maxevents));
	e->notifyfds[0] = tmp[0];
	e->notifyfds[1] = tmp[1];

	struct epoll_event ev = {0};
	ev.data.fd = e->notifyfds[0];
	ev.events = EPOLLIN;
	if(0 != epoll_ctl(e->epfd,EPOLL_CTL_ADD,ev.data.fd,&ev)){
		close(epfd);
		close(tmp[0]);
		close(tmp[1]);
		free(e->events);
		return -1;
	}
	e->threadid = thread_id();
	dlist_init(&e->handles);	
	return 0;
}

static inline void _engine_del(engine *e)
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
	close(e->epfd);
	close(e->notifyfds[0]);
	close(e->notifyfds[1]);
	free(e->events);
}

int32_t _engine_run(engine *e,int32_t timeout)
{
	int32_t ret = 0;
	int32_t i,nfds;
	handle *h;	
	do{
		errno = 0;
		nfds = TEMP_FAILURE_RETRY(epoll_wait(e->epfd,e->events,e->maxevents,timeout > 0 ? timeout:-1));
		if(nfds > 0){
			e->status |= INLOOP;
			for(i=0; i < nfds ; ++i)
			{
				struct epoll_event *event = &e->events[i];
				if(event->data.fd == e->notifyfds[0]){
					int32_t _;
					while(TEMP_FAILURE_RETRY(read(e->notifyfds[0],&_,sizeof(_))) > 0);
					goto loopend;	
				}else{
					h = cast(handle*,event->data.ptr);
					h->on_events(h,event->events);
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
	}while(timeout < 0);
loopend:	
	if(e->status & CLOSING){
		ret = -EENGCLOSE;
#ifdef _CHUCKLUA
		engine_del_lua(e);
#else
		engine_del(e);
#endif
	}	
	return ret;
}
