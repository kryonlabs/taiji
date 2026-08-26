#include <errno.h>
#include <time.h>
#include "sys9.h"

typedef unsigned long long uvlong;
typedef long long vlong;
typedef unsigned char uchar;

static uvlong order = 0x0001020304050607ULL;

static void
be2vlong(vlong *to, uchar *f)
{
	uchar *t, *o;
	int i;

	t = (uchar*)to;
	o = (uchar*)&order;
	for(i = 0; i < 8; i++)
		t[o[i]] = f[i];
}

int
clock_gettime(clockid_t clock_id, struct timespec *tp)
{
	uchar b[8];
	vlong t;
	int fd;

	if(tp == 0){
		errno = EFAULT;
		return -1;
	}
	if(clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC){
		errno = EINVAL;
		return -1;
	}
	fd = _OPEN("/dev/bintime", OREAD|OCEXEC);
	if(fd < 0){
		errno = ENOSYS;
		return -1;
	}
	if(_PREAD(fd, b, sizeof b, 0) != sizeof b){
		_CLOSE(fd);
		errno = EIO;
		return -1;
	}
	_CLOSE(fd);
	be2vlong(&t, b);
	tp->tv_sec = t/1000000000LL;
	tp->tv_nsec = t%1000000000LL;
	return 0;
}
