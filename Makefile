all:
	cd src && make
install:
	cp bin/sproxy bin/instance /usr/bin/
clear:
	rm bin/sproxy bin/instance
