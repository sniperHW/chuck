#include <stdio.h>
#include "fiber/chk_fiber.h"
#include "util/chk_time.h"


void fiber_main(void *arg) {
	uint32_t id = (uint32_t)arg;

	for(;;) {
		printf("fiber %u\n",id);
		chk_fiber_sleep(1000,NULL);
	}
}

int main() {

	int32_t i = 0;

	chk_fiber_sheduler_init(1024*1024);

	printf("ok\n");

	for(; i < 10; ++i) {
		chk_fiber_spawn(fiber_main,(void*)i);
		printf("ok %d\n",i);
	}

	printf("spawn ok\n");
	for(;;) {

		chk_fiber_schedule();

		chk_sleepms(10);

	}

	return 0;
}
