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


enum {
    CREATE_BY_NEW      = 1,
    NEED_COPY_ON_WRITE = 1 << 1,
};

//bytechunk链表形成的buffer
struct chk_bytebuffer {
    chk_list_entry entry;
    uint32_t       datasize;   //属于本buffer的数据大小
    uint32_t       spos;       //起始数据在head中的下标
    uint32_t       append_pos;     
    chk_bytechunk *head;
    chk_bytechunk *tail;
    uint8_t        flags;
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

static inline void chk_bytebuffer_init(chk_bytebuffer *b,chk_bytechunk *o,uint32_t spos,uint32_t datasize) {
    assert(b);
    if(o){
        b->head = chk_bytechunk_retain(o);
        b->tail = NULL;
        b->datasize = datasize;
        b->spos  = spos;
        b->flags |= NEED_COPY_ON_WRITE;
    } else {
        b->tail = b->head = chk_bytechunk_new(NULL,datasize);
        b->spos = b->datasize = b->append_pos = 0;
    }
    ++buffercount;    
}

static inline void chk_bytebuffer_finalize(chk_bytebuffer *b) {
    if(b->head) chk_bytechunk_release(b->head);
    b->head = NULL;
    --buffercount;
}

static inline chk_bytebuffer *chk_bytebuffer_new(chk_bytechunk *b,uint32_t spos,uint32_t datasize) {
    chk_bytebuffer *buffer = calloc(1,sizeof(*buffer));
    chk_bytebuffer_init(buffer,b,spos,datasize);
    buffer->flags |= CREATE_BY_NEW;
    return buffer;
}

/*
* 浅拷贝,b和o共享数据
*/

static inline chk_bytebuffer *chk_bytebuffer_clone(chk_bytebuffer *b,chk_bytebuffer *o) {
    assert(o);
    if(!b){
        b = calloc(1,sizeof(*b));
        chk_bytebuffer_init(b,o->head,o->spos,o->datasize);
        b->flags |= CREATE_BY_NEW;
    }else 
        chk_bytebuffer_init(b,o->head,o->spos,o->datasize);         
    return b;
}

static inline chk_bytebuffer *chk_bytebuffer_do_copy(chk_bytebuffer *b,chk_bytechunk *c,uint32_t spos,uint32_t size) {
    assert(b && c);
    chk_bytechunk *curr;
    uint32_t pos = 0;
    uint32_t dataremain,copysize;
    b->datasize = size;
    b->spos = 0;
    b->head = curr = chk_bytechunk_new(NULL,b->datasize);
    dataremain = size;
    while(c) {
        copysize = c->cap;
        copysize = copysize > dataremain ? dataremain:copysize;
        memcpy(&curr->data[pos],c->data + spos,copysize);
        dataremain -= copysize;
        pos += copysize;
        spos = 0;
        c = c->next;
    }
    b->flags ^= NEED_COPY_ON_WRITE;
    b->tail = b->head;
    b->append_pos = b->datasize;
    return b;
}

/*
* 深拷贝,b和o数据独立
*/
static inline chk_bytebuffer *chk_bytebuffer_deep_clone(chk_bytebuffer *b,chk_bytebuffer *o) {
    assert(o);
    uint8_t  create_by_new = 0;
    chk_bytechunk *c1;
    if(!b){
        b = calloc(1,sizeof(*b));
        create_by_new = 1;
    }
    c1 = b->head;
    b->head = NULL;
    chk_bytebuffer_do_copy(b,o->head,o->spos,o->datasize);
    if(c1) chk_bytechunk_release(c1);
    if(create_by_new) b->flags |= CREATE_BY_NEW;
    return b;
}

static inline void chk_bytebuffer_del(chk_bytebuffer *b) {
    chk_bytebuffer_finalize(b);
    if(b->flags & CREATE_BY_NEW) free(b);
}


/*
* 向buffer尾部添加数据,如空间不足会扩张
*/

static inline int32_t chk_bytebuffer_append(chk_bytebuffer *b,uint8_t *v,uint32_t size) {
    uint32_t copysize;
    if(b->flags & NEED_COPY_ON_WRITE) {
        //写时拷贝
        chk_bytebuffer_do_copy(b,b->head,b->spos,b->datasize);
    }
    if(!b->tail) return -1;
    b->datasize += size;
    while(size) {
        if(b->append_pos >= b->tail->cap) {
            //空间不足,扩展
            b->tail->next = chk_bytechunk_new(NULL,size);
            b->tail = b->tail->next;
            b->append_pos = 0; 
        }
        copysize = b->tail->cap - b->append_pos;
        if(copysize > size) copysize = size;
        memcpy(b->tail->data + b->append_pos,v,copysize);
        b->append_pos += copysize;
        v += copysize;
        size -= copysize;
    }
    return 0;
}


static inline int32_t chk_bytebuffer_append_byte(chk_bytebuffer *b,uint8_t v) {
    return chk_bytebuffer_append(b,&v,1);
}


static inline int32_t chk_bytebuffer_append_word(chk_bytebuffer *b,uint16_t v) {
    return chk_bytebuffer_append(b,(uint8_t*)&v,2);
}


static inline int32_t chk_bytebuffer_append_dword(chk_bytebuffer *b,uint32_t v) {
    return chk_bytebuffer_append(b,(uint8_t*)&v,4);
}


static inline int32_t chk_bytebuffer_append_qword(chk_bytebuffer *b,uint64_t v) {
    return chk_bytebuffer_append(b,(uint8_t*)&v,8);
}

static inline uint32_t chk_bytebuffer_read(chk_bytebuffer *b,char *out,uint32_t out_len) {
    assert(b && out);
    uint32_t remain,copysize,pos,size;
    chk_bytechunk *c = b->head;
    if(!c) return 0;
    size = remain = out_len > b->datasize ? b->datasize : out_len;
    pos = b->spos;
    while(remain) {
        copysize = c->cap - pos;
        if(copysize > remain) copysize = remain;
        memcpy(out,c->data + pos,copysize);
        remain -= copysize;
        pos += copysize;
        out += copysize;
        if(remain) {
            c = c->next;
            pos = 0;
        }
    }
    return size;
}


#endif