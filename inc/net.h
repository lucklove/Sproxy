#ifndef NET_H
#define NET_H

#include <netinet/in.h>

int conn_nonb(int, const struct sockaddr *, int);
int open_client_socket(const char *, unsigned short);

#endif
