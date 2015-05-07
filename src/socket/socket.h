#ifndef __SOCKET_H__
#define __SOCKET_H__

#include "comm.h"

//status
enum{
	SOCKET_CLOSE    = 1 << 1,
	SOCKET_INLOOP   = 1 << 2,
	SOCKET_RELEASE  = 1 << 3,
	SOCKET_END      = SOCKET_RELEASE,
};

//type
enum{
	STREAM   = 1,
	DATAGRAM = 2,
};

typedef struct socket_{
	handle  base;
	int32_t status;
	int32_t type;
    list    pending_recv;//尚未处理的读请求
	void   *ud;
	void    (*dctor)(void*);
	void    (*pending_dctor)(iorequest*);//use to clear pending iorequest
}socket_;

typedef struct stream_socket_{
	socket_ base;
	list    pending_send;//尚未处理的发请求
	void    (*callback)(struct stream_socket_*,void*,int32_t,int32_t);
}stream_socket_;

typedef struct dgram_socket_{
	socket_ base;
	void   (*callback)(struct dgram_socket_*,
					   void*,int32_t,int32_t,
					   int32_t recvflags);
}dgram_socket_;


stream_socket_*
new_stream_socket(int32_t fd);

dgram_socket_ *
new_datagram_socket(int32_t fd);

static inline void 
socket_set_pending_dctor(socket_ *s,
						 void (*pending_dctor)(iorequest*))
{
	s->pending_dctor = pending_dctor;
}

void 
close_socket(socket_ *s);

enum{
	IO_POST = 1,
	IO_NOW  = 2,
};

int32_t 
stream_socket_send(stream_socket_*,iorequest*,int32_t flag);

int32_t 
stream_socket_recv(stream_socket_*,iorequest*,int32_t flag);

//Synchronous send noly
int32_t 
datagram_socket_send(dgram_socket_*,iorequest*);

int32_t 
datagram_socket_recv(dgram_socket_*,iorequest*,
					 int32_t flag,int32_t *recvflags);

//use by subclass to construct base part
void    
construct_stream_socket(stream_socket_*);

void    
construct_datagram_socket(dgram_socket_*);

#endif