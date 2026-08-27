#ifndef __SYS_SIGNALFD_H
#define __SYS_SIGNALFD_H
#pragma lib "/$M/lib/ape/libap.a"

#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SFD_CLOEXEC	02000000
#define SFD_NONBLOCK	00004000

struct signalfd_siginfo {
	unsigned int	ssi_signo;
	int		ssi_errno;
	int		ssi_code;
	unsigned int	ssi_pid;
	unsigned int	ssi_uid;
	int		ssi_fd;
	unsigned int	ssi_tid;
	unsigned int	ssi_band;
	unsigned int	ssi_overrun;
	unsigned int	ssi_trapno;
	int		ssi_status;
	int		ssi_int;
	unsigned long long ssi_ptr;
	unsigned long long ssi_utime;
	unsigned long long ssi_stime;
	unsigned long long ssi_addr;
	unsigned char	_pad[48];
};

extern int signalfd(int, const sigset_t *, int);

#ifdef __cplusplus
}
#endif

#endif
