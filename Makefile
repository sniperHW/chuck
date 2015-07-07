libchuck:
	cd src;make libchuck-release

chuck:
	cd src;make chuck.so-debug;cp chuck.so ../

examples:
	cd samples;make