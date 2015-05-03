#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "spinlock.h"

spinlock *spin_create()
{
	spinlock *sp = malloc(sizeof(*sp));
	sp->lock_count = 0;
#ifdef _WIN
	sp->owner.p = NULL;
	sp->owner.x = 0;	
#else
	sp->owner = 0;
#endif	
	return sp;
}

void spin_destroy(spinlock *sp)
{
	free(sp);
}



