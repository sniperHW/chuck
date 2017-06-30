#ifndef _CHK_REDIS_CLIENT_H
#define _CHK_REDIS_CLIENT_H

#include <stdint.h>
#include "event/chk_event_loop.h"
#include "socket/chk_socket_helper.h"
#include "lua/chk_lua.h"
#include "chk_ud.h"  


#define REDIS_REPLY_STRING  1
#define REDIS_REPLY_ARRAY   2
#define REDIS_REPLY_INTEGER 3
#define REDIS_REPLY_NIL     4
#define REDIS_REPLY_STATUS  5
#define REDIS_REPLY_ERROR   6


#define REDIS_OK     0
#define REDIS_RETRY -1
#define REDIS_ERR   -2  


typedef struct redisReply redisReply;

typedef struct chk_redisclient chk_redisclient;

//连接被关闭后回调
typedef void (*chk_redis_disconnect_cb)(chk_redisclient*,chk_ud ud,int32_t err);
//连接回调
typedef void (*chk_redis_connect_cb)(chk_redisclient*,chk_ud ud,int32_t err);
//请求回调
typedef void (*chk_redis_reply_cb)(chk_redisclient*,redisReply*,chk_ud ud);

struct redisReply {
    int32_t      type;    /* REDIS_REPLY_* */
    int64_t      integer; /* The integer when type is REDIS_REPLY_INTEGER */
    int32_t      len;     /* Length of string */
    char        *str;     /* Used for both REDIS_REPLY_ERROR and REDIS_REPLY_STRING */
    size_t       elements;/* number of elements, for REDIS_REPLY_ARRAY */
    redisReply **element; /* elements vector for REDIS_REPLY_ARRAY */
};

/**
 * 连接redis服务器
 * @param e 事件处理循环
 * @param addr 服务器地址结构
 * @param cntcb 连接回调
 * @param ud 传递给连接回调的用户数据
 * @param dcntcb 连接关闭回调
 */

int32_t chk_redis_connect(chk_event_loop *e,chk_sockaddr *addr,chk_redis_connect_cb cntcb,chk_ud ud);

/**
 * 设置redis连接断开处理函数    
 * @param c redis连接
 * @param cb 连接断开后的回调函数
 * @param ud 回调参数
 */

void    chk_redis_set_disconnect_cb(chk_redisclient *c,chk_redis_disconnect_cb cb,chk_ud ud);

/**
 * 关闭redis连接,当连接结构销毁时回调dcntcb
 * @param c redis连接
 */

void chk_redis_close(chk_redisclient *c);

/**
 * 执行redis命令(二进制安全)
 * @param c redis连接
 * @param cb 请求执行回调(成功/出错)
 * @param ud 传递给cb的用户数据
 */

int32_t chk_redis_execute(chk_redisclient*,chk_redis_reply_cb cb,chk_ud ud,const char *fmt,...);


/**
 * 执行redis命令(二进制安全)
 * @param c redis连接
 * @param cmd redis命令
 * @param cb 请求执行回调(成功/出错)
 * @param ud 传递给cb的用户数据
 * @param L  lua状态
 * @param start_idx 参数在lua栈中的起始位置
 * @param param_size 参数的数量
 */
int32_t chk_redis_execute_lua(chk_redisclient*,const char *cmd,chk_redis_reply_cb cb,chk_ud ud,lua_State *L,int32_t start_idx,int32_t param_size);



#endif    
