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
#include "util/chk_obj_pool.h"

#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

typedef struct chk_bytechunk  chk_bytechunk;

typedef struct chk_bytebuffer chk_bytebuffer;

struct chk_bytechunk {
	uint32_t        refcount;
	uint32_t        cap;
	chk_bytechunk  *next;
	char            data[0];
};


enum {
    CREATE_BY_NEW      = 1,       
    NEED_COPY_ON_WRITE = 1 << 1,
    READ_ONLY          = 1 << 2,   
};

enum {
    READ_ONLY_BUFFER = -1,
    INVAILD_BUFFER   = -2,
};

//bytechunk链表形成的buffer
struct chk_bytebuffer {
    chk_list_entry entry;
    uint32_t       datasize;     //属于本buffer的数据大小
    uint32_t       spos;         //起始数据在head中的下标
    uint32_t       append_pos;     
    chk_bytechunk *head;
    chk_bytechunk *tail;
    uint8_t        flags;
};

#ifdef USE_BUFFER_POOL

DECLARE_OBJPOOL(chk_bytebuffer)

extern chk_bytebuffer_pool *bytebuffer_pool;

extern int32_t lock_bytebuffer_pool;

#define LOCK(L) while (__sync_lock_test_and_set(&lock_bytebuffer_pool,1)) {}
#define UNLOCK(L) __sync_lock_release(&lock_bytebuffer_pool);

#ifndef INIT_BYTEBUFFER_POOL_SIZE
#define INIT_BYTEBUFFER_POOL_SIZE 4096
#endif

#define NEW_BYTEBUFFER()         ({                                             \
    chk_bytebuffer *bytebuffer;                                                 \
    LOCK();                                                                     \
    if(NULL == bytebuffer_pool) {                                               \
        bytebuffer_pool = chk_bytebuffer_pool_new(INIT_BYTEBUFFER_POOL_SIZE);   \
    }                                                                           \
    bytebuffer = chk_bytebuffer_new_obj(bytebuffer_pool);                       \
    UNLOCK();                                                                   \
    bytebuffer;                                                                 \
})

#define FREE_BYTEBUFFER(BYTEBUFFER) do{                                         \
    LOCK();                                                                     \
    chk_bytebuffer_release_obj(bytebuffer_pool,BYTEBUFFER);                     \
    UNLOCK();                                                                   \
}while(0)

#else

#define NEW_BYTEBUFFER() ((chk_bytebuffer*)calloc(1,sizeof(chk_bytebuffer)))

#define FREE_BYTEBUFFER(BYTEBUFFER) free(BYTEBUFFER) 

#endif


/*
*如果ptr非空,则构造一个至少能容纳len字节的chunk,并把ptr指向的len个字节拷贝到data中
*否则,构造一个至少能容纳len字节的chunk,不对数据初始化
*/
static inline chk_bytechunk *chk_bytechunk_new(void *ptr,uint32_t len) {

    static const uint32_t min_buf_len = 64;

	chk_bytechunk *b;
	len           = len < min_buf_len ? min_buf_len : chk_size_of_pow2(len);
	uint32_t size = sizeof(*b) + len;
    b 			  = cast(chk_bytechunk*,malloc(size));
    b->next       = NULL;
	if(b) {
		if(ptr) memcpy(b->data,ptr,len);
		b->cap = len;
		b->refcount = 1;
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

static inline void chk_bytebuffer_init(chk_bytebuffer *b,chk_bytechunk *o,uint32_t spos,uint32_t datasize,uint8_t flags) {
    assert(b);
    b->flags  = flags;
    if(o){
        b->head = chk_bytechunk_retain(o);
        b->tail = b->head;
        b->datasize = datasize;
        b->spos   = spos;
    } else {
        b->tail   = b->head = chk_bytechunk_new(NULL,datasize);
        b->spos   = b->datasize = b->append_pos = 0;
    }    
}

static inline void chk_bytebuffer_finalize(chk_bytebuffer *b) {
    if(b->head) chk_bytechunk_release(b->head);
    b->head = NULL;
}

static inline chk_bytebuffer *chk_bytebuffer_new(uint32_t initcap) {
    chk_bytebuffer *buffer = NEW_BYTEBUFFER();
    chk_bytebuffer_init(buffer,NULL,0,initcap,CREATE_BY_NEW);
    return buffer;
}

static inline chk_bytebuffer *chk_bytebuffer_new_bychunk(chk_bytechunk *b,uint32_t spos,uint32_t datasize) {
    chk_bytebuffer *buffer = NEW_BYTEBUFFER();
    chk_bytebuffer_init(buffer,b,spos,datasize,CREATE_BY_NEW);
    return buffer;
}

static inline chk_bytebuffer *chk_bytebuffer_new_bychunk_readonly(chk_bytechunk *b,uint32_t spos,uint32_t datasize) {
    chk_bytebuffer *buffer = NEW_BYTEBUFFER();
    chk_bytebuffer_init(buffer,b,spos,datasize,(CREATE_BY_NEW | READ_ONLY));
    return buffer;
}

static inline chk_bytebuffer *chk_bytebuffer_share(chk_bytebuffer *b,chk_bytebuffer *o) {
    assert(o);
    assert(b);
    chk_bytebuffer_finalize(b);
    chk_bytebuffer_init(b,o->head,o->spos,o->datasize,NEED_COPY_ON_WRITE);      
    return b;
}

static inline chk_bytebuffer *chk_bytebuffer_clone(chk_bytebuffer *o) {
    assert(o);
    chk_bytebuffer *b = NEW_BYTEBUFFER(); 
    chk_bytebuffer_init(b,o->head,o->spos,o->datasize,CREATE_BY_NEW|NEED_COPY_ON_WRITE);
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

static inline void chk_bytebuffer_del(chk_bytebuffer *b) {
    chk_bytebuffer_finalize(b);
    if(b->flags & CREATE_BY_NEW) FREE_BYTEBUFFER(b);
}


/*
* 向buffer尾部添加数据,如空间不足会扩张
*/

static inline int32_t chk_bytebuffer_append(chk_bytebuffer *b,uint8_t *v,uint32_t size) {
    uint32_t copysize;

    if(b->flags & READ_ONLY) {
        return READ_ONLY_BUFFER;
    }

    if(b->flags & NEED_COPY_ON_WRITE) {
        //写时拷贝
        chk_bytebuffer_do_copy(b,b->head,b->spos,b->datasize);
    }

    if(!b->tail) {
        return INVAILD_BUFFER;
    }

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
    return chk_bytebuffer_append(b,&v,sizeof(v));
}


static inline int32_t chk_bytebuffer_append_word(chk_bytebuffer *b,uint16_t v) {
    return chk_bytebuffer_append(b,(uint8_t*)&v,sizeof(v));
}


static inline int32_t chk_bytebuffer_append_dword(chk_bytebuffer *b,uint32_t v) {
    return chk_bytebuffer_append(b,(uint8_t*)&v,sizeof(v));
}


static inline int32_t chk_bytebuffer_append_qword(chk_bytebuffer *b,uint64_t v) {
    return chk_bytebuffer_append(b,(uint8_t*)&v,sizeof(v));
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