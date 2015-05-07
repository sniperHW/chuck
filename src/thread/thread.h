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
#include "thread/sync.h"

typedef struct
{
	pthread_t threadid;
    int32_t   joinable;
}thread;


enum
{
	DETACH   = 1,
	JOINABLE = 1 << 2,
    WAITRUN  = 1 << 3,
};

thread*
thread_new(int32_t flag,
           void *(*routine)(void*),
           void *ud);

void*
thread_del(thread*);

void*
thread_join(thread*);

pid_t
thread_id();

#endif
