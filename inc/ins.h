#ifndef INS_H
#define INS_H

#include <unistd.h>

#define IM_CHILD 1

struct instance {
        pid_t pid;
        int   sock;
	int   in;
        int   out;
};

void add_instance(int);
void close_instance(int);
void instance_init(int);
void instance_destroy(void);
void push_to_instance(int);
void set_client_num(int);
void set_server_num(int);
int find_instance(int);


#endif
