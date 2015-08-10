#ifndef _CHK_BYTEBUFFER_H
#define _CHK_BYTEBUFFER_H

/*
* 固定长度带引用计数的二进制chunk,用于组成chunk链表
* chunk只带了一个cap,不记录有效数据的字节数,有效数据数量需由使用模块记录
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "util/chk_atomic.h"
#include "util/chk_util.h"
#include "util/chk_list.h"

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

typedef struct chk_bytechunk  chk_bytechunk;

typedef struct chk_bytebuffer chk_bytebuffer;

extern uint32_t chunkcount;
extern uint32_t buffercount;

struct chk_bytechunk {
	uint32_t        refcount;
	uint32_t        cap;
	chk_bytechunk  *next;
	char            data[0];
};

//bytechunk链表形成的buffer
struct chk_bytebuffer {
    chk_list_entry entry;
    uint32_t       datasize;   //属于本buffer的数据大小
    uint32_t       spos;       //起始数据在head中的下标    
    chk_bytechunk *head;
};

/*
*如果ptr非空,则构造一个至少能容纳len字节的chunk,并把ptr指向的len个字节拷贝到data中
*否则,构造一个至少能容纳len字节的chunk,不对数据初始化
*/
static inline chk_bytechunk *chk_bytechunk_new(void *ptr,uint32_t len) {
	chk_bytechunk *b;
	len           = len < 64 ? 64 : chk_size_of_pow2(len);
	uint32_t size = sizeof(*b) + len;
    b 			  = cast(chk_bytechunk*,malloc(size));
    b->next       = NULL;
	if(b) {
		if(ptr) memcpy(b->data,ptr,len);
		b->cap = len;
		b->refcount = 1;
        ++chunkcount;
	}
	return b;
}

static inline chk_bytechunk *chk_bytechunk_retain(chk_bytechunk *b) {
    if(b){
        assert(b->refcount > 0);
        chk_atomic_increase_fetch(&b->refcount);
    }
	return b;
}

static inline void chk_bytechunk_release(chk_bytechunk *b) {
	if(0 >= chh_atomic_decrease_fetch(&b->refcount)) {
        assert(b->refcount == 0);
        if(b->next) chk_bytechunk_release(b->next);
        --chunkcount;
        free(b);    
    }
}

/*  从b的pos位置开始尝试读取size字节的数据,
*   如不能在b中读取到足够的字节数,会到b->next继续. 
*/
static inline chk_bytechunk *chk_bytechunk_read(chk_bytechunk *b,char *out,
										  uint32_t *pos,uint32_t *size) {
    uint32_t rsize,copy_size;
    rsize = 0;
    while(*size && b) {
    	copy_size = b->cap - *pos;
    	copy_size = copy_size > *size ? *size : copy_size;
    	memcpy(out,b->data + *pos,copy_size);
    	rsize    += copy_size;
    	*size    -= copy_size;
    	*pos     += copy_size;
    	if(*size){
    		out += copy_size;
    		*pos = 0;
    		b    = b->next;
    	}
    }
    *size = rsize;
    return b;
}

static inline chk_bytechunk *chk_bytechunk_write(chk_bytechunk *b,char *in,
										  uint32_t *pos,uint32_t *size) {
    uint32_t wsize,copy_size;
    wsize = 0;
    while(*size && b) {
    	copy_size = b->cap - *pos;
    	copy_size = copy_size > *size ? *size : copy_size;
    	memcpy(b->data + *pos,in,copy_size);
    	wsize    += copy_size;
    	*size    -= copy_size;
    	*pos     += copy_size;
    	if(*size){
    		in  += copy_size;
    		*pos = 0;
    		b    = b->next;
    	}
    }
    *size = wsize;
    return b;
}

static inline chk_bytebuffer *chk_bytebuffer_new(chk_bytechunk *b,uint32_t spos,uint32_t datasize) {
    chk_bytebuffer *buffer = calloc(1,sizeof(*buffer));
    if(b) buffer->head     = chk_bytechunk_retain(b);
    else  buffer->head     = chk_bytechunk_new(NULL,datasize);
    buffer->spos           = spos;
    buffer->datasize       = datasize;
    ++buffercount;
    return buffer;
}

static inline chk_bytebuffer *chk_bytebuffer_clone(chk_bytebuffer *o) {
    chk_bytebuffer *buffer = NULL;
    if(o) {
        ++buffercount;
        buffer = calloc(1,sizeof(*buffer));
        buffer->head     = chk_bytechunk_retain(o->head);
        buffer->spos     = o->spos;
        buffer->datasize = o->datasize;        
    }
    return buffer;
}

static inline void chk_bytebuffer_del(chk_bytebuffer *b) {
    if(b->head) chk_bytechunk_release(b->head);
    --buffercount;
    free(b);
}

#endif