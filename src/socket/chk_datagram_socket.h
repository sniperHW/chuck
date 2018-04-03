#ifndef _CHK_DATAGRAM_SOCKET_H
#define _CHK_DATAGRAM_SOCKET_H

/*
* 数据报套接字
*/

#include "event/chk_event.h"
#include "util/chk_bytechunk.h"
#include "util/chk_timer.h"
#include "chk_ud.h"


typedef struct chk_datagram_socket chk_datagram_socket;

typedef struct chk_datagram_event {
	chk_sockaddr    src;
	chk_bytebuffer *buff;
}chk_datagram_event;

typedef void (*chk_datagram_socket_cb)(chk_datagram_socket*,chk_datagram_event*,int32_t error);

chk_datagram_socket *chk_datagram_socket_new(int32_t fd,int32_t addr_type);

void chk_datagram_socket_close(chk_datagram_socket *s);

int32_t chk_datagram_socket_sendto(chk_datagram_socket *s,chk_bytebuffer *buff,chk_sockaddr *dst);

chk_ud chk_datagram_socket_getUd(chk_datagram_socket *s);

void chk_datagram_socket_setUd(chk_datagram_socket *s,chk_ud ud);

int32_t chk_datagram_socket_set_broadcast(chk_datagram_socket *s);

int32_t chk_datagram_socket_broadcast(chk_datagram_socket *s,chk_bytebuffer *buff,chk_sockaddr *addr);


#endif