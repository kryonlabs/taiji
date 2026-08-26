#ifndef __SYS_INOTIFY_H
#define __SYS_INOTIFY_H
#pragma lib "/$M/lib/ape/libap.a"

#ifdef __cplusplus
extern "C" {
#endif

#define IN_ACCESS		0x00000001
#define IN_MODIFY		0x00000002
#define IN_ATTRIB		0x00000004
#define IN_CLOSE_WRITE		0x00000008
#define IN_CLOSE_NOWRITE	0x00000010
#define IN_OPEN			0x00000020
#define IN_MOVED_FROM		0x00000040
#define IN_MOVED_TO		0x00000080
#define IN_CREATE		0x00000100
#define IN_DELETE		0x00000200
#define IN_DELETE_SELF		0x00000400
#define IN_MOVE_SELF		0x00000800

struct inotify_event {
	int		wd;
	unsigned int	mask;
	unsigned int	cookie;
	unsigned int	len;
	char		name[1];
};

extern int inotify_init(void);
extern int inotify_init1(int);
extern int inotify_add_watch(int, const char *, unsigned int);
extern int inotify_rm_watch(int, int);

#ifdef __cplusplus
}
#endif

#endif
