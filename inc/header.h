#ifndef HEADER_H
#define HEADER_H

#include "list.h"

struct head_node {
	char *key;
	char *val;
	struct list_node node;
};

struct http_pkg {
        char *mem;    
        char *method, *url, *protocol, *host, *path;
        short int port;
	struct head_node *head;
	char *data;
	int data_len;
};

int parse_initial_request(char *, int, struct http_pkg *);
void dump_pkg(struct http_pkg *);
void destroy_pkg(struct http_pkg *);
void set_header(struct http_pkg *, char *, char *);
int pkg_to_buf(char *, struct http_pkg *);

#endif
