chuck:
	cd src;$(MAKE) chuck.so;mv chuck.so ../lib
libchuck.a:
	cd src;$(MAKE) libchuck.a
readline:
	cd src;$(MAKE) readline.so;mv readline.so ../lib
testcase:
	cd src;$(MAKE) testcase
benchmark:
	cd src;$(MAKE) benchmark
benchmark_brocast:
	cd src;$(MAKE) benchmark_brocast
udp:
	cd src;$(MAKE) udp		
pbc:
	cd deps/pbc;$(MAKE);cp build/libpbc.a ../../lib
pbc-binding-lua53:
	cd deps/pbc/binding/lua53;$(MAKE);cp protobuf.lua protobuf.so ../../../../lib
cjson:
	cd deps/lua-cjson-2.1.0;$(MAKE);cp cjson.so ../../lib			