#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>

enum {
	Panelh = 26,
	Startw = 72,
	Clockw = 128,
	TaskMinw = 72,
	TaskMaxw = 180,
	Menuw = 132,
	Menuh = 24,
	Maxtasks = 64,
};

typedef struct Task Task;
struct Task {
	int id;
	Rectangle r;
	char label[64];
};

Image *face;
Image *light;
Image *light2;
Image *shadow;
Image *dark;
Image *text;
Image *selbg;
Image *seltext;
Rectangle startr;
Rectangle clockr;
Rectangle taskr;
char clockbuf[64];
Task tasks[Maxtasks];
int ntasks;
char *wsysdir;

char*
findwsysdir(void)
{
	if(access("/dev/wsys", AEXIST) == 0)
		return "/dev/wsys";
	if(access("/mnt/wsys/wsys", AEXIST) == 0)
		return "/mnt/wsys/wsys";
	return nil;
}

int
readfile(char *path, char *buf, int nbuf)
{
	int fd, n;

	fd = open(path, OREAD);
	if(fd < 0)
		return -1;
	n = read(fd, buf, nbuf-1);
	close(fd);
	if(n < 0)
		return -1;
	buf[n] = 0;
	while(n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')){
		buf[--n] = 0;
	}
	return n;
}

void
readtasks(void)
{
	Dir *d;
	char buf[128], path[128];
	int fd, i, id, n, nd;

	ntasks = 0;
	if(wsysdir == nil)
		wsysdir = findwsysdir();
	if(wsysdir == nil)
		return;
	fd = open(wsysdir, OREAD);
	if(fd < 0)
		return;
	nd = dirreadall(fd, &d);
	close(fd);
	if(nd < 0)
		return;
	for(i = 0; i < nd && ntasks < Maxtasks; i++){
		id = atoi(d[i].name);
		if(id <= 0)
			continue;
		snprint(path, sizeof path, "%s/%d/label", wsysdir, id);
		n = readfile(path, buf, sizeof buf);
		if(n <= 0)
			continue;
		if(strcmp(buf, "lpanel") == 0)
			continue;
		tasks[ntasks].id = id;
		strecpy(tasks[ntasks].label, tasks[ntasks].label+sizeof tasks[ntasks].label, buf);
		ntasks++;
	}
	free(d);
}

void
bevel(Image *img, Rectangle r, int down)
{
	Image *tl, *br;

	tl = down ? dark : light2;
	br = down ? light2 : dark;
	line(img, r.min, Pt(r.max.x-1, r.min.y), 0, 0, 0, tl, ZP);
	line(img, r.min, Pt(r.min.x, r.max.y-1), 0, 0, 0, tl, ZP);
	line(img, Pt(r.min.x, r.max.y-1), subpt(r.max, Pt(1,1)), 0, 0, 0, br, ZP);
	line(img, Pt(r.max.x-1, r.min.y), subpt(r.max, Pt(1,1)), 0, 0, 0, br, ZP);
	r = insetrect(r, 1);
	tl = down ? shadow : light;
	br = down ? light : shadow;
	line(img, r.min, Pt(r.max.x-1, r.min.y), 0, 0, 0, tl, ZP);
	line(img, r.min, Pt(r.min.x, r.max.y-1), 0, 0, 0, tl, ZP);
	line(img, Pt(r.min.x, r.max.y-1), subpt(r.max, Pt(1,1)), 0, 0, 0, br, ZP);
	line(img, Pt(r.max.x-1, r.min.y), subpt(r.max, Pt(1,1)), 0, 0, 0, br, ZP);
}

void
drawtext(Rectangle r, char *s, int center)
{
	Point p;

	p = Pt(r.min.x+6, r.min.y+(Dy(r)-font->height)/2);
	if(center)
		p.x = r.min.x + (Dx(r)-stringwidth(font, s))/2;
	if(p.x < r.min.x+4)
		p.x = r.min.x+4;
	string(screen, p, text, ZP, font, s);
}

void
drawitem(Rectangle r, char *s, int hover)
{
	Point p;
	Image *fg;

	draw(screen, r, hover ? selbg : face, nil, ZP);
	p = Pt(r.min.x+8, r.min.y+(Dy(r)-font->height)/2);
	fg = hover ? seltext : text;
	string(screen, p, fg, ZP, font, s);
}

void
setclock(void)
{
	Tm *tm;

	tm = localtime(time(nil));
	snprint(clockbuf, sizeof clockbuf, "%04d-%02d-%02d %02d:%02d",
		tm->year+1900, tm->mon+1, tm->mday, tm->hour, tm->min);
}

void
redraw(void)
{
	int i, n, w, x;
	Rectangle r;

	setclock();
	readtasks();
	draw(screen, screen->r, face, nil, ZP);
	bevel(screen, screen->r, 0);
	startr = screen->r;
	startr.max.x = startr.min.x + Startw;
	startr = insetrect(startr, 3);
	draw(screen, startr, face, nil, ZP);
	bevel(screen, startr, 0);
	drawtext(startr, "Start", 1);
	clockr = screen->r;
	clockr.min.x = clockr.max.x - Clockw;
	clockr = insetrect(clockr, 3);
	draw(screen, clockr, face, nil, ZP);
	bevel(screen, clockr, 1);
	drawtext(clockr, clockbuf, 1);
	r = screen->r;
	r = insetrect(r, 2);
	r.min.x = startr.max.x + 6;
	r.max.x = clockr.min.x - 6;
	draw(screen, r, face, nil, ZP);
	taskr = r;
	n = ntasks;
	if(n > 0){
		w = Dx(taskr)/n;
		if(w < TaskMinw)
			w = TaskMinw;
		if(w > TaskMaxw)
			w = TaskMaxw;
		x = taskr.min.x;
		for(i = 0; i < n; i++){
			tasks[i].r = Rect(x, taskr.min.y+3, x+w-4, taskr.max.y-3);
			x += w;
			if(tasks[i].r.max.x > taskr.max.x){
				tasks[i].r = ZR;
				continue;
			}
			draw(screen, tasks[i].r, face, nil, ZP);
			bevel(screen, tasks[i].r, 0);
			drawtext(insetrect(tasks[i].r, 2), tasks[i].label, 0);
		}
	}
	flushimage(display, 1);
}

void
focustask(int id)
{
	char path[128];
	int fd;

	if(wsysdir == nil)
		wsysdir = findwsysdir();
	if(wsysdir == nil)
		return;
	snprint(path, sizeof path, "%s/%d/wctl", wsysdir, id);
	fd = open(path, OWRITE);
	if(fd < 0)
		return;
	fprint(fd, "unhide\ncurrent\n");
	close(fd);
}

void
run(char *cmd)
{
	int fd;

	fd = open("/dev/wctl", OWRITE);
	if(fd < 0)
		return;
	fprint(fd, "new -r 40 70 680 500 %s", cmd);
	close(fd);
}

void
startmenu(Mouse *m)
{
	Event e;
	int fd;

	fd = open("/dev/wctl", OWRITE);
	if(fd < 0)
		return;
	fprint(fd, "new -r %d %d %d %d -noborder -notitle lpanel -menu",
		screen->r.min.x, screen->r.max.y,
		screen->r.min.x+Menuw, screen->r.max.y+6*Menuh+6);
	close(fd);
	while(m->buttons)
		if(eread(Emouse, &e) == Emouse)
			*m = e.mouse;
}

void
menumode(void)
{
	static char *items[] = {
		"New rc",
		"Acme",
		"Stats",
		"Kbmap",
		"Page",
		"Exit panel",
		nil,
	};
	Event e;
	Rectangle r;
	int hover, i, lasthover, n, sel;

	for(n = 0; items[n] != nil; n++)
		;
	draw(screen, screen->r, face, nil, ZP);
	bevel(screen, screen->r, 0);
	for(i = 0; i < n; i++){
		r = Rect(screen->r.min.x+3, screen->r.min.y+3+i*Menuh,
			screen->r.max.x-3, screen->r.min.y+3+(i+1)*Menuh);
		drawitem(r, items[i], 0);
	}
	flushimage(display, 1);
	lasthover = -1;
	for(;;){
		if(event(&e) != Emouse)
			continue;
		if(e.mouse.buttons & 4)
			exits(nil);
		hover = -1;
		if(ptinrect(e.mouse.xy, insetrect(screen->r, 3))){
			hover = (e.mouse.xy.y - screen->r.min.y - 3)/Menuh;
			if(hover < 0 || hover >= n)
				hover = -1;
		}
		if(hover != lasthover){
			if(lasthover >= 0){
				r = Rect(screen->r.min.x+3, screen->r.min.y+3+lasthover*Menuh,
					screen->r.max.x-3, screen->r.min.y+3+(lasthover+1)*Menuh);
				drawitem(r, items[lasthover], 0);
			}
			if(hover >= 0){
				r = Rect(screen->r.min.x+3, screen->r.min.y+3+hover*Menuh,
					screen->r.max.x-3, screen->r.min.y+3+(hover+1)*Menuh);
				drawitem(r, items[hover], 1);
			}
			flushimage(display, 1);
			lasthover = hover;
		}
		if((e.mouse.buttons & 1) == 0)
			continue;
		sel = hover;
		while(e.mouse.buttons)
			eread(Emouse, &e);
		if(sel < 0 || sel >= n)
			exits(nil);
		break;
	}
	switch(sel){
	case 0:
		run("rc -i");
		break;
	case 1:
		run("acme");
		break;
	case 2:
		run("stats -lmisce");
		break;
	case 3:
		run("q9kbsetup -reset");
		break;
	case 4:
		run("page");
		break;
	case 5:
		exits(nil);
	}
	exits(nil);
}

void
borderless(void)
{
	int fd;
	Rectangle r;

	fd = open("/dev/wctl", OWRITE);
	if(fd < 0)
		return;
	r = screen->r;
	r.max.y = r.min.y + Panelh;
	fprint(fd, "noborder\nnotitle\nresize -r %d %d %d %d\ncurrent\n",
		r.min.x, r.min.y, r.max.x, r.max.y);
	close(fd);
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		exits("resize");
	redraw();
}

void
main(int argc, char **argv)
{
	Event e;
	int timer, k;

	if(initdraw(nil, nil, "lpanel") < 0)
		sysfatal("initdraw: %r");
	face = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xC0C0C0FF);
	light = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xC0C0C0FF);
	light2 = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xFFFFFFFF);
	shadow = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x808080FF);
	dark = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x000000FF);
	text = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x000000FF);
	selbg = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x000080FF);
	seltext = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xFFFFFFFF);
	einit(Emouse);
	if(argc > 1 && strcmp(argv[1], "-menu") == 0)
		menumode();
	borderless();
	redraw();
	timer = etimer(0, 1000);
	for(;;){
		k = event(&e);
		if(k == Emouse){
			if((e.mouse.buttons & 1) && ptinrect(e.mouse.xy, startr))
				startmenu(&e.mouse);
			else if(e.mouse.buttons & 1){
				int i;
				for(i = 0; i < ntasks; i++){
					if(Dx(tasks[i].r) > 0 && ptinrect(e.mouse.xy, tasks[i].r)){
						focustask(tasks[i].id);
						while(e.mouse.buttons)
							eread(Emouse, &e);
						redraw();
						break;
					}
				}
			}
			else if(e.mouse.buttons & 4)
				exits(nil);
		}else if(k == timer)
			redraw();
	}
}
