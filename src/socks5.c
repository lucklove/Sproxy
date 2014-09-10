#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "socks5.h"
#include "task.h"
#include "net.h"
#include "config.h"

int
select_method(struct task *tsk, char *recv_buffer)
{
	char reply_buffer[2] = { 0 };
	Method_select_request *method_request;
	Method_select_response *method_response;
	
	method_request = (Method_select_request *)recv_buffer;
	method_response = (Method_select_response *)reply_buffer;
	method_response->version = VERSION;
	
	if((int)method_request->version != VERSION) {
		method_response->select_method = 0xff;
		send(tsk->pair.client, method_response, sizeof(Method_select_response), 0);
		return -1;
	}

	method_response->select_method = 0x00;
	if(send(tsk->pair.client, method_response, sizeof(Method_select_response), 0) == -1) {
		return -1;
	}
	tsk->act = CONNECT_SERVER;
	return 0;
}

int
parse_command(struct task *tsk, char *recv_buffer)
{
	char reply_buffer[BUFFER_SIZE] = { 0 };
	Socks5_request *socks5_request;
	Socks5_response *socks5_response;
	
	socks5_request = (Socks5_request *)recv_buffer;
	if((socks5_request->version != VERSION) ||
		(socks5_request->cmd != CONNECT) ||
		(socks5_request->address_type == IPV6)) {
		printf("connect command error\n");
		return -1;
	}
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	if(socks5_request->address_type == IPV4) {
		memcpy(&sin.sin_addr.s_addr, &socks5_request->address_type + sizeof(socks5_request->address_type), 4);
		memcpy(&sin.sin_port, &socks5_request->address_type + sizeof(socks5_request->address_type) + 4, 2);
	} else if(socks5_request->address_type == DOMAIN) {
		char domain_length= *(&socks5_request->address_type + sizeof(socks5_request->address_type));
		char target_domain[256] = { 0 };
		strncpy(target_domain, &socks5_request->address_type + 2, (unsigned int)domain_length);
		struct hostent *phost = gethostbyname(target_domain);
		if(phost == NULL) {
			printf("Resolve %s error!\n", target_domain);
			return -1;
		}
		memcpy(&sin.sin_addr, phost->h_addr_list[0], phost->h_length);
		memcpy(&sin.sin_port, &socks5_request->address_type + sizeof(socks5_request->address_type)
			+ sizeof(domain_length) + domain_length, 2);
	}
	int real_server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(real_server_sock < 0) {
		perror("socket creat failed\n");
		return -1;
	}
	memset(reply_buffer, 0, sizeof(BUFFER_SIZE));
	socks5_response = (Socks5_response *)reply_buffer;
	socks5_response->version = VERSION;
	socks5_response->reserved = 0x00;
	socks5_response->address_type = 0x01;
	memset(socks5_response + 4, 0, 6);
	int ret = conn_nonb(real_server_sock, (struct sockaddr *)&sin, sizeof(struct sockaddr_in));
	if(ret == 0) {
		socks5_response->reply = 0x00;
		if(send(tsk->pair.client, socks5_response, 10, 0) == -1) {
			perror("send error");
			return -1;
		}
	} else {
		perror("Connect to read server error");
		socks5_response->reply = 0x01;
		send(tsk->pair.client, socks5_response, 10, 0);
		return -1;
	}
	tsk->pair.server = real_server_sock;
	return 0;
}
