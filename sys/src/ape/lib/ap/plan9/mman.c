#include <errno.h>
#include <sys/mman.h>

void *
mmap(void *, size_t, int, int, int, off_t)
{
	errno = ENOSYS;
	return MAP_FAILED;
}

int
munmap(void *, size_t)
{
	errno = ENOSYS;
	return -1;
}

int
mprotect(void *, size_t, int)
{
	errno = ENOSYS;
	return -1;
}

int
msync(void *, size_t, int)
{
	errno = ENOSYS;
	return -1;
}
