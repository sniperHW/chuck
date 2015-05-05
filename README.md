#chuck

* first, Chuck is my son's name.

* second, Chuck is a high perference and easily use C/lua network library under Linux/Freebsd.

#build

download and make [lua 5.3](http://www.lua.org/)

static library for c:

	make libchuck

dynamic library for lua:

	make chuck.so

c example:

	make samples


#examples

start a echo server:

	lua src/samples/lua/echoserver.lua

and then,telnet to 127.0.0.1 8010.



