#ifndef _DECODER_H
#define _DECODER_H
/*
* 分包接口
*/

#include "util/chk_bytechunk.h"

typedef struct chk_decoder chk_decoder;

struct chk_decoder {
	
	/**
	 * 接收到网络数据之后更新解包器信息 
	 * @param d 解包器
	 * @param b 接收到的数据块链表头(数据可能被存放在多个chunk中形成链表)
	 * @param spos 接收到的数据在b中的起始下标 
	 * @param size 接收到的数据大小
	 */

	void (*update)(chk_decoder *d,chk_bytechunk *b,uint32_t spos,uint32_t size);
	
	/**
	 * 解包,如果解出返回一个包含数据包的buffer 
	 * @param d 解包器
	 * @param err 解包错误码 
	 */
	chk_bytebuffer *(*unpack)(chk_decoder *d,int32_t *err);

	/**
	 * 解包器释放函数
	 * @param d 解包器
	 */	
	void (*release)(chk_decoder *d);

};


//一个解包器,包头4字节,表示后面数据大小.
typedef struct {
	void (*update)(chk_decoder*,chk_bytechunk *b,uint32_t spos,uint32_t size);
	chk_bytebuffer *(*unpack)(chk_decoder*,int32_t *err);
	void (*release)(chk_decoder*);
	uint32_t       spos;
	uint32_t       size;
	uint32_t       max;
	chk_bytechunk *b;
}packet_decoder;

void packet_decoder_update(chk_decoder *_,chk_bytechunk *b,uint32_t spos,uint32_t size);

chk_bytebuffer *packet_decoder_unpack(chk_decoder *_,int32_t *err);

void packet_decoder_release(chk_decoder *_);

packet_decoder *packet_decoder_new(uint32_t max);



#endif