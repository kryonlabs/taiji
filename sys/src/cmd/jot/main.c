#include "inc.h"
#include <cursor.h>

RKeyboardctl *kbctl;
Keyboardctl *fwdkc, kbdctl2;
Mousectl *mctl;
int shiftdown;	// TODO: needed?
int ctldown;

Text text;
Image *colors[NCOL];
char filename[1024];
char *startdir;

Cursor quest = {
	{-7,-7},
	{0x0f, 0xf0, 0x1f, 0xf8, 0x3f, 0xfc, 0x7f, 0xfe, 
	 0x7c, 0x7e, 0x78, 0x7e, 0x00, 0xfc, 0x01, 0xf8, 
	 0x03, 0xf0, 0x07, 0xe0, 0x07, 0xc0, 0x07, 0xc0, 
	 0x07, 0xc0, 0x07, 0xc0, 0x07, 0xc0, 0x07, 0xc0, },
	{0x00, 0x00, 0x0f, 0xf0, 0x1f, 0xf8, 0x3c, 0x3c, 
	 0x38, 0x1c, 0x00, 0x3c, 0x00, 0x78, 0x00, 0xf0, 
	 0x01, 0xe0, 0x03, 0xc0, 0x03, 0x80, 0x03, 0x80, 
	 0x00, 0x00, 0x03, 0x80, 0x03, 0x80, 0x00, 0x00, }
};



void
panic(char *s)
{
	fprint(2, "error: %s: %r\n", s);
	threadexitsall("error");
}

void*
emalloc(ulong size)
{
	void *p;

	p = malloc(size);
	if(p == nil)
		panic("malloc failed");
	memset(p, 0, size);
	return p;
}

void*
erealloc(void *p, ulong size)
{
	p = realloc(p, size);
	if(p == nil)
		panic("realloc failed");
	return p;
}

char*
estrdup(char *s)
{
	char *p;

	p = malloc(strlen(s)+1);
	if(p == nil)
		panic("strdup failed");
	strcpy(p, s);
	return p;
}





/*
 * /dev/snarf updates when the file is closed, so we must open our own
 * fd here rather than use snarffd
 */
void
putsnarf(void)
{
	int fd, i, n;

	if(snarffd<0 || nsnarf==0)
		return;
	fd = open("/dev/snarf", OWRITE|OCEXEC);
	if(fd < 0)
		return;
	/* snarf buffer could be huge, so fprint will truncate; do it in blocks */
	for(i=0; i<nsnarf; i+=n){
		n = nsnarf-i;
		if(n >= 256)
			n = 256;
		if(fprint(fd, "%.*S", n, snarf+i) < 0)
			break;
	}
	close(fd);
}

void
setsnarf(char *s, int ns)
{
	free(snarf);
	snarf = runesmprint("%.*s", ns, s);
	nsnarf = runestrlen(snarf);
	snarfversion++;
}

void
getsnarf(void)
{
	int i, n;
	char *s, *sn;

	if(snarffd < 0)
		return;
	sn = nil;
	i = 0;
	seek(snarffd, 0, 0);
	for(;;){
		if(i > MAXSNARF)
			break;
		if((s = realloc(sn, i+1024+1)) == nil)
			break;
		sn = s;
		if((n = read(snarffd, sn+i, 1024)) <= 0)
			break;
		i += n;
	}
	if(i == 0)
		return;
	sn[i] = 0;
	setsnarf(sn, i);
	free(sn);
}




void
readfile(char *path)
{
	int fd, n, ns;
	char *s, buf[1024];
	Rune *rs;

	fd = open(path, OREAD);
	if(fd < 0)
		return;
	s = nil;
	ns = 0;
	while(n = read(fd, buf, sizeof(buf)), n > 0) {
		s = realloc(s, ns+n);
		memcpy(s+ns, buf, n);
		ns += n;
	}
	close(fd);


	rs = runesmprint("%.*s", ns, s);
	free(s);
	xdelete(&text, 0, text.nr);
	xinsert(&text, rs, runestrlen(rs), 0);
	free(rs);
}

int
writefile(char *path)
{
	int fd;
	char *s;

	fd = create(path, OWRITE|OTRUNC, 0666);
	if(fd < 0)
		return 1;

	s = smprint("%.*S", text.nr, text.r);
	write(fd, s, strlen(s));
	close(fd);
	free(s);
	return 0;
}


void
confused(void)
{
	setcursor(mctl, &quest);
	sleep(300);
	setcursor(mctl, nil);
}

void
editmenu(Text *x, Mousectl *mc)
{
	enum {
		Cut,
		Paste,
		Snarf,
		Plumb,
		Look
	};
	static char *menu2str[] = {
		"cut",
		"paste",
		"snarf",
		"plumb",
		"look",
		nil
	};
	static Menu menu2 = { menu2str };

	switch(menuhit(2, mc, &menu2, nil)){
	case Cut:
		xsnarf(x);
		xcut(x);
		break;
	case Paste:
		xpaste(x);
		break;
	case Snarf:
		xsnarf(x);
		break;
	case Plumb:
		if(xplumb(x, "jot", startdir, 31*1024))
			confused();
		break;
	case Look:
		xlook(x);
		break;
	}
}

void
filemenu(Text *x, Mousectl *mc)
{
	USED(x);
	enum {
		Write,
		Exit
	};
	static char *str[] = {
		"write",
		"exit",
		nil
	};
	static Menu menu = { str };

	switch(menuhit(3, mc, &menu, nil)){
		case Write:
			if(filename[0] == '\0'){
				snprint(filename, sizeof(filename), "jot.out");
			}
			if(writefile(filename)){
			memset(filename, 0, sizeof(filename));
			confused();
		}
		break;
	case Exit:
		threadexitsall(nil);
	}
}

void
mousectl(Text *x, Mousectl *mc)
{
	int but;

	for(but = 1; but < 6; but++)
		if(mc->buttons == 1<<(but-1))
			goto found;
	return;
found:

/*	if(shiftdown && but > 3)
		wkeyctl(w, but == 4 ? Kscrolloneup : Kscrollonedown);
	else*/ if(ptinrect(mc->xy, x->scrollr) || but > 3)
		xscroll(x, mc, but);
	else if(but == 1)
		xselect(x, mc);
	else if(but == 2)
		editmenu(x, mc);
	else if(but == 3)
		filemenu(x, mc);
}

void
xgetsel(Text *x, uint *q0, uint *q)
{
	if(x->selflip){
		*q0 = x->q1;
		*q = x->q0;
	}else{
		*q0 = x->q0;
		*q = x->q1;
	}
}

void
xleftright(Text *x, int dir, int extend)
{
	uint q0, q;

	xgetsel(x, &q0, &q);
	if(dir < 0 && -dir > q)
		q = 0;
	else
		q = MIN(q+dir, x->nr);
	xsetselect(x, extend ? q0 : q, q);
	xshow(x, q);
}

void
xupdown(Text *x, int dir, int extend)
{
	Point p;
	int py;
	uint q0, q;

	xgetsel(x, &q0, &q);

	xshow(x, q);
	p = frptofchar(x, q-x->org);
	if(x->posx >= 0)
		p.x = x->posx;
	py = p.y;
	p.y += dir*x->font->height;
	if(p.y < x->Frame.r.min.y ||
	   p.y > x->Frame.r.max.y-x->font->height){
		xscrolln(x, dir);
		p.y = py;
	}
	q = x->org+frcharofpt(x, p);

	xsetselect(x, extend ? q0 : q, q);
	xshow(x, q);
	x->posx = p.x;
}

void
xline(Text *x, int dir, int extend)
{
	uint q0, q;

	xgetsel(x, &q0, &q);

	if(dir < 0){
		while(q > 0 && x->r[q-1] != '\n' &&
		      q != x->qh)
			q--;
	}else{
		while(q < x->nr && x->r[q] != '\n')
			q++;
	}

	xsetselect(x, extend ? q0 : q, q);
	xshow(x, q);
}

void
xbegend(Text *x, int dir, int extend)
{
	uint q0, q;

	xgetsel(x, &q0, &q);

	if(dir < 0)
		q = 0;
	else
		q = x->nr;

	xsetselect(x, extend ? q0 : q, q);
	xshow(x, q);
}

void
keyctl(Text *x, Rune r)
{
	int nlines, n;

	nlines = x->maxlines;	/* need signed */
	switch(r){

	/* Scrolling */
	case Kscrollonedown:
		n = mousescrollsize(x->maxlines);
		xscrolln(x, MAX(n, 1));
		break;
	case Kpgdown:
		if(ctldown)
			xscrolln(x, nlines*2/3);
		else
			xscrolln(x, nlines*1/3);
		break;
	case Kscrolloneup:
		n = mousescrollsize(x->maxlines);
		xscrolln(x, -MAX(n, 1));
		break;
	case Kpgup:
		if(ctldown)
			xscrolln(x, -nlines*2/3);
		else
			xscrolln(x, -nlines*1/3);
		break;

	/* Cursor movement */
	case Kdown:
		xupdown(x, 1, shiftdown);
		break;
	case Kup:
		xupdown(x, -1, shiftdown);
		break;
	case Kleft:
		xleftright(x, -1, shiftdown);
		break;
	case Kright:
		xleftright(x, 1, shiftdown);
		break;
	case Khome:
		if(ctldown)
			xbegend(x, -1, shiftdown);
		else
			xline(x, -1, shiftdown);
		break;
	case Kend:
		if(ctldown)
			xbegend(x, 1, shiftdown);
		else
			xline(x, 1, shiftdown);
		break;

	case CTRL('A'):
		xline(x, -1, shiftdown);
		break;
	case CTRL('E'):
		xline(x, 1, shiftdown);
		break;
	case CTRL('B'):
		xplacetick(x, x->qh);
		break;

	case Kesc:
		xsnarf(x);
		xcut(x);
		break;
	case Kdel:
		xtype(x, CTRL('H'));
		break;

	default:
		xtype(x, r);
		break;
	}
}

void
setsize(Text *x)
{
	Rectangle scrollr, textr;

	draw(screen, screen->r, colors[BACK], nil, ZP);
	scrollr = textr = insetrect(screen->r, 1);
	scrollr.max.x = scrollr.min.x + 12;
	textr.min.x = scrollr.max.x + 4;
	xinit(x, textr, scrollr, font, screen, colors);
}

void
mthread(void*)
{
	while(readmouse(mctl) != -1){
		mousectl(&text, mctl);
	}
}

void
dumpkbd(char *s)
{
	Rune *rs;
	int i;

	rs = runesmprint("%s", s);
	for(i = 0; rs[i]; i++)
		print("%C %X\n", rs[i], rs[i]);
	print("\n");
	free(rs);
}

void
kbthread(void*)
{
	char *s;
	Rune r;

	for(;;){
		s = recvp(kbctl->c);
//dumpkbd(s);
		if(*s == 'k' || *s == 'K'){
			shiftdown = utfrune(s+1, Kshift) != nil;
			ctldown = utfrune(s+1, Kctl) != nil;
		}
		if(*s == 'c'){
			chartorune(&r, s+1);
			if(r){
				if(fwdkc)
					send(fwdkc->c, &r);
				else
					keyctl(&text, r);
				flushimage(display, 1);
			}
		}
		free(s);
	}
}

void
resthread(void*)
{
	for(;;){
		recvul(mctl->resizec);
		if(getwindow(display, Refnone) < 0)
			sysfatal("resize failed: %r");

		setsize(&text);
	}
}

void
threadmain(int argc, char *argv[])
{
	char buf[1024];
//	newwindow(nil);

	if(initdraw(nil, nil, "jot") < 0)
		sysfatal("initdraw: %r");

	kbctl = initkbd(nil, nil);
	if(kbctl == nil)
		sysfatal("inikeyboard: %r");
	kbdctl2.c = chancreate(sizeof(Rune), 20);

	mctl = initmouse("/dev/mouse", screen);
	if(mctl == nil)
		sysfatal("initmouse: %r");
	snarffd = open("/dev/snarf", OREAD|OCEXEC);
	if(getwd(buf, sizeof(buf)) == nil)
		startdir = estrdup(".");
	else
		startdir = estrdup(buf);

	colors[BACK] = allocimagemix(display, DPaleyellow, DWhite);
	colors[HIGH] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DDarkyellow);
	colors[BORD] = allocimage(display, Rect(0,0,2,2), screen->chan, 1, DYellowgreen);
	colors[TEXT] = display->black;
	colors[HTEXT] = display->black;

	setsize(&text);

	timerinit();
	threadcreate(mthread, nil, mainstacksize);
	threadcreate(kbthread, nil, mainstacksize);
	threadcreate(resthread, nil, mainstacksize);

	if(argc > 1){
		strncpy(filename, argv[1], sizeof(filename));
		readfile(filename);
	}
}
