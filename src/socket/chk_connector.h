#ifndef _CHK_CONNECTOR_H
#define _CHK_CONNECTOR_H

#include <stdint.h>
#include "socket/chk_socket_helper.h"
#include "chk_ud.h"

typedef void (*connect_cb)(int32_t fd,chk_ud ud,int32_t err);

int32_t chk_async_connect(int fd,chk_event_loop *e,chk_sockaddr *peer,chk_sockaddr *local,connect_cb cb,chk_ud ud,uint32_t timeout);  

int32_t chk_easy_async_connect(chk_event_loop *e,chk_sockaddr *peer,chk_sockaddr *local,connect_cb cb,chk_ud ud,uint32_t timeout);  

#endif