CFLAGS  = -g -O2 -Wall -fno-strict-aliasing 
LDFLAGS = -lpthread -lrt -lm
DEPDIR  = 
INCLUDE = -I./ -I../deps
DEFINE  =
LIBNAME = libchuck.a
MAKE    =
CC      = 
# Platform-specific overrides
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
ifeq ($(uname_S),Linux)
	MAKE += make
	CC += gcc
	DEFINE += -D_LINUX
endif

ifeq ($(uname_S),FreeBSD)
	MAKE += gmake
	CC += clang
	DEFINE += -D_BSD
endif

source =  util/chk_log.c\
		  util/chk_timer.c\
		  util/chk_exception.c\
		  util/chk_refobj.c\
		  lua/chk_lua.c\
		  socket/chk_stream_socket.c\
		  socket/chk_socket_helper.c\
		  socket/chk_acceptor.c\
		  socket/chk_connector.c\
		  event/chk_event_loop.c\
		  redis/chk_client.c\
		  thread/chk_thread.c


$(LIBNAME):$(source)
	$(CC) -fPIC $(CFLAGS) -c $(source) $(INCLUDE) $(DEFINE) -D_DEBUG
	ar -rc $(LIBNAME) *.o
	rm *.o

testlog:test/testlog.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/testlog test/testlog.c $(LIBNAME) $(INCLUDE) $(DEFINE) -D_DEBUG $(LDFLAGS)

testexception:test/testexception.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/testexception test/testexception.c $(LIBNAME) $(INCLUDE) $(DEFINE) -D_DEBUG $(LDFLAGS)

testpacket:test/testpacket.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/testpacket test/testpacket.c $(LIBNAME) $(INCLUDE) $(DEFINE) -D_DEBUG $(LDFLAGS)

testtimer:test/testtimer.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/testtimer test/testtimer.c $(LIBNAME) $(INCLUDE) $(DEFINE) -D_DEBUG $(LDFLAGS)

tcpecho:test/tcpecho.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/tcpecho test/tcpecho.c $(LIBNAME) $(INCLUDE) $(DEFINE) -D_DEBUG $(LDFLAGS)

echobrocast:test/echobrocast.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/echobrocast test/echobrocast.c $(LIBNAME) $(INCLUDE) $(DEFINE) -D_DEBUG $(LDFLAGS)

echoclient:test/echoclient.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/echoclient test/echoclient.c $(LIBNAME) $(INCLUDE) $(DEFINE) -D_DEBUG $(LDFLAGS)	

testconnect:test/testconnect.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/testconnect test/testconnect.c $(LIBNAME) $(INCLUDE) $(DEFINE) -D_DEBUG $(LDFLAGS)

testredis:test/testredis.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/testredis test/testredis.c $(LIBNAME) $(INCLUDE) $(DEFINE) -D_DEBUG $(LDFLAGS)						

testlua:test/testlua.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/testlua test/testlua.c $(LIBNAME) $(INCLUDE) $(DEFINE) -D_DEBUG $(LDFLAGS) -llua -lm -ldl

clean:
	rm *.o
	rm *.a
	rm *.so	