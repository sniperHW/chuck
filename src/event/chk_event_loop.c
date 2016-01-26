#include <assert.h>
#include "thread/chk_thread.h"

#define _CORE_

#include "event/chk_event_loop.h" 
#include "event/chk_event_loop_define.h"

extern int32_t pipe2(int pipefd[2], int flags);
static int32_t stopevent = 1;

#define READY_TO_HANDLE(ENTRY)                                              \
    (chk_handle*)(((char*)(ENTRY))-sizeof(chk_dlist_entry))


enum {
	INLOOP  =  1 << 1,
	CLOSING =  1 << 2,
};

#ifdef _LINUX
#	include "chk_event_loop_epoll.h"
#elif  _BSD
#	include "chk_event_loop_kqueue.h"
#else
#	error "un support platform!"		
#endif

int32_t chk_loop_run_once(chk_event_loop *e,uint32_t ms) {
	return _loop_run(e,ms,1);
}

int32_t chk_loop_add_handle(chk_event_loop *e,chk_handle *h,chk_event_callback cb) {
	if(h->loop) chk_events_remove(h);
	return h->handle_add(e,h,cb);
}

void chk_loop_end(chk_event_loop *e) {
	TEMP_FAILURE_RETRY(write(e->notifyfds[1],&stopevent,sizeof(stopevent)));
}

chk_event_loop *chk_loop_new() {
	chk_event_loop *ep = calloc(1,sizeof(*ep));
	if(0 != chk_loop_init(ep)) {
		free(ep);
		ep = NULL;
	}
	return ep;
}

void chk_loop_del(chk_event_loop *e) {
	assert(e->threadid == chk_thread_id());
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

void chk_loop_remove_handle(chk_handle *h) {
	chk_events_remove(h);
}

