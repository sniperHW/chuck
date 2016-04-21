#include "socket/chk_socket_helper.h"
#include "util/chk_error.h"

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

int32_t easy_listen(int32_t fd,chk_sockaddr *server) {
	errno = 0;
	if(easy_bind(fd,server) != 0)
		 return chk_error_bind;
	if(listen(fd,SOMAXCONN) != 0)
		return chk_error_listen;
	return chk_error_ok;
}

int32_t easy_connect(int32_t fd,chk_sockaddr *server,chk_sockaddr *local) {
	int32_t ret;
	if(local && chk_error_ok != easy_bind(fd,local))
	   return chk_error_bind;
    if(server->addr_type == SOCK_ADDR_IPV4)    
	   ret = connect(fd,cast(struct sockaddr*,server),sizeof(server->in));
	else if(server->addr_type == SOCK_ADDR_UN)
       ret = connect(fd,cast(struct sockaddr*,server),sizeof(server->un));
    else
        return chk_error_unsupport_addr_type;
    
    if(ret == chk_error_ok || ret == EINPROGRESS)
        return chk_error_ok;
    else
        return chk_error_connect;
}

int32_t easy_bind(int32_t fd,chk_sockaddr *addr) {
    int32_t ret = chk_error_ok;

    if(addr->addr_type == SOCK_ADDR_IPV4)
        ret = bind(fd,(struct sockaddr*)addr,sizeof(addr->in));
    else if(addr->addr_type == SOCK_ADDR_UN)
        ret = bind(fd,(struct sockaddr*)addr,sizeof(addr->un));
    else
        ret = -1;
    return ret == chk_error_ok ? ret : chk_error_bind;    
}

int32_t easy_addr_reuse(int32_t fd,int32_t yes) {
	errno = 0;
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
		return chk_error_setsockopt;
	return chk_error_ok;	
}

int32_t easy_noblock(int32_t fd,int32_t noblock) {
    int32_t flags;
	if((flags = fcntl(fd, F_GETFL, 0)) < 0){
    	return chk_error_fcntl;
	}
    if(!noblock){
        flags &= (~O_NONBLOCK);
    }else {
        flags |= O_NONBLOCK;
    }

    return fcntl(fd, F_SETFL, flags) == chk_error_ok ? chk_error_ok : chk_error_fcntl;	
}

int32_t easy_close_on_exec(int32_t fd) {
	int32_t flags;;
    if((flags = fcntl(fd, F_GETFL, 0)) < 0){
    	return chk_error_fcntl;
	}
	return fcntl(fd, F_SETFD, flags|FD_CLOEXEC) == chk_error_ok ? chk_error_ok : chk_error_fcntl;
}

int32_t easy_sockaddr_ip4(chk_sockaddr *addr,const char *ip,uint16_t port) {
    memset(cast(void*,addr),0,sizeof(*addr));
    addr->addr_type = SOCK_ADDR_IPV4;
    addr->in.sin_family = AF_INET;
    addr->in.sin_port = htons(port);
    if(inet_pton(AF_INET,ip,&addr->in.sin_addr) == 1)
        return chk_error_ok;
    return chk_error_invaild_sockaddr;
}

int32_t easy_sockaddr_un(chk_sockaddr *addr,const char *path) {
    memset(cast(void*,addr),0,sizeof(*addr));
    addr->addr_type = SOCK_ADDR_UN;    
    addr->un.sun_family = AF_LOCAL;
    strncpy(addr->un.sun_path,path,sizeof(addr->un.sun_path)-1);
    return chk_error_ok;
}

//get the first ipv4 address of name
int32_t easy_hostbyname_ipv4(const char *name,char *host,size_t len) {
   
#ifdef _MACH
    struct hostent *result;
    if(NULL == (result = gethostbyname(name)))
        return chk_error_invaild_hostname;
    if(inet_ntop(AF_INET, result->h_addr_list[0],host, len) != NULL)
        return chk_error_ok;
    return chk_error_invaild_hostname;
#else    
    int     h_err;
    char    buf[8192];
    struct hostent ret, *result;
    if(gethostbyname_r(name, &ret, buf, 8192, &result, &h_err) != 0)
        return chk_error_invaild_hostname;
    if(inet_ntop(AF_INET, result->h_addr_list[0],host, len) != NULL)
        return chk_error_ok;
    return chk_error_invaild_hostname;
#endif
}