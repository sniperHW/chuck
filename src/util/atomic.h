/*
    Copyright (C) <2014>  <sniperHW@163.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef _ATOMIC_H
#define _ATOMIC_H
#include <stdint.h>

#define COMPARE_AND_SWAP(PTR,OLD,NEW)\
	({int __result;\
	  do __result = __sync_val_compare_and_swap(PTR,OLD,NEW) == OLD?1:0;\
	  while(0);\
	  __result;})

#define ATOMIC_INCREASE_FETCH(PTR) __sync_add_and_fetch(PTR,1)
#define ATOMIC_DECREASE_FETCH(PTR) __sync_sub_and_fetch(PTR,1)

#define ATOMIC_FETCH_INCREASE(PTR) __sync_fetch_and_add(PTR,1)
#define ATOMIC_FETCH_DECREASE(PTR) __sync_fetch_and_sub(PTR,1)


#define FENCE __sync_synchronize

#endif
