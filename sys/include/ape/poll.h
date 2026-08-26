#ifndef __POLL_H
#define __POLL_H
#pragma lib "/$M/lib/ape/libap.a"

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long nfds_t;

struct pollfd {
	int	fd;
	short	events;
	short	revents;
};

#define POLLIN		0x0001
#define POLLPRI		0x0002
#define POLLOUT		0x0004
#define POLLERR		0x0008
#define POLLHUP		0x0010
#define POLLNVAL	0x0020

#define POLLRDNORM	POLLIN
#define POLLRDBAND	POLLPRI
#define POLLWRNORM	POLLOUT
#define POLLWRBAND	POLLOUT

extern int poll(struct pollfd *, nfds_t, int);

#ifdef __cplusplus
}
#endif

#endif
