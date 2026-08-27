#ifndef __SYS_TIMERFD_H
#define __SYS_TIMERFD_H
#pragma lib "/$M/lib/ape/libap.a"

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TFD_CLOEXEC	02000000
#define TFD_NONBLOCK	00004000
#define TFD_TIMER_ABSTIME	1

struct itimerspec {
	struct timespec	it_interval;
	struct timespec	it_value;
};

extern int timerfd_create(clockid_t, int);
extern int timerfd_settime(int, int, const struct itimerspec *, struct itimerspec *);
extern int timerfd_gettime(int, struct itimerspec *);

#ifdef __cplusplus
}
#endif

#endif
