#include "lib.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int
atpath(int dirfd, const char *path, char **out)
{
	char *base, *p;
	int n;

	if(path == 0){
		errno = EFAULT;
		return -1;
	}
	if(path[0] == '/' || dirfd == AT_FDCWD){
		*out = (char*)path;
		return 0;
	}
	if(dirfd < 0 || dirfd >= OPEN_MAX || !(_fdinfo[dirfd].flags&FD_ISOPEN)){
		errno = EBADF;
		return -1;
	}
	base = _fdinfo[dirfd].name;
	if(base == 0){
		errno = ENOSYS;
		return -1;
	}
	n = strlen(base) + 1 + strlen(path) + 1;
	p = malloc(n);
	if(p == 0){
		errno = ENOMEM;
		return -1;
	}
	strcpy(p, base);
	if(p[0] != 0 && p[strlen(p)-1] != '/')
		strcat(p, "/");
	strcat(p, path);
	*out = p;
	return 0;
}

static void
freepath(const char *path, char *p)
{
	if(p != path)
		free(p);
}

int
openat(int dirfd, const char *path, int flags, ...)
{
	char *p;
	int fd, mode;
	va_list va;

	if(atpath(dirfd, path, &p) < 0)
		return -1;
	if(flags&O_CREAT){
		va_start(va, flags);
		mode = va_arg(va, int);
		va_end(va);
		fd = open(p, flags, mode);
	}else
		fd = open(p, flags);
	freepath(path, p);
	return fd;
}

int
mkdirat(int dirfd, const char *path, mode_t mode)
{
	char *p;
	int r;

	if(atpath(dirfd, path, &p) < 0)
		return -1;
	r = mkdir(p, mode);
	freepath(path, p);
	return r;
}

int
fstatat(int dirfd, const char *path, struct stat *st, int flags)
{
	char *p;
	int r;

	if(path == 0){
		errno = EFAULT;
		return -1;
	}
	if((flags&AT_EMPTY_PATH) && path[0] == 0)
		return fstat(dirfd, st);
	if(flags & ~(AT_SYMLINK_NOFOLLOW|AT_EMPTY_PATH)){
		errno = EINVAL;
		return -1;
	}
	if(atpath(dirfd, path, &p) < 0)
		return -1;
	r = stat(p, st);
	freepath(path, p);
	return r;
}

int
unlinkat(int dirfd, const char *path, int flags)
{
	char *p;
	int r;

	if(flags & ~AT_REMOVEDIR){
		errno = EINVAL;
		return -1;
	}
	if(atpath(dirfd, path, &p) < 0)
		return -1;
	r = (flags&AT_REMOVEDIR) ? rmdir(p) : unlink(p);
	freepath(path, p);
	return r;
}

int
faccessat(int dirfd, const char *path, int mode, int flags)
{
	char *p;
	int r;

	if(flags != 0){
		errno = EINVAL;
		return -1;
	}
	if(atpath(dirfd, path, &p) < 0)
		return -1;
	r = access(p, mode);
	freepath(path, p);
	return r;
}
