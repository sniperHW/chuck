#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include "util/chk_timer.h"
#include "config.h"

uint64_t tick = 0;


uint32_t count[9] = {0};


int32_t cb(uint64_t tick,void*ud) {
	chk_timer *t = *((chk_timer**)ud);
	switch(chk_timer_timeout(t)){
		case 1:count[0]++;break;
		case 101:count[1]++;break;
		case 999:count[2]++;break;
		case 1001:count[3]++;break;
		case 1000:count[4]++;break;
		case 60000:count[5]++;break;
		case 60001:count[6]++;break;
		case 9104999:count[7]++;break;
		case MAX_TIMEOUT:count[8]++;break;												
	}
	return 0;
}

#define N 1000000

chk_timer* ids[N];


int32_t on_timeout_cb(uint64_t tick,void*ud)
{ return 0; }

void test1() {
    clock_t t;
    int i;
    chk_timermgr *mgr = chk_timermgr_new();

    t = clock();
    for (i = 0; i < N; ++i) {
        ids[i] = chk_timer_register(mgr, rand() * rand(), on_timeout_cb, NULL, 0);
    }
    printf("create time: %dms\n", (int)((clock()-t)*1000/CLOCKS_PER_SEC));

    t = clock();
    for (i = 0; i < N; ++i)
        chk_timer_unregister(ids[i]);
    printf("cancel time: %dms\n", (int)((clock()-t)*1000/CLOCKS_PER_SEC));

    t = clock();
    for (i = 0; i < N; ++i) {
        ids[i] = chk_timer_register(mgr, rand() * rand(), on_timeout_cb, NULL, 0);
    }
    printf("create time: %dms\n", (int)((clock()-t)*1000/CLOCKS_PER_SEC));

    t = clock();
    for (i = 0; i < N; ++i) {
        chk_timer_tick(mgr, i);
    }
    printf("update time: %dms\n", (int)((clock()-t)*1000/CLOCKS_PER_SEC));

    chk_timermgr_del(mgr);
}

void  test2() {

	uint32_t c = 0;
	chk_timer *t1,*t2,*t3,*t4,*t5,*t6,*t7,*t8,*t9;
	chk_timermgr *m = chk_timermgr_new();
	t1 = chk_timer_register(m,1,cb,&t1,tick);
	t2 = chk_timer_register(m,101,cb,&t2,tick);
	t3 = chk_timer_register(m,999,cb,&t3,tick);
	t4 = chk_timer_register(m,1001,cb,&t4,tick);
	t5 = chk_timer_register(m,1000,cb,&t5,tick);
	t6 = chk_timer_register(m,60000,cb,&t6,tick);
	t7 = chk_timer_register(m,60001,cb,&t7,tick);
	t8 = chk_timer_register(m,9104999,cb,&t8,tick);
	t9 = chk_timer_register(m,MAX_TIMEOUT,cb,&t9,tick);							
	while(1){
		tick = chk_tmer_inctick(tick);
		if(tick % (1000*3600*24)  == 0) {
			printf("[%d]%d,%d,%d,%d,%d,%d,%d,%d,%d\n",c++,count[0],count[1],count[2],count[3],count[4],count[5],count[6],count[7],count[8]);
		}
		chk_timer_tick(m,tick);	
	}
}

int main(){
	test1();
	test2();
	return 0;
}