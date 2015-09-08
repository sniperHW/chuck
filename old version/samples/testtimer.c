#include <stdio.h>
#include <assert.h>
#include "util/timewheel.h"
#include "util/time.h"

uint64_t tick = 0;

uint64_t count[9] = {0};

int32_t cb(uint32_t event,uint64_t now,void*ud) {
	timer *t = *((timer**)ud);
	assert(timer_expire(t) == tick);
	switch(timer_timeout(t)){
		case 1:count[0]++;break;
		case 101:count[1]++;break;
		case 999:count[2]++;break;
		case 1001:count[3]++;break;
		case 1000:count[4]++;break;
		case 60000:count[5]++;break;
		case 60001:count[6]++;break;
		case 65536:count[7]++;break;
		case MAX_TIMEOUT:count[8]++;break;												
	}	
	return 0;
}



int main() {

	uint32_t c = 0;
	timer *t1,*t2,*t3,*t4,*t5,*t6,*t7,*t8,*t9;
	wheelmgr *m = wheelmgr_new();
	t1 = wheelmgr_register(m,1,cb,&t1,tick);
	t2 = wheelmgr_register(m,101,cb,&t2,tick);
	t3 = wheelmgr_register(m,999,cb,&t3,tick);
	t4 = wheelmgr_register(m,1001,cb,&t4,tick);
	t5 = wheelmgr_register(m,1000,cb,&t5,tick);
	t6 = wheelmgr_register(m,60000,cb,&t6,tick);
	t7 = wheelmgr_register(m,60001,cb,&t7,tick);
	t8 = wheelmgr_register(m,65536,cb,&t8,tick);
	t9 = wheelmgr_register(m,MAX_TIMEOUT,cb,&t9,tick);							
	while(1){
		wheelmgr_tick(m,tick);
		++tick;		
		if(tick % (1000*3600*24)  == 0) {
			printf("[%d]%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld\n",c++,count[0],count[1],count[2],count[3],count[4],count[5],count[6],count[7],count[8]);
		}	
	}
	return 0;
}