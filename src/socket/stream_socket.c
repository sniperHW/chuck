#include <assert.h>
#include "socket/socket.h"
#include "engine/engine.h"
#include "socket/socket_helper.h"

extern int32_t is_read_enable(handle*h);
extern int32_t is_write_enable(handle*h);

static int32_t imp_engine_add(engine *e,handle *h,generic_callback callback){
	assert(e && h && callback);
	if(h->e) return -EASSENG;
	int32_t ret;
#ifdef _LINUX
	ret = event_add(e,h,EPOLLRDHUP);
#elif   _BSD
	if((ret = event_add(e,h,EVFILT_READ)) == 0 &&
	   (ret = event_add(e,h,EVFILT_READ)) == 0){
		disable_read(h);
		disable_write(h)
	}else if(h->e){
		event_remove(h);
	}
#else
	return -EUNSPPLAT;
#endif
	if(ret == 0){
		easy_noblock(h->fd,1);
		((socket_*)h)->stream_callback = (void(*)(handle*,void*,int32_t,int32_t))callback;
	}
	return ret;
}

static void process_read(socket_ *s){
	iorequest *req = 0;
	int32_t bytes_transfer = 0;
	while((req = (iorequest*)list_pop(&s->pending_recv))!=NULL){
		errno = 0;
		bytes_transfer = TEMP_FAILURE_RETRY(readv(((handle*)s)->fd,req->iovec,req->iovec_count));	
		
		if(bytes_transfer < 0 && errno == EAGAIN){
				//将请求重新放回到队列
				list_pushback(&s->pending_recv,(listnode*)req);
				break;
		}else{
			s->stream_callback((handle*)s,req,bytes_transfer,errno);
			if(s->status & SOCKET_CLOSE)
				return;			
		}
	}	
	if(!list_size(&s->pending_recv)){
		//没有接收请求了,取消EPOLLIN
		disable_read((handle*)s);
	}	
}

static void process_write(socket_ *s){
	iorequest *req = 0;
	int32_t bytes_transfer = 0;
	while((req = (iorequest*)list_pop(&s->pending_send))!=NULL){
		errno = 0;	
		bytes_transfer = TEMP_FAILURE_RETRY(writev(((handle*)s)->fd,req->iovec,req->iovec_count));
		if(bytes_transfer < 0 && errno == EAGAIN){
				//将请求重新放回到队列
				list_pushback(&s->pending_send,(listnode*)req);
				break;
		}else{
			s->stream_callback((handle*)s,req,bytes_transfer,errno);
			if(s->status & SOCKET_CLOSE)
				return;
		}
	}
	if(!list_size(&s->pending_send)){
		//没有接收请求了,取消EPOLLOUT
		disable_write((handle*)s);
	}		
}

static void on_events(handle *h,int32_t events){
	socket_ *s = (socket_*)h;
	if(s->status & SOCKET_CLOSE)
		return;
	do{
		s->status |= SOCKET_INLOOP;
		if(events & EVENT_READ){
			process_read(s);	
			if(s->status & SOCKET_CLOSE) 
				break;								
		}		
		if(events & EVENT_WRITE)
			process_write(s);			
		s->status ^= SOCKET_INLOOP;
	}while(0);
	if(s->status & SOCKET_CLOSE){
		if(s->dctor) 
			s->dctor(s);		
		close(h->fd);
		free(h);		
	}
}

void    construct_stream_socket(socket_ *s){
	((handle*)s)->on_events = on_events;
	((handle*)s)->imp_engine_add = imp_engine_add;
	s->status = SOCKET_STREAM;	
}	

handle *new_stream_socket(int32_t fd){
	socket_ *s = calloc(1,sizeof(*s));
	((handle*)s)->fd = fd;
	((handle*)s)->on_events = on_events;
	((handle*)s)->imp_engine_add = imp_engine_add;
	s->status = SOCKET_STREAM;
	easy_close_on_exec(fd);
	return (handle*)s;
}

int32_t stream_socket_recv(handle *h,iorequest *req,int32_t flag){
	socket_ *s = (socket_*)h;
	if(!h->e)
		return -ENOASSENG;
	else if(s->status & SOCKET_CLOSE)
		return -ESOCKCLOSE;
	else if(!(s->status & SOCKET_STREAM))
		return -EINVISOKTYPE;
	errno = 0;
	if(flag == IO_NOW && list_size(&s->pending_recv)){
		int32_t bytes_transfer = TEMP_FAILURE_RETRY(readv(h->fd,req->iovec,req->iovec_count));
		if(bytes_transfer >= 0)
			return bytes_transfer;
		else if(errno != EAGAIN)
			return -errno;
	}
	list_pushback(&s->pending_recv,(listnode*)req);
	if(!is_read_enable(h)) enable_read(h);
	return -EAGAIN;	
}

int32_t stream_socket_send(handle *h,iorequest *req,int32_t flag){
	socket_ *s = (socket_*)h;
	if(!h->e)
		return -ENOASSENG;
	else if(s->status & SOCKET_CLOSE)
		return -ESOCKCLOSE;
	else if(!(s->status & SOCKET_STREAM))
		return -EINVISOKTYPE;

	errno = 0;
	if(flag == IO_NOW && list_size(&s->pending_send)){
		int32_t bytes_transfer = TEMP_FAILURE_RETRY(writev(h->fd,req->iovec,req->iovec_count));
		if(bytes_transfer >= 0)
			return bytes_transfer;
		else if(errno != EAGAIN)
			return -errno;
	}
	list_pushback(&s->pending_send,(listnode*)req);
	if(!is_write_enable(h)) enable_write(h);
	return -EAGAIN;	
}