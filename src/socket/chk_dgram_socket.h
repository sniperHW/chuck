#ifndef _CHK_DGRAM_SOCKET_H
#define _CHK_DGRAM_SOCKET_H

/*
* 数据报套接字
*/

typedef struct {
	chk_bytebuffer *msg;           //数据
	chk_sockaddr   *peeraddr;      //对端地址
	chk_sockaddr   *destaddr;      //数据报的目标地址
	chk_sockaddr   *localaddr;     //接收到数据报的本地接口地址
}chk_dgram_pkt;

typedef struct chk_dgram_socket chk_dgram_socket;

typedef void (*chk_dgram_socket_cb)(chk_dgram_socket*,chk_dgram_pkt *pkt,int32_t error);

chk_dgram_socket *chk_dgram_socket_new(int family,uint32_t recvbuff_size,uint32_t max_send_packet_size);

int32_t chk_dgram_socket_bind(chk_dgram_socket *s,chk_sockaddr *local);

int32_t chk_dgram_socket_connect(chk_dgram_socket *s,chk_sockaddr *peer);

void chk_dgram_socket_close(chk_dgram_socket *s,uint32_t delay);

void chk_dgram_socket_send(chk_dgram_socket *s,chk_bytebuffer *buffer,send_finish_callback cb,chk_ud ud);

void chk_dgram_socket_sendto(chk_dgram_socket *s,chk_bytebuffer *buffer,chk_sockaddr *peeraddr,send_finish_callback cb,chk_ud ud);

void chk_dgram_socket_setud(chk_dgram_socket *s,chk_ud ud);

chk_ud chk_dgram_socket_getud(chk_dgram_socket *s);


#endif