#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"

/*
 * /dev/ldt - a per-process local descriptor table.
 *
 * Foreign runtimes that emulate other operating systems (linuxrun)
 * need a writable LDT entry to install thread-local storage segments:
 * their ABI reaches TLS memory through %gs, and %gs must select a real
 * segment.  Writing 8-byte x86 descriptors at byte offset i*8 installs
 * LDT entry i for the writing process; descriptors are forced to user
 * privilege and gate descriptors are refused, so the LDT can only
 * describe user memory.  The table is loaded on every switch back into
 * the process (see ldtload in mmu.c).
 */

enum {
	Qdir,
	Qdata,
	Qmark,
	Qnstack,
};

static Dirtab ldttab[] = {
	".",	{Qdir, 0, QTDIR},	0,	0555,
	"ldt",	{Qdata, 0},		0,	0600,
	"mark",	{Qmark, 0},		0,	0600,
	"notestack",	{Qnstack, 0},	0,	0600,
};

static Chan*
ldtattach(char *spec)
{
	return devattach('z', spec);
}

static Walkqid*
ldtwalk(Chan* c, Chan *nc, char** name, int nname)
{
	return devwalk(c, nc, name, nname, ldttab, nelem(ldttab), devgen);
}

static int
ldtstat(Chan* c, uchar *dp, int n)
{
	return devstat(c, dp, n, ldttab, nelem(ldttab), devgen);
}

static Chan*
ldtopen(Chan* c, int omode)
{
	return devopen(c, omode, ldttab, nelem(ldttab), devgen);
}

static void
ldtclose(Chan *c)
{
	USED(c);
}

static long
ldtread(Chan* c, void* a, long n, vlong off)
{
	if(c->qid.path == Qdir)
		return devdirread(c, a, n, ldttab, nelem(ldttab), devgen);
	if(up->ldtbase == 0)
		return 0;
	if(off >= BY2PG)
		return 0;
	if(off+n > BY2PG)
		n = BY2PG - off;
	memmove(a, (void*)(up->ldtbase+off), n);
	return n;
}

static long
ldtwrite(Chan* c, void* a, long n, vlong off)
{
	int i, nent;
	ulong *tab;

	if(c->qid.path == Qmark){
		/* mark this process as running foreign binaries so the
		 * kernel routes its int $0x80 to the note handler */
		up->foreign = 1;
		return n;
	}
	if(c->qid.path == Qnstack){
		/* the loader attached its private note stack in user
		 * space; record the top address so notify() builds note
		 * frames there instead of below the guest sp */
		if(n >= 4)
			up->notestack = ((ulong*)a)[0];
		return n;
	}
	if(c->qid.path == Qdir)
		error(Eperm);
	if(n <= 0)
		return 0;
	if(off % 8 || n % 8)
		error("ldt writes must be 8-byte aligned");
	if(off+n > BY2PG)
		error(Ebadarg);

	if(up->ldtbase == 0){
		tab = malloc(BY2PG);
		if(tab == nil)
			error(Enomem);
		memset(tab, 0, BY2PG);
		if(waserror()){
			free(tab);
			nexterror();
		}
		up->ldtbase = (ulong)tab;
		poperror();
	}

	memmove((void*)(up->ldtbase+off), a, n);

	/* force user privilege and refuse anything that is not a
	 * plain code/data segment descriptor: the S bit (0x10 in the
	 * type field) is what separates segments from system/gate
	 * descriptors */
	nent = n/8;
	tab = (ulong*)(up->ldtbase+off);
	for(i = 0; i < nent; i++){
		ulong d1;

		d1 = tab[i*2+1];
		if((d1 & SEGTYPE) < (0x10<<8))
			error("ldt: gate descriptors are not allowed");
		d1 &= ~SEGPL(3);
		d1 |= SEGPL(3);
		tab[i*2+1] = d1 | SEGP;
	}

	/* we are the running process: install now too (entry i of this
	 * page shows up as GDT slot TLSSEG+i and LDT entry i) */
	ldtload(up);
	return n;
}

Dev ldtdevtab = {
	'z',
	"ldt",

	devreset,
	devinit,
	devshutdown,
	ldtattach,
	ldtwalk,
	ldtstat,
	ldtopen,
	devcreate,
	ldtclose,
	ldtread,
	devbread,
	ldtwrite,
	devbwrite,
	devremove,
	devwstat,
};
