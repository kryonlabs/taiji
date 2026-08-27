#include <errno.h>
#include <sys/timerfd.h>

int
timerfd_create(clockid_t, int)
{
	errno = ENOSYS;
	return -1;
}

int
timerfd_settime(int, int, const struct itimerspec *, struct itimerspec *)
{
	errno = ENOSYS;
	return -1;
}

int
timerfd_gettime(int, struct itimerspec *)
{
	errno = ENOSYS;
	return -1;
}
