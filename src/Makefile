CFLAGS = -g -Wall -fno-strict-aliasing 
LDFLAGS = -lpthread -lrt -lm
SHARED  = -fPIC -shared
DEPDIR  = 
INCLUDE = -I./ -I../deps/lua-5.3.0/src
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
			  test/stream/tcp_echoserver.c\
			  test/stream/tcp_echoclient.c\
			  test/datagram/udp_echoserver.c\
			  test/datagram/udp_echoclient.c\
			  test/datagram/datagram/dgram_raw_server.c\
			  test/datagram/datagram/dgram_raw_client.c\
			  test/datagram/datagram/dgram_rpk_server.c\
			  test/datagram/datagram/dgram_rpk_client.c\
			  test/testpacket.c\
			  test/testdecoder.c\
			  test/stream/connection/conn_echo_server.c\
			  test/stream/connection/conn_broad_server.c\
			  test/stream/connection/conn_broad_client.c

release:$(source)
	$(CC) $(SHARED) -O2 $(CFLAGS) -c $(source) $(INCLUDE) $(DEFINE)
	ar -rc $(LIBNAME) *.o

debug:$(source)
	$(CC) $(SHARED) $(CFLAGS) -c $(source) $(INCLUDE) $(DEFINE) -D_DEBUG
	ar -rc $(LIBNAME) *.o

testcase:$(test-source)
	$(CC) $(CFLAGS) -c $(test-source) $(INCLUDE) $(DEFINE)
	$(CC) $(CFLAGS) -o testexception testexception.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o testthread testthread.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o tcp_echoserver tcp_echoserver.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o tcp_echoclient tcp_echoclient.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o udp_echoserver udp_echoserver.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o udp_echoclient udp_echoclient.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o testpacket testpacket.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o testdecoder testdecoder.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o conn_echo_server conn_echo_server.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o conn_broad_server conn_broad_server.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o conn_broad_client conn_broad_client.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o dgram_raw_server dgram_raw_server.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o dgram_raw_client dgram_raw_client.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o dgram_rpk_server dgram_rpk_server.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o dgram_rpk_client dgram_rpk_client.o $(LIBNAME) $(INCLUDE) $(LDFLAGS) $(DEFINE)


chucklua:$(source)
	rm *.o
	$(CC) $(SHARED) $(CFLAGS) -c luachuck.c $(source) $(INCLUDE) $(DEFINE) -D_DEBUG
	$(CC) $(CFLAGS) $(SHARED) -o chuck.so *.o $(LDFLAGS)


clean:
	rm *.o
	rm *.a	