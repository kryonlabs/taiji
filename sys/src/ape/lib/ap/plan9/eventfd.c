#include <errno.h>
#include <sys/eventfd.h>

int
eventfd(unsigned int, int)
{
	errno = ENOSYS;
	return -1;
}

int
eventfd_read(int, eventfd_t *)
{
	errno = ENOSYS;
	return -1;
}

int
eventfd_write(int, eventfd_t)
{
	errno = ENOSYS;
	return -1;
}
