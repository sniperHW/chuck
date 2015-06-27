#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "spinlock.h"

spinlock *spin_create()
{
	spinlock *sp = malloc(sizeof(*sp));
	sp->lock_count = 0;
	sp->owner = 0;
	return sp;
}

void spin_destroy(spinlock *sp)
{
	free(sp);
}



