#define __CORE__
#include <assert.h>
#include "event/chk_event_loop.h"
#include "socket/chk_acceptor.h"
#include "socket/chk_socket_helper.h"
#include "util/chk_log.h"
#include "util/chk_error.h"

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif


struct chk_acceptor{
    _chk_handle;
    void           *ud;      
    chk_acceptor_cb cb;
};

static int32_t loop_add(chk_event_loop *e,chk_handle *h,chk_event_callback cb) {
	int32_t ret;
	assert(e && h && cb);
	ret = chk_events_add(e,h,CHK_EVENT_READ);
	if(ret == 0) {
		easy_noblock(h->fd,1);
		cast(chk_acceptor*,h)->cb = cast(chk_acceptor_cb,cb);
	}
	return ret;
}


static int _accept(chk_acceptor *a,chk_sockaddr *addr) {
	socklen_t len = 0;
	int32_t fd; 
	while((fd = accept(a->fd,cast(struct sockaddr*,addr),&len)) < 0){
#ifdef EPROTO
		if(errno == EPROTO || errno == ECONNABORTED)
#else
		if(errno == ECONNABORTED)
#endif
			continue;
		else
			return -errno;
	}
	return fd;
}

static void process_accept(chk_handle *h,int32_t events) {
	int 	     fd;
    chk_sockaddr addr;
    chk_acceptor *acceptor = cast(chk_acceptor*,h);
	if(events == CHK_EVENT_ECLOSE){
		acceptor->cb(acceptor,-1,NULL,acceptor->ud,CHK_ELOOPCLOSE);
		return;
	}
    do {
		fd = _accept(acceptor,&addr);
		if(fd >= 0)
		   acceptor->cb(acceptor,fd,&addr,acceptor->ud,0);
		else if(fd < 0 && fd != -EAGAIN)
		   acceptor->cb(acceptor,-1,NULL,acceptor->ud,-fd);
	}while(fd >= 0 && chk_is_read_enable(h));	      
}

int32_t chk_acceptor_enable(chk_acceptor *a) {
	return chk_enable_read(cast(chk_handle*,a));
}

int32_t acceptor_disable(chk_acceptor *a) {
	return chk_disable_read(cast(chk_handle*,a));
}

chk_acceptor *chk_acceptor_new(int32_t fd,void *ud) {
	chk_acceptor *a = calloc(1,sizeof(*a));
	a->ud = ud;
	a->fd = fd;
	a->on_events  = process_accept;
	a->handle_add = loop_add;
	easy_close_on_exec(fd);
	return a;
}

void chk_acceptor_del(chk_acceptor *a) {
	chk_events_remove(cast(chk_handle*,a));
	close(a->fd);
	free(a);
}
