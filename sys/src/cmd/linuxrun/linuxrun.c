#include <u.h>
#include <libc.h>
#include </386/include/ureg.h>
typedef struct Ureg Ureg;

/*
 * linuxrun - execute static Linux i386 binaries on the Plan 9 kernel.
 *
 * The ELF is mapped with segattach("memory") at its program-header
 * addresses (the 386 kernel has no NX, so the pages execute), Linux
 * syscall sites (int $0x80) are rewritten to ud2, and the program is
 * entered by bouncing off a ud2 trap whose note handler installs the
 * initial registers.  Every later ud2 fault is a syscall: the handler
 * emulates it from the Ureg registers and resumes.  Raw-syscall static
 * binaries (no libc, no TLS) are the target; dynamic ELF programs need
 * the interpreter, TLS and futex work that is not built yet.
 */

enum {
	Elfident = 16,
	EiClass = 4,
	EiData = 5,
	Elfclass32 = 1,
	Elfdata2lsb = 1,
	Em386 = 3,
	EtExec = 2,
	EtDyn = 3,
	PtLoad = 1,
	PtInterp = 3,

	Piebase = 0x08000000,	/* where PIE mains land */
	Interpbase = 0x48000000,/* where ld-linux lands */

	Maxph = 64,
	Rungap = 64*1024,	/* merge PT_LOADs closer than this */

	Pgsz = 4096,

	/* fixed arenas: brk is carved from the low end of the map
	 * segment to stay inside the kernel's small per-process
	 * segment-slot budget */
	Brkbase = 0x40000000,
	Brksize = 8*1024*1024,
	Mapbase = 0x40000000,
	Mapsize = 64*1024*1024,
	Stackbase = 0x60000000,
	Stacksize = 128*1024,

	/* Linux errnos, returned negative */
	Enoent = 2,
	Ebadf = 9,
	Enomem = 12,
	Eacces = 13,
	Efault = 14,
	Einval = 22,
	Enotty = 25,
	Enosys = 38,

	/* Linux open flags */
	LoCreat = 0x40,
	LoExcl = 0x80,
	LoTrunc = 0x200,
	LoAppend = 0x400,

	TrapUD = 6,		/* x86 #UD */
};

typedef struct Ehdr Ehdr;
typedef struct Phdr Phdr;
struct Ehdr {
	uchar ident[Elfident];
	ushort type;
	ushort machine;
	ulong entry;
	ulong phoff;
	ushort phentsize;
	ushort phnum;
};
struct Phdr {
	ulong type;
	ulong offset;
	ulong vaddr;
	ulong filesz;
	ulong memsz;
	ulong flags;
	ulong align;
};

struct Liovec {		/* Linux struct iovec */
	void *base;
	ulong len;
};

int verbose;
int analyzeonly;
int started;
ulong entrypc;
ulong mainentry;	/* AT_ENTRY: the main program's entry */
ulong stacktop;
Phdr ph[Maxph];
int nph;
ulong brkcur;
char exitstr[16];
int ldtfd = -1;
ulong phdrva;
ulong randva;
ulong sysinfova;
int phdrmode;	/* 0: phdr array, 1: ehdr, 2: omit */
ulong execfnva;
ulong platformva;
int nphhdrs;
char interppath[256];
ulong interpbase;
int dynamic;

static ulong loadelf(int, Ehdr*, ulong);
static void initsysinfo(void);
static long syssetthreadarea(ulong);

static void
fatal(char *fmt, ...)
{
	char buf[256];
	va_list arg;

	va_start(arg, fmt);
	vseprint(buf, buf+sizeof buf, fmt, arg);
	va_end(arg);
	fprint(2, "linuxrun: %s\n", buf);
	exits("linuxrun");
}

static ushort
le16(uchar *p)
{
	return p[0] | p[1]<<8;
}

static ulong
le32(uchar *p)
{
	return p[0] | (ulong)p[1]<<8 | (ulong)p[2]<<16 | (ulong)p[3]<<24;
}

static void
dumpsegments(void)
{
	char path[64], buf[4096];
	int fd, n;

	snprint(path, sizeof path, "/proc/%d/segment", getpid());
	fd = open(path, OREAD);
	if(fd < 0)
		return;
	n = readn(fd, buf, sizeof buf-1);
	close(fd);
	if(n <= 0)
		return;
	buf[n] = 0;
	fprint(2, "linuxrun: segments:\n%s", buf);
}

static void *
segat(ulong va, ulong len)
{
	void *p;

	p = segattach(0, "memory", (void*)va, len);
	if(p == (void*)-1){
		fprint(2, "linuxrun: segattach %#lux %#lux: %r\n", va, len);
		dumpsegments();
		fatal("cannot attach guest memory");
	}
	return p;
}

static int
readat(int fd, void *buf, long n, vlong off)
{
	if(seek(fd, off, 0) < 0)
		return -1;
	return readn(fd, buf, n);
}

/*
 * Map the PT_LOAD segments at their addresses, rewriting int $0x80
 * (cd 80) into ud2 (0f 0b) in the executable bytes.  Fresh segment
 * pages are zero, so bss needs no extra work.  A byte pair inside
 * some other instruction would be rewritten too; that only matters
 * for binaries that never execute an int 80, which is not a thing we
 * care to run.
 */
static ulong
loadelf(int fd, Ehdr *eh, ulong base)
{
	uchar buf[Maxph*sizeof(Phdr)];
	uchar *seg;
	ulong va, lo, hi, off;
	int i, j, k, patched;

	if(readat(fd, buf, eh->phnum*eh->phentsize, eh->phoff) < 0)
		fatal("read program headers: %r");
	nph = 0;
	for(i = 0; i < eh->phnum; i++){
		ph[nph].type = le32(buf+i*eh->phentsize+0);
		ph[nph].offset = le32(buf+i*eh->phentsize+4);
		ph[nph].vaddr = le32(buf+i*eh->phentsize+8);
		ph[nph].filesz = le32(buf+i*eh->phentsize+16);
		ph[nph].memsz = le32(buf+i*eh->phentsize+20);
		ph[nph].flags = le32(buf+i*eh->phentsize+24);
		ph[nph].align = le32(buf+i*eh->phentsize+28);
		ph[nph].vaddr += base;
		if(ph[nph].type == PtInterp){
			uchar ipath[256];
			int n;

			n = ph[nph].filesz;
			if(n > 255)
				n = 255;
			if(readat(fd, ipath, n, ph[nph].offset) < 0)
				fatal("read interp: %r");
			ipath[n] = 0;
			memmove(interppath, ipath, n+1);
			dynamic = 1;
		}
		nph++;
	}

	patched = 0;
	i = 0;
	while(i < nph){
		/* collect a run of PT_LOADs that fit in one segment
		 * (the kernel allows only a handful of attachable
		 * segments per process) */
		if(ph[i].type != PtLoad || ph[i].memsz == 0){
			i++;
			continue;
		}
		lo = ph[i].vaddr & ~(Pgsz-1);
		hi = ph[i].vaddr + ph[i].memsz;
		for(j = i+1; j < nph; j++){
			if(ph[j].type != PtLoad || ph[j].memsz == 0)
				break;
			if((ph[j].vaddr & ~(Pgsz-1)) - hi > Rungap)
				break;
			hi = ph[j].vaddr + ph[j].memsz;
		}
		seg = segat(lo, ((hi - lo + Pgsz-1) / Pgsz) * Pgsz);
		while(i < j){
			off = ph[i].vaddr - lo;
			if(readat(fd, seg+off, ph[i].filesz, ph[i].offset) < 0)
				fatal("read segment at %#lux: %r", ph[i].offset);
			if(ph[i].flags & 1){	/* PF_X */
				for(k = 0; k+1 < (int)ph[i].filesz; k++){
					uchar *q = seg + off + k;

					if(q[0] == 0xcd && q[1] == 0x80){
						q[0] = 0x0f;
						q[1] = 0x0b;
						patched++;
					}
				}
			}
			if(verbose)
				fprint(2, "linuxrun: load vaddr %#lux filesz %#lux memsz %#lux%s\n",
					ph[i].vaddr, ph[i].filesz, ph[i].memsz,
					(ph[i].flags & 1) ? " x" : "");
			i++;
		}
	}
	if(patched == 0 && !dynamic)
		fprint(2, "linuxrun: warning: no int 80 sites patched\n");
	else if(verbose)
		fprint(2, "linuxrun: patched %d syscall sites\n", patched);
	return eh->entry + base;
}

/*
 * Build the Linux initial stack: the argv0 string near the top, then
 * argc/argv/envp/auxv below it, stack pointer 16-byte aligned.
 */
static ulong
buildstack(int nargs, char **args)
{
	uchar *st;
	ulong sp, strp;
	ulong *vec;
	ulong argv[16];
	int i, naux, nvec;

	if(nargs > 15)
		nargs = 15;
	st = segat(Stackbase, Stacksize);
	strp = Stackbase + Stacksize - 512;
	for(i = nargs-1; i >= 0; i--){
		int l;

		l = strlen(args[i]) + 1;
		strp -= l;
		strcpy((char*)st + (strp - Stackbase), args[i]);
		argv[i] = strp;
	}
	execfnva = argv[0];
	platformva = strp - 8;
	strcpy((char*)st + (platformva - Stackbase), "i686");
	randva = platformva - 32;
	for(i = 0; i < 16; i++)
		st[randva - Stackbase + i] = nsec() >> (i*3);

	/* PHDR PAGESZ PHNUM BASE ENTRY CLKTCK PHENT FLAGS UID EUID
	 * GID EGID HWCAP SECURE SYSINFO RANDOM EXECFN PLATFORM NULL */
	naux = 19;
	nvec = 1 + nargs + 1 + 1 + 2*naux;
	sp = (randva - 16) & ~15UL;
	sp = (sp - nvec*4) & ~15UL;
	vec = (ulong*)(st + (sp - Stackbase));
	vec[0] = nargs;
	for(i = 0; i < nargs; i++)
		vec[1+i] = argv[i];
	vec[1+nargs] = 0;
	vec[2+nargs] = 0;
	i = 3+nargs;
	if(phdrmode != 2){
		vec[i++] = 3; vec[i++] = phdrva;
		vec[i++] = 5; vec[i++] = nphhdrs;
	}
	vec[i++] = 6; vec[i++] = Pgsz;
	vec[i++] = 7; vec[i++] = interpbase;
	vec[i++] = 9; vec[i++] = mainentry;
	vec[i++] = 17; vec[i++] = 100;
	vec[i++] = 4; vec[i++] = 32;
	vec[i++] = 8; vec[i++] = 0;
	vec[i++] = 11; vec[i++] = 0;
	vec[i++] = 12; vec[i++] = 0;
	vec[i++] = 13; vec[i++] = 0;
	vec[i++] = 14; vec[i++] = 0;
	vec[i++] = 16; vec[i++] = 0;
	vec[i++] = 23; vec[i++] = 0;
	vec[i++] = 32; vec[i++] = sysinfova;
	vec[i++] = 25; vec[i++] = randva;
	vec[i++] = 31; vec[i++] = execfnva;
	vec[i++] = 15; vec[i++] = platformva;
	vec[i++] = 0; vec[i++] = 0;
	return sp;
}

/* Linux struct utsname: six 65-byte fields */
static long
sysuname(ulong addr)
{
	char *p;
	static char *fields[6] = {
		"Linux", "taiji", "5.15.0-taiji", "#1 SMP Tue Sep 1 12:00:00 UTC 2026",
		"i686", "(none)"
	};
	int i;

	if(addr == 0)
		return -Efault;
	p = (char*)addr;
	memset(p, 0, 6*65);
	for(i = 0; i < 6; i++)
		strncpy(p+i*65, fields[i], 64);
	return 0;
}

static long
syswritev(ulong fd, ulong iov, ulong cnt)
{
	struct Liovec *v;
	long total, n;
	ulong i;

	v = (struct Liovec*)iov;
	total = 0;
	for(i = 0; i < cnt; i++)
		total += v[i].len;
	for(i = 0; i < cnt; i++){
		if(v[i].len == 0)
			continue;
		n = write((int)fd, v[i].base, v[i].len);
		if(n < 0)
			return -Ebadf;
	}
	return total;
}

static long
sysreadv(ulong fd, ulong iov, ulong cnt)
{
	struct Liovec *v;
	long total, n;
	ulong i;

	v = (struct Liovec*)iov;
	total = 0;
	for(i = 0; i < cnt; i++){
		if(v[i].len == 0)
			continue;
		n = read((int)fd, v[i].base, v[i].len);
		if(n < 0)
			return -Ebadf;
		total += n;
		if(n < (long)v[i].len)
			break;
	}
	return total;
}

static long
sysopen(ulong path, ulong flags, ulong mode)
{
	int pmode, fd;
	char *p;

	USED(mode);
	if(path == 0)
		return -Efault;
	p = (char*)path;
	switch(flags & 3){
	default:
		return -Einval;
	case 0: pmode = OREAD; break;
	case 1: pmode = OWRITE; break;
	case 2: pmode = ORDWR; break;
	}
	if(flags & LoTrunc)
		pmode |= OTRUNC;
	if(flags & LoCreat){
		if(access(p, AEXIST) < 0){
			fd = create(p, pmode, 0666);
			if(fd < 0)
				return -Enoent;
			return fd;
		}
		if(flags & LoExcl)
			return -Enoent;
	}
	fd = open(p, pmode);
	if(fd < 0){
		if(verbose)
			fprint(2, "linuxrun: open %s: %r\n", p);
		return -Enoent;
	}
	return fd;
}

static ulong mapbump = Mapbase + Brksize + Pgsz;

static void
zerorange(ulong va, ulong len)
{
	ulong a;

	for(a = va & ~(Pgsz-1); a < va+len; a += Pgsz)
		memset((void*)a, 0, Pgsz);
}

static long
sysmmap(ulong addr, ulong len, ulong prot, ulong flags, ulong fd, ulong off)
{
	ulong va;

	USED(prot);
	if(verbose)
		fprint(2, "linuxrun: mmap? addr=%#lux len=%#lux flags=%#lux fd=%d off=%#lux\n",
			addr, len, flags, (int)fd, off);
	if(len == 0)
		return -Einval;
	len = ((len + Pgsz-1) / Pgsz) * Pgsz;
	if(flags & 0x10){	/* MAP_FIXED: the caller chose the address */
		if(addr == 0 || addr + len > Mapbase + Mapsize || addr < Mapbase)
			return -Enomem;
		va = addr;
		zerorange(va, len);
	}else{
		if(addr != 0 && addr >= Mapbase && addr + len <= Mapbase + Mapsize)
			va = addr;	/* hint we can honour */
		else{
			if(mapbump + len > Mapbase + Mapsize)
				return -Enomem;
			va = mapbump;
			mapbump += len;
		}
	}
	if(!(flags & 0x20)){	/* not MAP_ANONYMOUS: fill from the file */
		if(fd > 0x10000)
			return -Ebadf;
		if(readat((int)fd, (void*)va, len, off) < 0)
			return -Ebadf;
	}
	if(verbose)
		fprint(2, "linuxrun: mmap addr=%#lux len=%#lux flags=%#lux fd=%d off=%#lux -> %#lux\n",
			addr, len, flags, (int)fd, off, va);
	return va;
}

/* fill an i386 struct stat64 so ld.so accepts our files */
static long
fillstat64(ulong addr, int fd)
{
	uchar *b;
	Dir *d;

	if(addr == 0)
		return 0;
	d = dirfstat(fd);
	if(d == nil)
		return -Ebadf;
	b = (uchar*)addr;
	memset(b, 0, 96);
	*(ulong*)(b+16) = 0x81a4;		/* S_IFREG|0444 */
	*(ulong*)(b+20) = 1;			/* nlink */
	*(ulong*)(b+12) = (ulong)d->qid.path;	/* __st_ino */
	*(vlong*)(b+88) = d->qid.path;	/* st_ino: ld.so dedups libraries
					 * by dev:ino, so every file must
					 * be unique */
	*(vlong*)(b+44) = d->length;		/* st_size */
	if(verbose)
		fprint(2, "linuxrun: fstat64 fd=%d size=%llud\n", fd, d->length);
	*(ulong*)(b+52) = 4096;			/* blksize */
	free(d);
	return 0;
}

/* Linux i386 struct user_desc */
typedef struct Userdesc Userdesc;
struct Userdesc {
	int entry_number;
	ulong base_addr;
	int limit;
	int flags;
};

#define UDSeg32 (1<<0)
#define UDLimpages (1<<4)

/*
 * set_thread_area: install the TLS descriptor in the process' foreign
 * descriptor page (entry 0 of the page = GDT slot TLSSEG, which is
 * selector 0x33 = entry_number 6 - exactly how Linux numbers i386
 * TLS).  glibc loads %gs itself once the entry exists.
 */
static long
syssetthreadarea(ulong udesc)
{
	Userdesc *ud;
	ulong d0, d1, base;
	static uchar desc[8];
	static int inited;

	if(udesc == 0)
		return -Efault;
	ud = (Userdesc*)udesc;
	base = ud->base_addr;
	d0 = (base & 0xFFFF)<<16 | 0xFFFF;
	d1 = (base & 0xFF000000) | ((base>>16) & 0xFF);
	d1 |= 0x8000;		/* present */
	d1 |= 3<<13;		/* DPL 3 */
	d1 |= 0x12<<8;		/* data, writable */
	d1 |= 1<<23;		/* granularity 4k */
	d1 |= 1<<22;		/* 32-bit */
	desc[0] = d0 & 0xFF;
	desc[1] = (d0>>8) & 0xFF;
	desc[2] = (d0>>16) & 0xFF;
	desc[3] = (d0>>24) & 0xFF;
	desc[4] = d1 & 0xFF;
	desc[5] = (d1>>8) & 0xFF;
	desc[6] = (d1>>16) & 0xFF;
	desc[7] = (d1>>24) & 0xFF;
	if(!inited){
		inited = 1;
		ldtfd = open("/dev/ldt", OWRITE);
		if(ldtfd < 0){
			if(bind("#z", "/dev", MAFTER) >= 0)
				ldtfd = open("/dev/ldt", OWRITE);
		}
	}
	if(ldtfd < 0){
		fprint(2, "linuxrun: ldt open: %r\n");
		return -Enosys;
	}
	if(seek(ldtfd, 0, 0) < 0){
		fprint(2, "linuxrun: ldt seek: %r\n");
		return -Enosys;
	}
	if(write(ldtfd, desc, 8) != 8){
		fprint(2, "linuxrun: ldt write: %r\n");
		return -Enosys;
	}
	ud->entry_number = 6;
	return 0;
}

/* glibc's i386 syscall stub calls through the TCB sysinfo pointer
 * (AT_SYSINFO) rather than int $0x80 for some syscalls; give it a
 * ud2;ret trampoline so those funnel into the same emulator. */
static void
initsysinfo(void)
{
	uchar *p;

	p = (uchar*)(Mapbase + Brksize);
	p[0] = 0x0f;
	p[1] = 0x0b;
	p[2] = 0xc3;
	sysinfova = (ulong)p;
}

static long
dosyscall(Ureg *ur)
{
	ulong nr, a1, a2, a3, a4;
	long r;

	nr = ur->ax;
	a1 = ur->bx;
	a2 = ur->cx;
	a3 = ur->dx;
	a4 = ur->si;
	r = -Enosys;
	switch(nr){
	case 1:		/* exit */
	case 252:	/* exit_group */
		snprint(exitstr, sizeof exitstr, "%lux", a1 & 0xff);
		exits(exitstr);
		return 0;
	case 3:		/* read */
		r = read((int)a1, (void*)a2, a3);
		if(r < 0)
			r = -Ebadf;
		break;
	case 4:		/* write */
		r = write((int)a1, (void*)a2, a3);
		if(r < 0)
			r = -Ebadf;
		break;
	case 5:		/* open */
		r = sysopen(a1, a2, ur->si);
		break;
	case 295:	/* openat: only the AT_FDCWD form */
		if(a1 != 0xffffff9cUL)
			r = -Ebadf;
		else
			r = sysopen(a2, a3, ur->si);
		break;
	case 300:	/* fstatat64: AT_EMPTY_PATH on an fd, or AT_FDCWD */
		if(a2 != 0 && ((char*)a2)[0] == 0 || a4 & 0x1000)
			r = fillstat64(a3, (int)a1);
		else if(a1 != 0xffffff9cUL)
			r = -Ebadf;
		else{
			int sfd;

			sfd = open((char*)a2, OREAD);
			if(sfd < 0)
				r = -Enoent;
			else{
				r = fillstat64(a3, sfd);
				close(sfd);
			}
		}
		break;
	case 6:		/* close */
		close((int)a1);
		r = 0;
		break;
	case 8:		/* creat */
		r = sysopen(a1, LoCreat|LoTrunc|1, 0666);
		break;
	case 10:	/* unlink */
		r = remove((char*)a1) < 0 ? -Enoent : 0;
		break;
	case 13:	/* time */
		r = time(nil);
		if(a1 != 0)
			*(ulong*)a1 = r;
		break;
	case 19:	/* lseek */
		r = seek((int)a1, a2, a3);
		break;
	case 20:	/* getpid */
	case 224:	/* gettid */
		r = getpid();
		break;
	case 24:	/* getuid */
	case 47:	/* getgid */
	case 49:	/* geteuid */
	case 50:	/* getegid */
		r = 0;
		break;
	case 33:	/* access */
		r = access((char*)a1, AEXIST) < 0 ? -Enoent : 0;
		break;
	case 45:	/* brk */
		if(a1 >= Brkbase && a1 < Brkbase+Brksize)
			brkcur = a1;
		r = brkcur;
		break;
	case 54:	/* ioctl */
		r = -Enotty;
		break;
	case 90:	/* old_mmap: ebx points at the arg struct */
		{
			ulong *m;

			m = (ulong*)a1;
			r = sysmmap(m[0], m[1], m[2], m[3], m[4], m[5]);
		}
		break;
	case 91:	/* munmap */
		r = 0;
		break;
	case 122:	/* uname */
		r = sysuname(a1);
		break;
	case 140:	/* _llseek: fd, hi, lo, result*, whence */
		{
			vlong o;

			o = seek((int)a1, ((vlong)a2<<32) | a3, ur->di);
			if(ur->si != 0)
				*(vlong*)ur->si = o;
			r = 0;
		}
		break;
	case 145:	/* readv */
		r = sysreadv(a1, a2, a3);
		break;
	case 146:	/* writev */
		r = syswritev(a1, a2, a3);
		break;
	case 76:	/* getrlimit */
	case 191:	/* ugetrlimit: fill in an infinite rlimit */
		if(a2 != 0){
			*(ulong*)a2 = 0x7fffffff;
			*(ulong*)(a2+4) = 0x7fffffff;
		}
		r = 0;
		break;
	case 78:	/* gettimeofday */
		{
			vlong t;

			t = nsec();
			if(a1 != 0){
				*(ulong*)a1 = t/1000000000;
				*(ulong*)(a1+4) = (t/1000)%1000000;
			}
		}
		r = 0;
		break;
	case 125:	/* mprotect */
	case 219:	/* madvise */
	case 172:	/* prctl */
	case 174:	/* rt_sigaction */
	case 175:	/* rt_sigprocmask */
	case 238:	/* sendfile: not yet */
	case 240:	/* futex: pretend success; single-threaded guests poll */
		r = 0;
		break;
	case 163:	/* mremap */
		r = -Enomem;
		break;
	case 192:	/* mmap2 */
		r = sysmmap(a1, a2, a3, ur->si, ur->di, ur->bp * Pgsz);
		break;
	case 196:	/* lstat64 */
	case 197:	/* fstat64: real mode/size so ld.so accepts the file */
		r = fillstat64(a2, (int)a1);
		break;
	case 180:	/* pread64: fd, buf, count, poslo, poshi */
		r = seek((int)a1, ((vlong)ur->di<<32) | ur->si, 0);
		if(r < 0)
			r = -Ebadf;
		else
			r = read((int)a1, (void*)a2, a3);
		break;
	case 258:	/* set_tid_address */
		r = getpid();
		break;
	case 265:	/* clock_gettime */
		{
			vlong t;

			t = nsec();
			if(a2 != 0){
				*(ulong*)a2 = t/1000000000;
				*(ulong*)(a2+4) = t%1000000000;
			}
		}
		r = 0;
		break;
	case 355:	/* getrandom */
		{
			ulong i, x;

			x = nsec();
			for(i = 0; i < a2; i++){
				x = x*1103515245 + 12345;
				((uchar*)a1)[i] = (x>>16) & 0xFF;
			}
		}
		r = a2;
		break;
	case 243:	/* set_thread_area */
		r = syssetthreadarea(a1);
		break;
	}
	if(verbose)
		fprint(2, "linuxrun: sys %lux -> %ld\n", nr, r);
	return r;
}



/*
 * The first ud2 trap starts the program (install entry registers);
 * later ud2 traps are syscalls to emulate and resume.  Anything else
 * goes to the default disposition.
 */
static int
traphandler(void *v, char *msg)
{
	Ureg *ur;
	uchar *pc;

	USED(msg);
	if(v == nil)
		return 0;
	ur = v;
	if(ur->trap != TrapUD){
		/* dump guest faults so the caller of a bad call is visible */
		if(started){
			ulong *stk;
			int i;

			fprint(2, "linuxrun: guest fault trap=%lux pc=%lux sp=%lux\n",
				ur->trap, ur->pc, ur->sp);
			fprint(2, "linuxrun: ax=%lux bx=%lux cx=%lux dx=%lux si=%lux di=%lux bp=%lux\n",
				ur->ax, ur->bx, ur->cx, ur->dx, ur->si, ur->di, ur->bp);
			stk = (ulong*)ur->sp;
			for(i = 0; i < 12; i++)
				fprint(2, "linuxrun:  sp+%d = %lux\n", i*4, stk[i]);
		}
		return 0;
	}
	pc = (uchar*)ur->pc;
	if(pc[0] != 0x0f || pc[1] != 0x0b)
		return 0;
	if(!started){
		started = 1;
		ur->pc = entrypc;
		ur->sp = stacktop;
		ur->ax = 0;
		ur->bx = 0;
		ur->cx = 0;
		ur->dx = 0;
		ur->si = 0;
		ur->di = 0;
		ur->bp = 0;
		return 1;
	}
	ur->ax = dosyscall(ur);
	ur->pc += 2;
	return 1;
}

static uchar trapinsn[2] = {0x0f, 0x0b};

static void
runguest(void)
{
	void (*f)(void);

	atnotify(traphandler, 1);
	f = (void(*)(void))trapinsn;
	f();
	fatal("returned from the guest");	/* not reached */
}

static void
usage(void)
{
	fprint(2, "usage: linuxrun [-nv] prog\n");
	exits("usage");
}

void
main(int argc, char *argv[])
{
	Ehdr eh;
	uchar hdr[64];
	int fd, i;

	ARGBEGIN{
	case 'n':
		analyzeonly = 1;
		break;
	case 'v':
		verbose = 1;
		break;
	case 'p':
		phdrmode = 1;
		break;
	case 'P':
		phdrmode = 2;
		break;
	default:
		usage();
	}ARGEND
	if(argc < 1)
		usage();

	fd = open(argv[0], OREAD);
	if(fd < 0)
		fatal("open %s: %r", argv[0]);
	if(readat(fd, hdr, sizeof hdr, 0) < 0)
		fatal("read %s: %r", argv[0]);

	memset(&eh, 0, sizeof eh);
	memmove(eh.ident, hdr, Elfident);
	if(eh.ident[EiClass] != Elfclass32 || eh.ident[EiData] != Elfdata2lsb)
		fatal("%s: not a 32-bit little-endian ELF", argv[0]);
	eh.type = le16(hdr+16);
	eh.machine = le16(hdr+18);
	eh.entry = le32(hdr+24);
	eh.phoff = le32(hdr+28);
	eh.phentsize = le16(hdr+42);
	eh.phnum = le16(hdr+44);
	if(eh.machine != Em386)
		fatal("%s: not an i386 binary", argv[0]);
	if(eh.type != EtExec && eh.type != EtDyn)
		fatal("%s: not an executable (type %d)", argv[0], eh.type);
	if(eh.phnum > Maxph)
		fatal("%s: too many program headers", argv[0]);

	if(analyzeonly){
		print("linuxrun: %s: static 386 Linux ELF entry %#lux\n",
			argv[0], eh.entry);
		exits(nil);
	}

	nphhdrs = eh.phnum;
	entrypc = loadelf(fd, &eh, eh.type == EtDyn ? Piebase : 0);
	mainentry = entrypc;
	close(fd);
	/* the phdrs live in the first PT_LOAD; translate the file offset */
	phdrva = 0;
	if(phdrmode == 1){
		for(i = 0; i < nph; i++){
			if(ph[i].type == PtLoad && ph[i].memsz > 0){
				phdrva = ph[i].vaddr & ~(Pgsz-1);
				break;
			}
		}
		if(verbose)
			fprint(2, "linuxrun: phdr(mode ehdr) %#lux\n", phdrva);
	}
	for(i = 0; i < nph; i++){
		if(ph[i].type == PtLoad && ph[i].memsz > 0){
			if(eh.phoff >= ph[i].offset &&
			    eh.phoff < ph[i].offset + ph[i].filesz)
				phdrva = ph[i].vaddr + (eh.phoff - ph[i].offset);
			break;
		}
	}
	if(verbose)
		fprint(2, "linuxrun: phdr %#lux\n", phdrva);

	/* dynamic: load the interpreter and hand control to it */
	if(dynamic){
		int ifd;
		Ehdr ieh;
		uchar ihdr[64];

		if(interppath[0] == 0)
			fatal("no interpreter path");
		ifd = open(interppath, OREAD);
		if(ifd < 0)
			fatal("open %s: %r", interppath);
		if(readat(ifd, ihdr, sizeof ihdr, 0) < 0)
			fatal("read %s: %r", interppath);
		memset(&ieh, 0, sizeof ieh);
		memmove(ieh.ident, ihdr, Elfident);
		ieh.type = le16(ihdr+16);
		ieh.machine = le16(ihdr+18);
		ieh.entry = le32(ihdr+24);
		ieh.phoff = le32(ihdr+28);
		ieh.phentsize = le16(ihdr+42);
		ieh.phnum = le16(ihdr+44);
		if(ieh.machine != Em386 || ieh.type != EtDyn)
			fatal("%s: not an i386 shared interpreter", interppath);
		interpbase = Interpbase;
		loadelf(ifd, &ieh, interpbase);
		close(ifd);
		/* ld.so enters; AT_ENTRY/AT_PHDR still describe the main */
		entrypc = interpbase + ieh.entry;
		if(verbose)
			fprint(2, "linuxrun: interp %s base %#lux entry %#lux\n",
				interppath, interpbase, entrypc);
	}

	brkcur = Brkbase;
	segat(Mapbase, Mapsize);
	initsysinfo();

	stacktop = buildstack(argc, argv);
	if(verbose)
		fprint(2, "linuxrun: entry %#lux stack %#lux\n", entrypc, stacktop);

	runguest();
	exits(nil);
}
