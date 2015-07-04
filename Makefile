libchuck:
	cd src;make libchuck-release

chuck:
	cd src;make chuck.so-release;cp chuck.so ../

examples:
	cd samples;make