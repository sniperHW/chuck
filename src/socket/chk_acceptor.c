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
    chk_acceptor *acceptor = cast(chk_acceptor*,h);	
	ret = chk_watch_handle(e,h,CHK_EVENT_READ);
	if(ret == chk_error_ok) {
		easy_noblock(h->fd,1);
		acceptor->cb = cast(chk_acceptor_cb,cb);			
	}
	return ret;
}


static int32_t _accept(chk_acceptor *a,chk_sockaddr *addr,int32_t *fd) {
	socklen_t len = 0;
	while((*fd = accept(a->fd,cast(struct sockaddr*,addr),&len)) < 0){
#ifdef EPROTO
		if(errno == EPROTO || errno == ECONNABORTED)
#else
		if(errno == ECONNABORTED)
#endif
			continue;
		else{
			if(errno != EAGAIN){
		    	CHK_SYSLOG(LOG_ERROR,"accept() failed errno:%d",errno); 			
			}
			return errno;
		}
	}
	return 0;
}

static void do_callback(chk_acceptor *acceptor,int32_t fd,chk_sockaddr *addr,chk_ud ud,int32_t err){
	acceptor->cb(acceptor,fd,addr,acceptor->ud,err);
}

static void process_accept(chk_handle *h,int32_t events) {
	int32_t 	 fd;
	int32_t      ret;
    chk_sockaddr addr;
    chk_acceptor *acceptor = cast(chk_acceptor*,h);
	if(events == CHK_EVENT_LOOPCLOSE){
		do_callback(acceptor,-1,NULL,acceptor->ud,chk_error_loop_close);
		return;
	}
    do {
		ret = _accept(acceptor,&addr,&fd);
		if(ret == 0)
		   do_callback(acceptor,fd,&addr,acceptor->ud,0);
		else if(ret != EAGAIN){
		   CHK_SYSLOG(LOG_ERROR,"_accept() failed ret:%d",ret);	
		   do_callback(acceptor,-1,NULL,acceptor->ud,chk_error_accept);
		}
	}while(0 == ret && chk_is_read_enable(h));
}

int32_t chk_acceptor_resume(chk_acceptor *a) {
	return chk_enable_read(cast(chk_handle*,a));
}

int32_t chk_acceptor_pause(chk_acceptor *a) {
	return chk_disable_read(cast(chk_handle*,a));
}

void chk_acceptor_init(chk_acceptor *a,int32_t fd,SSL_CTX *ctx,chk_ud ud) {
	a->ud = ud;
	a->fd = fd;
	a->on_events  = process_accept;
	a->handle_add = loop_add;
	a->loop = NULL;
	a->ctx = ctx;
	easy_close_on_exec(fd);
}

void chk_acceptor_finalize(chk_acceptor *a) {
	chk_unwatch_handle(cast(chk_handle*,a));
	if(a->fd >= 0) {
		close(a->fd);
		a->fd = -1;
	}
	if(a->ctx) {
		SSL_CTX_free(a->ctx);
	}
}

chk_acceptor *chk_acceptor_new(int32_t fd,SSL_CTX *ctx,chk_ud ud) {
	chk_acceptor *a = calloc(1,sizeof(*a));
	if(!a){
		CHK_SYSLOG(LOG_ERROR,"calloc chk_acceptor failed");	
		return NULL;
	}
	chk_acceptor_init(a,fd,ctx,ud);
	return a;
}

int32_t chk_acceptor_get_fd(chk_acceptor *a) {
	return a->fd;
}

void chk_acceptor_del(chk_acceptor *a) {
	chk_acceptor_finalize(a);
	free(a);
}

chk_ud chk_acceptor_get_ud(chk_acceptor *a) {
	return a->ud;
}

void chk_acceptor_set_ud(chk_acceptor *a,chk_ud ud) {
	a->ud = ud;
}

int32_t chk_acceptor_start(chk_acceptor *a,chk_event_loop *loop,chk_sockaddr *addr,chk_acceptor_cb cb) {
	if(NULL == a || NULL == loop || NULL == addr || NULL == cb) {
		CHK_SYSLOG(LOG_ERROR,"NULL == a || NULL == loop || NULL == addr || NULL == cb");
		return -1;
	}

	easy_addr_reuse(a->fd,1);
	
	if(chk_error_ok != easy_listen(a->fd,addr)) {
		CHK_SYSLOG(LOG_ERROR,"easy_listen() failed");
		return -1;
	}

	if(0 != chk_loop_add_handle(loop,(chk_handle*)a,cb)) {
		CHK_SYSLOG(LOG_ERROR,"chk_loop_add_handle() failed");
		return -1;
	}
	return 0;
}

static chk_acceptor *_chk_listen(chk_event_loop *loop,chk_sockaddr *addr,SSL_CTX *ctx,chk_acceptor_cb cb,chk_ud ud) {
	int32_t       fd,family;
	chk_acceptor *a = NULL;
	errno = 0;

	if(addr->addr_type == SOCK_ADDR_IPV4) {
		family = AF_INET;
	} else if(addr->addr_type == SOCK_ADDR_IPV6) {
		family = AF_INET6;
	} else if(addr->addr_type == SOCK_ADDR_UN) {
		family = AF_LOCAL;
	} else {
		CHK_SYSLOG(LOG_ERROR,"invaild addr addr_type");
		return NULL;
	}

	fd = socket(family,SOCK_STREAM,IPPROTO_TCP);

	if(0 > fd) {
		CHK_SYSLOG(LOG_ERROR,"socket() failed errno:%d",errno); 
		return NULL;
	}

	a = chk_acceptor_new(fd,ctx,ud);

	if(NULL == a) {
		CHK_SYSLOG(LOG_ERROR,"chk_acceptor_new() failed");
		close(fd);
		return NULL;
	}

	if(0 != chk_acceptor_start(a,loop,addr,cb)) {
		CHK_SYSLOG(LOG_ERROR,"chk_acceptor_start() failed");
		chk_acceptor_del(a);
		return NULL;
	}

	return a;
}

chk_acceptor *chk_listen(chk_event_loop *loop,chk_sockaddr *addr,chk_acceptor_cb cb,chk_ud ud) {

	if(NULL == loop || NULL == addr || NULL == cb) {
		CHK_SYSLOG(LOG_ERROR,"NULL == loop || NULL == addr || NULL == cb");		
		return NULL;
	}

	return _chk_listen(loop,addr,NULL,cb,ud);
}

chk_acceptor *chk_ssl_listen(chk_event_loop *loop,chk_sockaddr *addr,SSL_CTX *ctx,chk_acceptor_cb cb,chk_ud ud) {
	if(NULL == loop || NULL == addr || NULL == cb || NULL == ctx) {
		CHK_SYSLOG(LOG_ERROR,"NULL == loop || NULL == addr || NULL == cb || NULL == ctx");		
		return NULL;
	}
	
	return _chk_listen(loop,addr,ctx,cb,ud);	
}

SSL_CTX *chk_acceptor_get_ssl_ctx(chk_acceptor *a) {
	if(!a) {
		return NULL;
	}
	return a->ctx;
}

