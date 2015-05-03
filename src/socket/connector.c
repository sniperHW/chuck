#include <assert.h>
#include "socket/connector.h"
#include "engine/engine.h"
#include "socket/socket_helper.h"

int32_t connect_timeout(uint32_t event,uint64_t _,void *ud){
	if(event == TEVENT_TIMEOUT){
		connector *c = (connector*)ud;
		c->callback(-1,ETIMEDOUT,c->ud);
		close(((handle*)c)->fd);
		free(c);
	}
	return -1;//one shot timer,return -1
}

static int32_t imp_engine_add(engine *e,handle *h,generic_callback callback)
{
	assert(e && h && callback);
	if(h->e) return -EASSENG;
	int32_t ret;
	connector *c = (connector*)h;
#ifdef _LINUX			
	ret = event_add(e,h,EVENT_READ | EVENT_WRITE);
#elif _BSD
	if(0 == (ret = event_add(e,h,EVENT_READ))){
		ret = event_add(e,h,EVENT_WRITE);
	}else{
		event_remove(e,h);
		return ret;
	}		
#else
		return -EUNSPPLAT;
#endif
	if(ret == 0){
		h->e = e;
		c->callback = (void (*)(int32_t fd,int32_t err,void*))callback;
		if(c->timeout){
			c->t = engine_regtimer(e,c->timeout,connect_timeout,c);
		}	
	}
	return ret;
}

static void process_connect(handle *h,int32_t events){
	connector *c = (connector*)h;
	int32_t err = 0;
	int32_t fd = -1;
	socklen_t len = sizeof(err);
	if(c->t){
		unregister_timer(c->t);
	}
	do{
		if(getsockopt(h->fd, SOL_SOCKET, SO_ERROR, &err, &len) == -1){
			((connector*)h)->callback(-1,err,((connector*)h)->ud);
		    break;
		}
		if(err){
		    errno = err;
		    ((connector*)h)->callback(-1,err,((connector*)h)->ud);    
		    break;
		}
		//success
		fd = h->fd;
	}while(0);    
	if(fd != -1){
		event_remove(h);
		((connector*)h)->callback(fd,0,((connector*)h)->ud);
	}else{
		close(h->fd);
	}		
	free(h);
}


handle *connector_new(int32_t fd,void *ud,uint32_t timeout){
	connector *c = calloc(1,sizeof(*c));
	((handle*)c)->fd = fd;
	((handle*)c)->on_events = process_connect;
	((handle*)c)->imp_engine_add = imp_engine_add;
	c->timeout = timeout;
	c->ud = ud;
	easy_close_on_exec(fd);
	return (handle*)c;
}