#ifndef __SYS_MMAN_H
#define __SYS_MMAN_H
#pragma lib "/$M/lib/ape/libap.a"

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROT_NONE	0x00
#define PROT_READ	0x01
#define PROT_WRITE	0x02
#define PROT_EXEC	0x04

#define MAP_SHARED	0x0001
#define MAP_PRIVATE	0x0002
#define MAP_FIXED	0x0010
#define MAP_ANON	0x1000
#define MAP_ANONYMOUS	MAP_ANON

#define MS_ASYNC	0x0001
#define MS_SYNC		0x0002
#define MS_INVALIDATE	0x0004

#define MAP_FAILED	((void*)-1)

extern void *mmap(void *, size_t, int, int, int, off_t);
extern int munmap(void *, size_t);
extern int mprotect(void *, size_t, int);
extern int msync(void *, size_t, int);

#ifdef __cplusplus
}
#endif

#endif
