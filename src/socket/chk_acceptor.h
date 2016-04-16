#ifndef _CHK_ACCEPTOR_H
#define _CHK_ACCEPTOR_H

#include "socket/chk_socket_helper.h"

typedef struct chk_acceptor chk_acceptor;

typedef void (*chk_acceptor_cb)(chk_acceptor*,int32_t fd,chk_sockaddr*,void *ud,int32_t err);

/**
 * 恢复acceptor的执行
 * @param a 接受器
 */

int32_t chk_acceptor_resume(chk_acceptor *a);

/**
 * 暂停acceptor的执行
 * @param a 接受器
 */

int32_t chk_acceptor_pause(chk_acceptor *a);

/**
 * 创建一个接受器
 * @param fd 文件描述符
 * @param ud 传递给chk_acceptor_cb的用户数据
 */

chk_acceptor *chk_acceptor_new(int32_t fd,void *ud);

/**
 * 删除接受器
 * @param a 接受器
 */

void chk_acceptor_del(chk_acceptor *a); 


/**
 * 创建一个在ip:port上监听的接收器并注册到loop上
 * @param loop event_loop
 * @param ip 监听地址(ipv4)
 * @param port 监听端口
 * @param cb 事件回调函数
 * @param ud 用户传递数据,调用cb时传回
 */

chk_acceptor *chk_listen_tcp_ip4(chk_event_loop *loop,const char *ip,int16_t port,chk_acceptor_cb cb,void *ud);

int32_t chk_acceptor_get_fd(chk_acceptor *a);

void *chk_acceptor_get_ud(chk_acceptor *a);

void chk_acceptor_set_ud(chk_acceptor *a,void *ud);

#endif