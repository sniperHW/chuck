#define _CORE_
#include <assert.h>
#include "event/chk_event_loop.h"
#include "socket/chk_acceptor.h"
#include "socket/chk_socket_helper.h"
#include "socket/chk_acceptor_define.h"
#include "util/chk_log.h"
#include "util/chk_error.h"

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif


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
	if(events == CHK_EVENT_LOOPCLOSE){
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

int32_t chk_acceptor_resume(chk_acceptor *a) {
	return chk_enable_read(cast(chk_handle*,a));
}

int32_t chk_acceptor_pause(chk_acceptor *a) {
	return chk_disable_read(cast(chk_handle*,a));
}

void chk_acceptor_init(chk_acceptor *a,int32_t fd,void *ud) {
	assert(a);
	a->ud = ud;
	a->fd = fd;
	a->on_events  = process_accept;
	a->handle_add = loop_add;
	a->loop = NULL;
	easy_close_on_exec(fd);
}

void chk_acceptor_finalize(chk_acceptor *a) {
	assert(a);
	chk_events_remove(cast(chk_handle*,a));
	if(a->fd >= 0) {
		close(a->fd);
		a->fd = -1;
	}
}

chk_acceptor *chk_acceptor_new(int32_t fd,void *ud) {
	chk_acceptor *a = calloc(1,sizeof(*a));
	chk_acceptor_init(a,fd,ud);
	return a;
}

int32_t chk_acceptor_get_fd(chk_acceptor *a) {
	return a->fd;
}

void chk_acceptor_del(chk_acceptor *a) {
	chk_acceptor_finalize(a);
	free(a);
}

void *chk_acceptor_getud(chk_acceptor *a) {
	return a->ud;
}

chk_acceptor *chk_listen_tcp_ip4(chk_event_loop *loop,const char *ip,int16_t port,chk_acceptor_cb cb,void *ud) {
	assert(loop && cb && ip);
	chk_sockaddr  server;
	int32_t       fd,ret;
	chk_acceptor *a = NULL;
	do {
		if(0 != easy_sockaddr_ip4(&server,ip,port)) break;
		if(0 > (fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP))) break;
		easy_addr_reuse(fd,1);
		if(0 == (ret = easy_listen(fd,&server))){
			a = chk_acceptor_new(fd,ud);
			chk_loop_add_handle(loop,(chk_handle*)a,cb);
		}else close(fd);
	}while(0);	
	return a;
}

