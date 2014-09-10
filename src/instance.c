/**     @file proxy.c
 *      @author Joshua <gnu.crazier@gmail.com>
 */    
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <time.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "config.h"
#include "task.h"
#include "unix.h"
#include "debug.h"
#include "server.h"
#include "ins.h"
#include "proxy_config.h"

/**	@brief 子进程(即实例)的主函数
 *	从父进程接受套接字并向任务队列从添加一个任务而已
 *	当然实现得做一些初始化工作
 */	
int
main(int argc, char *argv[])
{
	int             client_socket;
D
	if(strncmp(argv[1], "-r", 2) == 0) {
		redirect_flag = 1;
		redirect_host = argv[1];
		redirect_port = argv[2];
	} else if(strncmp(argv[1], "-h", 2) == 0) {
		http_proxy_flag = 1;
	} else if(strncmp(argv[1], "-s", 2) == 0) {
		socks5_proxy_flag = 1;
	} else {
		printf("Please do not execute me directly!\n");
	}
	task_init();
	instance_init(IM_CHILD);
	while(1) {
		if((client_socket = recv_fd(STDIN_FILENO)) < 0) {
			perror("recv_fd");
			continue;
		}
		add_task(client_socket);
        }
	return 0;
}
