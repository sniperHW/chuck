#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <unistd.h>
#include <assert.h>

extern int32_t pipe2(int pipefd[2], int flags);

typedef struct{
	int32_t kfd;
	struct kevent* events;
	int    maxevents;
	//handle_t timerfd;	
	int32_t notifyfds[2];//0 for read,1 for write

	//for timer
   	//struct kevent change;	
}kqueue_;

int32_t event_add(engine *e,handle *h,int32_t events){	
	struct kevent ke;
	kqueue_ *kq = (kqueue_*)e;
	EV_SET(&ke, h->fd, events, EV_ADD, 0, 0, h);
	errno = 0;
	if(0 != kevent(kq->kfd, &ke, 1, NULL, 0, NULL))
		return -errno;
	
	if(events == EVFILT_READ)
		h->set_read = 1;
	else if(events == EVFILT_WRITE)
		h->set_write = 1;
	h->e = e;
	return 0;	
}

int32_t event_remove(handle *h){
	struct kevent ke;
	kqueue_ *kq = (kqueue_*)h->e;
	EV_SET(&ke, h->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	kevent(kq->kfd, &ke, 1, NULL, 0, NULL);
	EV_SET(&ke, h->fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	kevent(kq->kfd, &ke, 1, NULL, 0, NULL);
	h->events = 0;
	h->e = NULL;	
	return 0;	
}

int32_t event_enable(handle *h,int32_t events){
	struct kevent ke;
	kqueue_ *kq = (kqueue_*)h->e;
	EV_SET(&ke, h->fd, events,EV_ENABLE, 0, 0, h);
	errno = 0;
	if(0 != kevent(kq->kfd, &ke, 1, NULL, 0, NULL))
		return -errno;
	if(events == EVFILT_READ)
		h->set_read = 1;
	else if(events == EVFILT_WRITE)
		h->set_write = 1;
	return 0;
}

int32_t event_disable(handle *h,int32_t events){
	struct kevent ke;
	kqueue_ *kq = (kqueue_*)h->e;
	EV_SET(&ke, h->fd, events,EV_DISABLE, 0, 0, h);
	errno = 0;
	if(0 != kevent(kq->kfd, &ke, 1, NULL, 0, NULL))
		return -errno;

	if(events == EVFILT_READ)
		h->set_read = 0;
	else if(events == EVFILT_WRITE)
		h->set_write = 0;
	
	return 0;
}


engine *engine_new(){
	int32_t kfd = kqueue();
	if(kfd < 0) return NULL;
	fcntl (kfd, F_SETFD, FD_CLOEXEC);
	int32_t tmp[2];
	if(pipe2(tmp,O_NONBLOCK|O_CLOEXEC) != 0){
		close(kfd);
		return NULL;
	}		
	kqueue_ *kq = calloc(1,sizeof(*kq));
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

void engine_del(engine *e){
	kqueue_ *kq = (kqueue_*)e;
	//if(kq->timerfd)
	//	kn_timerfd_destroy(kq->timerfd);	
	close(kq->kfd);
	close(kq->notifyfds[0]);
	close(kq->notifyfds[1]);
	free(kq->events);
	free(kq);
}


int32_t engine_run(engine *e){
	kqueue_ *kq = (kqueue_*)e;
	for(;;){
		errno = 0;
		int32_t i;
		handle *h;
		int32_t nfds = TEMP_FAILURE_RETRY(kevent(kq->kfd, &kq->change, /*kq->timerfd? 1 : */0, kq->events,kq->maxevents, NULL));	
		if(nfds > 0){
			for(i=0; i < nfds ; ++i)
			{
				if(kq->events[i].udata == kq->notifyfds[0]){
					int32_t _;
					while(TEMP_FAILURE_RETRY(read(kq->notifyfds[0],&_,sizeof(_))) > 0);
					return 0;	
				}else{
					h = (handle_t)kq->events[i].udata;
					h->on_events(h,kq->events[i].filter);;
				}
			}
			if(nfds == kq->maxevents){
				free(kq->events);
				kq->maxevents <<= 2;
				kq->events = calloc(1,sizeof(*kq->events)*kq->maxevents);
			}				
		}else if(nfds < 0){
			return -errno;
		}	
	}
	return 0;
}

void engine_stop(engine *e){
	kqueue_ *kq = (kqueue*_)e;
	int32_t _;
	TEMP_FAILURE_RETRY(write(kq->notifyfds[1],&_,sizeof(_)));
}


/*
kn_timer_t kn_reg_timer(engine_t e,uint64_t timeout,kn_cb_timer cb,void *ud){
	kn_kqueue *kq = (kn_kqueue*)e;
	if(!kq->timerfd){
		kq->timerfd = kn_new_timerfd(1);
		((handle_t)kq->timerfd)->ud = kn_new_timermgr();
		EV_SET(&kq->change, 1, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, 1, kq->timerfd);		
	}
	return reg_timer_imp(((handle_t)kq->timerfd)->ud,timeout,cb,ud);
}
*/
