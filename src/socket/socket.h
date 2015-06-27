#ifndef __SOCKET_H__
#define __SOCKET_H__

#include "comm.h"

//status
enum{
	SOCKET_CLOSE     = 1 << 1,
	SOCKET_INLOOP    = 1 << 2,
	SOCKET_READABLE  = 1 << 3,
	SOCKET_WRITEABLE = 1 << 4,
	SOCKET_END       = SOCKET_WRITEABLE,
};

//type
enum{
	STREAM   = 1,
	DATAGRAM = 2,
};

#define socket_head								\
	handle_head;								\
	int32_t status;								\
	int32_t type;								\
    list    pending_recv;						\
	void   *ud;									\
	void    (*dctor)(void*);					\
	void    (*pending_dctor)(iorequest*)

typedef struct socket_{
	socket_head;
}socket_;

typedef struct stream_socket_ stream_socket_;
typedef struct dgram_socket_  dgram_socket_; 

#define stream_socket_head						\
		socket_head;							\
		list    pending_send;					\
		void    (*callback)						\
				(stream_socket_*,				\
					void*,int32_t,int32_t)

#define dgram_socket_head						\
		socket_head;							\
		void   (*callback)						\
			   (dgram_socket_*,					\
			    void*,int32_t,int32_t,			\
				int32_t recvflags);		

struct stream_socket_{
	stream_socket_head;
};

struct dgram_socket_{
	dgram_socket_head;
};


stream_socket_ *new_stream_socket(int32_t fd);

dgram_socket_  *new_datagram_socket(int32_t fd);

static inline void socket_set_pending_dctor(socket_ *s,void (*pending_dctor)(iorequest*))
{
	s->pending_dctor = pending_dctor;
}

void close_socket(socket_ *s);

enum{
	IO_POST = 1,
	IO_NOW  = 2,
};

int32_t stream_socket_send(stream_socket_*,iorequest*,int32_t flag);

int32_t stream_socket_recv(stream_socket_*,iorequest*,int32_t flag);

//Synchronous send noly
int32_t datagram_socket_send(dgram_socket_*,iorequest*);

int32_t datagram_socket_recv(dgram_socket_*,iorequest*,int32_t flag,int32_t *recvflags);

//use by subclass to construct base part
void    stream_socket_init(stream_socket_*,int32_t fd);

void    datagram_socket_init(dgram_socket_*,int32_t fd);

#endif