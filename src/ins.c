#include "ins.h"
#include "config.h"
#include "debug.h"
#include "unix.h"
#include "proxy_config.h"
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

static struct instance *ins_list = NULL;
static int		shm_id = 0;

void
close_instance(int index)
{
        if(ins_list[index].sock != 0) {
                close(ins_list[index].sock);
                ins_list[index].sock = 0;
        }
}


void
instance_destroy()
{
	for(int i = 0; i < INSTANCE_NUM; i++) {
		close_instance(i);
		kill(ins_list[i].pid, SIGTERM);
	}
	shmctl(shm_id, IPC_RMID, NULL);
	
}

static void
execute_instance(int listen_sock)
{
	dup2(listen_sock, STDIN_FILENO);
	close(listen_sock);
	if(redirect_flag) {
		if(execl(INSTANCE_PATH, "instance", "-r", redirect_host, redirect_port, NULL)) {
			perror("execl");
			return;
		}
	} else if(http_proxy_flag) {
		if(execl(INSTANCE_PATH, "instance", "-h", NULL)) {
			perror("execl");
			return;
		}
	} else if(socks5_proxy_flag) {
		if(execl(INSTANCE_PATH, "instance", "-s", NULL)) {
			perror("execl");
			return;
		}
	}
} 	
		
void
add_instance(int index)
{
        pid_t pid;
        int sockets[2];
        close_instance(index);
        if(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets)) {
                perror("socketpair");
                kill(getpid(), SIGINT);
        }
        pid = fork();
        switch(pid) {
                case 0:
			execute_instance(sockets[1]);
                        break;
                case -1:
                        perror("fork");
                        kill(getpid(), SIGTERM);
                        break;
                default:
                        break;
        }
        close(sockets[1]);
        ins_list[index].pid = pid;
        ins_list[index].sock = sockets[0];
        ins_list[index].out = 0;
        ins_list[index].in = 0;
	printf("new instance pid %d, index = %d\n", pid, index);
}

void
instance_init(int child_flag)
{
	int key = ftok(INSTANCE_PATH, redirect_flag);
	assert(key != -1);
	if(child_flag) {
		shm_id = shmget(key, 0, 0);
	} else {
		shm_id = shmget(key, sizeof(struct instance) * INSTANCE_NUM, IPC_CREAT | 0666);
	}
	assert(shm_id != -1);
	ins_list = (struct instance *)shmat(shm_id, NULL, 0);
	if(child_flag)
		return;
	memset(ins_list, 0, sizeof(struct instance) * INSTANCE_NUM);
	for(int i = 0; i < INSTANCE_NUM; i++)
                add_instance(i);
}

void
dump_instance_stat()
{
	system("clear");
	printf("INDEX\tPID\tCLIENT\tSERVER\n");
	for(int i = 0; i < INSTANCE_NUM; i++)
		printf("%d\t%d\t%d\t%d\n", i, ins_list[i].pid, ins_list[i].in, ins_list[i].out);
}

void
push_to_instance(int sock)
{
        int min_load_index = 0;
//	dump_instance_stat();
        for(int i = 0; i < INSTANCE_NUM; i++) {
                if(ins_list[i].in < ins_list[min_load_index].in)
                        min_load_index = i;
        }
        if(send_fd(ins_list[min_load_index].sock, sock) < 0)
                perror("unix_send");
}

int
find_instance(pid_t pid)
{
        for(int i = 0; i < INSTANCE_NUM; i++) {
                if(ins_list[i].pid == pid)
                        return i;
        }
        return -1;
}

void 
set_client_num(int client_num)
{
	int index;
	if((index = find_instance(getpid())) == -1)
		exit(-1);
	ins_list[index].in = client_num;
}

void
set_server_num(int server_num)
{
	int index;
	if((index = find_instance(getpid())) == -1)
		exit(-1);
	ins_list[index].out = server_num;
}
