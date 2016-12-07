chuck:
	cd src;$(MAKE) chuck.so;mv chuck.so ../lib
libchuck.a:
	cd src;$(MAKE) libchuck.a
readline:
	cd src;$(MAKE) readline.so;mv readline.so ../lib
testcase:
	cd src;$(MAKE) testcase		