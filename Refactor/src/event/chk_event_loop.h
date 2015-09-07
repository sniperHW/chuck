#ifndef _CHK_EVENT_LOOP_H
#define _CHK_EVENT_LOOP_H

#include <stdint.h>
#include "util/chk_timer.h"  
#include "util/chk_list.h"  
#include "event/chk_event.h"


int32_t         chk_loop_init(chk_event_loop*);

void            chk_loop_finalize(chk_event_loop*);

chk_event_loop *chk_loop_new();

void            chk_loop_del(chk_event_loop*);

int32_t         chk_loop_run(chk_event_loop*);

int32_t         chk_loop_run_once(chk_event_loop*,uint32_t ms);

void            chk_loop_end(chk_event_loop*);

chk_timer      *chk_loop_addtimer(chk_event_loop*,uint32_t timeout,chk_timeout_cb,void *ud);

int32_t         chk_loop_add_handle(chk_event_loop*,chk_handle*,chk_event_callback);

void            chk_loop_remove_handle(chk_handle*);

#endif