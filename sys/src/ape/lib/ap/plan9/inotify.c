#include <errno.h>
#include <sys/inotify.h>

int
inotify_init(void)
{
	errno = ENOSYS;
	return -1;
}

int
inotify_init1(int)
{
	errno = ENOSYS;
	return -1;
}

int
inotify_add_watch(int, const char *, unsigned int)
{
	errno = ENOSYS;
	return -1;
}

int
inotify_rm_watch(int, int)
{
	errno = ENOSYS;
	return -1;
}
