CFLAGS  = -g -Wall -fno-strict-aliasing 
LDFLAGS = -lpthread -lm
DEPDIR  = 
INCLUDE = -I./ -I../deps/lua-5.3.0/src -I../deps -I../ -I../deps/ -I../deps/linenoise -I../deps/openssl-1.1.0c/include
DEFINE  = -DUSE_BUFFER_POOL
LIBNAME = libchuck.a
MAKE    =
CC      =
LIBRARY = -L../deps/lua-5.3.0/src
HTTP_PARSER = ../deps/http-parser/libhttp_parser.a
OPENSSL = ../deps/openssl-1.1.0c/libssl.a
CRYPTO = ../deps/openssl-1.1.0c/libcrypto.a

# Platform-specific overrides
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
ifeq ($(uname_S),Linux)
	MAKE += make
	CC += gcc
	DEFINE += -D_LINUX
	LDFLAGS += -lrt -ldl
	SHAREFLAGS += -shared
endif

ifeq ($(uname_S),Darwin)
	MAKE += make
	CC += gcc
	DEFINE += -D_MACH
	SHAREFLAGS += -bundle -undefined dynamic_lookup	
endif

source.lib =  util/chk_log.c\
			  util/chk_timer.c\
			  util/chk_exception.c\
			  util/chk_bytechunk.c\
			  util/lookup8.c\
			  util/chk_signal.c\
			  util/chk_util.c\
			  util/sds.c\
			  util/base64.c\
			  util/sha1.c\
			  util/chk_error.c\
			  lua/chk_lua.c\
			  socket/chk_stream_socket.c\
			  socket/chk_datagram_socket.c\
			  socket/chk_socket_helper.c\
			  socket/chk_acceptor.c\
			  socket/chk_connector.c\
			  socket/chk_decoder.c\
			  socket/chk_buffer_reader.c\
			  event/chk_event_loop.c\
			  redis/chk_client.c\
			  thread/chk_thread.c

source.so =   util/chk_log.c\
			  util/chk_timer.c\
			  util/chk_exception.c\
			  util/chk_bytechunk.c\
			  util/lookup8.c\
			  util/chk_util.c\
			  util/chk_signal.c\
			  util/sds.c\
			  util/base64.c\
			  util/sha1.c\
			  util/chk_error.c\
			  lua/chk_lua.c\
			  socket/chk_stream_socket.c\
			  socket/chk_datagram_socket.c\
			  socket/chk_socket_helper.c\
			  socket/chk_acceptor.c\
			  socket/chk_connector.c\
			  socket/chk_decoder.c\
			  event/chk_event_loop.c\
			  redis/chk_client.c\
			  thread/chk_thread.c

$(LIBNAME):
	$(CC) -fpic $(CFLAGS) -c $(source.lib) $(INCLUDE) $(DEFINE)
	ar -rc $(LIBNAME) *.o
	rm *.o
	mv $(LIBNAME) ../lib/$(LIBNAME)

$(HTTP_PARSER):
	cd ../deps/http-parser/;$(MAKE) package

$(OPENSSL):
	cd ../deps/openssl-1.1.0c/;./config;$(MAKE)

$(CRYPTO):
	cd ../deps/openssl-1.1.0c/;./config;$(MAKE)		

../deps/linenoise/linenoise.o:
	cd ../deps/linenoise/;$(MAKE)	

chuck.so:$(source.so) $(HTTP_PARSER) $(OPENSSL) $(CRYPTO)
	$(CC) -fpic $(CFLAGS) -c luabind/chuck.c $(source.so) $(INCLUDE) $(DEFINE) -DCHUCK_LUA
	$(CC) $(CFLAGS) $(SHAREFLAGS) -o chuck.so *.o  $(HTTP_PARSER) $(OPENSSL) $(CRYPTO) $(LDFLAGS) $(LIBRARY)
	rm *.o

readline.so:../deps/linenoise/linenoise.o
	$(CC) -fpic $(CFLAGS) -c luabind/readline.c $(INCLUDE) $(DEFINE) -DCHUCK_LUA
	$(CC) $(CFLAGS) $(SHAREFLAGS) -o readline.so readline.o ../deps/linenoise/linenoise.o $(LDFLAGS) $(LIBRARY)
	rm *.o		

testcase:
	$(CC) $(CFLAGS) -o ../test/bin/sslserver ../test/sslserver.c ../lib/$(LIBNAME) $(OPENSSL) $(CRYPTO) $(INCLUDE) $(DEFINE) $(LDFLAGS)
	$(CC) $(CFLAGS) -o ../test/bin/sslclient ../test/sslclient.c ../lib/$(LIBNAME) $(OPENSSL) $(CRYPTO) $(INCLUDE) $(DEFINE) $(LDFLAGS)	
	$(CC) $(CFLAGS) -o ../test/bin/testlog ../test/testlog.c ../lib/$(LIBNAME) $(OPENSSL) $(CRYPTO) $(INCLUDE) $(DEFINE) $(LDFLAGS)
	$(CC) $(CFLAGS) -o ../test/bin/testexception ../test/testexception.c ../lib/$(LIBNAME) $(OPENSSL) $(CRYPTO) $(INCLUDE) $(DEFINE) $(LDFLAGS)
	$(CC) $(CFLAGS) -o ../test/bin/testhttppacket ../test/testhttppacket.c ../lib/$(LIBNAME) $(HTTP_PARSER) $(OPENSSL) $(CRYPTO) $(INCLUDE) $(DEFINE) $(LDFLAGS)	
	$(CC) $(CFLAGS) -o ../test/bin/testtimer ../test/testtimer.c ../lib/$(LIBNAME) $(OPENSSL) $(CRYPTO) $(INCLUDE) $(DEFINE) $(LDFLAGS)
	$(CC) $(CFLAGS) -o ../test/bin/tcpecho ../test/tcpecho.c ../lib/$(LIBNAME) $(OPENSSL) $(CRYPTO) $(INCLUDE) $(DEFINE) $(LDFLAGS)	
	$(CC) $(CFLAGS) -o ../test/bin/test_objpool ../test/test_objpool.c ../lib/$(LIBNAME) $(OPENSSL) $(CRYPTO) $(INCLUDE) $(DEFINE) $(LDFLAGS)	
	$(CC) $(CFLAGS) -o ../test/bin/teststring ../test/teststring.c ../lib/$(LIBNAME) $(OPENSSL) $(CRYPTO) $(INCLUDE) $(DEFINE) $(LDFLAGS)		
	$(CC) $(CFLAGS) -o ../test/bin/test_bytebuffer ../test/test_bytebuffer.c ../lib/$(LIBNAME) $(OPENSSL) $(CRYPTO) $(INCLUDE) $(DEFINE) $(LDFLAGS)			
	$(CC) $(CFLAGS) -o ../test/bin/testconnect ../test/testconnect.c ../lib/$(LIBNAME) $(OPENSSL) $(CRYPTO) $(INCLUDE) $(DEFINE) $(LDFLAGS)
	$(CC) $(CFLAGS) -o ../test/bin/testredis ../test/testredis.c ../lib/$(LIBNAME) $(OPENSSL) $(CRYPTO) $(INCLUDE) $(DEFINE) $(LDFLAGS)						
	$(CC) $(CFLAGS) -o ../test/bin/testlua ../test/testlua.c ../lib/$(LIBNAME) $(OPENSSL) $(CRYPTO) $(INCLUDE) $(DEFINE) $(LDFLAGS) -llua -lm -ldl $(LIBRARY)

benchmark:
	$(CC) $(CFLAGS) -o ../test/bin/benchmark ../test/benchmark.c ../lib/$(LIBNAME) $(OPENSSL) $(CRYPTO) $(INCLUDE) $(DEFINE) $(LDFLAGS)	
benchmark_brocast:
	$(CC) $(CFLAGS) -o ../test/bin/benchmark_brocast ../test/benchmark_brocast.c ../lib/$(LIBNAME) $(OPENSSL) $(CRYPTO) $(INCLUDE) $(DEFINE) $(LDFLAGS)	
udp:
	$(CC) $(CFLAGS) -o ../test/bin/udp ../test/udp.c ../lib/$(LIBNAME) $(OPENSSL) $(CRYPTO) $(INCLUDE) $(DEFINE) $(LDFLAGS)	


clean:
	rm *.o
	rm *.a
	rm *.so	