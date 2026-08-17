#include "inc.h"

/*
 * Windows-2000-style boot splash, Plan 9 flavored: an ntldr-like
 * boot manager menu with countdown, then the animated segmented
 * progress bar while the desktop comes up.
 */

enum {
	BootNormal,
	BootSafe,

	Nsegs = 10,
	SegW = 12,
	SegGap = 4,
	FrameMs = 60,
	Nframes = 42,
	MenuTickMs = 100,
	MenuTimeout = 6,
	DwellMs = 1000,

	Menupad = 8,
	Menuitemh = 20,
};

static char *bootstr[] = {
	"Plan 9",
	"Plan 9 (safe mode - no panel)",
	nil,
};

static void
splashcolors(Image **navy, Image **blue, Image **dim)
{
	*navy = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x000080FF);
	*blue = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x3A6EA5FF);
	*dim = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x404040FF);
}

static Rectangle
menurect(int i, Rectangle clip)
{
	Rectangle r;

	r.min.x = clip.min.x + 40;
	r.min.y = clip.min.y + 90 + i*(Menuitemh+6);
	r.max.x = r.min.x + 320;
	r.max.y = r.min.y + Menuitemh;
	return r;
}

static void
drawmenu(int sel, Rectangle clip, int left)
{
	char buf[64];
	int i;

	draw(screen, clip, display->black, nil, ZP);
	string(screen, Pt(clip.min.x+40, clip.min.y+34),
		display->white, ZP, font, "Plan 9 Boot Manager");
	string(screen, Pt(clip.min.x+40, clip.min.y+52),
		display->white, ZP, font, "pbm 1.0 - Plan 9 from Bell Labs");
	for(i = 0; bootstr[i]; i++){
		Rectangle r = menurect(i, clip);
		if(i == sel){
			draw(screen, r, display->white, nil, ZP);
			string(screen, Pt(r.min.x+Menupad, r.min.y+2),
				display->black, ZP, font, bootstr[i]);
		}else{
			string(screen, Pt(r.min.x+Menupad, r.min.y+2),
				display->white, ZP, font, bootstr[i]);
		}
	}
	snprint(buf, sizeof buf, "booting default in %d seconds", (left+999)/1000);
	string(screen, Pt(clip.min.x+40, clip.max.y-60),
		display->white, ZP, font, buf);
	string(screen, Pt(clip.min.x+40, clip.max.y-40),
		display->white, ZP, font, "click or dwell on an entry to choose");
	flushimage(display, 1);
}

static int
entryat(Point p, Rectangle clip)
{
	int i;

	for(i = 0; bootstr[i]; i++)
		if(ptinrect(p, menurect(i, clip)))
			return i;
	return -1;
}

static int
bootmenu(Rectangle clip)
{
	Timer *timer;
	Point last;
	int sel, left, dwell, click, i;
	static Alt alts[3];

	sel = 0;
	left = MenuTimeout*1000;
	dwell = 0;
	click = 0;
	last = mctl->xy;
	drawmenu(sel, clip, left);
	for(;;){
		timer = timerstart(MenuTickMs);
		alts[0] = ALT(timer->c, nil, CHANRCV);
		alts[1] = ALT(mctl->c, &mctl->Mouse, CHANRCV);
		alts[2].op = CHANEND;
		switch(alt(alts)){
		case 0:
			left -= MenuTickMs;
			if(left <= 0)
				return BootNormal;
			if(click){
				i = entryat(last, clip);
				if(i >= 0)
					return i;
			}
			if(sel != 0 && dwell >= DwellMs && entryat(last, clip) == sel)
				return sel;
			drawmenu(sel, clip, left);
			break;
		case 1:
			timercancel(timer);
			if(ptinrect(mctl->xy, clip))
				last = mctl->xy;
			if(mctl->buttons & 1){
				click = 1;
				i = entryat(last, clip);
				if(i >= 0)
					return i;
			}else if(click){
				click = 0;
				i = entryat(last, clip);
				if(i >= 0)
					return i;
			}
			i = entryat(mctl->xy, clip);
			if(i >= 0 && i != sel){
				sel = i;
				dwell = 0;
				drawmenu(sel, clip, left);
			}
			if(entryat(last, clip) != sel)
				dwell = 0;
			else
				dwell += MenuTickMs;
			break;
		}
	}
}

static void
progressframe(Rectangle bar, int pos, Image *blue, Image *dim)
{
	int i, x;
	Rectangle seg;

	x = bar.min.x + 1;
	for(i = 0; i < Nsegs; i++){
		seg = Rect(x, bar.min.y+1, x+SegW, bar.max.y-1);
		if(i >= pos && i < pos+3)
			draw(screen, seg, blue, nil, ZP);
		else
			draw(screen, seg, dim, nil, ZP);
		x += SegW + SegGap;
	}
}

/* ---- logon screen ---- */

typedef struct Logon Logon;
struct Logon {
	Rectangle dlg;
	Rectangle ok;
	Rectangle shut;
	Rectangle pass;
	char passbuf[32];
	int npass;
};

static Image *lgface;
static Image *lgnavy;
static Image *lgdim;

static void
logonbutton(Rectangle r, char *label)
{
	Point pt;

	winborder(screen, r, display->white, lgdim);
	winborder(screen, insetrect(r, 1), lgface, lgdim);
	draw(screen, insetrect(insetrect(r, 1), 1), lgface, nil, ZP);
	pt = Pt(r.min.x+(Dx(r)-stringwidth(font, label))/2,
		r.min.y+(Dy(r)-font->height)/2);
	string(screen, pt, display->black, ZP, font, label);
}

static void
logondraw(Logon *lg, int blink)
{
	Point pt;
	Rectangle r, title;
	int i, cx;

	draw(screen, lg->dlg, lgface, nil, ZP);
	winborder(screen, lg->dlg, display->black, lgdim);
	title = Rect(lg->dlg.min.x+3, lg->dlg.min.y+3, lg->dlg.max.x-3, lg->dlg.min.y+24);
	draw(screen, title, lgnavy, nil, ZP);
	string(screen, Pt(title.min.x+8, title.min.y+(Dy(title)-font->height)/2),
		display->white, ZP, font, "Log on to Plan 9");

	pt = Pt(lg->dlg.min.x+20, title.max.y+22);
	string(screen, pt, display->black, ZP, font, "User name:");
	r = Rect(pt.x+140, pt.y-3, lg->dlg.max.x-20, pt.y+font->height+1);
	winborder(screen, r, lgdim, display->white);
	draw(screen, insetrect(r, 2), display->white, nil, ZP);
	string(screen, Pt(r.min.x+4, pt.y), display->black, ZP, font, "glenda");

	pt = Pt(lg->dlg.min.x+20, pt.y+34);
	string(screen, pt, display->black, ZP, font, "Password:");
	winborder(screen, lg->pass, lgdim, display->white);
	draw(screen, insetrect(lg->pass, 2), display->white, nil, ZP);
	for(i = 0; i < lg->npass; i++)
		string(screen, Pt(lg->pass.min.x+5+i*7, pt.y),
			display->black, ZP, font, "*");
	if(blink){
		cx = lg->pass.min.x+5+lg->npass*7;
		draw(screen, Rect(cx, pt.y, cx+1, pt.y+font->height),
			display->black, nil, ZP);
	}

	logonbutton(lg->ok, "OK");
	logonbutton(lg->shut, "Shut Down");
	string(screen, Pt(lg->dlg.min.x+20, lg->dlg.max.y-26),
		lgdim, ZP, font, "any password is fine - press Enter or click OK");
	flushimage(display, 1);
}

static void
logonreboot(void)
{
	int fd;

	fd = open("#c/reboot", OWRITE);
	if(fd >= 0){
		write(fd, "reboot", 6);
		close(fd);
	}
}

static ulong
lerpcol(ulong c0, ulong c1, int i, int n)
{
	int r, g, b;

	r = ((c0>>24 & 0xFF)*(n-i) + (c1>>24 & 0xFF)*i) / n;
	g = ((c0>>16 & 0xFF)*(n-i) + (c1>>16 & 0xFF)*i) / n;
	b = ((c0>>8 & 0xFF)*(n-i) + (c1>>8 & 0xFF)*i) / n;
	return r<<24 | g<<16 | b<<8 | 0xFF;
}

static void
logonbackdrop(Rectangle clip)
{
	Rectangle band;
	ulong c;
	int i;

	for(i = 0; i < 32; i++){
		c = lerpcol(0x3A6EA5FF, 0xA6CAF0FF, i, 31);
		band = Rect(clip.min.x, clip.min.y + Dy(clip)*i/32,
			clip.max.x, clip.min.y + Dy(clip)*(i+1)/32);
		draw(screen, band, getcolor(nil, c), nil, ZP);
	}
}

/*
 * Keyboard input arrives in one of two forms: raw /dev/kbd lines
 * ('k' + the runes currently held down, 'K' on release) or, when the
 * console is read instead, "c<C>" strings, one rune per key press.
 * Returns TRUE when the login is complete.
 */
static int
logonrune(Logon *lg, Rune r)
{
	if(r == '\n' || r == '\r')
		return 1;
	if(r == Kbs || r == Kdel){
		if(lg->npass > 0)
			lg->npass--;
	}else if(r >= ' ' && r < 0x7f && lg->npass < nelem(lg->passbuf)-1)
		lg->npass++;
	return 0;
}

static int
logonkey(Logon *lg, char *ks)
{
	static char kdown[128];
	Rune r;
	char *p;
	int n;

	if(ks[0] == 'c'){
		p = ks+1;
		while(*p){
			n = chartorune(&r, p);
			p += n;
			if(logonrune(lg, r)){
				free(ks);
				return 1;
			}
		}
		free(ks);
		return 0;
	}
	if(ks[0] != 'k' && ks[0] != 'K'){
		free(ks);
		return 0;
	}
	if(ks[0] == 'k'){
		p = ks+1;
		while(*p){
			n = chartorune(&r, p);
			p += n;
			if(r == Kshift || r == Kctl || r == Kalt)
				continue;
			if(utfrune(kdown, r) != nil)
				continue;	/* still held from an earlier event */
			if(logonrune(lg, r)){
				free(ks);
				return 1;
			}
		}
	}
	n = strlen(ks+1);
	if(n >= sizeof kdown)
		n = sizeof kdown-1;
	memmove(kdown, ks+1, n);
	kdown[n] = 0;
	free(ks);
	return 0;
}

static void
logon(Logon *lg, Rectangle clip)
{
	Timer *timer;
	char *ks;
	int blink, done;
	static Alt alts[4];

	logonbackdrop(clip);

	blink = 0;
	done = 0;
	logondraw(lg, 1);
	while(!done){
		timer = timerstart(400);
		alts[0] = ALT(timer->c, nil, CHANRCV);
		alts[1] = ALT(mctl->c, &mctl->Mouse, CHANRCV);
		alts[2].op = CHANEND;
		switch(alt(alts)){
		case 0:
			blink = !blink;
			logondraw(lg, blink);
			break;
		case 1:
			if((mctl->buttons & 1) && ptinrect(mctl->xy, lg->ok))
				done = 1;
			else if((mctl->buttons & 1) && ptinrect(mctl->xy, lg->shut)){
				logonreboot();
				done = 1;
			}
			break;
		}
		timercancel(timer);
		while(nbrecv(kbctl->c, &ks) == 1){
			if(logonkey(lg, ks)){
				done = 1;
				break;
			}
			logondraw(lg, blink);
		}
	}
	logondraw(lg, 0);
}

static void
progressbar(Rectangle clip)
{
	Image *navy, *blue, *dim;
	Rectangle bar;
	char *s = "Starting Plan 9";
	int i, w;

	splashcolors(&navy, &blue, &dim);
	w = Nsegs*(SegW+SegGap) - SegGap;
	bar = Rect((clip.min.x+clip.max.x)/2 - w/2 - 2,
		(clip.min.y+clip.max.y)/2 - 8,
		(clip.min.x+clip.max.x)/2 + w/2 + 2,
		(clip.min.y+clip.max.y)/2 + 8);

	draw(screen, clip, display->black, nil, ZP);
	border(screen, bar, 1, navy, ZP);
	string(screen, Pt((clip.min.x+clip.max.x)/2 - stringwidth(font, s)/2,
		bar.max.y+16), display->white, ZP, font, s);
	flushimage(display, 1);

	for(i = 0; i < Nframes; i++){
		progressframe(bar, i % (Nsegs+3), blue, dim);
		flushimage(display, 1);
		sleep(FrameMs);
	}
}

void
splashthread(void*)
{
	Rectangle clip;
	Logon lg;
	int boot;

	threadsetname("splash");
	if(getenv("q9nosplash") != nil){
		sendul(splashdone, 1);
		sendul(splashdone, 1);
		sendul(splashdone, 1);
		return;
	}
	clip = screen->r;
	boot = bootmenu(clip);
	if(boot == BootSafe)
		panelsetedge("off");
	progressbar(clip);

	memset(&lg, 0, sizeof lg);
	lg.dlg = Rect((clip.min.x+clip.max.x)/2-190, (clip.min.y+clip.max.y)/2-105,
		(clip.min.x+clip.max.x)/2+190, (clip.min.y+clip.max.y)/2+105);
	lg.pass = Rect(lg.dlg.min.x+160, lg.dlg.min.y+77, lg.dlg.max.x-20, lg.dlg.min.y+77+font->height+6);
	lg.ok = Rect(lg.dlg.max.x-230, lg.dlg.max.y-58, lg.dlg.max.x-150, lg.dlg.max.y-34);
	lg.shut = Rect(lg.dlg.max.x-140, lg.dlg.max.y-58, lg.dlg.max.x-20, lg.dlg.max.y-34);
	lgface = getcolor(nil, 0xC0C0C0FF);
	lgnavy = getcolor(nil, 0x000080FF);
	lgdim = getcolor(nil, 0x808080FF);
	logon(&lg, clip);

	sendul(splashdone, 1);
	sendul(splashdone, 1);
	sendul(splashdone, 1);
}
