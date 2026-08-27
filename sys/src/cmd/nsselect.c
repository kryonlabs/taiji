#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>

enum {
	Maxsessions = 16,
	Fieldlen = 128,
	Cmdlen = 256,
	Buflen = 8192
};

typedef struct Session Session;
struct Session {
	char id[Fieldlen];
	char name[Fieldlen];
	char desc[Fieldlen];
	char command[Cmdlen];
};

Session sessions[Maxsessions];
int nsessions;
int selected;

Image *back;
Image *panel;
Image *panelhi;
Image *edge;
Image *text;
Image *muted;
Image *accent;

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
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
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
startsession(Session *s)
{
	putenv("namespace", s->id);
	putenv("session", s->id);
	execl("/bin/rc", "rc", "-c", s->command, nil);
	sysfatal("exec %s failed: %r", s->command);
}

void
usage(void)
{
	fprint(2, "usage: nsselect [-c catalog]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	Event e;
	int key, h;

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
	if(initdraw(nil, nil, "namespace") < 0)
		sysfatal("initdraw failed: %r");
	back = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x15202aff);
	panel = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x243240ff);
	panelhi = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x31516cff);
	edge = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x8bb7d9ff);
	text = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xf2f7fbff);
	muted = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xaebbc6ff);
	accent = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x70c0e8ff);
	einit(Emouse|Ekeyboard);
	redraw();
	for(;;){
		key = event(&e);
		if(key == Emouse && (e.mouse.buttons & 1)){
			h = hit(e.mouse.xy);
			if(h >= 0){
				selected = h;
				redraw();
				startsession(&sessions[selected]);
			}
		}else if(key == Ekeyboard){
			switch(e.kbdc){
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
				exits("cancel");
			}
		}
	}
}
