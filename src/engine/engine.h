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
#ifndef _ENGINE_H
#define _ENGINE_H

#include <stdint.h>
#include "comm.h"
#include "util/timewheel.h"  
#include "lua/lua_util.h"  

engine*
engine_new();

void    
engine_del(engine*);

int32_t 
engine_run(engine*);

int32_t
engine_runonce(engine*,uint32_t timeout);

void    
engine_stop(engine*);

int32_t 
engine_add(engine*,handle*,generic_callback);

int32_t 
engine_remove(handle*);

timer*
engine_regtimer(engine*,uint32_t timeout,
                int32_t(*)(uint32_t,uint64_t,void*),
                void*);

engine*
lua_toengine(lua_State *L, int index);

void
reg_luaengine(lua_State *L);

#define engine_associate(E,H,C)\
            engine_add((E),(handle*)(H),(generic_callback)(C))

//private function
int32_t 
event_add(engine*,handle*,int32_t events);

int32_t 
event_remove(handle*);

int32_t 
event_enable(handle*,int32_t events);

int32_t 
event_disable(handle*,int32_t events);

static inline int32_t 
enable_read(handle *h)
{    
    return event_enable(h,EVENT_READ);
}

static inline int32_t 
disable_read(handle *h)
{
    return event_disable(h,EVENT_READ);
}

static inline int32_t 
enable_write(handle *h)
{   
    return event_enable(h,EVENT_WRITE);
}

static inline int32_t 
disable_write(handle *h)
{
    return event_disable(h,EVENT_WRITE);         
}

    
#endif
