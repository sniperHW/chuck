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

static void do_callback(chk_acceptor *acceptor,int32_t fd,chk_sockaddr *addr,void *ud,int32_t err){
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

void chk_acceptor_init(chk_acceptor *a,int32_t fd,void *ud) {
	a->ud = ud;
	a->fd = fd;
	a->on_events  = process_accept;
	a->handle_add = loop_add;
	a->loop = NULL;
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

chk_acceptor *chk_acceptor_new(int32_t fd,void *ud) {
	chk_acceptor *a = calloc(1,sizeof(*a));
	if(!a){
		CHK_SYSLOG(LOG_ERROR,"calloc chk_acceptor failed");	
		return NULL;
	}
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

void *chk_acceptor_get_ud(chk_acceptor *a) {
	return a->ud;
}

void chk_acceptor_set_ud(chk_acceptor *a,void *ud) {
	a->ud = ud;
}

chk_acceptor *chk_listen_tcp_ip4(chk_event_loop *loop,const char *ip,int16_t port,chk_acceptor_cb cb,void *ud) {
	chk_sockaddr  server;
	int32_t       fd,ret;
	chk_acceptor *a = NULL;
	do {
		if(chk_error_ok != easy_sockaddr_ip4(&server,ip,port)){ 
			CHK_SYSLOG(LOG_ERROR,"easy_sockaddr_ip4() failed %s:%d",ip,port);
			break;
		}
		if(0 > (fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP))){
		    CHK_SYSLOG(LOG_ERROR,"socket() failed errno:%d",errno); 			
			break;
		}
		easy_addr_reuse(fd,1);
		if(chk_error_ok == (ret = easy_listen(fd,&server))){
			a = chk_acceptor_new(fd,ud);
			if(!a) {
				close(fd);
				break;
			}
			if(0 != chk_loop_add_handle(loop,(chk_handle*)a,cb)){
				free(a);
				close(fd);
				break;	
			}
		}else{ 
			close(fd);
		}
	}while(0);	
	return a;
}

chk_acceptor *chk_ssl_listen_tcp_ip4(chk_event_loop *loop,const char *ip,int16_t port,SSL_CTX *ctx, chk_acceptor_cb cb,void *ud) {
	chk_sockaddr  server;
	int32_t       fd,ret;
	chk_acceptor *a = NULL;
	do {
		if(chk_error_ok != easy_sockaddr_ip4(&server,ip,port)){ 
			CHK_SYSLOG(LOG_ERROR,"easy_sockaddr_ip4() failed %s:%d",ip,port);
			break;
		}
		if(0 > (fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP))){
		    CHK_SYSLOG(LOG_ERROR,"socket() failed errno:%d",errno); 			
			break;
		}
		easy_addr_reuse(fd,1);
		if(chk_error_ok == (ret = easy_listen(fd,&server))){
			a = chk_acceptor_new(fd,ud);
			if(!a){
				close(fd);
				break;
			}
			if(0 != chk_loop_add_handle(loop,(chk_handle*)a,cb)) {
				free(a);
				close(fd);
				break;
			}
			a->ctx = ctx;
		}else { 
			close(fd);
		}
	}while(0);	
	return a;	
}

SSL_CTX *chk_acceptor_get_ssl_ctx(chk_acceptor *a) {
	if(!a) {
		return NULL;
	}
	return a->ctx;
}

