#ifndef _CHK_ATOMIC_H
#define _CHK_ATOMIC_H

#define chk_compare_and_swap(PTR,OLD,NEW)									({	\
	int __result;																\
	do __result = __sync_val_compare_and_swap(PTR,OLD,NEW) == OLD?1:0;			\
	  while(0);																	\
	__result;																})	

#define chk_atomic_increase_fetch(PTR) __sync_add_and_fetch(PTR,1)

#define chh_atomic_decrease_fetch(PTR) __sync_sub_and_fetch(PTR,1)

#define chk_atomic_fetch_increase(PTR) __sync_fetch_and_add(PTR,1)

#define chk_atomic_fetch_decrease(PTR) __sync_fetch_and_sub(PTR,1)

#define chk_fence __sync_synchronize


#endif