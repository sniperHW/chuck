#define _CORE_
#include <assert.h>
#include "util/chk_error.h"
#include "util/chk_log.h"
#include "socket/chk_socket_helper.h"
#include "socket/chk_datagram_socket.h"
#include "event/chk_event_loop.h"
#include "socket/chk_datagram_socket_define.h"


#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

static int32_t loop_add(chk_event_loop *e,chk_handle *h,chk_event_callback cb) {
	int32_t ret,flags;
	chk_datagram_socket *s = cast(chk_datagram_socket*,h);
	if(!e || !h || !cb){		
		if(!e) {
			CHK_SYSLOG(LOG_ERROR,"chk_event_loop == NULL");				
		}

		if(!h) {
			CHK_SYSLOG(LOG_ERROR,"chk_handle == NULL");				
		}

		if(!cb) {
			CHK_SYSLOG(LOG_ERROR,"chk_event_callback == NULL");				
		}				

		return chk_error_invaild_argument;
	}
	if(s->closed){
		CHK_SYSLOG(LOG_ERROR,"chk_datagram_socket close");			
		return chk_error_socket_close;
	}

	flags = CHK_EVENT_READ;	
	if(chk_error_ok == (ret = chk_watch_handle(e,h,flags))) {
		easy_noblock(h->fd,1);
		s->cb = cast(chk_datagram_socket_cb,cb);
	}
	else {
		CHK_SYSLOG(LOG_ERROR,"chk_watch_handle() failed:%d",ret);		
	}
	return ret;
}

static void process_read(chk_datagram_socket *s) {
	int32_t bytes;
	struct iovec  _iovec;
	struct msghdr _msghdr;
	chk_datagram_event ev;

	_iovec.iov_base = s->recvbuff;
	_iovec.iov_len  = sizeof(s->recvbuff);

	_msghdr = (struct msghdr){
		.msg_name    = &ev.src,
		.msg_namelen = sizeof(ev.src),
		.msg_iov     = &_iovec,
		.msg_iovlen  = 1,
		.msg_flags   = 0,
		.msg_control = NULL,
		.msg_controllen = 0
	};
	bytes = TEMP_FAILURE_RETRY(recvmsg(s->fd,&_msghdr,0));
	if(bytes > 0) {
		ev.buff = chk_bytebuffer_new(bytes);
		chk_bytebuffer_append(ev.buff,(uint8_t*)s->recvbuff,(uint32_t)bytes);
		ev.src.addr_type = s->addr_type;
		s->cb(s,&ev,_msghdr.msg_flags == MSG_TRUNC ? chk_error_msg_trunc : 0);
		chk_bytebuffer_del(ev.buff);
	} else {
		if(errno != EAGAIN) {
			CHK_SYSLOG(LOG_ERROR,"recvmsg errno:%s",strerror(errno));
			s->cb(s,NULL,chk_error_dgram_read);
		}
	}
}

static void release_socket(chk_datagram_socket *s) {
	chk_unwatch_handle(cast(chk_handle*,s));	
	if(s->fd >= 0) { 
		close(s->fd);
	}
	free(s);
}

static void on_events(chk_handle *h,int32_t events) {
	chk_datagram_socket *s = cast(chk_datagram_socket*,h);

	s->inloop = 1;
	if(events == CHK_EVENT_LOOPCLOSE) {
		s->cb(s,NULL,chk_error_loop_close);
	} else {
		if(events & CHK_EVENT_READ){
			process_read(s);
		}				
	}
	s->inloop = 0;
	if(s->closed) {
		release_socket(s);		
	}
}

int32_t chk_datagram_socket_init(chk_datagram_socket *s,int32_t fd,int32_t addr_type) {
	assert(s);
	easy_close_on_exec(fd);
	s->fd         = fd;
	s->on_events  = on_events;
	s->handle_add = loop_add;
	s->addr_type  = addr_type;
	s->loop       = NULL;
	return 0;	
}


chk_datagram_socket *chk_datagram_socket_new(int32_t fd,int32_t addr_type) {
	chk_datagram_socket *s = calloc(1,sizeof(*s));
	if(!s) {
		CHK_SYSLOG(LOG_ERROR,"calloc chk_datagram_socket failed");			
		return NULL;
	}
	if(0 != chk_datagram_socket_init(s,fd,addr_type)) {
		free(s);
		return NULL;
	}
	return s;
}

void chk_datagram_socket_close(chk_datagram_socket *s) {
	if(s->closed)
		return;
	s->closed  = 1;	
	if(!s->inloop) {
		release_socket(s);
	}	
}

chk_ud chk_datagram_socket_getUd(chk_datagram_socket *s) {
	return s->ud;
}

void chk_datagram_socket_setUd(chk_datagram_socket *s,chk_ud ud){
	s->ud = ud;
}

int32_t chk_datagram_socket_sendto(chk_datagram_socket *s,chk_bytebuffer *buff,chk_sockaddr *dst) {

	if(buff->flags & READ_ONLY) {
		CHK_SYSLOG(LOG_ERROR,"chk_bytebuffer is read only");		
		return chk_error_buffer_read_only;
	}

	if(NULL == dst) {
		chk_bytebuffer_del(buff);
		return chk_error_invaild_argument;
	}

	if(s->closed) {
		CHK_SYSLOG(LOG_ERROR,"chk_stream_socket close");	
		chk_bytebuffer_del(buff);	
		return chk_error_socket_close;
	}
	int              ret      = 0;
	int32_t          i        = 0;
	chk_bytechunk   *chunk    = buff->head;
	uint32_t         pos      = buff->spos;
	uint32_t         datasize = buff->datasize; 
	uint32_t         size     = 0;
	while(i < MAX_WBAF && chunk && datasize) {
		s->wsendbuf[i].iov_base = chunk->data + pos;
		size = MIN(chunk->cap - pos,datasize);
		datasize -= size;
		s->wsendbuf[i].iov_len = size;
		++i;			
		chunk = chunk->next;
		pos = 0;		
	}

	if(i >= MAX_WBAF) {
		chk_bytebuffer_del(buff);	
		return chk_error_packet_too_large;		
	} else {
		struct msghdr _msghdr = {
			.msg_name       = dst,
			.msg_namelen    = chk_sockaddr_size(dst),
			.msg_iov        = s->wsendbuf,
			.msg_iovlen     = i,
			.msg_flags      = 0,
			.msg_control    = NULL,
			.msg_controllen = 0
		};		
		ret = TEMP_FAILURE_RETRY(sendmsg(s->fd,&_msghdr,0)); 
		chk_bytebuffer_del(buff);
		if(ret >= 0) {
			return chk_error_ok;
		} else {
			CHK_SYSLOG(LOG_ERROR,"sendmsg errno:%s",strerror(errno));
			return chk_error_send_failed;
		}
	}
}


int32_t chk_datagram_socket_set_broadcast(chk_datagram_socket *s) {
	if(s->closed) {
		return chk_error_socket_close;
	}
	if(0 == s->boradcast) {
		const int opt=-1;
		int nb=0;
		nb=setsockopt(s->fd,SOL_SOCKET,SO_BROADCAST,(char*)&opt,sizeof(opt));//设置套接字类型
		if(nb==-1) {	
			CHK_SYSLOG(LOG_ERROR,"set SO_BROADCAST failed errno:%s",strerror(errno));
			return chk_error_dgram_set_boradcast;
		}
		s->boradcast = 1;		
	}
	return chk_error_ok;		
}


int32_t chk_datagram_socket_broadcast(chk_datagram_socket *s,chk_bytebuffer *buff,chk_sockaddr *addr) {
	if(s->closed) {
		chk_bytebuffer_del(buff);
		return chk_error_socket_close;
	}
	if(0 == s->boradcast) {
		chk_bytebuffer_del(buff);	
		return chk_error_dgram_boradcast_flag;
	}
	int              ret      = 0;
	int32_t          i        = 0;
	chk_bytechunk   *chunk    = buff->head;
	uint32_t         pos      = buff->spos;
	uint32_t         datasize = buff->datasize; 
	uint32_t         size     = 0;
	while(i < MAX_WBAF && chunk && datasize) {
		s->wsendbuf[i].iov_base = chunk->data + pos;
		size = MIN(chunk->cap - pos,datasize);
		datasize -= size;
		s->wsendbuf[i].iov_len = size;
		++i;			
		chunk = chunk->next;
		pos = 0;		
	}

	if(i >= MAX_WBAF) {
		chk_bytebuffer_del(buff);	
		return chk_error_packet_too_large;		
	} else {
		struct msghdr _msghdr = {
			.msg_name       = addr,
			.msg_namelen    = chk_sockaddr_size(addr),
			.msg_iov        = s->wsendbuf,
			.msg_iovlen     = i,
			.msg_flags      = 0,
			.msg_control    = NULL,
			.msg_controllen = 0
		};		
		ret = TEMP_FAILURE_RETRY(sendmsg(s->fd,&_msghdr,0)); 
		chk_bytebuffer_del(buff);
		if(ret >= 0) {
			return chk_error_ok;
		} else {
			CHK_SYSLOG(LOG_ERROR,"sendmsg errno:%s",strerror(errno));
			return chk_error_send_failed;
		}
	}
}