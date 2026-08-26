#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

int
main(void)
{
	struct timespec ts, nap;
	struct pollfd pfd;
	struct termios tio;
	struct winsize ws;
	void *p;

	memset(&tio, 0, sizeof tio);
	tio.c_iflag = ISTRIP|ICRNL|IXON|IXOFF;
	tio.c_oflag = OPOST|ONLCR;
	tio.c_cflag = CS7;
	tio.c_lflag = ICANON|ECHO;
	if(clock_gettime(CLOCK_REALTIME, &ts) < 0){
		perror("clock_gettime");
		return 1;
	}
	nap.tv_sec = 0;
	nap.tv_nsec = 1000000;
	if(nanosleep(&nap, 0) < 0){
		perror("nanosleep");
		return 1;
	}
	pfd.fd = -1;
	pfd.events = POLLIN;
	if(poll(&pfd, 1, 0) < 0){
		perror("poll");
		return 1;
	}
	if(tcgetattr(0, &tio) == 0)
		(void)ioctl(0, TIOCGWINSZ, &ws);
	cfmakeraw(&tio);
	if((tio.c_lflag & (ICANON|ECHO)) != 0){
		fprintf(stderr, "cfmakeraw left canonical or echo enabled\n");
		return 1;
	}
	p = mmap(0, 4096, PROT_READ, MAP_PRIVATE, -1, 0);
	if(p != MAP_FAILED || errno != ENOSYS){
		fprintf(stderr, "mmap did not fail with ENOSYS\n");
		return 1;
	}
	if(epoll_create(1) != -1 || errno != ENOSYS){
		fprintf(stderr, "epoll did not fail with ENOSYS\n");
		return 1;
	}
	if(inotify_init() != -1 || errno != ENOSYS){
		fprintf(stderr, "inotify did not fail with ENOSYS\n");
		return 1;
	}
	puts("taiji-posix smoke ok");
	return 0;
}
