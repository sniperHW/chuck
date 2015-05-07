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

#ifndef _ACCEPTOR_H
#define _ACCEPTOR_H

#include "comm.h"
#include "lua/lua_util.h"  

//typedef void (*accepted_callback)(int32_t fd,sockaddr_*,void *ud);

typedef struct{
    handle  base;
    void   *ud;      
    void    (*callback)(int32_t fd,sockaddr_*,void *ud);
    luaRef  luacallback;
}acceptor;    

handle*
acceptor_new(int32_t fd,void *ud);

void    
acceptor_del(handle*); 

void    
reg_luaacceptor(lua_State *L);   


#endif