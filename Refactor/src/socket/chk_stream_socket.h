#ifndef  _CHK_STREAM_SOCKET_H
#define _CHK_STREAM_SOCKET_H

/*
* 流套接字
*/

#include "event/chk_event.h"
#include "util/chk_bytechunk.h"
#include "socket/chk_decoder.h"

typedef struct chk_stream_socket chk_stream_socket;

typedef struct chk_stream_socket_option chk_stream_socket_option;

typedef void (*chk_stream_socket_cb)(chk_stream_socket*,int32_t err,chk_bytebuffer*);

struct chk_stream_socket_option {
	uint32_t     recv_buffer_size;       //接收缓冲大小
	uint32_t     recv_timeout;           //接收超时
	uint32_t     send_timeout;           //发送超时
	chk_decoder *decoder;
};

chk_stream_socket *chk_stream_socket_new(int32_t fd,chk_stream_socket_option*);

void               chk_stream_socket_close(chk_stream_socket*);
/*
*发送一个buffer,调用之后buffer会在适当的时候release,请勿继续使用
*/
int32_t            chk_stream_socket_send(chk_stream_socket*,chk_bytebuffer*);

void               chk_stream_socket_setUd(chk_stream_socket*,void*ud);

void              *chk_stream_socket_getUd(chk_stream_socket*);


#endif