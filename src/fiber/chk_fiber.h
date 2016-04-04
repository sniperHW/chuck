/*	
    Copyright (C) <2012-2014>  <huangweilook@21cn.com>

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
#ifndef _CHK_FIBER_H
#define _CHK_FIBER_H
#include <stdint.h>
#include "util/chk_refobj.h"

typedef chk_refhandle chk_fiber_t;

/*
*    init fiber system for current thread 
*/
int                 chk_fiber_sheduler_init(uint32_t stack_size);

int                 chk_fiber_sheduler_clear();

int                 chk_fiber_schedule();

int                 chk_fiber_yield();

int                 chk_fiber_sleep(uint32_t ms,void **);

int                 chk_fiber_block(uint32_t ms,void **);

int                 chk_fiber_wakeup(chk_fiber_t,void **);

chk_fiber_t         chk_fiber_spawn(void(*fun)(void*),void *param);

static inline int chk_is_vaild_fiber(chk_fiber_t u){
    return chk_is_vaild_refhandle(u);
}

/*
*   get current running fiber or current thread
*   if not running in a fiber context,the return value should be empty_ident,
*   use chk_is_vaild_fiber() to check the return value
*/
chk_fiber_t     chk_current_fiber();

//void *ut_dic_get(uthread_t,const char *key);
//int      ut_dic_set(uthread_t,const char *key,void *value,void (*value_destroyer)(void*));


#endif
