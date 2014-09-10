INSTALL_PATH = "/usr/bin"
INSTANCE_PATH = "${INSTALL_PATH}/instance"
VPATH = src:../inc

all:
	make sproxy
	make instance
sproxy: proxy.c debug.c unix.c ins.c proxy_config.c debug.h config.h mem.h ins.h unix.h proxy_config.h
	gcc -Wall -O -g -std=gnu99 -I../inc proxy.c unix.c debug.c ins.c proxy_config.c -o ../bin/sproxy	 	\
		 -DINSTANCE_PATH=\"${INSTANCE_PATH}\" 
instance: instance.c debug.c core.c list.c server.c task.c http.c unix.c ins.c proxy_config.c config.h debug.h 		\
	ins.h mem.h task.h core.h header.h list.h server.h unix.h proxy_config.h socks5.h net.h
	gcc -Wall -O -g -std=gnu99 -I../inc -lpthread instance.c debug.c core.c list.c server.c socks5.c 		\
		task.c http.c unix.c ins.c proxy_config.c net.c -o ../bin/instance -DINSTANCE_PATH=\"${INSTANCE_PATH}\"
