#ifndef  _CHK_STREAM_SOCKET_H
#define _CHK_STREAM_SOCKET_H

/*
* 流套接字
*/

#include "event/chk_event.h"
#include "util/chk_bytechunk.h"
#include "util/chk_timer.h"
#include "socket/chk_decoder.h"

#define MAX_WBAF          1024

#define MAX_SEND_SIZE     1024*64

typedef struct chk_stream_socket chk_stream_socket;

typedef struct chk_stream_socket_option chk_stream_socket_option;

typedef void (*chk_stream_socket_cb)(chk_stream_socket*,chk_bytebuffer*,int32_t error);

struct chk_stream_socket_option {
	uint32_t     recv_buffer_size;       //接收缓冲大小
	chk_decoder *decoder;
};

struct chk_stream_socket {
	_chk_handle;
	chk_stream_socket_option option;
	struct iovec         wsendbuf[MAX_WBAF];
    struct iovec         wrecvbuf[2];
    uint32_t             status;
    uint32_t             next_recv_pos;
    chk_bytechunk       *next_recv_buf;
    void                *ud;        
    chk_list             send_list;             //待发送的包
    chk_timer           *timer;                 //用于最后的发送处理
    chk_stream_socket_cb cb;
    uint8_t              create_by_new;
};

void chk_stream_socket_init(chk_stream_socket *s,int32_t fd,chk_stream_socket_option *option);

/**
 * 创建stream_socket
 * @param fd 文件描述符
 * @param option 相关选项
 */

chk_stream_socket *chk_stream_socket_new(int32_t fd,chk_stream_socket_option *option);

/**
 * 关闭stream_socket,同时关联的fd被关闭
 * @param s stream_socket
 * @param now 如果now=0,则读端关闭,如果写队列中还有数据则尝试将数据写完,否则连接立即关闭
 */

void chk_stream_socket_close(chk_stream_socket *s,int32_t now);

/**
 * 发送一个buffer
 * @param s stream_socket
 * @param b 待发送缓冲,调用chk_stream_socket_send之后b不能再被别处使用
 */

int32_t chk_stream_socket_send(chk_stream_socket *s,chk_bytebuffer *b);

/**
 * 设置chk_stream_socket关联的用户数据
 * @param s stream_socket
 * @param ud 用户数据
 */

void chk_stream_socket_setUd(chk_stream_socket *s,void*ud);

/**
 * 获取chk_stream_socket关联的用户数据
 * @param s stream_socket
 */

void *chk_stream_socket_getUd(chk_stream_socket *s);

/**
 * 暂停事件处理(移除读写监听)
 * @param s stream_socket
 */

void  chk_stream_socket_pause(chk_stream_socket *s);

/**
 * 恢复事件处理
 * @param s stream_socket
 */

void  chk_stream_socket_resume(chk_stream_socket *s);

#endif