#include "core.h"
#include "task.h"
#include "server.h"
#include "config.h"
#include "debug.h"
#include "mem.h"
#include "ins.h"
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

pthread_cond_t cnd;
static pthread_mutex_t mtx;
static struct task *task_head = NULL;
static int client_num = 0;

int 
get_client_num()
{
	return client_num;
}
	
static struct task *
take_first_free_task()
{
	struct list_node *node;
	pthread_mutex_lock(&mtx);
	while(task_head == NULL)
		pthread_cond_wait(&cnd, &mtx);
	node = &task_head->node;
	task_head = container_of(node->next, struct task, node);
	while(1) {
		if(container_of(node, struct task, node)->serv_flag == 0) {
			container_of(node, struct task, node)->serv_flag = 1;
			pthread_mutex_unlock(&mtx);
			return container_of(node, struct task, node);
		} 
		node = node->next;
		if(node == task_head->node.prev) {
			pthread_cond_wait(&cnd, &mtx);
			node = &task_head->node;
			continue;
		}
	}
}

static void
put_task(struct task *tsk)
{
	pthread_mutex_lock(&mtx);
	tsk->serv_flag = 0;
	pthread_mutex_unlock(&mtx);
}
	
void *
task_scanner(void *omit)
{
	struct task *list;
	while(1) {
		list = take_first_free_task();
		int ret = proxy_session(list);
		if(time(NULL) - list->flush_time > ALIVE_TIME || ret == -1) {
			del_task(list);
		} else {
			put_task(list);
		}
	}
	return NULL;
}

void
task_init()
{
	pthread_t th;
	pthread_cond_init(&cnd, NULL);
	pthread_mutex_init(&mtx, NULL);
	server_init();
	for(int i = 0; i < MAX_TASK_THREAD; i++) {
		pthread_create(&th, NULL, task_scanner, NULL);
		pthread_detach(th);
	}
#ifndef NDEBUG
	debug_init(MAX_TASK_THREAD);
#endif
}

void
add_task(int clientfd)
{
//S
	struct task *new = NULL;
	Malloc(new, sizeof(struct task));
	assert(new != NULL);
	memset(new, 0, sizeof(struct task));
	new->pair.client = clientfd;
	new->pair.server = -1;
	new->flush_time = time(NULL);
	new->serv_flag = 0;
	pthread_mutex_lock(&mtx);
	if(task_head == NULL) {
		list_add(&new->node, NULL);
		task_head = new;
	} else {
		list_add(&new->node, task_head->node.prev);
	}
	client_num++;
	pthread_cond_signal(&cnd);
	pthread_mutex_unlock(&mtx);
	set_client_num(client_num);
}

int
del_task(struct task *tsk)
{
	pthread_mutex_lock(&mtx);
	if(tsk == NULL) {
		pthread_mutex_unlock(&mtx);
		return -1;
	}
	assert(task_head != NULL);
	if(tsk == task_head)
		task_head = container_of(task_head->node.next, struct task, node);
	list_del(&tsk->node);
	if(task_head->node.prev == LIST_POISON1 && task_head->node.next == LIST_POISON2)
		task_head = NULL;
	destroy_pkg(&tsk->pkg);
	Free(tsk->c2s_buf);	
	if(tsk->pair.server != -1)
		close_server(tsk);
	close(tsk->pair.client);
	Free(tsk);
	client_num--;
	pthread_mutex_unlock(&mtx);
	set_client_num(client_num);
	return 0;

}	

void
dump_task_list()
{
	pthread_mutex_lock(&mtx);
	struct task *list = task_head;
	printf("cur time:%ld\n", time(NULL));
	printf("dump_task_list:\n");
	for(;;) {
		if(list == NULL)
			break;
		printf("\tserver=%d\tclient=%d\tflush_time=%d\tserv_flag=%d\n", 
			list->pair.server, list->pair.client, list->flush_time, list->serv_flag);
		list = container_of(list->node.next, struct task, node);
		if(list == task_head)
			break;
	}
	pthread_mutex_unlock(&mtx);
}
