#define _CORE_
#include <assert.h>
#include "util/chk_error.h"
#include "util/chk_log.h"
#include "socket/chk_socket_helper.h"
#include "socket/chk_dgram_socket.h"
#include "event/chk_event_loop.h"
#include "socket/chk_dgram_socket_define.h"

/*status*/
enum{
	SOCKET_CLOSE     = 1 << 1,  /*连接完全关闭,对象可以被销毁*/
	SOCKET_INLOOP    = 1 << 2,
};


static int32_t loop_add(chk_event_loop *e,chk_handle *h,chk_event_callback cb) {
	int32_t ret,flags;
	chk_dgram_socket *s = cast(chk_dgram_socket*,h);
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
	if(s->status & (SOCKET_CLOSE)){
		CHK_SYSLOG(LOG_ERROR,"chk_dgram_socket close");			
		return chk_error_socket_close;
	}
	if(!send_list_empty(s))
		flags = CHK_EVENT_READ | CHK_EVENT_WRITE;
	else
		flags = CHK_EVENT_READ;	
	if(chk_error_ok == (ret = chk_watch_handle(e,h,flags))) {
		//easy_noblock(h->fd,1);
		s->cb = cast(chk_dgram_socket_cb,cb);
	}
	else {
		CHK_SYSLOG(LOG_ERROR,"chk_watch_handle() failed:%d",ret);		
	}
	return ret;
}