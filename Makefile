all:
	cd src && make
install:
	cp bin/jproxy bin/instance /usr/bin/
clear:
	rm bin/jproxy bin/instance
