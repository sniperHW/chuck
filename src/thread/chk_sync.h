#ifndef  _CHK_SYNC_H
#define  _CHK_SYNC_H
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <errno.h>
#include "util/chk_time.h"

typedef struct chk_mutex     chk_mutex;

typedef struct chk_condition chk_condition;

/*Mutex*/
struct chk_mutex {
	pthread_mutex_t     m_mutex;
	pthread_mutexattr_t m_attr;
};

/*Condition*/
struct chk_condition {
	pthread_cond_t cond;
	chk_mutex     *mtx;
};

static inline void chk_mutex_init(chk_mutex *m) {
	pthread_mutex_init(&m->m_mutex,NULL);
}

static inline void chk_mutex_uninit(chk_mutex *m) {
	pthread_mutex_destroy(&m->m_mutex);
}

static inline chk_mutex *chk_mutex_new() {
	chk_mutex *m = malloc(sizeof(*m));
	if(!m) return NULL;
	chk_mutex_init(m);
	return m;
}

static inline void chk_mutex_del(chk_mutex *m) {
	chk_mutex_uninit(m);
	free(m);
}

static inline int32_t chk_mutex_lock(chk_mutex *m) {
	return pthread_mutex_lock(&m->m_mutex);
}

static inline int32_t chk_mutex_trylock(chk_mutex *m) {
	return pthread_mutex_trylock(&m->m_mutex);
}

static inline int32_t chk_mutex_unlock(chk_mutex *m) {
	return pthread_mutex_unlock(&m->m_mutex);
}

static inline void chk_condition_init(chk_condition *c,chk_mutex *mtx) {
	c->mtx = mtx;
	pthread_cond_init(&c->cond,NULL);
}

static inline void chk_condition_uninit(chk_condition *c) {
	pthread_cond_destroy(&c->cond);
}

static inline chk_condition *chk_condition_new(chk_mutex *mtx) {
	chk_condition *c = malloc(sizeof(*c));
	if(!c) return NULL;
	chk_condition_init(c,mtx);
	return c;
}

static inline void chk_condition_del(chk_condition *c) {
	chk_condition_uninit(c);
	free(c);
}


static inline int32_t chk_condition_wait(chk_condition *c) {
	return pthread_cond_wait(&c->cond,&c->mtx->m_mutex);
}

static inline int32_t chk_condition_timedwait(chk_condition *c,int32_t ms) {
	struct timespec ts;
	uint64_t msec;
    clock_gettime(CLOCK_REALTIME, &ts);
	msec = ms%1000;
	ts.tv_nsec += (msec*1000*1000);
	ts.tv_sec  += (ms/1000);
	if(ts.tv_nsec >= 1000*1000*1000) {
		ts.tv_sec += 1;
		ts.tv_nsec %= (1000*1000*1000);
	}
    return pthread_cond_timedwait(&c->cond,&c->mtx->m_mutex,&ts);
}

static inline int32_t chk_condition_signal(chk_condition *c) {
	return pthread_cond_signal(&c->cond);
}

static inline int32_t chk_condition_broadcast(chk_condition *c) {
	return pthread_cond_broadcast(&c->cond);
}

#endif
