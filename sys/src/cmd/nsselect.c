#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>

enum {
	Maxsessions = 16,
	Fieldlen = 128,
	Cmdlen = 256,
	Buflen = 8192
};

typedef struct Session Session;
typedef struct Launch Launch;
struct Session {
	char id[Fieldlen];
	char name[Fieldlen];
	char desc[Fieldlen];
	char command[Cmdlen];
};
struct Launch {
	char id[Fieldlen];
	char command[Cmdlen];
};

Session sessions[Maxsessions];
int nsessions;
int selected;
int launchfd = -1;
int launchpid = -1;

Image *back;
Image *panel;
Image *panelhi;
Image *edge;
Image *text;
Image *muted;
Image *accent;
Mousectl *mousectl;
Keyboardctl *keyboardctl;
Alt alts[4];
Mouse mouse;
Rune key;
int resize;
int mainstacksize = 32*1024;

char *catalog = "/lib/namespace.sessions";

void
trim(char *s)
{
	char *e;

	while(*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
		memmove(s, s+1, strlen(s));
	e = s+strlen(s);
	while(e > s && (e[-1] == ' ' || e[-1] == '\t' ||
	    e[-1] == '\r' || e[-1] == '\n'))
		*--e = 0;
}

void
copystr(char *dst, int ndst, char *src)
{
	int n;

	if(ndst <= 0)
		return;
	dst[0] = 0;
	if(src == nil)
		return;
	n = strlen(src);
	if(n >= ndst)
		n = ndst-1;
	memmove(dst, src, n);
	dst[n] = 0;
}

int
loadsessions(char *path)
{
	char buf[Buflen+1], *lines[Maxsessions*2], *fields[4];
	int fd, n, nl, nf, i;

	fd = open(path, OREAD);
	if(fd < 0)
		return -1;
	n = read(fd, buf, Buflen);
	close(fd);
	if(n < 0)
		return -1;
	buf[n] = 0;
	nl = getfields(buf, lines, nelem(lines), 1, "\n");
	nsessions = 0;
	for(i = 0; i < nl && nsessions < Maxsessions; i++){
		trim(lines[i]);
		if(lines[i][0] == 0 || lines[i][0] == '#')
			continue;
		nf = getfields(lines[i], fields, nelem(fields), 0, "|");
		if(nf < 4)
			continue;
		trim(fields[0]);
		trim(fields[1]);
		trim(fields[2]);
		trim(fields[3]);
		if(fields[0][0] == 0 || fields[1][0] == 0 || fields[3][0] == 0)
			continue;
		copystr(sessions[nsessions].id, sizeof sessions[nsessions].id,
		    fields[0]);
		copystr(sessions[nsessions].name, sizeof sessions[nsessions].name,
		    fields[1]);
		copystr(sessions[nsessions].desc, sizeof sessions[nsessions].desc,
		    fields[2]);
		copystr(sessions[nsessions].command,
		    sizeof sessions[nsessions].command, fields[3]);
		nsessions++;
	}
	return nsessions;
}

Rectangle
cardrect(int i)
{
	int cols, rows, w, h, gap, totalw, totalh, x, y;
	Rectangle r;

	cols = nsessions > 1 ? 2 : 1;
	rows = (nsessions + cols - 1) / cols;
	w = 250;
	h = 104;
	gap = 18;
	totalw = cols*w + (cols-1)*gap;
	totalh = rows*h + (rows-1)*gap;
	x = screen->r.min.x + (Dx(screen->r)-totalw)/2;
	y = screen->r.min.y + (Dy(screen->r)-totalh)/2 + 24;
	r.min = Pt(x + (i%cols)*(w+gap), y + (i/cols)*(h+gap));
	r.max = addpt(r.min, Pt(w, h));
	return r;
}

void
drawcenter(char *s, int y, Image *color)
{
	int x;

	x = screen->r.min.x + (Dx(screen->r)-stringwidth(font, s))/2;
	string(screen, Pt(x, y), color, ZP, font, s);
}

void
drawsession(int i)
{
	Rectangle r, stripe;
	Point p;
	Image *fill;

	r = cardrect(i);
	fill = i == selected ? panelhi : panel;
	draw(screen, r, fill, nil, ZP);
	border(screen, r, 1, edge, ZP);
	stripe = r;
	stripe.max.y = stripe.min.y + 5;
	draw(screen, stripe, accent, nil, ZP);
	p = addpt(r.min, Pt(16, 22));
	string(screen, p, text, ZP, font, sessions[i].name);
	p.y += 24;
	string(screen, p, muted, ZP, font, sessions[i].desc);
	p.y += 26;
	string(screen, p, muted, ZP, font, sessions[i].id);
}

void
redraw(void)
{
	int i;

	draw(screen, screen->r, back, nil, ZP);
	drawcenter("Select namespace", screen->r.min.y + 76, text);
	drawcenter("Choose the session namespace to start", screen->r.min.y + 100,
	    muted);
	for(i = 0; i < nsessions; i++)
		drawsession(i);
	drawcenter("Enter starts  |  arrows move  |  Esc exits",
	    screen->r.max.y - 46, muted);
	flushimage(display, 1);
}

void
resized(void)
{
	if(getwindow(display, Refnone) < 0)
		sysfatal("can't reattach to window");
	redraw();
}

int
hit(Point p)
{
	int i;

	for(i = 0; i < nsessions; i++)
		if(ptinrect(p, cardrect(i)))
			return i;
	return -1;
}

void
waitmousefree(void)
{
	int fd, i;

	for(i = 0; i < 50; i++){
		fd = open("/dev/mouse", ORDWR);
		if(fd >= 0){
			close(fd);
			return;
		}
		sleep(100);
	}
}

int
readfull(int fd, void *buf, int n)
{
	char *p;
	int r, total;

	p = buf;
	total = 0;
	while(total < n){
		r = read(fd, p + total, n - total);
		if(r <= 0)
			return total;
		total += r;
	}
	return total;
}

int
writefull(int fd, void *buf, int n)
{
	char *p;
	int r, total;

	p = buf;
	total = 0;
	while(total < n){
		r = write(fd, p + total, n - total);
		if(r <= 0)
			return total;
		total += r;
	}
	return total;
}

void
launcher(int fd)
{
	Launch l;

	memset(&l, 0, sizeof l);
	if(readfull(fd, &l, sizeof l) != sizeof l)
		exits(nil);
	close(fd);
	if(l.id[0] == 0 || l.command[0] == 0)
		exits("empty launch");
	putenv("namespace", l.id);
	putenv("session", l.id);
	waitmousefree();
	execl("/bin/rc", "rc", "-c", l.command, nil);
	sysfatal("exec %s failed: %r", l.command);
}

void
startlauncher(void)
{
	int fd[2], pid;

	if(pipe(fd) < 0)
		sysfatal("launcher pipe failed: %r");
	switch(pid = rfork(RFPROC|RFFDG|RFENVG|RFNOTEG)){
	case -1:
		sysfatal("launcher rfork failed: %r");
	case 0:
		close(fd[1]);
		launcher(fd[0]);
		exits(nil);
	default:
		close(fd[0]);
		launchfd = fd[1];
		launchpid = pid;
		break;
	}
}

void
waitsession(void)
{
	Waitmsg *w;

	while((w = wait()) != nil){
		if(w->pid == launchpid){
			if(w->msg[0] != 0)
				exits(w->msg);
			free(w);
			return;
		}
		free(w);
	}
}

void
startsession(Session *s)
{
	Launch l;

	if(keyboardctl != nil){
		closekeyboard(keyboardctl);
		keyboardctl = nil;
	}
	if(mousectl != nil){
		closemouse(mousectl);
		mousectl = nil;
	}
	if(display != nil)
		closedisplay(display);
	memset(&l, 0, sizeof l);
	copystr(l.id, sizeof l.id, s->id);
	copystr(l.command, sizeof l.command, s->command);
	if(launchfd < 0 || writefull(launchfd, &l, sizeof l) != sizeof l)
		sysfatal("launch %s failed: %r", s->command);
	close(launchfd);
	launchfd = -1;
	waitsession();
	threadexitsall(nil);
}

void
usage(void)
{
	fprint(2, "usage: nsselect [-c catalog]\n");
	exits("usage");
}

void
threadmain(int argc, char **argv)
{
	int h;

	ARGBEGIN{
	case 'c':
		catalog = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND
	if(argc != 0)
		usage();

	if(loadsessions(catalog) <= 0)
		sysfatal("no namespace sessions in %s", catalog);
	startlauncher();
	if(initdraw(nil, nil, "namespace") < 0)
		sysfatal("initdraw failed: %r");
	mousectl = initmouse(nil, screen);
	if(mousectl == nil)
		sysfatal("initmouse failed: %r");
	keyboardctl = initkeyboard(nil);
	if(keyboardctl == nil)
		sysfatal("initkeyboard failed: %r");
	back = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x15202aff);
	panel = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x243240ff);
	panelhi = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x31516cff);
	edge = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x8bb7d9ff);
	text = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xf2f7fbff);
	muted = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xaebbc6ff);
	accent = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x70c0e8ff);
	alts[0].c = mousectl->c;
	alts[0].v = &mouse;
	alts[0].op = CHANRCV;
	alts[1].c = mousectl->resizec;
	alts[1].v = &resize;
	alts[1].op = CHANRCV;
	alts[2].c = keyboardctl->c;
	alts[2].v = &key;
	alts[2].op = CHANRCV;
	alts[3].op = CHANEND;
	redraw();
	for(;;){
		switch(alt(alts)){
		case 0:
			if((mouse.buttons & 1) == 0)
				break;
			h = hit(mouse.xy);
			if(h >= 0){
				selected = h;
				redraw();
				startsession(&sessions[selected]);
			}
			break;
		case 1:
			resized();
			break;
		case 2:
			switch(key){
			case Kleft:
			case Kup:
				if(selected > 0)
					selected--;
				redraw();
				break;
			case Kright:
			case Kdown:
			case '\t':
				if(selected < nsessions-1)
					selected++;
				redraw();
				break;
			case '\n':
			case '\r':
				startsession(&sessions[selected]);
				break;
			case Kesc:
			case Keof:
				threadexitsall("cancel");
			}
			break;
		}
	}
}
