#ifndef DEBUG_H
#define DEBUG_H

#include <pthread.h>

#ifdef NDEBUG
#define D
#define R
#define S
#define M(ptr)
#define F(ptr)
#else
#define D fprintf(stderr, "file:%s, line:%d, thread:%ld, pid:%d\n", __FILE__, __LINE__, pthread_self(), getpid());
#define R debug_record(pthread_self(), __FILE__, __LINE__);
#define S debug_stat();
#define M(ptr) debug_mem_new(__FILE__, __LINE__, ptr);
#define F(ptr) debug_mem_free(__FILE__, __LINE__, ptr);
#endif

struct debug_t {
	pthread_t key;
	char *file;
	int line;
};

extern struct debug_t *d_list;

void debug_init(int);
void debug_record(pthread_t, char *, int);
void debug_stat(void);
void debug_mem_new(char *, int, void *);
void debug_mem_free(char *, int, void *);

#endif
