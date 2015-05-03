CFLAGS  = -g -Wall -fno-strict-aliasing
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
	$(CC) $(CFLAGS)  -o testbit testbit.c $(LIB) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS)  -o testlist testlist.c $(LIB) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS)  -o testdlist testdlist.c $(LIB) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS)  -o testminheap testminheap.c ../minheap.c $(LIB) $(INCLUDE) $(LDFLAGS) $(DEFINE)		
	$(CC) $(CFLAGS)  -o testtimewheel testtimewheel.c ../timewheel.c ../time.c $(LIB) $(INCLUDE) $(LDFLAGS) $(DEFINE)