#define _CORE_
#include <assert.h>
#include "socket/chk_connector.h"
#include "event/chk_event_loop.h"
#include "util/chk_log.h"
#include "util/chk_error.h"

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

typedef struct chk_connector chk_connector;

struct chk_connector {
    _chk_handle; 
    chk_ud     ud;
    connect_cb cb;
    uint32_t   timeout;
    chk_timer *t;    
};

int32_t connect_timeout(uint64_t tick,chk_ud ud) {
	chk_connector *c = cast(chk_connector*,ud.v.val);
	c->cb(-1,c->ud,chk_error_connect_timeout);
	close(c->fd);
	free(c);
	return -1;
}

static int32_t loop_add(chk_event_loop *e,chk_handle *h,chk_event_callback cb) {
	int32_t ret;
	chk_connector *c = cast(chk_connector*,h);
	assert(e && h && cb);
	ret = chk_watch_handle(e,h,CHK_EVENT_READ | CHK_EVENT_WRITE);
	if(ret == chk_error_ok) {
		c->cb = cast(connect_cb,cb);
		if(c->timeout) {
			c->t = chk_loop_addtimer(e,c->timeout,connect_timeout,chk_ud_make_void(c));
		}			
	}
	return ret;
}

static void _process_connect(chk_connector *c) {
	int32_t err = 0;
	int32_t fd = -1;
	socklen_t len = sizeof(err);
	if(c->t) {
		chk_timer_unregister(c->t);
		c->t = NULL;
	}
	do {
		if(getsockopt(c->fd, SOL_SOCKET, SO_ERROR, &err, &len) == -1){
			CHK_SYSLOG(LOG_ERROR,"getsockopt() failed fd:%d,errno:%s",c->fd,strerror(errno));
			c->cb(-1,c->ud,chk_error_connect);
		    break;
		}
		if(err) {
			CHK_SYSLOG(LOG_ERROR,"getsockopt() got error:%s fd:%d",strerror(err),c->fd);			
		    c->cb(-1,c->ud,chk_error_connect);    
		    break;
		}
		//success
		fd = c->fd;
	}while(0);
	chk_unwatch_handle(cast(chk_handle*,c));    
	if(fd != -1) c->cb(fd,c->ud,chk_error_ok);
	else close(c->fd);	
}

static void process_connect(chk_handle *h,int32_t events) {
	chk_connector *c = cast(chk_connector*,h);
	if(events == CHK_EVENT_LOOPCLOSE){
		c->cb(-1,c->ud,chk_error_loop_close);
		close(c->fd);
	}else _process_connect(c);
	free(h);
}

chk_connector *chk_connector_new(int32_t fd,chk_ud ud,uint32_t timeout) {
	chk_connector *c = calloc(1,sizeof(*c));
	if(!c){
		CHK_SYSLOG(LOG_ERROR,"calloc chk_connector failed");	
		return NULL;
	}
	c->fd = fd;
	c->on_events  = process_connect;
	c->handle_add = loop_add;
	c->timeout = timeout;
	c->ud = ud;
	easy_close_on_exec(fd);
	return c;
}

int32_t chk_async_connect(int fd,chk_event_loop *e,chk_sockaddr *peer,chk_sockaddr *local,connect_cb cb,chk_ud ud,uint32_t timeout) {
	chk_connector *c;
	int32_t        ret;
	if(NULL == e || NULL == peer || NULL == cb) {
		CHK_SYSLOG(LOG_ERROR,"NULL == peer || NULL == e || NULL == cb");
		return -1;
	}	
	easy_noblock(fd,1);
	ret = easy_connect(fd,peer,local);
	if(chk_error_ok == ret){
		c   = chk_connector_new(fd,ud,timeout);
		if(c) {
			chk_loop_add_handle(e,cast(chk_handle*,c),cast(chk_event_callback,cb));
		} else {
			CHK_SYSLOG(LOG_ERROR,"call chk_connector_new() failed");
			close(fd);
		}
	} else {
		CHK_SYSLOG(LOG_ERROR,"easy_connect() failed ret:%d",ret);	
		close(fd);
	}
	return ret;
}

int32_t chk_easy_async_connect(chk_event_loop *e,chk_sockaddr *peer,chk_sockaddr *local,connect_cb cb,chk_ud ud,uint32_t timeout) {
	int fd,family;
	if(NULL == e || NULL == peer || NULL == cb) {
		CHK_SYSLOG(LOG_ERROR,"NULL == peer || NULL == e || NULL == cb");
		return -1;
	}

	if(peer->addr_type == SOCK_ADDR_IPV4) {
		family = AF_INET;
	} else if(peer->addr_type == SOCK_ADDR_IPV6) {
		family = AF_INET6;
	} else if(peer->addr_type == SOCK_ADDR_UN) {
		family = AF_LOCAL;
	} else {
		CHK_SYSLOG(LOG_ERROR,"invaild peer addr_type");
		return -1;
	}
	errno = 0;
	fd = socket(family,SOCK_STREAM,IPPROTO_TCP);
	if(0 > fd) {
		CHK_SYSLOG(LOG_ERROR,"chk_connect socket failed errno:%s",strerror(errno));
		return -1;
	}		
	return chk_async_connect(fd,e,peer,local,cb,ud,timeout);
}



