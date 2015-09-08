#include <stdio.h>
#include "util/time.h"
#include "util/timewheel.h"

int count = 0;

int32_t callback1(uint32_t _0,uint64_t _1,void *_2){
	printf("%s,%lld\n",(const char *)_2,systick64() - _1);
	++count;
	return 0;
}

uint64_t i;

int32_t callback2(uint32_t _0,uint64_t _1,void *_2){
	printf("%s,%lld,%d\n",(const char *)_2,i - _1,count);
	return 0;
}


void test1(){
	wheelmgr *m = wheelmgr_new();
	i = systick64();
	wheelmgr_register(m,50,callback1,"ms",i);
	wheelmgr_tick(m,i + 1);	
	wheelmgr_tick(m,i + 1000);
	printf("%d\n",count);	
}

void test2(){
	count = 0;
	wheelmgr *m = wheelmgr_new();
	i = systick64();	
	wheelmgr_register(m,1000*3600*23,callback2,"hour",i);
	wheelmgr_register(m,MAX_TIMEOUT,callback2,"hour",i);
	wheelmgr_register(m,1000,callback2,"sec",i);
	wheelmgr_register(m,1001,callback2,"sec",i);
	wheelmgr_register(m,60*1000,callback2,"min",i);
	wheelmgr_register(m,2*60*1000,callback2,"min",i);
	wheelmgr_register(m,5*60*1000 + 102,callback2,"min",i);
	while(1){
		++count;
		wheelmgr_tick(m,++i);//systick64());
		//SLEEPMS(0);
	}	
}


int main(){
	test1();
	printf("test1 finish,please enter any key\n");
	getchar();
	test2();
	return 0;
}