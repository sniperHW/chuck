CFLAGS = -g -Wall -fno-strict-aliasing 
LDFLAGS = -lpthread -lrt -lm
SHARED  = -fPIC
INCLUDE = -I../../
DEFINE  =
LIB     = -llua

# Platform-specific overrides
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
ifeq ($(uname_S),Linux)
	LDFLAGS += -ldl
	DEFINE += -D_LINUX
endif

ifeq ($(uname_S),FreeBSD)
	DEFINE += -D_BSD
endif

all:
	$(CC) $(CFLAGS)  -o testlua testlua.c ../lua_util.c $(LIB) $(INCLUDE) $(LDFLAGS) $(DEFINE)