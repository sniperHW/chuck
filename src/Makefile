CFLAGS  = -g -Wall -fno-strict-aliasing 
LDFLAGS = -lpthread -lm
DEPDIR  = 
INCLUDE = -I./ -I../deps/lua-5.3.0/src -I../deps -I../
DEFINE  = -DUSE_BUFFER_POOL
LIBNAME = libchuck.a
MAKE    =
CC      =
LIBRARY = -L../deps/lua-5.3.0/src
# Platform-specific overrides
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
ifeq ($(uname_S),Linux)
	MAKE += make
	CC += gcc
	DEFINE += -D_LINUX
	LDFLAGS += -lrt
	SHAREFLAGS += -shared
endif

ifeq ($(uname_S),Darwin)
	MAKE += make
	CC += clang
	DEFINE += -D_MACH
	SHAREFLAGS += -bundle -undefined dynamic_lookup	
endif

source.lib =  util/chk_log.c\
			  util/chk_timer.c\
			  util/chk_exception.c\
			  util/chk_refobj.c\
			  util/chk_bytechunk.c\
			  util/chk_string.c\
			  util/lookup8.c\
			  http/chk_http.c\
			  lua/chk_lua.c\
			  socket/chk_stream_socket.c\
			  socket/chk_socket_helper.c\
			  socket/chk_acceptor.c\
			  socket/chk_connector.c\
			  event/chk_event_loop.c\
			  redis/chk_client.c\
			  thread/chk_thread.c\
			  fiber/chk_fiber.c

source.so =   util/chk_log.c\
			  util/chk_timer.c\
			  util/chk_exception.c\
			  util/chk_refobj.c\
			  util/chk_bytechunk.c\
			  util/chk_string.c\
			  lua/chk_lua.c\
			  socket/chk_stream_socket.c\
			  socket/chk_socket_helper.c\
			  socket/chk_acceptor.c\
			  socket/chk_connector.c\
			  event/chk_event_loop.c\
			  redis/chk_client.c\
			  thread/chk_thread.c\
			  fiber/chk_fiber.c


$(LIBNAME):$(source.lib)
	$(CC) -fpic $(CFLAGS) -c $(source.lib) $(INCLUDE) $(DEFINE)
	ar -rc $(LIBNAME) *.o
	rm *.o

chuck.so:$(source.so)
	$(CC) -fpic $(CFLAGS) -c luabind/chuck.c $(source.so) $(INCLUDE) $(DEFINE)
	$(CC) $(CFLAGS) $(SHAREFLAGS) -o chuck.so *.o $(HTTP_PARSER) $(JEMALLOC) $(LDFLAGS) $(LIBRARY)
	rm *.o

testlog:test/testlog.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/testlog test/testlog.c $(LIBNAME) $(INCLUDE) $(DEFINE) $(LDFLAGS)

testexception:test/testexception.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/testexception test/testexception.c $(LIBNAME) $(INCLUDE) $(DEFINE) $(LDFLAGS)

testpacket:test/testpacket.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/testpacket test/testpacket.c $(LIBNAME) $(INCLUDE) $(DEFINE) $(LDFLAGS)

testhttppacket:test/testhttppacket.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/testhttppacket test/testhttppacket.c $(LIBNAME) ../deps/http-parser/libhttp_parser.a $(INCLUDE) $(DEFINE) $(LDFLAGS)	

testtimer:test/testtimer.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/testtimer test/testtimer.c $(LIBNAME) $(INCLUDE) $(DEFINE) $(LDFLAGS)

tcpecho:test/tcpecho.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/tcpecho test/tcpecho.c $(LIBNAME) $(INCLUDE) $(DEFINE) $(LDFLAGS)

testfiber:test/testfiber.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/testfiber test/testfiber.c $(LIBNAME) $(INCLUDE) $(DEFINE) $(LDFLAGS)	

test_objpool:test/test_objpool.c $(LIBNAME)	
	$(CC) $(CFLAGS) -o test/test_objpool test/test_objpool.c $(LIBNAME) $(INCLUDE) $(DEFINE) $(LDFLAGS)	

teststring:test/teststring.c $(LIBNAME)	
	$(CC) $(CFLAGS) -o test/teststring test/teststring.c $(LIBNAME) $(INCLUDE) $(DEFINE) $(LDFLAGS)	

test_bytebuffer:test/test_bytebuffer.c $(LIBNAME)	
	$(CC) $(CFLAGS) -o test/test_bytebuffer test/test_bytebuffer.c $(LIBNAME) $(INCLUDE) $(DEFINE) $(LDFLAGS)	

echobrocast:test/echobrocast.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/echobrocast test/echobrocast.c $(LIBNAME) $(INCLUDE) $(DEFINE) $(LDFLAGS)

echoclient:test/echoclient.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/echoclient test/echoclient.c $(LIBNAME) $(INCLUDE) $(DEFINE) $(LDFLAGS)	

testconnect:test/testconnect.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/testconnect test/testconnect.c $(LIBNAME) $(INCLUDE) $(DEFINE) $(LDFLAGS)

testredis:test/testredis.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/testredis test/testredis.c $(LIBNAME) $(INCLUDE) $(DEFINE) $(LDFLAGS)						

testlua:test/testlua.c $(LIBNAME)
	$(CC) $(CFLAGS) -o test/testlua test/testlua.c $(LIBNAME) $(INCLUDE) $(DEFINE) $(LDFLAGS) -llua -lm -ldl

clean:
	rm *.o
	rm *.a
	rm *.so	