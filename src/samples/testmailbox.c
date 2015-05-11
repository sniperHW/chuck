#include "chuck.h"
#include "thread/thread.h"
#include "thread/thdmailbox.h"
#include "util/time.h"

volatile int32_t count = 0;

void *Producer1(void *arg)
{
	printf("Producer1\n");
	tmailbox Consumer = query_mailbox("Consumer");
	while(!is_vaild_refhandle(&Consumer)){
		Consumer = query_mailbox("Consumer");
		FENCE();
		SLEEPMS(1);
	}	
	int32_t i;
	while(1){
		for(i = 0; i < 50000; ++i){
			mail *m = calloc(1,sizeof(mail));
			if(0 != send_mail(Consumer,m))
			{
				free(m);
				return NULL;
			}
		}
		SLEEPMS(0);
	}
    return NULL;
}

void *Producer2(void *arg)
{
	printf("Producer2\n");
	tmailbox Consumer = query_mailbox("Consumer");
	while(!is_vaild_refhandle(&Consumer)){
		Consumer = query_mailbox("Consumer");
		FENCE();
		SLEEPMS(1);
	}	
	int32_t i;
	while(1){
		for(i = 0; i < 50000; ++i){
			mail *m = calloc(1,sizeof(mail));
			if(0 != send_mail(Consumer,m))
			{
				free(m);
				return NULL;
			}
		}
		SLEEPMS(0);
	}
    return NULL;
}

engine *econsumer;

static void on_mail(mail *mail_){
	static uint64_t tick = 0;
	if(tick == 0){
		tick = systick64();
	}
	++count;
	if(count == 1000000){
        uint64_t now = systick64();
        uint64_t elapse = (uint64_t)(now-tick);
		printf("count:%ld\n",(uint64_t)(count*1000/elapse));			
		count = 0;
		tick = systick64();
		//engine_stop(econsumer);
	}
}

void *Consumer(void *arg)
{
	printf("Consumer\n");
	econsumer = engine_new();
	mailbox_setup(econsumer,"Consumer",on_mail);
	SLEEPMS(5000);
	engine_run(econsumer);
    return NULL;
}


int main(){
	thread *t1 = thread_new(JOINABLE,Producer1,NULL);
	thread *t2 = thread_new(JOINABLE,Producer2,NULL);
	thread *t3 = thread_new(JOINABLE,Consumer,NULL);

	thread_join(t1);
	thread_join(t2);
	thread_join(t3);


	return 0;
}
