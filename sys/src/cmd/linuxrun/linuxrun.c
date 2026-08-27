#include <u.h>
#include <libc.h>

enum {
	Elfident = 16,
	EiClass = 4,
	EiData = 5,
	Elfclass32 = 1,
	Elfclass64 = 2,
	Elfdata2lsb = 1,
	Elfdata2msb = 2,

	Elf32ehsize = 52,
	Elf64ehsize = 64,
	Elf32phsize = 32,
	Elf64phsize = 56,
	Maxphsize = 128,

	EtExec = 2,
	EtDyn = 3,

	Em386 = 3,
	EmX8664 = 62,

	PtLoad = 1,
	PtDynamic = 2,
	PtInterp = 3,
	PtPhdr = 6,
	PtTls = 7,
	PtGnuStack = 0x6474e551,

	PfX = 1,
	PfW = 2,
	PfR = 4,
};

typedef struct Elf Elf;
typedef struct Phdr Phdr;

struct Elf {
	int	class;
	int	lsb;
	ushort	type;
	ushort	mach;
	uvlong	entry;
	uvlong	phoff;
	ushort	phentsize;
	ushort	phnum;
};

struct Phdr {
	ulong	type;
	ulong	flags;
	uvlong	offset;
	uvlong	vaddr;
	uvlong	paddr;
	uvlong	filesz;
	uvlong	memsz;
	uvlong	align;
};

static int analyzeonly;

static ushort
get16(uchar *p, int lsb)
{
	if(lsb)
		return p[0] | p[1]<<8;
	return p[0]<<8 | p[1];
}

static ulong
get32(uchar *p, int lsb)
{
	if(lsb)
		return (ulong)p[0] | (ulong)p[1]<<8 | (ulong)p[2]<<16 | (ulong)p[3]<<24;
	return (ulong)p[0]<<24 | (ulong)p[1]<<16 | (ulong)p[2]<<8 | (ulong)p[3];
}

static uvlong
get64(uchar *p, int lsb)
{
	uvlong v;
	int i;

	v = 0;
	if(lsb)
		for(i = 7; i >= 0; i--)
			v = (v<<8) | p[i];
	else
		for(i = 0; i < 8; i++)
			v = (v<<8) | p[i];
	return v;
}

static void
put16le(uchar *p, ushort v)
{
	p[0] = v;
	p[1] = v>>8;
}

static void
put32le(uchar *p, ulong v)
{
	p[0] = v;
	p[1] = v>>8;
	p[2] = v>>16;
	p[3] = v>>24;
}

static char*
machine(ushort m)
{
	switch(m){
	case Em386:
		return "386";
	case EmX8664:
		return "amd64";
	default:
		return "unknown";
	}
}

static char*
etype(ushort t)
{
	switch(t){
	case EtExec:
		return "exec";
	case EtDyn:
		return "dyn";
	default:
		return "other";
	}
}

static void
flagstr(ulong flags, char *buf)
{
	buf[0] = (flags&PfR) ? 'r' : '-';
	buf[1] = (flags&PfW) ? 'w' : '-';
	buf[2] = (flags&PfX) ? 'x' : '-';
	buf[3] = 0;
}

static int
readelf(int fd, char *path, Elf *e)
{
	uchar hdr[Elf64ehsize];
	int n, hsize;

	n = pread(fd, hdr, sizeof hdr, 0);
	if(n < Elfident || memcmp(hdr, "\177ELF", 4) != 0){
		fprint(2, "linuxrun: %s is not an ELF file\n", path);
		return -1;
	}
	e->class = hdr[EiClass];
	if(e->class != Elfclass32 && e->class != Elfclass64){
		fprint(2, "linuxrun: %s has unsupported ELF class %d\n", path, e->class);
		return -1;
	}
	e->lsb = hdr[EiData] == Elfdata2lsb;
	if(hdr[EiData] != Elfdata2lsb && hdr[EiData] != Elfdata2msb){
		fprint(2, "linuxrun: %s has unsupported ELF byte order %d\n", path, hdr[EiData]);
		return -1;
	}
	hsize = e->class == Elfclass64 ? Elf64ehsize : Elf32ehsize;
	if(n < hsize){
		fprint(2, "linuxrun: %s has a short ELF header\n", path);
		return -1;
	}
	e->type = get16(hdr+16, e->lsb);
	e->mach = get16(hdr+18, e->lsb);
	if(e->class == Elfclass64){
		e->entry = get64(hdr+24, e->lsb);
		e->phoff = get64(hdr+32, e->lsb);
		e->phentsize = get16(hdr+54, e->lsb);
		e->phnum = get16(hdr+56, e->lsb);
	}else{
		e->entry = get32(hdr+24, e->lsb);
		e->phoff = get32(hdr+28, e->lsb);
		e->phentsize = get16(hdr+42, e->lsb);
		e->phnum = get16(hdr+44, e->lsb);
	}
	return 0;
}

static int
readphdr(int fd, char *path, Elf *e, int idx, Phdr *p)
{
	uchar buf[Maxphsize];
	int minsize, n;
	vlong off;

	minsize = e->class == Elfclass64 ? Elf64phsize : Elf32phsize;
	if(e->phentsize < minsize || e->phentsize > sizeof buf){
		fprint(2, "linuxrun: %s has unsupported program-header entry size %ud\n", path, e->phentsize);
		return -1;
	}
	off = e->phoff + (uvlong)idx*e->phentsize;
	n = pread(fd, buf, e->phentsize, off);
	if(n != e->phentsize){
		fprint(2, "linuxrun: %s has a short program-header table\n", path);
		return -1;
	}
	memset(p, 0, sizeof *p);
	if(e->class == Elfclass64){
		p->type = get32(buf+0, e->lsb);
		p->flags = get32(buf+4, e->lsb);
		p->offset = get64(buf+8, e->lsb);
		p->vaddr = get64(buf+16, e->lsb);
		p->paddr = get64(buf+24, e->lsb);
		p->filesz = get64(buf+32, e->lsb);
		p->memsz = get64(buf+40, e->lsb);
		p->align = get64(buf+48, e->lsb);
	}else{
		p->type = get32(buf+0, e->lsb);
		p->offset = get32(buf+4, e->lsb);
		p->vaddr = get32(buf+8, e->lsb);
		p->paddr = get32(buf+12, e->lsb);
		p->filesz = get32(buf+16, e->lsb);
		p->memsz = get32(buf+20, e->lsb);
		p->flags = get32(buf+24, e->lsb);
		p->align = get32(buf+28, e->lsb);
	}
	return 0;
}

static void
readinterp(int fd, Phdr *p, char *buf, int nbuf)
{
	int n;

	if(nbuf <= 0)
		return;
	buf[0] = 0;
	if(p->filesz == 0)
		return;
	if(p->filesz >= nbuf)
		n = nbuf - 1;
	else
		n = p->filesz;
	if(pread(fd, buf, n, p->offset) != n){
		strcpy(buf, "<unreadable>");
		return;
	}
	buf[n] = 0;
}

static int
inspect(char *path, int quietok)
{
	Elf e;
	Phdr p;
	char flags[4], interp[256];
	int fd, i, loads, dynamic, hasinterp, hastls, hasstack;

	fd = open(path, OREAD);
	if(fd < 0){
		fprint(2, "linuxrun: open %s: %r\n", path);
		return -1;
	}
	if(readelf(fd, path, &e) < 0){
		close(fd);
		return -1;
	}
	print("linuxrun: %s: ELF%d %s %s %s entry %#llux\n",
		path,
		e.class == Elfclass64 ? 64 : 32,
		e.lsb ? "little-endian" : "big-endian",
		machine(e.mach),
		etype(e.type),
		e.entry);
	print("linuxrun: phdr: offset %#llux entsize %ud count %ud\n",
		e.phoff, e.phentsize, e.phnum);

	loads = 0;
	dynamic = e.type == EtDyn;
	hasinterp = 0;
	hastls = 0;
	hasstack = 0;
	interp[0] = 0;
	for(i = 0; i < e.phnum; i++){
		if(readphdr(fd, path, &e, i, &p) < 0){
			close(fd);
			return -1;
		}
		switch(p.type){
		case PtLoad:
			flagstr(p.flags, flags);
			print("linuxrun: load[%d]: off %#llux vaddr %#llux filesz %#llux memsz %#llux flags %s align %#llux\n",
				loads, p.offset, p.vaddr, p.filesz, p.memsz, flags, p.align);
			loads++;
			break;
		case PtInterp:
			hasinterp = 1;
			readinterp(fd, &p, interp, sizeof interp);
			print("linuxrun: interp: %s\n", interp);
			break;
		case PtDynamic:
			dynamic = 1;
			print("linuxrun: dynamic segment present\n");
			break;
		case PtTls:
			hastls = 1;
			print("linuxrun: tls segment present\n");
			break;
		case PtGnuStack:
			hasstack = 1;
			flagstr(p.flags, flags);
			print("linuxrun: gnu-stack: flags %s\n", flags);
			break;
		}
	}
	close(fd);
	if(loads == 0){
		fprint(2, "linuxrun: %s has no loadable segments\n", path);
		return -1;
	}
	if(e.mach != Em386)
		print("linuxrun: unsupported-machine: %s needs an architecture backend\n", machine(e.mach));
	if(dynamic || hasinterp)
		print("linuxrun: unsupported-dynamic: interpreter/dynamic linking is not wired yet\n");
	if(hastls)
		print("linuxrun: unsupported-tls: Linux TLS setup is not wired yet\n");
	if(!dynamic && !hasinterp && e.mach == Em386)
		print("linuxrun: candidate: static 386 Linux ELF with %d load segment%s\n",
			loads, loads == 1 ? "" : "s");
	if(!hasstack)
		print("linuxrun: note: no GNU-stack program header\n");
	if(!quietok)
		fprint(2, "linuxrun: Linux syscall/stack execution is not implemented yet; analysis only\n");
	return 0;
}

static int
writesmoke(char *path)
{
	uchar b[Elf32ehsize+Elf32phsize];
	int fd;

	memset(b, 0, sizeof b);
	b[0] = 0x7f;
	b[1] = 'E';
	b[2] = 'L';
	b[3] = 'F';
	b[EiClass] = Elfclass32;
	b[EiData] = Elfdata2lsb;
	b[6] = 1;
	put16le(b+16, EtExec);
	put16le(b+18, Em386);
	put32le(b+20, 1);
	put32le(b+24, 0x08048054);
	put32le(b+28, Elf32ehsize);
	put16le(b+40, Elf32ehsize);
	put16le(b+42, Elf32phsize);
	put16le(b+44, 1);

	put32le(b+Elf32ehsize+0, PtLoad);
	put32le(b+Elf32ehsize+4, 0);
	put32le(b+Elf32ehsize+8, 0x08048000);
	put32le(b+Elf32ehsize+12, 0x08048000);
	put32le(b+Elf32ehsize+16, sizeof b);
	put32le(b+Elf32ehsize+20, sizeof b);
	put32le(b+Elf32ehsize+24, PfR|PfX);
	put32le(b+Elf32ehsize+28, 0x1000);

	fd = create(path, OWRITE, 0666);
	if(fd < 0){
		fprint(2, "linuxrun: create %s: %r\n", path);
		return -1;
	}
	if(write(fd, b, sizeof b) != sizeof b){
		fprint(2, "linuxrun: write %s: %r\n", path);
		close(fd);
		remove(path);
		return -1;
	}
	close(fd);
	return 0;
}

static void
selftest(void)
{
	char *path;

	path = "/tmp/linuxrun-smoke.elf";
	if(writesmoke(path) < 0)
		exits("smoke");
	if(inspect(path, 1) < 0){
		remove(path);
		exits("smoke");
	}
	remove(path);
	print("taiji-linuxrun-smoke-ok\n");
	exits(nil);
}

static void
usage(void)
{
	fprint(2, "usage: linuxrun [-n] linux-elf [args...]\n");
	fprint(2, "       linuxrun -s\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	ARGBEGIN{
	case 'n':
		analyzeonly = 1;
		break;
	case 's':
		selftest();
		break;
	default:
		usage();
	}ARGEND
	if(argc < 1)
		usage();
	if(inspect(argv[0], analyzeonly) < 0)
		exits("bad elf");
	if(analyzeonly)
		exits(nil);
	exits("not implemented");
}
