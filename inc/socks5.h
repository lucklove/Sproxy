#ifndef SOCKS5_H
#define SOCKS5_H

#include "task.h"

#define VERSION 0x05
#define CONNECT 0x01
#define BIND	0x02
#define ASSOC	0x03
#define IPV4	0x01
#define DOMAIN	0x03
#define IPV6	0x04
#define BAD_CMD	0x07

#define SELECT_METHOD 0
#define CONNECT_SERVER 1

typedef struct {
	char version;
	char select_method;
} Method_select_response;

typedef struct {
	char version;
	char number_methods;
	char methods[255];
} Method_select_request;

typedef struct {
	char version;
	char result;
} Auth_response;

typedef struct {
	char version;
	char name_len;
	char name[255];
	char pwd_len;
	char pwd[255];
} Auth_request;

typedef struct {
	char version;
	char reply;
	char reserved;
	char address_type;
	char address_port[1];
} Socks5_response;

typedef struct {
	char version;
	char cmd;
	char reserved;
	char address_type;
} Socks5_request;

int select_method(struct task *, char *);
int parse_command(struct task *, char *);

#endif
