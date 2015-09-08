CFLAGS = -g -Wall -fno-strict-aliasing 
LDFLAGS = -lpthread -lrt -llua -lm -ljemalloc
DEPDIR  = 
INCLUDE = -I./../ 
DEFINE  =

# Platform-specific overrides
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
ifeq ($(uname_S),Linux)
	LDFLAGS += -ldl
	DEFINE += -D_LINUX
endif

ifeq ($(uname_S),FreeBSD)
	LDFLAGS += -lexecinfo
	DEFINE += -D_BSD
endif

source =  testpacket.c\
		  testdecoder.c\
		  testexception.c\
		  testthread.c

all:
	@if [ ! -d "bin" ]; then mkdir bin; fi	
	$(CC) $(CFLAGS) -c $(source) $(INCLUDE) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/testpacket testpacket.o ../libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/testdecoder testdecoder.o ../libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/testexception testexception.o ../libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/testthread testthread.o ../libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	rm *.o		  