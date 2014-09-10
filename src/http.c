#include "task.h"
#include "config.h"
#include "debug.h"
#include "mem.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

static inline int 
parse_request_head(char *buffer, char *method, char *url, char *protocol)
{
        char tmp_buf[BUFFER_SIZE] = { 0 };
        char *end_of_line;
        memcpy(tmp_buf, buffer, BUFFER_SIZE - 1); 
        end_of_line = strchr(tmp_buf, '\n');
        if(end_of_line == NULL)
                return -1; 
        *end_of_line = '\0';
        if(sscanf(tmp_buf, "%[^ ] %[^ ] %[^ ]", method, url, protocol ) != 3)
                return -1;
	int pro_len = strlen(protocol);
	for(int i = 0; i < pro_len; i++) {
		if(protocol[i] == '\r' || protocol[i] == '\n') {
			protocol[i] = '\0';
			break;
		}
	}
        return 0;
}

static inline int
get_line_len(char *buffer, int buf_size)
{
	char *str = buffer;
	while(*str != '\n' && str < buffer + buf_size)
		str++;
	if(str == buffer + buf_size)
		return -1;
	return str - buffer + 1;
}

void
destroy_pkg(struct http_pkg *pkg)
{
	if(pkg->mem != NULL) {
		Free(pkg->mem);
		pkg->mem = NULL;
	}
	if(pkg->head != NULL) {
		struct head_node *node = pkg->head;
		do {
			Free(node->key);
			Free(node->val);
			node = container_of(node->node.next, struct head_node, node);
			//NOT Free()! no zuo no die
			free(container_of(node->node.prev, struct head_node, node));
		} while(node != pkg->head);
		pkg->head = NULL;
	}
	if(pkg->data != NULL) {
		Free(pkg->data);
		pkg->data = NULL;
		pkg->data_len = 0;
	}
}

static inline int
get_head_node(char **key, char **val, char *buffer, int buf_size)
{
	int len = get_line_len(buffer, buf_size);
	if(len == -1)
		return -1;
	if(buffer[len-2] == '\r') {
		buffer[len-2] = '\0';
	} else {
		buffer[len-1] = '\0';
	}
	char *con = strchr(buffer, ':');
	if(con == NULL) {
		if(buffer[len-2] == '\0') {
			buffer[len-2] = '\r';
		} else {
			buffer[len-1] = '\n';
		}
		return -1;
	}
	*con = '\0';
	*key = strdup(buffer);
	assert(key != NULL);
	*val = strdup(con + 1);
	assert(val != NULL);
	*con = ':';
	if(buffer[len-2] == '\0') {
		buffer[len-2] = '\r';
	} else {
		buffer[len-1] = '\n';
	}
	return len;
}

char **
get_header_val(struct http_pkg *pkg, char *key)
{
	struct head_node *node = pkg->head;
	if(node == NULL)
		return NULL;
	do {
		if(strncasecmp(node->key, key, strlen(key))) {
			node = container_of(node->node.next, struct head_node, node);
			continue;
		}
		return &node->val;
	} while(node != pkg->head);
	return NULL;
}

void
add_header(struct http_pkg *pkg, char *key, char *val)
{
	struct head_node *node = NULL;
	Malloc(node, sizeof(struct head_node));
	node->key = key;
	node->val = val;
	if(pkg->head == NULL) {
		list_add(&node->node, NULL);
		pkg->head = node;
	} else {
		list_add(&node->node, pkg->head->node.prev);
	}
}

void
set_header(struct http_pkg *pkg, char *key, char *val)
{
	char **old_val = get_header_val(pkg, key);
	if(old_val == NULL) {
		add_header(pkg, key, *old_val);
	} else {
		Free(*old_val);
		*old_val = val;	
	}
}

int 
parse_initial_request(char *buffer, int buf_size, struct http_pkg *pkg){
	int index = 0;
	int linelen = get_line_len(buffer, buf_size);
	destroy_pkg(pkg);
	if(linelen == -1)
		return -1;
	index += linelen;
	memset(pkg, 0, sizeof(*pkg));
	Malloc(pkg->mem, 5 * linelen);
	pkg->method = pkg->mem;
	pkg->url = pkg->method + linelen;
	pkg->protocol = pkg->url + linelen;
	pkg->host = pkg->protocol + linelen;
	pkg->path = pkg->host + linelen;
	if(parse_request_head(buffer, pkg->method, pkg->url, pkg->protocol))
		return -1;
	if(strncasecmp(pkg->url, "http://", 7) == 0) {
                strncpy( pkg->url, "http", 4 ); /* make sure it's lower case */
                if ( sscanf( pkg->url, "http://%[^:/]:%hu%s", pkg->host, &pkg->port, pkg->path ) == 3 );
                else if ( sscanf( pkg->url, "http://%[^/]%s", pkg->host, pkg->path ) == 2 )
                {
                        pkg->port = 80;
                }
                else if ( sscanf( pkg->url, "http://%[^:/]:%hu", pkg->host, &pkg->port ) == 2 )
                {
                        pkg->path=pkg->url;
                }
                else if ( sscanf( pkg->url, "http://%[^/]", pkg->host ) == 1 )
                {
                	pkg->port = 80;
                        pkg->path = pkg->url;
                }
                else
			return -1;
      } else if ( strcmp( pkg->method, "CONNECT" ) == 0 ) {
                if ( sscanf( pkg->url, "%[^:]:%hu", pkg->host, &pkg->port ) == 2 ) ;
                else if ( sscanf( pkg->url, "%s", pkg->host ) == 1 )//equivalent to strcpy until first whitespace?
                    pkg->port = 443;
                else
			return -1;
        } else {
                return -1;
	}
	buffer[BUFFER_SIZE-1] = '\0';
	// FIXME:如果buffer中没有包含完整的报头(报头太大), 这里将无法正常工作
	char *end_of_header = strstr(buffer, "\r\n\r\n");
	if(end_of_header == NULL)
		end_of_header = strstr(buffer, "\n\n");
	if(end_of_header == NULL)
		return -1;
	while(buffer + index < end_of_header) {	
		char *key = NULL, *val = NULL;
		int len = get_head_node(&key, &val, buffer + index, buf_size - index);
		if(len == -1) {
			Free(key);
			Free(val);
			return -1;
		}
		add_header(pkg, key, val);
		index += len;
	}
	char **val_ptr = get_header_val(pkg, "content-length");
	if(val_ptr != NULL) {
		char *content_length = *val_ptr;
		int l;
		if(!sscanf(content_length, "%d", &l))
			return -1;
		while(*end_of_header == '\n' || *end_of_header == '\r')
			end_of_header++;
		Malloc(pkg->data, l);
		memcpy(pkg->data, end_of_header, l);
		pkg->data_len = l;
	}		
        return 0;
}

int
pkg_to_buf(char *buffer, struct http_pkg *pkg)
{
	int index = 0;
	if(pkg->mem == NULL)
		return -1;
	index += sprintf(buffer + index, "%s %s %s\r\n", pkg->method, pkg->path, pkg->protocol);
	if(pkg->head != NULL) {
		struct head_node *node = pkg->head;
		do {
			index += sprintf(buffer + index, "%s:%s\r\n", node->key, node->val);
			node = container_of(node->node.next, struct head_node, node);
		} while(node != pkg->head);
		pkg->head = NULL;
	}
	index += sprintf(buffer + index, "\r\n");
	if(pkg->data != NULL) {
		memcpy(buffer + index, pkg->data, pkg->data_len);
		index += pkg->data_len;
	}
	return index;
}
	
void
dump_pkg(struct http_pkg *pkg)
{
	if(pkg->mem != NULL) {
		printf("method:%s\n", pkg->method);
		printf("host:%s\n", pkg->host);
		printf("proto:%s\n", pkg->protocol);
		printf("url:%s\n", pkg->url);
		printf("path:%s\n", pkg->path);
		printf("port:%d\n", pkg->port);
	}
	if(pkg->head != NULL) {
		struct head_node *node = pkg->head;
		do {
			printf("%s:%s\r\n", node->key, node->val);
			node = container_of(node->node.next, struct head_node, node);
		} while(node != pkg->head);
	}
	if(pkg->data != NULL) {
		write(fileno(stdout), pkg->data, pkg->data_len);
	}
}
