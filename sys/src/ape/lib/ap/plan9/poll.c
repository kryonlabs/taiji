#include <errno.h>
#include <poll.h>
#include <sys/limits.h>
#include <sys/select.h>
#include <sys/time.h>

int
poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	fd_set rfds, wfds, efds;
	struct timeval tv, *tvp;
	nfds_t i;
	int maxfd, n, ready;

	if(fds == 0 && nfds != 0){
		errno = EFAULT;
		return -1;
	}
	if(nfds > OPEN_MAX){
		errno = EINVAL;
		return -1;
	}

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds);
	maxfd = -1;
	ready = 0;
	for(i = 0; i < nfds; i++){
		fds[i].revents = 0;
		if(fds[i].fd < 0)
			continue;
		if(fds[i].fd >= OPEN_MAX){
			fds[i].revents = POLLNVAL;
			ready++;
			continue;
		}
		if(fds[i].events & (POLLIN|POLLRDNORM|POLLPRI|POLLRDBAND))
			FD_SET(fds[i].fd, &rfds);
		if(fds[i].events & (POLLOUT|POLLWRNORM|POLLWRBAND))
			FD_SET(fds[i].fd, &wfds);
		FD_SET(fds[i].fd, &efds);
		if(fds[i].fd > maxfd)
			maxfd = fds[i].fd;
	}
	if(ready)
		return ready;

	if(timeout < 0)
		tvp = 0;
	else{
		tv.tv_sec = timeout/1000;
		tv.tv_usec = (timeout%1000)*1000;
		tvp = &tv;
	}

	n = select(maxfd+1, &rfds, &wfds, &efds, tvp);
	if(n < 0)
		return -1;

	ready = 0;
	for(i = 0; i < nfds; i++){
		if(fds[i].fd < 0)
			continue;
		if(FD_ISSET(fds[i].fd, &rfds))
			fds[i].revents |= POLLIN;
		if(FD_ISSET(fds[i].fd, &wfds))
			fds[i].revents |= POLLOUT;
		if(FD_ISSET(fds[i].fd, &efds))
			fds[i].revents |= POLLERR;
		if(fds[i].revents)
			ready++;
	}
	return ready;
}
