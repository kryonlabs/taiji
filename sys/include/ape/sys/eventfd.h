#ifndef __SYS_EVENTFD_H
#define __SYS_EVENTFD_H
#pragma lib "/$M/lib/ape/libap.a"

#ifdef __cplusplus
extern "C" {
#endif

#define EFD_SEMAPHORE	00000001
#define EFD_CLOEXEC	02000000
#define EFD_NONBLOCK	00004000

typedef unsigned long long eventfd_t;

extern int eventfd(unsigned int, int);
extern int eventfd_read(int, eventfd_t *);
extern int eventfd_write(int, eventfd_t);

#ifdef __cplusplus
}
#endif

#endif
