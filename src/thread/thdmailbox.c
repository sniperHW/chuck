#include "thdmailbox.h"
#include "thread/spinlock.h"
#include "engine/engine.h"
#include "util/time.h"

#define MAX_THREADS   512
#define MAX_QUENESIZE 4096

extern int32_t pipe2(int pipefd[2], int flags);

typedef struct{
	handle      base;
	refobj      refbase;
	list        global_queue;
	list        private_queue;
	int32_t     notifyfd;	
	void 		(*onmail)(mail *_mail);
	int32_t     lock;
	int16_t     wait;
	int16_t     dead;         
}tmailbox_;

static __thread tmailbox_ *mailbox_ = NULL;

typedef struct{
	char      *name;
	tmailbox_ *mailbox_;
}record;

static int32_t  mailboxs_lock = 0; 
static record   thread_mailboxs[MAX_THREADS] = {};

#define LOCK(q) while (__sync_lock_test_and_set(&q,1)) {}
#define UNLOCK(q) __sync_lock_release(&q);

tmailbox
query_mailbox(const char *name)
{
	uint16_t i;
	tmailbox m = {.identity=0,.ptr=NULL};
	LOCK(mailboxs_lock);
	for(i = 0; i < MAX_THREADS;++i){
		if(thread_mailboxs[i].name && 
		   strcmp(name,thread_mailboxs[i].name) == 0)
		{
			m = get_refhandle(&(thread_mailboxs[i].mailbox_->refbase));
		}
	}
	UNLOCK(mailboxs_lock);
	return m;
}

#define MAX_EVENTS 512

static inline mail* getmail()
{
	if(!list_size(&mailbox_->private_queue)){
		LOCK(mailbox_->lock);
		if(!list_size(&mailbox_->global_queue)){
			uint32_t try_ = 16;
			do{
				UNLOCK(mailbox_->lock);
				SLEEPMS(0);
				--try_;
				LOCK(mailbox_->lock);
			}while(try_ > 0 &&!list_size(&mailbox_->global_queue));
			if(try_ == 0){
				TEMP_FAILURE_RETRY(read(((handle*)mailbox_)->fd,&try_,sizeof(try_)));
				mailbox_->wait = 1;
				UNLOCK(mailbox_->lock);
				return NULL;
			}
		}
		list_pushlist(&mailbox_->private_queue,&mailbox_->global_queue);
		UNLOCK(mailbox_->lock);
	}
	return (mail*)list_pop(&mailbox_->private_queue);
}


static void 
on_events(handle *h,int32_t events){
	mail   *mail_;
	int32_t n = MAX_EVENTS;
	while((mail_ = getmail()) != NULL){
		mailbox_->onmail(mail_);
		if(mail_->dcotr) 
			mail_->dcotr(mail_);
		free(mail_);	
		if(--n == 0) break;
	}
}


static void 
mailbox_dctor(void *ptr){
	mail* mail_;	
	uint16_t i;
	tmailbox_* mailbox = (tmailbox_*)((char*)ptr - sizeof(handle));
	
	LOCK(mailboxs_lock);
	for(i = 0; i < MAX_THREADS;++i){
		if(thread_mailboxs[i].mailbox_ == mailbox){
			free(thread_mailboxs[i].name);
			thread_mailboxs[i].name = NULL;
			thread_mailboxs[i].mailbox_ = NULL;
		}
	}
	UNLOCK(mailboxs_lock);

	while((mail_ = (mail*)list_pop(&mailbox->private_queue)))
		mail_del(mail_);	
	while((mail_ = (mail*)list_pop(&mailbox->global_queue)))
		mail_del(mail_);				
	close(((handle*)mailbox)->fd);
	close(mailbox->notifyfd);	
	free(mailbox);			
}

int32_t 
mailbox_setup(engine *e,const char *name,
			  void (*onmail)(mail *_mail))
{
	uint16_t i;
	if(mailbox_)
		return -EALSETMBOX;
	LOCK(mailboxs_lock);
	for(i = 0; i < MAX_THREADS;++i){
		if(!thread_mailboxs[i].name){
			thread_mailboxs[i].name = calloc(1,strlen(name)+1);
			strcpy(thread_mailboxs[i].name,name);
			break;
		}
	}
	if(i >= MAX_THREADS){
		UNLOCK(mailboxs_lock);
		return -EMAXTHD;
	}

	int32_t tmp[2];
	if(pipe2(tmp,O_NONBLOCK|O_CLOEXEC) != 0){
		UNLOCK(mailboxs_lock);
		return -EPIPECRERR;
	}

	mailbox_  = calloc(1,sizeof(*mailbox_));
	handle *h = (handle*)mailbox_;
	h->fd = tmp[0];
	mailbox_->notifyfd = tmp[1];	
	h->e = e;
	h->on_events = on_events;
	mailbox_->onmail = onmail;
	refobj_init(&mailbox_->refbase,mailbox_dctor);
	if(event_add(e,h,EVENT_READ) != 0){
		UNLOCK(mailboxs_lock);
		close(tmp[0]);
		close(tmp[1]);
		free(mailbox_);
		mailbox_ = NULL;
		return -EENASSERR;	
	}
	mailbox_->wait = 1;
	thread_mailboxs[i].mailbox_ = mailbox_;
	UNLOCK(mailboxs_lock);
	return 0;
}

static inline tmailbox_* 
cast(tmailbox t){
	refobj *tmp = cast2refobj(t);
	if(!tmp) return NULL;
	return (tmailbox_*)((char*)tmp - sizeof(handle));
}


static inline int32_t 
notify(tmailbox_ *mailbox){
	int32_t ret;
	for(;;){
		errno = 0;
		uint64_t _ = 1;
		ret = TEMP_FAILURE_RETRY(write(mailbox->notifyfd,&_,sizeof(_)));
		if(!(ret < 0 && errno == EAGAIN))
			break;
	}
	return ret > 0 ? 0:-1;
}

int32_t 
send_mail(tmailbox t,mail *mail_)
{
	tmailbox_ *target = cast(t);
	if(!target) return -ETMCLOSE;
	if(mailbox_)
		mail_->sender = get_refhandle(&mailbox_->refbase);
	else
		mail_->sender = (refhandle){.identity=0,.ptr=NULL};
	int32_t ret = 0;	
	if(target == mailbox_){
		do{
			if(target->wait){
				LOCK(target->lock);
				if(target->wait && (ret = notify(target)) == 0)
					target->wait = 0;
				UNLOCK(target->lock);
				if(ret != 0) break;
			}
			list_pushback(&target->private_queue,(listnode*)mail_);
		}while(0);	
	}else{		
		LOCK(target->lock);
		int32_t i;
		for(i = 0;
			!target->dead && list_size(&target->global_queue) > MAX_QUENESIZE;
			++i)
		{
				UNLOCK(target->lock);
				if(i > 4000){
					SLEEPMS(1);
					i = 0;
				}else
					SLEEPMS(0);
				LOCK(target->lock);
		}
		if(target->dead){
			ret = -ETMCLOSE;
		}else{
			if(target->wait && (ret = notify(target)) == 0)
				target->wait = 0;
			if(ret == 0)
				list_pushback(&target->global_queue,(listnode*)mail_);
		}
		UNLOCK(target->lock);
	}
	refobj_dec(&target->refbase);//cast会调用refobj_inc,所以这里要调用refobj_dec
	return ret;
}

//call on thread end
void 
clear_thdmailbox()
{
	if(mailbox_){
		mailbox_->dead = 1;
		refobj_dec(&mailbox_->refbase);
	}
}
