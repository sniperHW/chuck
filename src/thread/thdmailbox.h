#ifndef _THDMAILBOX_H
#define _THDMAILBOX_H

#include "comm.h"
#include "util/refobj.h"
#include "util/list.h"

typedef refhandle tmailbox;

typedef struct{
	listnode  node;
	tmailbox  sender;
	void (*dcotr)(void*);
}mail;


static inline void
mail_del(void *_)
{
	mail *mail_ = (mail*)_;
	if(mail_->dcotr)
		mail_->dcotr(mail_);
	free(mail_);
}


int32_t 
mailbox_setup(engine*,const char *name,
			  void (*onmail)(mail *_mail));

tmailbox
query_mailbox(const char *name); 

int32_t
send_mail(tmailbox,mail*);

#endif