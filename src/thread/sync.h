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
#ifndef  _SYNC_H
#define  _SYNC_H
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <errno.h>
#include "util/time.h"
/*Mutex*/
typedef struct
{
	pthread_mutex_t     m_mutex;
	pthread_mutexattr_t m_attr;
}mutex;


static inline 
int32_t mutex_lock(mutex *m)
{
	return pthread_mutex_lock(&m->m_mutex);
}

static inline int32_t 
mutex_trylock(mutex *m)
{
	return pthread_mutex_trylock(&m->m_mutex);
}

static inline int32_t 
mutex_unlock(mutex *m)
{
	return pthread_mutex_unlock(&m->m_mutex);
}

static inline mutex*
mutex_new()
{
	mutex *m = malloc(sizeof(*m));
	pthread_mutexattr_init(&m->m_attr);
#ifdef _LINUX
	pthread_mutexattr_settype(&m->m_attr,PTHREAD_MUTEX_RECURSIVE_NP);
#elif _FREEBSD
	pthread_mutexattr_settype(&m->m_attr,PTHREAD_MUTEX_RECURSIVE);
#endif
	pthread_mutex_init(&m->m_mutex,&m->m_attr);
	return m;
}

static inline void 
mutex_del(mutex *m)
{
	pthread_mutexattr_destroy(&m->m_attr);
	pthread_mutex_destroy(&m->m_mutex);
	free(m);
}

/*Condition*/
typedef struct
{
	pthread_cond_t cond;
	mutex          *mtx;
}condition;


static inline int32_t 
condition_wait(condition *c)
{
	return pthread_cond_wait(&c->cond,&c->mtx->m_mutex);
}

static inline int32_t 
condition_timedwait(condition *c,int32_t ms)
{
	struct timespec ts;
	uint64_t msec;
    clock_gettime(CLOCK_MONOTONIC, &ts);
	msec = ms%1000;
	ts.tv_nsec += (msec*1000*1000);
	ts.tv_sec  += (ms/1000);
	if(ts.tv_nsec >= 1000*1000*1000){
		ts.tv_sec += 1;
		ts.tv_nsec %= (1000*1000*1000);
	}
    return pthread_cond_timedwait(&c->cond,&c->mtx->m_mutex,&ts);
}

static inline int32_t 
condition_signal(condition *c)
{
	return pthread_cond_signal(&c->cond);
}

static inline int32_t 
condition_broadcast(condition *c)
{
	return pthread_cond_broadcast(&c->cond);
}

static inline condition* 
condition_new(mutex *mtx)
{
	condition *c = malloc(sizeof(*c));
	c->mtx = mtx;
	pthread_cond_init(&c->cond,NULL);
	return c;
}

static inline void 
condition_del(condition *c)
{
	pthread_cond_destroy(&c->cond);
	free(c);
}

/*
typedef struct barrior
{
	condition_t cond;
	mutex_t 	mtx;
	int32_t     wait_count;
}*barrior_t;

static inline void barrior_wait(barrior_t b)
{
	mutex_lock(b->mtx);
	--b->wait_count;
	if(0 == b->wait_count)
	{
		condition_broadcast(b->cond);
	}else
	{
		while(b->wait_count > 0)
		{
			condition_wait(b->cond,b->mtx);
		}
	}
	mutex_unlock(b->mtx);
}

static inline barrior_t barrior_create(int waitcount)
{
	barrior_t b = malloc(sizeof(*b));
	b->wait_count = waitcount;
	b->mtx = mutex_create();
	b->cond = condition_create();
	return b;
}

static inline void barrior_destroy(barrior_t *b)
{
	mutex_destroy(&(*b)->mtx);
	condition_destroy(&(*b)->cond);
	free(*b);
	*b = 0;
}
*/
#endif
