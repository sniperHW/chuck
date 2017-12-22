#ifndef _CHK_OBJ_POOL_H
#define _CHK_OBJ_POOL_H

/*
* 非线程安全的对象分池
*/

#include "util/chk_util.h"
#include "util/chk_list.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define CHUNK_OBJSIZE 64
#define MAX_CHUNK 0x3FFFFF

#define DECLARE_OBJPOOL(TYPE)																\
																							\
typedef struct {																			\
	int16_t        next;																	\
	int16_t        idx;																		\
	uint32_t       chunkidx;																\
	TYPE           data;																	\
}TYPE##_obj_holder;																			\
																							\
typedef struct {																			\
	chk_list_entry 		list_entry;															\
	int16_t        		head;																\
	TYPE##_obj_holder   data[CHUNK_OBJSIZE];												\
}TYPE##_chunk;																				\
																							\
typedef struct {																			\
	uint32_t   		initnum;																\
	TYPE##_chunk**  chunks;																	\
	uint32_t   		chunkcount;																\
	chk_list   		freechunk;																\
	uint32_t        freecount;																\
	uint32_t        usedcount;																\
}TYPE##_pool;																				\
																							\
																							\
TYPE##_chunk *TYPE##_new_chunk(uint32_t idx){												\
	TYPE##_obj_holder *_obj;																\
	TYPE##_chunk* newchunk = calloc(1,sizeof(*newchunk));									\
	if(!newchunk) return NULL;																\
	uint32_t i;																				\
	for(i = 0; i < CHUNK_OBJSIZE;++i){														\
		_obj = &newchunk->data[i];															\
		_obj->chunkidx = idx;																\
		_obj->next = i + 1;																	\
		_obj->idx = i;																		\
	}																						\
	newchunk->data[CHUNK_OBJSIZE-1].next = -1;												\
	newchunk->head = 0;																		\
	return newchunk;																		\
}																							\
																							\
void* TYPE##_new_obj(TYPE##_pool *pool){													\
	uint32_t chunkcount,i;																	\
	TYPE##_chunk   *freechunk = (TYPE##_chunk*)chk_list_begin(&pool->freechunk);			\
	TYPE##_chunk  **tmp;																	\
	if(chk_unlikely(!freechunk)){															\
		chunkcount = pool->chunkcount+1;													\
		if(chunkcount > MAX_CHUNK) return NULL;												\
		tmp = calloc(chunkcount,sizeof(*tmp));												\
		if(!tmp) return NULL;																\
		for(i = 0; i < pool->chunkcount;++i) {												\
			tmp[i] = pool->chunks[i];														\
		}																					\
		i = pool->chunkcount;																\
		for(; i < chunkcount;++i) {															\
			tmp[i] = TYPE##_new_chunk(i);													\
			if(!tmp[i]) break;																\
			chk_list_pushback(&pool->freechunk,(chk_list_entry*)tmp[i]);					\
			pool->freecount += CHUNK_OBJSIZE;												\
		}																					\
		free(pool->chunks);																	\
		pool->chunks = tmp;																	\
		pool->chunkcount = i;																\
		freechunk = (TYPE##_chunk*)chk_list_begin(&pool->freechunk);						\
	}																						\
	if(!freechunk) return NULL;																\
	TYPE##_obj_holder *_obj = &freechunk->data[freechunk->head];							\
	freechunk->head = _obj->next;															\
	if(chk_unlikely(freechunk->head == -1))													\
		chk_list_pop(&pool->freechunk);														\
	memset(&_obj->data,0,sizeof(TYPE));														\
	--pool->freecount;++pool->usedcount;													\
	return (void*)&_obj->data;																\
}																							\
																							\
																							\
void TYPE##_release_obj(TYPE##_pool *pool,void *ptr){										\
	TYPE##_obj_holder *_obj;																\
	TYPE##_chunk 	  *_chunk; 																\
	_obj = (TYPE##_obj_holder*)((char*)ptr + sizeof(TYPE) - sizeof(*_obj));					\
	_chunk = pool->chunks[_obj->chunkidx];													\
	int32_t index = _obj->idx;																\
	assert(index >=0 && index < CHUNK_OBJSIZE);												\
	_obj->next = _chunk->head;																\
	if(chk_unlikely(-1 == _chunk->head))													\
		chk_list_pushback(&pool->freechunk,(chk_list_entry*)_chunk);						\
	_chunk->head = index;																	\
	++pool->freecount;--pool->usedcount;													\
}																							\
																							\
TYPE##_pool *TYPE##_pool_new(uint32_t initnum){												\
	uint32_t     chunkcount;																\
	uint32_t     i;																			\
	TYPE##_pool *pool;																		\
	if(initnum%CHUNK_OBJSIZE == 0) chunkcount = initnum/CHUNK_OBJSIZE;						\
	else chunkcount = initnum/CHUNK_OBJSIZE;												\
	if(chunkcount > MAX_CHUNK) return NULL;													\
	pool = calloc(1,sizeof(*pool));															\
	if(!pool) return NULL;																	\
	pool->chunks = calloc(chunkcount,sizeof(*pool->chunks));								\
	if(!pool->chunks) {																		\
		free(pool);																			\
		return NULL;																		\
	}																						\
	for(i = 0; i < chunkcount; ++i) {														\
		pool->chunks[i] = TYPE##_new_chunk(i);												\
		if(!pool->chunks[i]) break;															\
		pool->freecount += CHUNK_OBJSIZE;													\
		chk_list_pushback(&pool->freechunk,(chk_list_entry*)pool->chunks[i]);				\
	}																						\
	pool->chunkcount = i;																	\
	return pool;																			\
}																							\
																							\
void TYPE##_destroy_pool(TYPE##_pool *pool){											\
	uint32_t i;																				\
	for(i = 0; i < pool->chunkcount; ++i){													\
		free(pool->chunks[i]);																\
	}																						\
	free(pool->chunks);																		\
	free(pool);																				\
}

#endif