/**	@file proxy.c
 *	@author Joshua <gnu.crazier@gmail.com>
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
#include <signal.h>
#include <sys/wait.h>
#include "config.h"
#include "mem.h"
#include "debug.h"
#include "ins.h"
#include "proxy_config.h"

/**	@bref 初始化网络，获得监听套接字
 *	@return 成功则返回监听套接字
 *	@retval int型的监听套接字
 *	@note 若初始化失败则程序退出
 */
int
net_init(short listen_port)
{
	int server_socket;
 	struct sockaddr_in      server_addr;
	int so_val = 1;
 	server_socket = socket(AF_INET, SOCK_STREAM, 0);
 	if(server_socket < 0) {
		perror("socket"); 
		exit(1);
	}
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &so_val, sizeof (so_val));
 	memset(&server_addr, 0, sizeof(struct sockaddr_in));
 	server_addr.sin_family = AF_INET;
 	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
 	server_addr.sin_port = htons(listen_port);
	if(bind(server_socket, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr))) {
		perror("bind");
		exit(1);
	}
	if(listen(server_socket, LISTEN_QUEUE)) {
		perror("listen");
		exit(1);
	}
	return server_socket;
}

/**	@brief　监视实例是否退出
 *		若退出则重启实例
 *	@node   该函数作为参数传递给signal();
 *	@param	sig:该参数由系统传入
 */
void
sig_child(int sig)
{
	pid_t pid = 0;
	do {
		pid = waitpid(-1, NULL, WNOHANG);
		if(pid == 0)
			break;
		printf("%d崩溃...\n", pid);
		int index = find_instance(pid);
		if(index == -1)
			continue;
		close_instance(index);
		add_instance(index);
	} while(1);
}		

/**	@brief  收到退出信号时销毁所有实例并退出
 *	@node   该函数作为参数传递给signal();
 *	@param	sig:该参数由系统传入
 */
void
sig_exit(int sig)
{
	signal(SIGCHLD, SIG_IGN);
	instance_destroy();
	exit(0);
}

/**	@brief 调用signal植入信号处理函数
 */
void
sig_init()
{
	signal(SIGCHLD, sig_child);
	signal(SIGINT, sig_exit);
}

/** 	@brief 父进程的主函数，启动实例(子进程)并监听
 */
int
main(int argc, char *argv[])
{
	struct          sockaddr_in client_addr;
	int             client_socket;
	int		server_socket;
	socklen_t       length;
	short int	listen_port = 0;
	if(argc < 3) {
		printf("USAGE : %s PORT [ [ -h ] | [ -s ] | [-r redirect_host redirect_port] ]\n", argv[0]);
		exit(0);
	} else {
		if(!sscanf(argv[1], "%hd", &listen_port)) {
			printf("USAGE : %s PORT [ [ -h ] | [ -s ] | [-r redirect_host redirect_port] ]\n", argv[0]);
			exit(0);
		}
		if(strncmp(argv[2], "-h", 2) == 0) {
			http_proxy_flag = 1;
		} else if(strncmp(argv[2], "-s", 2) == 0) {
			socks5_proxy_flag = 1;
		} else if(strncmp(argv[2], "-r", 2) == 0) {
			redirect_flag = 1;
			if(argc != 5) {		
				printf("USAGE : %s PORT [ [ -h ] | [ -s ] | [-r redirect_host redirect_port] ]\n", argv[0]);
				exit(0);
			}
			redirect_host = argv[3];
			redirect_port = argv[4];
		}
	}


/*
else if(argc == 2) {
		if(!sscanf(argv[1], "%hd", &listen_port)) {
			printf("USAGE : %s PORT [-r redirect_host redirect_port]\n", argv[0]);
			exit(-1);
		}
	} else {
		if(!sscanf(argv[1], "%hd", &listen_port)) {
			printf("USAGE : %s PORT [-r redirect_host redirect_port]\n", argv[0]);
			exit(-1);
		}
		if((strncmp(argv[2], "-r", 2) != 0) || (argc != 5)) {
			printf("USAGE : %s PORT [-r redirect_host redirect_port]\n", argv[0]);
			exit(-1);
		}
		redirect_host = argv[3];
		redirect_port = argv[4];
		redirect_flag = 1;
	}
*/
	instance_init(0);
	server_socket = net_init(listen_port);
	sig_init();
	while(1) {
		client_socket = accept(server_socket, (struct sockaddr *)(&client_addr), &length);
		if(client_socket == -1)
			perror("");
		char ip[16];
		printf("accept %s\n", inet_ntop(AF_INET, &client_addr.sin_addr, ip, 16));
		push_to_instance(client_socket);
		close(client_socket);
        }
	return 0;
}
