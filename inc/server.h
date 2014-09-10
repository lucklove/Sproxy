#ifndef SERVER_H
#define SERVER_H

#include "task.h"

#define LACK_OF_PORT -2

void server_init(void);
void close_server(struct task *);
int get_server_num(void);
int open_server(struct task *);

#define SERVER_NUM (get_server_num())

#endif
