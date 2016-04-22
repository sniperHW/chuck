#include <assert.h>
#include "thread/chk_thread.h"
#include "util/chk_log.h"

#define _CORE_

#include "event/chk_event_loop.h" 
#include "event/chk_event_loop_define.h"

static int32_t stopevent = 1;

#define READY_TO_HANDLE(ENTRY)                                              \
    (chk_handle*)(((char*)(ENTRY))-sizeof(chk_dlist_entry))


enum {
	INLOOP  =  1 << 1,
	CLOSING =  1 << 2,
};

#ifdef _LINUX
#	include "chk_event_loop_epoll.h"
#elif  _MACH
#	include "chk_event_loop_kqueue.h"
#else
#	error "un support platform!"		
#endif

int32_t chk_loop_run_once(chk_event_loop *e,uint32_t ms) {
	return _loop_run(e,ms,1);
}

int32_t chk_loop_add_handle(chk_event_loop *e,chk_handle *h,chk_event_callback cb) {
	if(h->loop) return chk_error_duplicate_add_handle;
	return h->handle_add(e,h,cb);
}

void chk_loop_end(chk_event_loop *e) {
	TEMP_FAILURE_RETRY(write(e->notifyfds[1],&stopevent,sizeof(stopevent)));
}

chk_event_loop *chk_loop_new() {
	chk_event_loop *ep = calloc(1,sizeof(*ep));
	if(chk_error_ok != chk_loop_init(ep)) {
		CHK_SYSLOG(LOG_ERROR,"chk_loop_init() failed");
		free(ep);
		ep = NULL;
	}
	return ep;
}

void chk_loop_del(chk_event_loop *e) {
	if(e->threadid != chk_thread_current_tid()) {
		CHK_SYSLOG(LOG_ERROR,"e->threadid != chk_thread_current_tid()");
		return;
	}
	if(e->status & INLOOP)
		e->status |= CLOSING;
	else {
		chk_loop_finalize(e);
		free(e);
	}
}

int32_t chk_loop_run(chk_event_loop *e) {
	return _loop_run(e,0,0);
}

int32_t chk_loop_remove_handle(chk_handle *h) {
	return chk_unwatch_handle(h);
}

