#chuck

* first, Chuck is my son's name.

* second, Chuck is a high perference,asynchronous and easily use C/Lua network library under Linux/Freebsd.

#build

download and make [lua 5.3](http://www.lua.org/)

static library for c:

	make libchuck

dynamic library for lua:

	make chuck.so

c example:

	make samples


#examples

echo server:

	lua src/samples/lua/echoserver.lua

now try telnet to 127.0.0.1 8010.

asynchronous redis client:

start redis server on 127.0.0.1 6379

	lua src/samples/lua/redis_stress.lua 	



