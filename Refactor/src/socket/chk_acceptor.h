#ifndef _CHK_ACCEPTOR_H
#define _CHK_ACCEPTOR_H

#include "event/chk_event.h"


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


#endif