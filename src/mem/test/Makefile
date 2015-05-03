CFLAGS  = -O2 -g -Wall -fno-strict-aliasing
LDFLAGS = -lpthread
INCLUDE = -I../../ 
DEFINE  =
LIB     =

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
ifeq ($(uname_S),Linux)
	DEFINE += -D_LINUX
endif

ifeq ($(uname_S),FreeBSD)
	DEFINE += -D_BSD
endif 


all:
	$(CC) $(CFLAGS)  -o objpool objpool.c ../obj_allocator.c ../../util/time.c $(LIB) $(INCLUDE) $(LDFLAGS) $(DEFINE)