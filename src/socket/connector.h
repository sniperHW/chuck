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

#ifndef _CONNECTOR_H
#define _CONNECTOR_H

#include "comm.h"
#include "util/timewheel.h"
#include "lua/lua_util.h"    

//void (*connected_callback)(int32_t fd,int32_t err,void *ud);    

typedef struct{
    handle    base;  
    void     *ud;
    void      (*callback)(int32_t fd,int32_t err,void *ud);
    uint32_t  timeout;
    timer    *t;
    luaRef    luacallback; 
}connector;

handle*
connector_new(int32_t fd,void *ud,uint32_t timeout);    

//need not delete,engine will do for you

void    
reg_luaconnector(lua_State *L);

#endif