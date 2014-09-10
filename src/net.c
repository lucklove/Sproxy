#include "net.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int 
conn_nonb(int sockfd, const struct sockaddr *saptr, int salen)  
{  
        struct timeval timeo = { MAX_DELAY, 0 };
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo));
        if(connect(sockfd, saptr, salen)) {
                perror("connect");
                return -1; 
        }
        return 0;  
}  

int
open_client_socket(const char* hostname, unsigned short int Port)
{
        int sockfd;
        struct addrinfo hints, *servinfo, *p;
        int rv;
        char port[8];
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        sprintf(port, "%hd", Port);
        if((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                return -1;
        }

        for(p = servinfo; p != NULL; p = p->ai_next) {
                if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                        perror("socket");
                        continue;
                }

                if(conn_nonb(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                        close(sockfd);
                        perror("conn_nonb");
                        printf("host:%s:%s\n", hostname, port);
                        continue;
                }
                break;
        }
        if (p == NULL) {
                freeaddrinfo(servinfo);
                return -1;
        }
        freeaddrinfo(servinfo);
        return sockfd;
}

