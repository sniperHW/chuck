#ifndef _CHK_CONNECTOR_H
#define _CHK_CONNECTOR_H

#include <stdint.h>
#include "event/chk_event.h"

typedef void (*connect_cb)(int32_t fd,int32_t err,void *ud);

/*
* 阻塞connect,e,cb传NULL  
*/
int32_t chk_connect(int32_t fd,chk_sockaddr *server,chk_sockaddr *local,
                    chk_event_loop *e,connect_cb cb,void*ud,uint32_t timeout);    

#endif