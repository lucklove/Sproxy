/**	@file 实例的主要部分,处理报文的转发
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <time.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/time.h>
#include <assert.h>

#include "config.h"
#include "list.h"
#include "task.h"
#include "server.h"
#include "header.h"
#include "debug.h"
#include "mem.h"
#include "proxy_config.h"
#include "socks5.h"

#define SERVER_NAME "micro_proxy"
#define SERVER_URL "http://www.acme.com/software/micro_proxy/"
#define PROTOCOL "HTTP/1.0"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

static int
send_nonb(int sock, char *buff, int size)
{
	struct timeval timeout = { MAX_DELAY, 0 };
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(struct timeval));
	while(size > 0) {
		int send_size = send(sock, buff, size, 0);
		if(send_size <= 0)
			return -1;
		buff += send_size;
		size -= send_size;
	}
	return size;
}

static int 
build_headers(char *buffer, int status, char* title,char* mime_type, int length, time_t mod)
{

        time_t now;
        char timebuf[100];
        int index = 0;
        index += sprintf(buffer + index, "%s %d %s\r\n", PROTOCOL, status, title );
        index += sprintf(buffer + index, "Server: %s\r\n", SERVER_NAME );
        now = time( (time_t*) 0 );
        strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &now ) );
        index += sprintf(buffer + index, "Date: %s\r\n", timebuf );
        if(mime_type != (char*) 0)
                index += sprintf(buffer + index, "Content-Type: %s\r\n", mime_type);
        if(length >= 0)
                index += sprintf(buffer + index, "Content-Length: %d\r\n", length);
        if(mod != (time_t) -1) {
                strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &mod ) );
                index += printf(buffer + index, "Last-Modified: %s\r\n", timebuf );
        }
        index += sprintf(buffer + index, "Connection: close\r\n" );
        index += printf(buffer + index, "\r\n" );
        return index;
}

static void
send_error(int status, char* title, char* text, int sockfd)
{
        char buffer[BUFFER_SIZE];
        int index = 0;
        index += build_headers(buffer + index, status, title, "text/html", -1, -1);
        index += sprintf(buffer + index, "\
<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\n\
<html>\n\
  <head>\n\
    <meta http-equiv=\"Content-type\" content=\"text/html;charset=UTF-8\">\n\
    <title>%d %s</title>\n\
  </head>\n\
  <body bgcolor=\"#cc9999\" text=\"#000000\" link=\"#2020ff\" vlink=\"#4040cc\">\n\
    <h4>%d %s</h4>\n\n", status, title, status, title );
        index += sprintf(buffer + index, "%s\n\n", text );
        index += sprintf(buffer + index, "\
    <hr>\n\
    <address><a href=\"%s\">%s</a></address>\n\
  </body>\n\
</html>\n",
        SERVER_URL, SERVER_NAME);
        send(sockfd, buffer, strlen(buffer), 0);
}

int
proxy_session(struct task *tsk)
{
	int	ret = 0;
	char 	buffer[BUFFER_SIZE];
	errno = 0;
	// XXX:可以用tsk->pkg里的数据代替c2s_buf
	if(tsk->c2s_buf != NULL && tsk->buf_size != 0) {
		if(redirect_flag) {
			int any_error = open_server(tsk);
			if(any_error == -1) {
				perror("open_server");
				return -1;
			}
			if(any_error == LACK_OF_PORT) {
				tsk->flush_time = time(NULL);
				return 0;
			}
		} else if(http_proxy_flag) {
			destroy_pkg(&tsk->pkg);
			if(parse_initial_request(tsk->c2s_buf, tsk->buf_size, &tsk->pkg))
				return -1;
			int any_error = open_server(tsk);
			if(any_error == -1) {
				perror("open_server");
				return -1;
			}
			if(any_error == LACK_OF_PORT) {
				tsk->flush_time = time(NULL);
				return 0;
			}
			if(strncmp(tsk->pkg.method, "CONNECT", 7) == 0) {
				char *msg = "HTTP/1.0 200 Connection established\r\n\r\n";
				if(send_nonb(tsk->pair.client, msg, strlen(msg))) {
					perror("sendto");
					return -1;
				}
				return 0;
			}
			if(strncmp(tsk->pkg.method, "GET", 3) == 0) {
				/* FIXME:重组后的数据报可能比原来的大，但是这里假设    *
				 *	 它不会超过原来的大小，因此没有重新分配c2s_buf */
				tsk->buf_size = pkg_to_buf(tsk->c2s_buf, &tsk->pkg);
			}
			if(send_nonb(tsk->pair.server, tsk->c2s_buf, tsk->buf_size)) {
				perror("sendto");
				return -1;
			}
		}	
		Free(tsk->c2s_buf);
		tsk->buf_size = 0;
	}
	if((ret = recv(tsk->pair.client, buffer, BUFFER_SIZE, MSG_DONTWAIT)) != -1) {
		if(ret == 0)
			return -1;
		if(redirect_flag) {
			int any_error = open_server(tsk);
			if(any_error == -1) {
				perror("open_server");
				send_error( 400, "Bad Request", "No request found or request too long.", tsk->pair.client);
				return -1;
			}
			if(any_error == LACK_OF_PORT) {
				tsk->flush_time = time(NULL);
				return 0;
			}
		} else if(http_proxy_flag) {
			destroy_pkg(&tsk->pkg);
			if(parse_initial_request(buffer, ret, &tsk->pkg)) {
				if(tsk->pair.server == -1) {
					send_error( 400, "Bad Request", "No request found or request too long.", tsk->pair.client);
					return -1;
				}
			} else {
				int any_error = open_server(tsk);
				if(any_error == -1) {
					perror("open_server");
					return -1;
				}
				if(any_error == LACK_OF_PORT) {
					Malloc(tsk->c2s_buf, ret);
					memcpy(tsk->c2s_buf, buffer, ret);
					tsk->buf_size = ret;
					tsk->flush_time = time(NULL);
					return 0;
				}
				if(strncmp(tsk->pkg.method, "CONNECT", 7) == 0) {
					char *msg = "HTTP/1.0 200 Connection established\r\n\r\n";
					if(send_nonb(tsk->pair.client, msg, strlen(msg))) {
						perror("sentto");
						return -1;
					}
					return 0;
				}
//FOR TEST
				if(strncmp(tsk->pkg.method, "GET", 3) == 0) {
					/* FIXME:这里假设重组后的数据包大小不 *
					 *	 超过原来的大小---BUFFER_SIZE */
					ret = pkg_to_buf(buffer, &tsk->pkg);
				}
			}
		} else if(socks5_proxy_flag) {
			if(tsk->pair.server == -1) {
				switch(tsk->act) {
					case SELECT_METHOD:
						if(select_method(tsk, buffer))
							return -1;
						break;
					case CONNECT_SERVER:
						if(parse_command(tsk, buffer))
							return -1;
						break;
				}
				tsk->flush_time = time(NULL);
				return 0;
			} 
		}				
		if(send_nonb(tsk->pair.server, buffer, ret)) {
			perror("sendto");
			return -1;
		}
		tsk->flush_time = time(NULL);
	}
	if(tsk->pair.server <= 0) {
		return 0;
	}
	if((ret = recv(tsk->pair.server, buffer, BUFFER_SIZE, MSG_DONTWAIT)) != -1) {
		if(ret == 0) {
			return -1;
		}
		if(send_nonb(tsk->pair.client, buffer, ret)) {
			perror("sendto");
			return -1;
		}
		tsk->flush_time = time(NULL);
	} else {
		if(errno != EAGAIN) {
			perror("recv");
			return -1;
		}
	}
	//XXX:防止cpu占用率过高
	usleep(10);
	return 0;
}
