CFLAGS = -g -Wall -fno-strict-aliasing 
LDFLAGS = -lpthread -lrt -llua -lm 
DEPDIR  = 
INCLUDE = -I./ 
DEFINE  =
LIBNAME = libchuck.a

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


source =  util/exception.c\
		  util/minheap.c\
		  util/timewheel.c\
		  util/log.c\
		  util/string.c\
		  util/refobj.c\
		  util/timerfd.c\
		  packet/wpacket.c\
		  packet/rpacket.c\
		  packet/packet.c\
		  packet/rawpacket.c\
		  packet/luapacket.c\
		  thread/thread.c\
		  thread/spinlock.c\
		  socket/socket.c\
		  socket/acceptor.c\
		  socket/connector.c\
		  socket/stream_socket.c\
		  socket/datagram_socket.c\
		  socket/socket_helper.c\
		  socket/wrap/decoder.c\
		  socket/wrap/connection.c\
		  socket/wrap/datagram.c\
		  engine/engine.c\
		  lua/lua_util.c\
		  lua/lua_util_packet.c\
		  mem/obj_allocator.c	  

test-source = test/testexception.c\
			  test/testthread.c\
			  test/testpacket.c\
			  test/testdecoder.c

libchuck-release:$(source)
	$(CC) -fPIC -O2 $(CFLAGS) -c $(source) $(INCLUDE) $(DEFINE)
	ar -rc $(LIBNAME) *.o
	rm *.o

libchuck-debug:$(source)
	$(CC) -fPIC $(CFLAGS) -c $(source) $(INCLUDE) $(DEFINE) -D_DEBUG
	ar -rc $(LIBNAME) *.o
	rm *.o

testcase:$(test-source)
	$(CC) $(CFLAGS) -c $(test-source) $(INCLUDE) $(DEFINE)
	$(CC) $(CFLAGS) -o testexception testexception.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o testthread testthread.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o testpacket testpacket.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o testdecoder testdecoder.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	rm *.o

chuck.so-debug:$(source)
	$(CC) -fPIC $(CFLAGS) -c luachuck.c $(source) $(INCLUDE) $(DEFINE) -D_DEBUG
	$(CC) $(CFLAGS) -shared -o chuck.so *.o -lpthread -lrt -lm
	rm *.o

chuck.so-release:$(source)
	$(CC) -O2 -fPIC $(CFLAGS) -c luachuck.c $(source) $(INCLUDE) $(DEFINE) -D_DEBUG
	$(CC) -O2 $(CFLAGS) -shared -o chuck.so *.o -lpthread -lrt -lm
	rm *.o	

clean:
	rm *.o
	rm *.a
	rm *.so	