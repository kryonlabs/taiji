#include <errno.h>
#include <sys/epoll.h>

int
epoll_create(int)
{
	errno = ENOSYS;
	return -1;
}

int
epoll_create1(int)
{
	errno = ENOSYS;
	return -1;
}

int
epoll_ctl(int, int, int, struct epoll_event *)
{
	errno = ENOSYS;
	return -1;
}

int
epoll_wait(int, struct epoll_event *, int, int)
{
	errno = ENOSYS;
	return -1;
}
