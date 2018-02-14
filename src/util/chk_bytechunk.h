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
#include "util/chk_error.h"
#include <stdio.h>
#ifndef  cast
# define  cast(T,P) ((T)(P))
#endif

enum {
    CREATE_BY_NEW      = 1,       
    NEED_COPY_ON_WRITE = 1 << 1,
    READ_ONLY          = 1 << 2,   
};

enum {
    READ_ONLY_BUFFER = -1,
    INVAILD_BUFFER   = -2,
};

typedef struct chk_bytechunk  chk_bytechunk;

typedef struct chk_bytebuffer chk_bytebuffer;

struct chk_bytechunk {
	uint32_t        refcount;
	uint32_t        cap;
	chk_bytechunk  *next;
	char            data[0];
};


//bytechunk链表形成的buffer
struct chk_bytebuffer {
    chk_list_entry entry;
    uint32_t       internal;     //内部使用的字段
    uint32_t       datasize;     //属于本buffer的数据大小
    uint32_t       spos;         //起始数据在head中的下标
    uint32_t       append_pos;     
    chk_bytechunk *head;
    chk_bytechunk *tail;
    uint8_t        flags;
};

//chk_bytebuffer* new_bytebuffer();
//void free_bytebuffer(chk_bytebuffer*);

/*
*如果ptr非空,则构造一个至少能容纳len字节的chunk,并把ptr指向的len个字节拷贝到data中
*否则,构造一个至少能容纳len字节的chunk,不对数据初始化
*/
chk_bytechunk *chk_bytechunk_new(void *ptr,uint32_t len);

chk_bytechunk *chk_bytechunk_retain(chk_bytechunk *b);

void chk_bytechunk_release(chk_bytechunk *b);

/*  从b的pos位置开始尝试读取size字节的数据,
*   如不能在b中读取到足够的字节数,会到b->next继续. 
*/
chk_bytechunk *chk_bytechunk_read(chk_bytechunk *b,char *out,uint32_t *pos,uint32_t *size);

chk_bytechunk *chk_bytechunk_write(chk_bytechunk *b,char *in,uint32_t *pos,uint32_t *size);

int32_t chk_bytebuffer_init(chk_bytebuffer *b,chk_bytechunk *o,uint32_t spos,uint32_t datasize,uint8_t flags);

void chk_bytebuffer_finalize(chk_bytebuffer *b);

chk_bytebuffer *chk_bytebuffer_new(uint32_t initcap);

chk_bytebuffer *chk_bytebuffer_new_bychunk(chk_bytechunk *b,uint32_t spos,uint32_t datasize);

chk_bytebuffer *chk_bytebuffer_new_bychunk_readonly(chk_bytechunk *b,uint32_t spos,uint32_t datasize);

chk_bytebuffer *chk_bytebuffer_share(chk_bytebuffer *b,chk_bytebuffer *o);

chk_bytebuffer *chk_bytebuffer_clone(chk_bytebuffer *o);

chk_bytebuffer *chk_bytebuffer_do_copy(chk_bytebuffer *b,chk_bytechunk *c,uint32_t spos,uint32_t size);

void chk_bytebuffer_del(chk_bytebuffer *b);


/*
* 向buffer尾部添加数据,如空间不足会扩张
*/

int32_t chk_bytebuffer_append(chk_bytebuffer *b,uint8_t *v,uint32_t size);

int32_t chk_bytebuffer_append_byte(chk_bytebuffer *b,uint8_t v);

int32_t chk_bytebuffer_append_word(chk_bytebuffer *b,uint16_t v);

int32_t chk_bytebuffer_append_dword(chk_bytebuffer *b,uint32_t v);

int32_t chk_bytebuffer_append_qword(chk_bytebuffer *b,uint64_t v);

//从bytebuffer,pos开始的位置读取size数据到out,并将读取的数据从bytebuffer中丢弃
uint32_t chk_bytebuffer_read(chk_bytebuffer *b,uint32_t offset, char *out,uint32_t size);

//从bytebuffer头部读取size数据到out,并将读取的数据从bytebuffer中丢弃
uint32_t chk_bytebuffer_read_drain(chk_bytebuffer *b,char *out,uint32_t size);

int32_t chk_bytebuffer_rewrite(chk_bytebuffer *b,uint32_t offset,uint8_t *v,uint32_t size);

#endif