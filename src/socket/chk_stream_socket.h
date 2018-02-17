#ifndef  _CHK_STREAM_SOCKET_H
#define _CHK_STREAM_SOCKET_H

/*
* 流套接字
*/

#include "event/chk_event.h"
#include "util/chk_bytechunk.h"
#include "util/chk_timer.h"
#include "socket/chk_decoder.h"
#include "chk_ud.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

struct ssl_ctx {
    SSL_CTX *ctx;
    SSL *ssl;
};


typedef struct chk_stream_socket chk_stream_socket;

typedef struct chk_stream_socket_option chk_stream_socket_option;

typedef void (*chk_stream_socket_cb)(chk_stream_socket*,chk_bytebuffer*,int32_t error);

struct chk_stream_socket_option {
	uint32_t     recv_buffer_size;       //接收缓冲大小
	chk_decoder *decoder;
};

/**
 * 创建stream_socket
 * @param fd 文件描述符
 * @param option 相关选项
 */

chk_stream_socket *chk_stream_socket_new(int32_t fd,const chk_stream_socket_option *option);

/**
 * 关闭stream_socket,同时关联的fd被关闭,当stream_socket完成关闭后将其销毁
 * @param s stream_socket
 * @param delay控制延迟关闭参数(单位毫秒),如果设置为0,stream_socket丢弃所有未发送完成的数据立即关闭
 *        delay != 0,stream_socket将尝试继续发送剩余数据，一旦全部发送完成立即关闭，否则
 *        到达delay超时才关闭
 * 注意,不管stream_socket是否完成关闭,调用chk_stream_socket_close之后都应认为stream_socket已经失效
 * 不应该再对stream_socket执行任何操作    
 */

void chk_stream_socket_close(chk_stream_socket *s,uint32_t delay);

//执行半关闭(关闭写端)
void chk_stream_socket_shutdown_write(chk_stream_socket *s);

/**
 * 发送一个buffer,如果当前没有待发送的数据，会立刻尝试发送
 * @param s stream_socket
 * @param b 待发送缓冲,调用chk_stream_socket_send之后b不能再被别处使用
 */

int32_t chk_stream_socket_send(chk_stream_socket *s,chk_bytebuffer *b);


/**
 * 发送一个紧急buffer,如果当前没有待发送的数据，会立刻尝试发送
 * @param s stream_socket
 * @param b 待发送缓冲,调用chk_stream_socket_send之后b不能再被别处使用
 *
 * 一个stream_socket管理两个buffer发送队列，普通队列和urgent队列,urgent队列的发送优先级
 * 高于普通队列	
 *
 * urgent队列抢占普通队列的处理，如果普通队列中没有发送了一半的buffer,那么直到urgent队列发送完毕
 * 才会继续发送普通队列中的buffer,如果普通队列存在发送了一半的buffer,那么将等到这个buffer发送完毕
 * 再发送urgent队列中的buffer.普通队列中剩余的未启动发送的buffer则需要等到urgent队列发送完毕之后
 * 才被继续发送。
 *
 */

int32_t chk_stream_socket_send_urgent(chk_stream_socket *s,chk_bytebuffer *b);

/**
 * 设置chk_stream_socket关联的用户数据
 * @param s stream_socket
 * @param ud 用户数据
 */

void chk_stream_socket_setUd(chk_stream_socket *s,chk_ud ud);

/**
 * 获取chk_stream_socket关联的用户数据
 * @param s stream_socket
 */

chk_ud chk_stream_socket_getUd(chk_stream_socket *s);

int32_t chk_stream_socket_getfd(chk_stream_socket *s);

/**
 * 暂停事件处理(移除读监听)
 * @param s stream_socket
 */

void  chk_stream_socket_pause_read(chk_stream_socket *s);

/**
 * 恢复读监听
 * @param s stream_socket
 */

void  chk_stream_socket_resume_read(chk_stream_socket *s);


int32_t chk_ssl_accept(chk_stream_socket *s,SSL_CTX *ctx);

int32_t chk_ssl_connect(chk_stream_socket *s);

int32_t chk_stream_socket_getsockaddr(chk_stream_socket *s,chk_sockaddr *addr);

int32_t chk_stream_socket_getpeeraddr(chk_stream_socket *s,chk_sockaddr *addr);

void chk_stream_socket_nodelay(chk_stream_socket *s,int8_t on);

int32_t chk_stream_socket_set_close_callback(chk_stream_socket *s,void (*cb)(chk_stream_socket*,chk_ud),chk_ud ud);

#endif