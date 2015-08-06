#ifndef _CHK_SOCKET_HELPER_H
#define _CHK_SOCKET_HELPER_H

#include "event/chk_event.h"

int32_t easy_listen(int32_t fd,chk_sockaddr *server);

int32_t easy_connect(int32_t fd,chk_sockaddr *server,chk_sockaddr *local);

int32_t easy_bind(int32_t fd,chk_sockaddr *addr);

int32_t easy_addr_reuse(int32_t fd,int32_t yes);

int32_t easy_noblock(int32_t fd,int32_t noblock); 

int32_t easy_close_on_exec(int32_t fd);

int32_t easy_sockaddr_ip4(chk_sockaddr *addr,const char *ip,uint16_t port);

int32_t easy_sockaddr_un(chk_sockaddr *addr,const char *path);

//get the first ipv4 address of name
int32_t easy_hostbyname_ipv4(const char *name,char *host,size_t len);

#endif