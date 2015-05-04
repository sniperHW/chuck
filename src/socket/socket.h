#ifndef __SOCKET_H__
#define __SOCKET_H__

#include "comm.h"

enum{
	SOCKET_STREAM   = 1,
	SOCKET_DATAGRAM = 1 << 2,
	SOCKET_CLOSE    = 1 << 3,
	SOCKET_INLOOP   = 1 << 4,
};

typedef struct socket_{
	handle  base;
	int32_t status;
	list    pending_send;//尚未处理的发请求
    list    pending_recv;//尚未处理的读请求
    union{   
		void    (*stream_callback)(handle*,void*,int32_t,int32_t);
		void    (*datagram_callback)(handle*,void*,int32_t,int32_t,int32_t recvflags);
	};
	void *ud;
	void (*dctor)(void*);
	void (*pending_dctor)(iorequest*);//use to clear pending iorequest
}socket_;


handle *new_stream_socket(int32_t fd);
handle *new_datagram_socket(int32_t fd);

static inline void socket_set_pending_dctor(handle *h,void (*pending_dctor)(iorequest*)){
	((socket_*)h)->pending_dctor = pending_dctor;
}
void    close_socket(handle*);

enum{
	IO_POST = 1,
	IO_NOW  = 2,
};

int32_t stream_socket_send(handle*,iorequest*,int32_t flag);
int32_t stream_socket_recv(handle*,iorequest*,int32_t flag);

//Synchronous send noly
int32_t datagram_socket_send(handle*,iorequest*);
int32_t datagram_socket_recv(handle*,iorequest*,int32_t flag,int32_t *recvflags);

//use by subclass to construct base part
void    construct_stream_socket(socket_*);
void    construct_datagram_socket(socket_*);

#endif