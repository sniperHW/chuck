#ifndef _CHK_ACCEPTOR_H
#define _CHK_ACCEPTOR_H

#include "event/chk_event.h"


typedef struct chk_acceptor chk_acceptor;

typedef void (*chk_acceptor_cb)(chk_acceptor*,int32_t fd,chk_sockaddr*,void *ud,int32_t err);

int32_t       chk_acceptor_enable(chk_acceptor*);

int32_t       chk_acceptor_disable(chk_acceptor*);

chk_acceptor *chk_acceptor_new(int32_t fd,void *ud);

void          chk_acceptor_del(chk_acceptor*); 


#endif