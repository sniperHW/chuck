#ifndef _DECODER_H
#define _DECODER_H
/*
* 分包接口
*/

#include "util/chk_bytechunk.h"

typedef struct chk_decoder chk_decoder;

struct chk_decoder {
	/*
	*  接收到数据之后调用update
	*  b:接受到的chunk链表
	*  spos:数据开始下标
	*  size:数据大小
	*/
	void (*update)(chk_decoder*,chk_bytechunk *b,uint32_t spos,uint32_t size);
	
	/*
	*如果解包完成返回一个bytebuffer
	*/
	chk_bytebuffer *(*unpack)(chk_decoder*,int32_t *err);
	void (*dctor)(chk_decoder*);
};

static inline void chk_decoder_del(chk_decoder *d) {
	if(d->dctor) d->dctor(d);
	free(d);
}


#endif