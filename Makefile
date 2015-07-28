MAKE = 

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
ifeq ($(uname_S),Linux)
	MAKE += make
endif

ifeq ($(uname_S),FreeBSD)
	MAKE += gmake
endif


libchuck:
	cd src;$(MAKE) libchuck-debug

chuck:
	cd src;$(MAKE) chuck.so-debug;cp chuck.so ../

examples:
	cd samples;$(MAKE)

parser:
	gcc -g -o parser samples/testhttpparser.c ./deps/http-parser/libhttp_parser.a -I./deps