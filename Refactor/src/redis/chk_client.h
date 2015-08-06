#ifndef _CHK_REDIS_CLIENT_H
#define _CHK_REDIS_CLIENT_H

#include <stdint.h>
#include "event/chk_event_loop.h"  


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
typedef void (*chk_redis_disconnect_cb)(chk_redisclient*,int32_t err);
//连接回调
typedef void (*chk_redis_connect_cb)(chk_redisclient*,int32_t err,void *ud);
//请求回调
typedef void (*chk_redis_reply_cb)(chk_redisclient*,redisReply*,void *ud);

struct redisReply {
    int32_t      type;    /* REDIS_REPLY_* */
    int64_t      integer; /* The integer when type is REDIS_REPLY_INTEGER */
    int32_t      len;     /* Length of string */
    char        *str;     /* Used for both REDIS_REPLY_ERROR and REDIS_REPLY_STRING */
    size_t       elements;/* number of elements, for REDIS_REPLY_ARRAY */
    redisReply **element; /* elements vector for REDIS_REPLY_ARRAY */
};

int32_t chk_redis_connect(chk_event_loop*,chk_sockaddr *addr,
                          chk_redis_connect_cb cntcb,void *ud,
                          chk_redis_disconnect_cb dcntcb);

void    chk_redis_close(chk_redisclient*,int32_t err);

int32_t chk_redis_execute(chk_redisclient*,const char *str,chk_redis_reply_cb,void *ud);

#endif    
