#ifndef __SYS_EPOLL_H
#define __SYS_EPOLL_H
#pragma lib "/$M/lib/ape/libap.a"

#ifdef __cplusplus
extern "C" {
#endif

#define EPOLLIN		0x001
#define EPOLLPRI	0x002
#define EPOLLOUT	0x004
#define EPOLLERR	0x008
#define EPOLLHUP	0x010
#define EPOLLET		(1U << 31)
#define EPOLL_CTL_ADD	1
#define EPOLL_CTL_DEL	2
#define EPOLL_CTL_MOD	3

typedef union epoll_data {
	void		*ptr;
	int		fd;
	unsigned int	u32;
	unsigned long long u64;
} epoll_data_t;

struct epoll_event {
	unsigned int	events;
	epoll_data_t	data;
};

extern int epoll_create(int);
extern int epoll_create1(int);
extern int epoll_ctl(int, int, int, struct epoll_event *);
extern int epoll_wait(int, struct epoll_event *, int, int);

#ifdef __cplusplus
}
#endif

#endif
