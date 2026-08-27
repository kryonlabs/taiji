#include <errno.h>
#include <sys/signalfd.h>

int
signalfd(int, const sigset_t *, int)
{
	errno = ENOSYS;
	return -1;
}
