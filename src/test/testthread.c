#include <stdio.h>
#include "thread/thread.h"

void *routine1(void *_){
	printf("i'm first,arg:%d\n",(int)_);
	return (void*)2; 
}


void *routine2(void *_){
	printf("i'm routine2\n");
	return (void*)3;
}

int main(){
	thread *t1 = thread_new(JOINABLE|WAITRUN,routine1,(void*)1);
	printf("i'm second\n");
	printf("return value:%d of t1\n",(int)thread_del(t1));

	thread *t2 = thread_new(JOINABLE,routine2,NULL);
	printf("i'm main thread\n");
	printf("return value:%d of t2\n",(int)thread_del(t2));

	return 0;
}