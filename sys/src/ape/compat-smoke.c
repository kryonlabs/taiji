#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <stdint.h>
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
	struct stat st;
	FILE *fp;
	void *p, *q;
	char *line, *dup;
	size_t cap;
	int fds[2], dirfd, fd;

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
	if(pipe2(fds, O_CLOEXEC|O_NONBLOCK) < 0){
		perror("pipe2");
		return 1;
	}
	if((fcntl(fds[0], F_GETFD) & FD_CLOEXEC) == 0 ||
	    (fcntl(fds[1], F_GETFD) & FD_CLOEXEC) == 0){
		fprintf(stderr, "pipe2 did not set close-on-exec\n");
		return 1;
	}
	close(fds[0]);
	close(fds[1]);
	if(tcgetattr(0, &tio) == 0)
		(void)ioctl(0, TIOCGWINSZ, &ws);
	cfmakeraw(&tio);
	if((tio.c_lflag & (ICANON|ECHO)) != 0){
		fprintf(stderr, "cfmakeraw left canonical or echo enabled\n");
		return 1;
	}
	if(posix_memalign(&p, 16, 32) != 0 || (((unsigned long)p) & 15) != 0){
		fprintf(stderr, "posix_memalign failed\n");
		return 1;
	}
	free(p);
	q = aligned_alloc(16, 32);
	if(q == 0 || (((unsigned long)q) & 15) != 0){
		fprintf(stderr, "aligned_alloc failed\n");
		return 1;
	}
	free(q);
	if(strnlen("abcdef", 3) != 3 || strnlen("ab", 8) != 2){
		fprintf(stderr, "strnlen failed\n");
		return 1;
	}
	dup = strndup("abcdef", 3);
	if(dup == 0 || strcmp(dup, "abc") != 0){
		fprintf(stderr, "strndup failed\n");
		return 1;
	}
	free(dup);
	if(memmem("abcdef", 6, "cd", 2) == 0){
		fprintf(stderr, "memmem failed\n");
		return 1;
	}
	fp = tmpfile();
	if(fp == 0){
		perror("tmpfile");
		return 1;
	}
	fputs("line one\n", fp);
	rewind(fp);
	line = 0;
	cap = 0;
	if(getline(&line, &cap, fp) != 9 || strcmp(line, "line one\n") != 0){
		fprintf(stderr, "getline failed\n");
		return 1;
	}
	free(line);
	fclose(fp);
	(void)unlinkat(AT_FDCWD, "/tmp/taiji-ape-smoke/file", 0);
	(void)unlinkat(AT_FDCWD, "/tmp/taiji-ape-smoke", AT_REMOVEDIR);
	if(mkdirat(AT_FDCWD, "/tmp/taiji-ape-smoke", 0777) < 0){
		perror("mkdirat");
		return 1;
	}
	dirfd = open("/tmp/taiji-ape-smoke", O_RDONLY|O_DIRECTORY);
	if(dirfd < 0){
		perror("open smoke dir");
		return 1;
	}
	fd = openat(dirfd, "file", O_CREAT|O_RDWR|O_CLOEXEC, 0666);
	if(fd < 0){
		perror("openat");
		return 1;
	}
	if((fcntl(fd, F_GETFD) & FD_CLOEXEC) == 0){
		fprintf(stderr, "openat did not set close-on-exec\n");
		return 1;
	}
	close(fd);
	if(fstatat(dirfd, "file", &st, 0) < 0 || faccessat(dirfd, "file", F_OK, 0) < 0){
		perror("fstatat/faccessat");
		return 1;
	}
	if(unlinkat(dirfd, "file", 0) < 0){
		perror("unlinkat file");
		return 1;
	}
	close(dirfd);
	if(unlinkat(AT_FDCWD, "/tmp/taiji-ape-smoke", AT_REMOVEDIR) < 0){
		perror("unlinkat dir");
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
	if(eventfd(0, 0) != -1 || errno != ENOSYS){
		fprintf(stderr, "eventfd did not fail with ENOSYS\n");
		return 1;
	}
	if(timerfd_create(CLOCK_MONOTONIC, 0) != -1 || errno != ENOSYS){
		fprintf(stderr, "timerfd did not fail with ENOSYS\n");
		return 1;
	}
	if(signalfd(-1, 0, 0) != -1 || errno != ENOSYS){
		fprintf(stderr, "signalfd did not fail with ENOSYS\n");
		return 1;
	}
	puts("taiji-posix smoke ok");
	return 0;
}
