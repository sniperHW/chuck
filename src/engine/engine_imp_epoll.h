#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <unistd.h>
#include <assert.h>
#include "util/time.h"
#include "util/timewheel.h"
#include "util/timerfd.h"
#include "util/dlist.h"

extern int32_t pipe2(int pipefd[2], int flags);

enum{
	INLOOP  =  1 << 1,
	CLOSING =  1 << 2,
	LUAOBj  =  1 << 3,
};

typedef struct{
	int32_t    epfd;
	struct     epoll_event* events;
	int32_t    maxevents;
	timerfd   *tfd;
	wheelmgr  *timermgr;
	int32_t    notifyfds[2];//0 for read,1 for write
	dlist      handles;
	int32_t    status;
}epoll_;

int32_t 
event_add(engine *e,handle *h,
		  int32_t events)
{
	assert((events & EPOLLET) == 0);
	struct epoll_event ev = {0};
	epoll_ *ep = (epoll_*)e;
	ev.data.ptr = h;
	ev.events = events;
	errno = 0;
	if(0 != epoll_ctl(ep->epfd,EPOLL_CTL_ADD,h->fd,&ev)) 
		return -errno;
	h->events = events;
	h->e = e;
	dlist_pushback(&ep->handles,(dlistnode*)h);
	return 0;
}


int32_t 
event_remove(handle *h)
{
	epoll_ *ep = (epoll_*)h->e;
	struct epoll_event ev = {0};
	errno = 0;
	if(0 != epoll_ctl(ep->epfd,EPOLL_CTL_DEL,h->fd,&ev)) 
		return -errno; 
	h->events = 0;
	h->e = NULL;
	dlist_remove((dlistnode*)h);
	return 0;	
}

int32_t 
event_mod(handle *h,int32_t events)
{
	assert((events & EPOLLET) == 0);	
	struct epoll_event ev = {0};
	epoll_ *ep = (epoll_*)h->e;
	ev.data.ptr = h;
	ev.events = events;
	errno = 0;
	if(0 != epoll_ctl(ep->epfd,EPOLL_CTL_MOD,h->fd,&ev)) 
		return -errno; 
	h->events = events;		
	return 0;	
}


int32_t 
event_enable(handle *h,int32_t events)
{
	return event_mod(h,h->events | events);
}

int32_t 
event_disable(handle *h,int32_t events)
{
	return event_mod(h,h->events & (~events));
}

void 
timerfd_callback(void *ud)
{
	wheelmgr *mgr = (wheelmgr*)ud;
	wheelmgr_tick(mgr,systick64());
}

timer*
engine_regtimer(engine *e,uint32_t timeout,
			    int32_t(*cb)(uint32_t,uint64_t,void*),
			    void *ud)
{
	epoll_ *ep = (epoll_*)e;
	if(!ep->tfd){
		ep->timermgr = wheelmgr_new();
		ep->tfd      = timerfd_new(1,ep->timermgr);
		engine_associate(e,ep->tfd,timerfd_callback);
	}
	return wheelmgr_register(ep->timermgr,timeout,cb,ud,systick64());
}


static int32_t 
engine_init(engine *e)
{
	epoll_ *ep = (epoll_*)e;
	int32_t epfd = epoll_create1(EPOLL_CLOEXEC);
	if(epfd < 0) return -1;
	int32_t tmp[2];
	if(pipe2(tmp,O_NONBLOCK|O_CLOEXEC) != 0){
		close(epfd);
		return -1;
	}		
	ep->epfd = epfd;
	ep->maxevents = 64;
	ep->events = calloc(1,(sizeof(*ep->events)*ep->maxevents));
	ep->notifyfds[0] = tmp[0];
	ep->notifyfds[1] = tmp[1];

	struct epoll_event ev = {0};
	ev.data.fd = ep->notifyfds[0];
	ev.events = EPOLLIN;
	if(0 != epoll_ctl(ep->epfd,EPOLL_CTL_ADD,ev.data.fd,&ev)){
		close(epfd);
		close(tmp[0]);
		close(tmp[1]);
		free(ep->events);
		return -1;
	}
	dlist_init(&ep->handles);	
	return 0;
} 

engine* 
engine_new()
{
	epoll_ *ep = calloc(1,sizeof(*ep));
	if(0 != engine_init((engine*)ep)){
		free(ep);
		ep = NULL;
	}
	return (engine*)ep;
}

static int32_t 
lua_engine_new(lua_State *L)
{
	epoll_ *ep = (epoll_*)lua_newuserdata(L, sizeof(*ep));
	memset(ep,0,sizeof(*ep));
	if(0 != engine_init((engine*)ep)){
		free(ep);
		lua_pushnil(L);
		return 1;
	}
	ep->status &= LUAOBj;	
	luaL_getmetatable(L, LUAENGINE_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

static inline void
_engine_del(engine *e)
{
	epoll_ *ep = (epoll_*)e;
	handle *h;
	if(ep->tfd){
		engine_remove((handle*)ep->tfd);
		wheelmgr_del(ep->timermgr);
	}
	while((h = (handle*)dlist_pop(&ep->handles))){
		event_remove(h);
		h->on_events(h,EENGCLOSE);
	}
	close(ep->epfd);
	close(ep->notifyfds[0]);
	close(ep->notifyfds[1]);
	free(ep->events);
}
void 
engine_del(engine *e)
{
	epoll_ *ep = (epoll_*)e;
	if(ep->status & INLOOP)
		ep->status |= CLOSING;
	else{
		_engine_del(e);
		free(e);
	}
}

void
engine_del_lua(engine *e)
{
	epoll_ *ep = (epoll_*)e;
	if(ep->status & INLOOP)
		ep->status |= CLOSING;
	else{	
		_engine_del(e);
	}
}

int32_t
engine_runonce(engine *e,uint32_t timeout)
{
	epoll_ *ep = (epoll_*)e;
	errno = 0;
	int32_t i;
	int32_t ret = 0;
	handle *h;
	int32_t nfds = TEMP_FAILURE_RETRY(epoll_wait(ep->epfd,ep->events,ep->maxevents,timeout));
	if(nfds > 0){
		ep->status |= INLOOP;
		for(i=0; i < nfds ; ++i)
		{
			if(ep->events[i].data.fd == ep->notifyfds[0]){
				int32_t _;
				while(TEMP_FAILURE_RETRY(read(ep->notifyfds[0],&_,sizeof(_))) > 0);
				break;	
			}else{
				h = (handle*)ep->events[i].data.ptr;
				h->on_events(h,ep->events[i].events);;
			}
		}
		ep->status ^= INLOOP;
		if(nfds == ep->maxevents){
			free(ep->events);
			ep->maxevents <<= 2;
			ep->events = calloc(1,sizeof(*ep->events)*ep->maxevents);
		}				
	}else if(nfds < 0){
		ret = -errno;
	}
	if(ep->status & CLOSING){
		ret = -EENGCLOSE;
		if(ep->status & LUAOBj)
			engine_del_lua(e);
		else
			engine_del(e);
	}
	return ret;	
}

int32_t 
engine_run(engine *e)
{
	epoll_ *ep = (epoll_*)e;
	int32_t ret = 0;
	for(;;){
		errno = 0;
		int32_t i;
		handle *h;
		int32_t nfds = TEMP_FAILURE_RETRY(epoll_wait(ep->epfd,ep->events,ep->maxevents,-1));
		if(nfds > 0){
			ep->status |= INLOOP;
			for(i=0; i < nfds ; ++i)
			{
				if(ep->events[i].data.fd == ep->notifyfds[0]){
					int32_t _;
					while(TEMP_FAILURE_RETRY(read(ep->notifyfds[0],&_,sizeof(_))) > 0);
					break;	
				}else{
					h = (handle*)ep->events[i].data.ptr;
					h->on_events(h,ep->events[i].events);
				}
			}
			ep->status ^= INLOOP;
			if(ep->status & CLOSING)
				break;
			if(nfds == ep->maxevents){
				free(ep->events);
				ep->maxevents <<= 2;
				ep->events = calloc(1,sizeof(*ep->events)*ep->maxevents);
			}				
		}else if(nfds < 0){
			ret = -errno;
		}	
	}
	if(ep->status & CLOSING){
		ret = -EENGCLOSE;
		if(ep->status & LUAOBj)
			engine_del_lua(e);
		else
			engine_del(e);
	}	
	return ret;
}


void 
engine_stop(engine *e)
{
	epoll_ *ep = (epoll_*)e;
	int32_t _;
	TEMP_FAILURE_RETRY(write(ep->notifyfds[1],&_,sizeof(_)));
}

