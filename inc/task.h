#ifndef TASK_H
#define TASK_H

#include <stdio.h>
#include <netinet/in.h>
#include "list.h"
#include "header.h"

#define CLIENT_NUM (get_client_num())

struct cs_pair {
	int client;
	int server;
};

/*
 *XXX:c2s_buf在连接服务器的外出端口资源紧张的情况
 *    下被用于存储已经从客户端读取的数据
 */
    
struct task {
	struct cs_pair pair;
	int flush_time;
	int serv_flag;
	struct http_pkg pkg;
	char *c2s_buf;
	int  buf_size;
	int  act;
	struct list_node node;
};

void add_task(int);
int del_task(struct task *);
void task_init(void);
void dump_task_list(void);
int get_client_num(void);

#endif
