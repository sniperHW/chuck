#ifndef _CHK_CONNECTOR_H
#define _CHK_CONNECTOR_H

#include <stdint.h>
#include "socket/chk_socket_helper.h"
#include "chk_ud.h"

typedef void (*connect_cb)(int32_t fd,chk_ud ud,int32_t err);

/**
 * 建立到远端的连接
 * @param server 远端地址
 * @param local  本地地址(可以为NULL)
 * @param e 事件循环(如果使用异步连接必须)
 * @param cb 连接回调(如果使用异步连接必须)
 * @param ud 传递给cb的用户数据
 * @param timeout 连接超时(异步连接使用)
 */
int32_t chk_connect(chk_sockaddr *server,chk_sockaddr *local,chk_event_loop *e,connect_cb cb,chk_ud ud,uint32_t timeout);    

#endif