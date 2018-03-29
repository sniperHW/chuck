#ifndef _CHK_SOCKET_HELPER_H
#define _CHK_SOCKET_HELPER_H

#include "event/chk_event.h"

enum{
	SOCK_ADDR_NONE = 0,
	SOCK_ADDR_IPV4,
	SOCK_ADDR_IPV6,
	SOCK_ADDR_UN,
};

typedef struct {
    union{
        struct sockaddr_in  in;   //for ipv4 
        struct sockaddr_in6 in6;  //for ipv6
        struct sockaddr_un  un;   //for unix domain
    }; 
    int32_t addr_type;
}chk_sockaddr;

/**
 * 建立监听 
 * @param fd 文件描述符
 * @param server 监听地址
 */

int32_t easy_listen(int32_t fd,chk_sockaddr *server);

/**
 * 建立连接
 * @param fd 文件描述符
 * @param server 监听地址
 * @param local 本地地址 
 */

int32_t easy_connect(int32_t fd,chk_sockaddr *server,chk_sockaddr *local);

int32_t easy_bind(int32_t fd,chk_sockaddr *addr);

int32_t easy_addr_reuse(int32_t fd,int32_t yes);

int32_t easy_noblock(int32_t fd,int32_t noblock); 

int32_t easy_close_on_exec(int32_t fd);

int32_t easy_sockaddr_ip4(chk_sockaddr *addr,const char *ip,uint16_t port);

int32_t easy_sockaddr_un(chk_sockaddr *addr,const char *path);

int32_t easy_sockaddr_inet_ntop(chk_sockaddr *addr,char *out,int len);

int32_t easy_sockaddr_port(chk_sockaddr *addr,uint16_t *port);

size_t  chk_sockaddr_size(chk_sockaddr *addr);

//get the first ipv4 address of name
int32_t easy_hostbyname_ipv4(const char *name,char *host,size_t len);

#endif