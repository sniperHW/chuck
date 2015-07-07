/*
    Copyright (C) <2014>  <sniperHW@163.com>

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
#ifndef _THREAD_H
#define _THREAD_H
#include <pthread.h>
#include <stdint.h>
#include "comm.h"    
#include "thread/sync.h"
#include "util/refobj.h"
#include "util/list.h"

typedef refhandle thread_t;

typedef struct{
    listnode  node;
    thread_t  sender;
    void (*dctor)(void*);
}mail;

static inline void mail_del(mail *mail_)
{
    if(mail_->dctor) 
        mail_->dctor(mail_);
    free(mail_);
}

thread_t thread_new(void *(*routine)(void*),void *ud);

void    *thread_join(thread_t);

pid_t    thread_id();

int32_t  thread_sendmail(thread_t,mail*);

int32_t  thread_process_mail(engine*,void (*onmail)(mail *_mail));


#if _CHUCKLUA

#include "lua/lua_util.h"

void reg_luathread(lua_State *L);

#endif


#endif
