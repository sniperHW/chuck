/*
* 非线程安全的对象分池
*/

#include "obj_allocator.h"
#include "util/list.h"
#include "comm.h"
#include <string.h>
#include <assert.h>

#pragma pack(1)
typedef struct{
	uint32_t   next:10;
	uint32_t   chunkidx:22;
	uint16_t   idx;
#ifdef _DEBUG
	allocator *_allocator;
#endif
	char       data[0];
}obj;
#pragma pack()

typedef struct{
	listnode _lnode;
	uint16_t head;
	char     data[0];
}chunk;

typedef struct {
	allocator base;
	size_t    objsize;
	uint32_t  initnum;
	chunk   **chunks;
	uint32_t  chunkcount;
	int8_t    usesystem;
	list      freechunk;
}obj_allocator;

#define CHUNK_SIZE 1024
#define CHUNK_OBJSIZE (CHUNK_SIZE-1)
#define MAX_CHUNK 0x3FFFFF

static chunk*
new_chunk(uint32_t idx,size_t objsize)
{
	chunk* newchunk = calloc(1,sizeof(*newchunk) + CHUNK_SIZE*objsize);
	uint32_t i = 1;
	char *ptr = newchunk->data + objsize;
	for(; i < CHUNK_SIZE;++i){
		obj *_obj = (obj*)ptr;
		_obj->chunkidx = idx;
		if(i == CHUNK_OBJSIZE)
			_obj->next = 0;
		else
			_obj->next = i + 1;
		_obj->idx = i;
		ptr += objsize;		
	}
	newchunk->head = 1;
	return newchunk;
}

static void* 
_alloc(allocator *_,size_t size)
{
	obj_allocator *a = (obj_allocator*)_;	
	if(unlikely(a->usesystem)) return malloc(size);
	return _->calloc(_,1,1);
}

static void* 
_calloc(allocator *_,size_t num,size_t size)
{
	obj_allocator *a = (obj_allocator*)_;
	if(unlikely(a->usesystem)) return calloc(num,size);
	chunk *freechunk = (chunk*)list_begin(&a->freechunk);
	uint32_t objsize = a->objsize;
	if(unlikely(!freechunk)){
		uint32_t chunkcount = a->chunkcount*2;
		if(chunkcount > MAX_CHUNK) chunkcount = MAX_CHUNK;
		if(chunkcount == a->chunkcount) return NULL;
		chunk **tmp = calloc(chunkcount,sizeof(*tmp));
		uint32_t i = 0;
		for(; i < a->chunkcount;++i){
			tmp[i] = a->chunks[i];
		}
		i = a->chunkcount;
		for(; i < chunkcount;++i)	{
			tmp[i] = new_chunk(i,objsize);
			list_pushback(&a->freechunk,(listnode*)tmp[i]);			
		}
		free(a->chunks);
		a->chunks = tmp;
		a->chunkcount = chunkcount;
		freechunk = (chunk*)list_begin(&a->freechunk);
	}
	obj *_obj = (obj*)(((char*)freechunk->data) + freechunk->head * objsize);
	if(unlikely(!(freechunk->head = _obj->next)))
		list_pop(&a->freechunk);	
	memset(_obj->data,0,objsize-sizeof(*_obj));
#ifdef _DEBUG
	_obj->_allocator = (allocator*)a;
#endif	
	return (void*)_obj->data;
}


static void   
_free(allocator *_,void *ptr)
{
	obj_allocator *a = (obj_allocator*)_;	
	if(unlikely(a->usesystem)) return free(ptr);
	obj *_obj = (obj*)((char*)ptr - sizeof(obj));
#ifdef _DEBUG
	assert(_obj->_allocator == (allocator*)a);
#endif
	chunk *_chunk = a->chunks[_obj->chunkidx];
	uint32_t index = _obj->idx;
	assert(index >=1 && index <= CHUNK_OBJSIZE);
	_obj->next = _chunk->head;
	if(unlikely(!_chunk->head))
		list_pushback(&a->freechunk,(listnode*)_chunk);
	_chunk->head = index;	
}

allocator*
objpool_new(size_t objsize,uint32_t initnum)
{
	uint32_t chunkcount;
	if(initnum%CHUNK_OBJSIZE == 0)
		chunkcount = initnum/CHUNK_OBJSIZE;
	else
		chunkcount = initnum/CHUNK_OBJSIZE + 1;
	if(chunkcount > MAX_CHUNK) return NULL;		
	objsize = align_size(sizeof(obj) + objsize,8);
	obj_allocator *pool = calloc(1,sizeof(*pool));
	do{
		((allocator*)pool)->alloc = _alloc;
		((allocator*)pool)->calloc = _calloc;
		((allocator*)pool)->free = _free;
		if(objsize > 1024*1024){
			pool->usesystem = 1;
			break;
		}
		pool->objsize = objsize;
		pool->chunkcount = chunkcount;
		pool->chunks = calloc(chunkcount,sizeof(*pool->chunks));
		uint32_t i = 0;
		for(; i < chunkcount; ++i){
			pool->chunks[i] = new_chunk(i,objsize);
			list_pushback(&pool->freechunk,(listnode*)pool->chunks[i]);
		}
	}while(0);
	return (allocator*)pool;
}

void 
objpool_destroy(allocator *_)
{
	obj_allocator *a = (obj_allocator*)_;		
	uint32_t i = 0;
	for(; i < a->chunkcount; ++i){
		free(a->chunks[i]);
	}
	free(a->chunks);
	free(a);
}