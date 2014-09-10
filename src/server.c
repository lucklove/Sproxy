#include "server.h"
#include "debug.h"
#include "config.h"
#include "mem.h"
#include "ins.h"
#include "net.h"
#include "proxy_config.h"
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

static int server_num;
static pthread_mutex_t mtx;

void
server_init()
{
	server_num = 0;
	pthread_mutex_init(&mtx, NULL);
}

inline int
get_server_num()
{
	return server_num;
}

int
open_server(struct task *tsk)
{
        int     server_socket;
	int 	can_open = 0;
        if(tsk->pair.server != -1) {
		if(redirect_flag) {
			return 0;
		} else {
     			close_server(tsk);
		}
	}
	pthread_mutex_lock(&mtx);
	if(server_num < MAX_SERVER_NUM) {
		can_open = 1;
		server_num++;
	}
	pthread_mutex_unlock(&mtx);
	if(can_open) {
		if(redirect_flag) {
			server_socket = open_client_socket(redirect_host, (short)atol(redirect_port));
		} else {
	        	server_socket = open_client_socket(tsk->pkg.host, tsk->pkg.port);
		}
		if(server_socket < 0) {
			pthread_mutex_lock(&mtx);
			server_num--;
			pthread_mutex_unlock(&mtx);
			return -1;
		};
	} else {
		return LACK_OF_PORT;
	}
        tsk->pair.server = server_socket;
	set_server_num(server_num);
        return 0;
}

void
close_server(struct task *tsk)
{
	pthread_mutex_lock(&mtx);
	server_num--;
	pthread_mutex_unlock(&mtx);
	close(tsk->pair.server);
	Free(tsk->c2s_buf);
	tsk->pair.server = -1;
	set_server_num(server_num);
}
