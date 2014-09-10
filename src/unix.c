#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "debug.h"
#include "unix.h"

int 
send_fd(int fd, int sendfd)
{
	struct msghdr msg;
	struct iovec iov[1];
#if defined(CMSG_SPACE) && !defined(NO_MSGHDR_MSG_CONTROL)
	union {
		struct cmsghdr just_for_alignment;
		char control[CMSG_SPACE(sizeof(sendfd))];
	} control_un;
	struct cmsghdr *cmptr;

	memset(&msg, 0, sizeof(msg)); 

	msg.msg_control = control_un.control;
	msg.msg_controllen = CMSG_LEN(sizeof(sendfd));
	cmptr = CMSG_FIRSTHDR(&msg);
	cmptr->cmsg_len = CMSG_LEN(sizeof(sendfd));
	cmptr->cmsg_level = SOL_SOCKET;
	cmptr->cmsg_type = SCM_RIGHTS;
	*(int *) CMSG_DATA(cmptr) = sendfd;
#else
	msg.msg_accrights = (char *) &sendfd;
	msg.msg_accrightslen = sizeof(sendfd);
#endif

	msg.msg_name = 0;
	msg.msg_namelen = 0;

       /*
	* XXX We don't want to pass any data, just a file descriptor. However,
	* setting msg.msg_iov = 0 and msg.msg_iovlen = 0 causes trouble. See the
	* comments in the unix_recv_fd() routine.
	*/
	iov->iov_base = (void *)"";
	iov->iov_len = 1;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	return sendmsg(fd, &msg, 0);
}

int 
recv_fd(int fd)
{
	struct msghdr msg;
	int newfd;
	struct iovec iov[1];
//XXX
	char buf[1];
#if defined(CMSG_SPACE) && !defined(NO_MSGHDR_MSG_CONTROL)
	union {
		struct cmsghdr just_for_alignment;
		char control[CMSG_SPACE(sizeof(newfd))];
	} control_un;
	struct cmsghdr *cmptr;

	memset(&msg, 0, sizeof(msg)); /* Fix 200512 */
	msg.msg_control = control_un.control;
	msg.msg_controllen = CMSG_LEN(sizeof(newfd)); /* Fix 200506 */
#else
	msg.msg_accrights = (char *) &newfd;
	msg.msg_accrightslen = sizeof(newfd);
#endif

	msg.msg_name = 0;
	msg.msg_namelen = 0;
/*
* XXX We don't want to pass any data, just a file descriptor. However,
* setting msg.msg_iov = 0 and msg.msg_iovlen = 0 causes trouble: we need
* to read_wait() before we can receive the descriptor, and the code
* fails after the first descriptor when we attempt to receive a sequence
* of descriptors.
*/
	iov->iov_base = buf;
	iov->iov_len = 1;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	if(recvmsg(fd, &msg, 0) == -1)
		return -1;
#if defined(CMSG_SPACE) && !defined(NO_MSGHDR_MSG_CONTROL)
	cmptr = CMSG_FIRSTHDR(&msg); 
	if(cmptr != NULL && cmptr->cmsg_level == SOL_SOCKET && cmptr->cmsg_type == SCM_RIGHTS)
		return (*(int *) CMSG_DATA(cmptr));
	return -1;
#else
	if(msg.msg_accrightslen == sizeof(newfd))
		return newfd;
#endif
}
