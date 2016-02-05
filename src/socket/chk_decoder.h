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
	 * 解包器清理函数(如果解包器销毁时有特殊清理动作必须提供此函数)
	 * @param d 解包器
	 */	
	void (*dctor)(chk_decoder *d);
};

#endif