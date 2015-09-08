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

#ifndef _TIMEWHEEL_H
#define _TIMEWHEEL_H

#include <stdint.h>
#include "comm.h"
#include "lua/lua_util.h"    

#define MAX_TIMEOUT (1000*3600*24-1)

typedef struct timer timer;
typedef struct wheelmgr wheelmgr;

enum{
	TEVENT_TIMEOUT = 1,
	TEVENT_DESTROY,
};

typedef int32_t (*timer_cb)(uint32_t event,uint64_t tick,void*ud);

wheelmgr *wheelmgr_new();

void wheelmgr_del(wheelmgr*);

timer *wheelmgr_register(wheelmgr*,uint32_t ms,timer_cb cb,void *ud,uint64_t now);

void unregister_timer(timer*);

void wheelmgr_tick(wheelmgr*,uint64_t now);

//just for test
uint64_t timer_expire(timer*); 

uint32_t timer_timeout(timer*);

#ifdef _CHUCKLUA

void reg_luatimewheel(lua_State *L); 

#endif

#endif