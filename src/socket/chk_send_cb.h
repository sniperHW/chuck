#ifndef _SEND_CB_H
#define _SEND_CB_H

#include "util/chk_bytechunk.h"
#include "util/chk_obj_pool.h"
#include "chk_ud.h"


typedef void (*send_finish_callback)(void *socket,chk_ud ud,int32_t error);

typedef struct{
    chk_list_entry  entry;
    chk_ud          ud;
    chk_bytebuffer *buffer;
    send_finish_callback cb;
}st_send_cb;

DECLARE_OBJPOOL(st_send_cb)


extern st_send_cb_pool *send_cb_pool;

extern int32_t lock_send_cb_pool;

#define SEND_CB_POOL_LOCK(L) while (__sync_lock_test_and_set(&lock_send_cb_pool,1)) {}
#define SEND_CB_POOL_UNLOCK(L) __sync_lock_release(&lock_send_cb_pool);

#ifndef INIT_SEND_CB_POOL_SIZE
#define INIT_SEND_CB_POOL_SIZE 4096
#endif

#define POOL_NEW_SEND_CB()         ({                                 \
    st_send_cb *cb;                                                   \
    SEND_CB_POOL_LOCK();                                              \
    if(NULL == send_cb_pool) {                                        \
        send_cb_pool = st_send_cb_pool_new(INIT_SEND_CB_POOL_SIZE);   \
    }                                                                 \
    cb = st_send_cb_new_obj(send_cb_pool);                       	  \
    SEND_CB_POOL_UNLOCK();                                            \
    cb;                                                               \
})

#define POOL_RELEASE_SEND_CB(CB) do{                             	  \
    SEND_CB_POOL_LOCK();                                              \
    st_send_cb_release_obj(send_cb_pool,CB);                          \
    SEND_CB_POOL_UNLOCK();                                            \
}while(0)


#endif