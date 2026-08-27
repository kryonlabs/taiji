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
	EtExec = 2,
	EtDyn = 3,
	Em386 = 3,
	EmX8664 = 62,
};

static ushort
get16(uchar *p, int lsb)
{
	if(lsb)
		return p[0] | p[1]<<8;
	return p[0]<<8 | p[1];
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

void
main(int argc, char **argv)
{
	uchar hdr[64];
	int fd, n, class, lsb;
	ushort type, mach;
	uvlong entry;

	ARGBEGIN{
	default:
		fprint(2, "usage: linuxrun linux-elf [args...]\n");
		exits("usage");
	}ARGEND
	if(argc < 1){
		fprint(2, "usage: linuxrun linux-elf [args...]\n");
		exits("usage");
	}
	fd = open(argv[0], OREAD);
	if(fd < 0)
		sysfatal("open %s: %r", argv[0]);
	n = read(fd, hdr, sizeof hdr);
	close(fd);
	if(n < Elfident || memcmp(hdr, "\177ELF", 4) != 0){
		fprint(2, "linuxrun: %s is not an ELF file\n", argv[0]);
		exits("not elf");
	}
	class = hdr[EiClass];
	lsb = hdr[EiData] == Elfdata2lsb;
	if(hdr[EiData] != Elfdata2lsb && hdr[EiData] != Elfdata2msb){
		fprint(2, "linuxrun: %s has unsupported ELF byte order %d\n", argv[0], hdr[EiData]);
		exits("bad elf");
	}
	type = get16(hdr+16, lsb);
	mach = get16(hdr+18, lsb);
	entry = class == Elfclass64 ? get64(hdr+24, lsb) : get16(hdr+24, lsb) | (ulong)get16(hdr+26, lsb)<<16;

	print("linuxrun: %s: ELF%d %s %s entry %#llux\n",
		argv[0],
		class == Elfclass64 ? 64 : class == Elfclass32 ? 32 : 0,
		lsb ? "little-endian" : "big-endian",
		machine(mach),
		entry);
	if(type != EtExec && type != EtDyn)
		print("linuxrun: note: ELF type %ud is not executable/shared-object style\n", type);
	fprint(2, "linuxrun: Linux binary ABI loading is not implemented yet; use ape/taiji-posix-sh and linuxcc for source ports\n");
	exits("not implemented");
}
