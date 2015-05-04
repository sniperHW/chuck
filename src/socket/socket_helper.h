/*
    Copyright (C) <2015>  <sniperHW@163.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _SOCKET_HELPER_H
#define _SOCKET_HELPER_H

#include "./socket.h"
#include "lua/lua_util.h"

int32_t easy_listen(int32_t fd,sockaddr_ *server);

int32_t easy_connect(int32_t fd,sockaddr_ *server,sockaddr_ *local);

static inline int32_t easy_bind(int32_t fd,sockaddr_ *addr){
    return bind(fd,(struct sockaddr*)addr,sizeof(*addr));
}

static inline int32_t easy_addr_reuse(int32_t fd,int32_t yes){
	errno = 0;
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
		return -errno;
	return 0;	
}

static inline int32_t easy_noblock(int32_t fd,int32_t noblock){
	errno = 0;
	int32_t flags;
	if((flags = fcntl(fd, F_GETFL, 0)) < 0){
    	return -errno;
	}
    if(!noblock){
        flags &= (~O_NONBLOCK);
    }else {
        flags |= O_NONBLOCK;
    }

    return fcntl(fd, F_SETFL, flags) == 0 ? 0 : -errno;	
}

static inline int32_t easy_close_on_exec(int32_t fd){
	errno = 0;
	int32_t flags;
	if((flags = fcntl(fd, F_GETFL, 0)) < 0){
    	return -errno;
	}
	return fcntl(fd, F_SETFD, flags|FD_CLOEXEC) == 0 ? 0 : -errno;
}

static inline int32_t easy_sockaddr_ip4(sockaddr_ *addr,const char *ip,uint16_t port){
    memset((void*)addr,0,sizeof(*addr));
    addr->in.sin_family = AF_INET;
    addr->in.sin_port = htons(port);
    if(inet_pton(AF_INET,ip,&addr->in.sin_addr) == 1)
        return 0;
    return -1;
}

static inline int32_t easy_sockaddr_un(sockaddr_ *addr,const char *path){
    memset((void*)addr,0,sizeof(*addr));
    addr->un.sun_family = AF_LOCAL;
    strncpy(addr->un.sun_path,path,sizeof(addr->un.sun_path)-1);
    return 0;
}

void reg_luasocket_helper(lua_State *L);

#endif