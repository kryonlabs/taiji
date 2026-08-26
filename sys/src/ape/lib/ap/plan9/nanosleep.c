#include <errno.h>
#include <time.h>
#include "sys9.h"

int
nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
	long ms;

	if(rqtp == 0 || rqtp->tv_sec < 0 || rqtp->tv_nsec < 0 ||
	    rqtp->tv_nsec >= 1000000000L){
		errno = EINVAL;
		return -1;
	}
	ms = rqtp->tv_sec*1000 + (rqtp->tv_nsec + 999999L)/1000000L;
	if(rmtp != 0){
		rmtp->tv_sec = 0;
		rmtp->tv_nsec = 0;
	}
	if(_SLEEP(ms) < 0){
		errno = EINTR;
		return -1;
	}
	return 0;
}
