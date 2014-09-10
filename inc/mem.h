#ifndef MEM_H
#define MEM_H

#define Malloc(ptr, size)		\
do {					\
	if(ptr != NULL)			\
		free(ptr);		\
	ptr = malloc(size);		\
	if(ptr == NULL) {		\
		perror("malloc");	\
		exit(1);		\
	}				\
	memset(ptr, 0, size);		\
} while(0)

#define Free(ptr)			\
do {					\
	if(ptr != NULL)	{		\
		free(ptr);		\
		ptr = NULL;		\
	}				\
} while(0)

#endif
