CC = gcc
CFLAGS = -O2 -fPIC -Wall
LUADIR = /usr/local/include
TARGET = protobuf.so

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
ifeq ($(uname_S),Linux)
	SHAREFLAGS += -shared
endif

ifeq ($(uname_S),Darwin)
	SHAREFLAGS += -bundle -undefined dynamic_lookup	
endif


.PHONY : all clean

all : $(TARGET)

$(TARGET) : pbc-lua.c
	$(CC) $(CFLAGS) $(SHAREFLAGS) -o $@ -I../.. -I$(LUADIR) -L../../../../lib $^ -lpbc

clean :
	rm -f $(TARGET)
