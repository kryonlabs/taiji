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
	PtLoad = 1,
	PtInterp = 3,

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
ulong stacktop;
Phdr ph[Maxph];
int nph;
ulong brkcur;
char exitstr[16];

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
static void
loadelf(int fd, Ehdr *eh)
{
	uchar buf[Maxph*sizeof(Phdr)];
	uchar *seg;
	ulong va, lo, hi, off;
	int i, j, k, patched;

	USED(va);

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
		if(ph[nph].type == PtInterp)
			fatal("dynamic ELF: interpreter support is not built yet");
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
	if(patched == 0)
		fprint(2, "linuxrun: warning: no int 80 sites patched\n");
	else if(verbose)
		fprint(2, "linuxrun: patched %d syscall sites\n", patched);
}

/*
 * Build the Linux initial stack: the argv0 string near the top, then
 * argc/argv/envp/auxv below it, stack pointer 16-byte aligned.
 */
static ulong
buildstack(char *argv0)
{
	uchar *st;
	ulong sp, strp;
	ulong *vec;
	int naux, nvec;

	st = segat(Stackbase, Stacksize);
	strp = Stackbase + Stacksize - 64;
	strcpy((char*)st + (strp - Stackbase), argv0);

	naux = 4;	/* PAGESZ, ENTRY, UID, GID pairs */
	nvec = 1 + 1 + 1 + 1 + 2*naux + 1;
	sp = (strp - 16) & ~15UL;
	sp = (sp - nvec*4) & ~15UL;
	vec = (ulong*)(st + (sp - Stackbase));
	vec[0] = 1;			/* argc */
	vec[1] = strp;			/* argv[0] */
	vec[2] = 0;			/* argv end */
	vec[3] = 0;			/* envp end */
	vec[4] = 6; vec[5] = Pgsz;	/* AT_PAGESZ */
	vec[6] = 9; vec[7] = entrypc;	/* AT_ENTRY */
	vec[8] = 11; vec[9] = 0;	/* AT_UID */
	vec[10] = 13; vec[11] = 0;	/* AT_GID */
	vec[12] = 0; vec[13] = 0;	/* AT_NULL */
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
	if(fd < 0)
		return -Enoent;
	return fd;
}

static ulong mapbump = Mapbase + Brksize;

static long
sysmmap(ulong addr, ulong len, ulong prot, ulong flags, ulong fd, ulong off)
{
	ulong va;

	USED(addr);
	USED(prot);
	if(len == 0)
		return -Einval;
	len = ((len + Pgsz-1) / Pgsz) * Pgsz;
	if(mapbump + len > Mapbase + Mapsize)
		return -Enomem;
	va = mapbump;
	mapbump += len;
	if(!(flags & 0x20)){	/* not MAP_ANONYMOUS: fill from the file */
		if(readat((int)fd, (void*)va, len, off) < 0)
			return -Ebadf;
	}
	return va;
}

static long
dosyscall(Ureg *ur)
{
	ulong nr, a1, a2, a3;
	long r;

	nr = ur->ax;
	a1 = ur->bx;
	a2 = ur->cx;
	a3 = ur->dx;
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
	case 174:	/* rt_sigaction */
	case 175:	/* rt_sigprocmask */
	case 238:	/* futex: pretend success; TLS-less programs poll */
		r = 0;
		break;
	case 192:	/* mmap2 */
		r = sysmmap(a1, a2, a3, ur->si, ur->di, ur->bp * Pgsz);
		break;
	case 196:	/* lstat64 */
	case 197:	/* fstat64: a zeroed buffer keeps static startup happy */
		if(a2 != 0)
			memset((void*)a2, 0, 96);
		r = 0;
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
	if(ur->trap != TrapUD)
		return 0;
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
	int fd;

	ARGBEGIN{
	case 'n':
		analyzeonly = 1;
		break;
	case 'v':
		verbose = 1;
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
	if(eh.type != EtExec)
		fatal("%s: not a static executable (type %d)", argv[0], eh.type);
	if(eh.phnum > Maxph)
		fatal("%s: too many program headers", argv[0]);

	if(analyzeonly){
		print("linuxrun: %s: static 386 Linux ELF entry %#lux\n",
			argv[0], eh.entry);
		exits(nil);
	}

	entrypc = eh.entry;
	loadelf(fd, &eh);
	close(fd);

	brkcur = Brkbase;
	segat(Mapbase, Mapsize);

	stacktop = buildstack(argv[0]);
	if(verbose)
		fprint(2, "linuxrun: entry %#lux stack %#lux\n", entrypc, stacktop);

	runguest();
	exits(nil);
}
