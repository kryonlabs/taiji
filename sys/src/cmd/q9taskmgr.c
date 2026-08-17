#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>
#include <ctype.h>

/*
 * Windows-2000 style Task Manager: Applications lists rio windows
 * (End Task works through /dev/wsys), Processes lists /proc with
 * End Process, and Performance graphs CPU from /dev/cputime and
 * memory from /dev/swap. Refreshes once a second.
 */

enum {
	Tabh = 26,
	Btnw = 92,
	Btnh = 26,
	Rowh = 18,
	NTabs = 3,
	Maxwin = 64,
	Maxproc = 256,
	Nsample = 120,
};

enum {
	TabApps,
	TabProc,
	TabPerf,
};

typedef struct WinEnt WinEnt;
struct WinEnt {
	int id;
	char label[64];
};

typedef struct ProcEnt ProcEnt;
struct ProcEnt {
	int pid;
	char name[32];
	char user[32];
	char state[32];
};

char *tabnames[NTabs] = {
	"Applications",
	"Processes",
	"Performance",
};

void redraw(void);

Image *face, *light, *shadow, *dark, *white, *text, *hilite, *navy, *green, *red;
int tab;
WinEnt wins[Maxwin];
int nwins;
int winsel;
ProcEnt procs[Maxproc];
int nprocs;
int procsel;
int appsscroll, procscroll;

/* cpu sampling */
vlong lastuser, lastsys, lastidle;
double cpuhist[Nsample];
int ncpu;
vlong lasttotalmem, lastfreemem;
double memhist[Nsample];

static void
readwins(void)
{
	Dir *d;
	char path[128], buf[80];
	int fd, n, i, t, len;

	nwins = 0;
	fd = open("/dev/wsys", OREAD);
	if(fd < 0)
		return;
	n = dirreadall(fd, &d);
	close(fd);
	if(n < 0)
		return;
	for(i = 0; i < n && nwins < Maxwin; i++){
		t = atoi(d[i].name);
		if(t <= 0)
			continue;
		snprint(path, sizeof path, "/dev/wsys/%d/label", t);
		fd = open(path, OREAD);
		if(fd < 0)
			continue;
		len = read(fd, buf, sizeof(buf)-1);
		close(fd);
		if(len <= 0)
			continue;
		buf[len] = 0;
		while(len > 0 && (buf[len-1] == '\n' || buf[len-1] == ' '))
			buf[--len] = 0;
		if(strncmp(buf, "deleted", 7) == 0)
			continue;
		wins[nwins].id = t;
		strecpy(wins[nwins].label, wins[nwins].label+64, buf);
		nwins++;
	}
	free(d);
	if(winsel >= nwins)
		winsel = nwins-1;
}

static void
endtask(int i)
{
	char path[128];
	int fd;

	if(i < 0 || i >= nwins)
		return;
	snprint(path, sizeof path, "/dev/wsys/%d/wctl", wins[i].id);
	fd = open(path, OWRITE);
	if(fd >= 0){
		fprint(fd, "delete");
		close(fd);
	}
	sleep(200);
	readwins();
	redraw();
}

static void
readprocs(void)
{
	Dir *d;
	char path[128], buf[256];
	char *f[8];
	int fd, n, i, len, nf;

	nprocs = 0;
	fd = open("/proc", OREAD);
	if(fd < 0)
		return;
	n = dirreadall(fd, &d);
	close(fd);
	if(n < 0)
		return;
	for(i = 0; i < n && nprocs < Maxproc; i++){
		if(!isdigit(d[i].name[0]))
			continue;
		procs[nprocs].pid = atoi(d[i].name);
		snprint(path, sizeof path, "/proc/%s/status", d[i].name);
		fd = open(path, OREAD);
		if(fd < 0)
			continue;
		len = read(fd, buf, sizeof(buf)-1);
		close(fd);
		if(len <= 0)
			continue;
		buf[len] = 0;
		nf = tokenize(buf, f, nelem(f));
		strecpy(procs[nprocs].name, procs[nprocs].name+32, nf > 0 ? f[0] : "?");
		strecpy(procs[nprocs].user, procs[nprocs].user+32, nf > 1 ? f[1] : "?");
		strecpy(procs[nprocs].state, procs[nprocs].state+32, nf > 2 ? f[2] : "?");
		nprocs++;
	}
	free(d);
	if(procsel >= nprocs)
		procsel = nprocs-1;
}

static void
endprocess(int i)
{
	char path[128];
	int fd;

	if(i < 0 || i >= nprocs)
		return;
	snprint(path, sizeof path, "/proc/%d/note", procs[i].pid);
	fd = open(path, OWRITE);
	if(fd >= 0){
		fprint(fd, "kill");
		close(fd);
	}
	sleep(200);
	readprocs();
	redraw();
}

static void
samplecpu(void)
{
	char buf[256];
	char *f[8];
	int fd, n, nf;
	vlong user, sys, idle, total;
	double frac;

	fd = open("/dev/cputime", OREAD);
	if(fd < 0)
		return;
	n = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if(n <= 0)
		return;
	buf[n] = 0;
	nf = tokenize(buf, f, nelem(f));
	if(nf < 3)
		return;
	user = atoll(f[0]);
	sys = atoll(f[1]);
	idle = atoll(f[2]);
	total = (user-lastuser) + (sys-lastsys) + (idle-lastidle);
	if(total > 0)
		frac = (double)((user-lastuser) + (sys-lastsys)) / (double)total;
	else
		frac = 0;
	lastuser = user;
	lastsys = sys;
	lastidle = idle;
	memmove(cpuhist, cpuhist+1, (Nsample-1)*sizeof(double));
	cpuhist[Nsample-1] = frac;
	ncpu = Nsample;
}

static void
samplemem(void)
{
	char buf[256];
	char *f[16];
	int fd, n, nf;
	vlong total, used;
	double frac;

	fd = open("/dev/swap", OREAD);
	if(fd < 0)
		return;
	n = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if(n <= 0)
		return;
	buf[n] = 0;
	/* memory\n total used ... */
	nf = tokenize(buf, f, nelem(f));
	if(nf < 4)
		return;
	total = atoll(f[2]);
	used = atoll(f[3]);
	if(total > 0)
		frac = (double)used / (double)total;
	else
		frac = 0;
	memmove(memhist, memhist+1, (Nsample-1)*sizeof(double));
	memhist[Nsample-1] = frac;
}

void
bevel(Rectangle r, int down)
{
	Image *tl, *br;

	tl = down ? dark : light;
	br = down ? light : dark;
	line(screen, r.min, Pt(r.max.x-1, r.min.y), 0, 0, 0, tl, ZP);
	line(screen, r.min, Pt(r.min.x, r.max.y-1), 0, 0, 0, tl, ZP);
	line(screen, Pt(r.min.x, r.max.y-1), subpt(r.max, Pt(1,1)), 0, 0, 0, br, ZP);
	line(screen, Pt(r.max.x-1, r.min.y), subpt(r.max, Pt(1,1)), 0, 0, 0, br, ZP);
}

void
btn(Rectangle r, char *s)
{
	draw(screen, r, face, nil, ZP);
	bevel(r, 0);
	string(screen, Pt(r.min.x+8, r.min.y+(Dy(r)-font->height)/2), text, ZP, font, s);
}

static Rectangle
listarea(void)
{
	Rectangle r;

	r = Rect(screen->r.min.x+12, screen->r.min.y+8+Tabh+8,
		screen->r.max.x-12, screen->r.max.y-52);
	return r;
}

static void
drawgraph(Rectangle r, double *h, int n, Image *col)
{
	Point p, q;
	int i;

	draw(screen, r, navy, nil, ZP);
	for(i = 1; i < 4; i++)
		line(screen, Pt(r.min.x, r.min.y + Dy(r)*i/4), Pt(r.max.x, r.min.y + Dy(r)*i/4),
			0, 0, 0, hilite, ZP);
	for(i = 1; i < 6; i++)
		line(screen, Pt(r.min.x + Dx(r)*i/6, r.min.y), Pt(r.min.x + Dx(r)*i/6, r.max.y),
			0, 0, 0, hilite, ZP);
	for(i = 1; i < n; i++){
		p = Pt(r.min.x + Dx(r)*(i-1)/(Nsample-1), r.max.y - Dy(r)*h[i-1]);
		q = Pt(r.min.x + Dx(r)*i/(Nsample-1), r.max.y - Dy(r)*h[i]);
		line(screen, p, q, 0, 0, 0, col, ZP);
	}
}

void
redraw(void)
{
	Rectangle r, lr, row;
	char buf[128];
	int i, y, nrows;

	draw(screen, screen->r, face, nil, ZP);

	for(i = 0; i < NTabs; i++){
		r = Rect(screen->r.min.x+10+i*130, screen->r.min.y+8,
			screen->r.min.x+10+i*130+126, screen->r.min.y+8+Tabh);
		if(i == tab){
			draw(screen, r, white, nil, ZP);
			border(screen, r, 1, dark, ZP);
			string(screen, Pt(r.min.x+(Dx(r)-stringwidth(font, tabnames[i]))/2,
				r.min.y+(Tabh-font->height)/2), text, ZP, font, tabnames[i]);
		}else{
			draw(screen, r, face, nil, ZP);
			bevel(r, 0);
			string(screen, Pt(r.min.x+(Dx(r)-stringwidth(font, tabnames[i]))/2,
				r.min.y+(Tabh-font->height)/2), shadow, ZP, font, tabnames[i]);
		}
	}
	r = Rect(screen->r.min.x+10, screen->r.min.y+8+Tabh-1, screen->r.max.x-10, screen->r.max.y-46);
	draw(screen, r, white, nil, ZP);
	border(screen, r, 1, dark, ZP);

	lr = listarea();
	switch(tab){
	case TabApps:
		string(screen, Pt(lr.min.x+4, lr.min.y+4), text, ZP, font, "Task");
		y = lr.min.y + Rowh + 4;
		nrows = (lr.max.y - y)/Rowh;
		for(i = appsscroll; i < nwins && i < appsscroll+nrows; i++){
			row = Rect(lr.min.x+2, y, lr.max.x-2, y+Rowh);
			if(i == winsel){
				draw(screen, row, navy, nil, ZP);
				string(screen, Pt(row.min.x+6, row.min.y+2), white, ZP, font, wins[i].label);
			}else{
				string(screen, Pt(row.min.x+6, row.min.y+2), text, ZP, font, wins[i].label);
			}
			y += Rowh;
		}
		break;
	case TabProc:
		string(screen, Pt(lr.min.x+4, lr.min.y+4), text, ZP, font, "PID");
		string(screen, Pt(lr.min.x+70, lr.min.y+4), text, ZP, font, "Name");
		string(screen, Pt(lr.min.x+250, lr.min.y+4), text, ZP, font, "User");
		string(screen, Pt(lr.min.x+390, lr.min.y+4), text, ZP, font, "State");
		y = lr.min.y + Rowh + 4;
		nrows = (lr.max.y - y)/Rowh;
		for(i = procscroll; i < nprocs && i < procscroll+nrows; i++){
			row = Rect(lr.min.x+2, y, lr.max.x-2, y+Rowh);
			if(i == procsel){
				draw(screen, row, navy, nil, ZP);
				snprint(buf, sizeof buf, "%-8d %-24s %-20s %-14s",
					procs[i].pid, procs[i].name, procs[i].user, procs[i].state);
				string(screen, Pt(row.min.x+6, row.min.y+2), white, ZP, font, buf);
			}else{
				snprint(buf, sizeof buf, "%-8d %-24s %-20s %-14s",
					procs[i].pid, procs[i].name, procs[i].user, procs[i].state);
				string(screen, Pt(row.min.x+6, row.min.y+2), text, ZP, font, buf);
			}
			y += Rowh;
		}
		break;
	case TabPerf:
		string(screen, Pt(lr.min.x+4, lr.min.y+4), text, ZP, font, "CPU Usage");
		drawgraph(insetrect(Rect(lr.min.x+4, lr.min.y+22, lr.min.x+4+Dx(lr)*2/3-8, lr.min.y+22+(Dy(lr)-60)/2), 1),
			cpuhist, ncpu, green);
		snprint(buf, sizeof buf, "%d%%", (int)(cpuhist[Nsample-1]*100));
		string(screen, Pt(lr.min.x+8+Dx(lr)*2/3-8+16, lr.min.y+26), text, ZP, font, buf);
		string(screen, Pt(lr.min.x+4, lr.min.y+22+(Dy(lr)-60)/2+8), text, ZP, font, "Memory Usage");
		drawgraph(insetrect(Rect(lr.min.x+4, lr.min.y+22+(Dy(lr)-60)/2+26,
			lr.min.x+4+Dx(lr)*2/3-8, lr.min.y+22+(Dy(lr)-60)/2+26+(Dy(lr)-60)/2), 1),
			memhist, Nsample, red);
		snprint(buf, sizeof buf, "%d%%", (int)(memhist[Nsample-1]*100));
		string(screen, Pt(lr.min.x+8+Dx(lr)*2/3-8+16, lr.min.y+22+(Dy(lr)-60)/2+30), text, ZP, font, buf);
		break;
	}

	if(tab == TabApps){
		btn(Rect(screen->r.max.x-320, screen->r.max.y-40, screen->r.max.x-320+Btnw, screen->r.max.y-40+Btnh), "End Task");
		btn(Rect(screen->r.max.x-220, screen->r.max.y-40, screen->r.max.x-220+Btnw+10, screen->r.max.y-40+Btnh), "New Task");
		btn(Rect(screen->r.max.x-110, screen->r.max.y-40, screen->r.max.x-110+Btnw, screen->r.max.y-40+Btnh), "Switch To");
	}else if(tab == TabProc){
		btn(Rect(screen->r.max.x-320, screen->r.max.y-40, screen->r.max.x-320+Btnw+10, screen->r.max.y-40+Btnh), "End Process");
	}else{
		snprint(buf, sizeof buf, "Processes: %d   Tasks: %d   CPU: %d%%",
			nprocs, nwins, (int)(cpuhist[Nsample-1]*100));
		string(screen, Pt(screen->r.min.x+14, screen->r.max.y-32), text, ZP, font, buf);
	}
	flushimage(display, 1);
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		exits("resize");
	redraw();
}

static int
tabat(Point p)
{
	int i;
	Rectangle r;

	for(i = 0; i < NTabs; i++){
		r = Rect(screen->r.min.x+10+i*130, screen->r.min.y+8,
			screen->r.min.x+10+i*130+126, screen->r.min.y+8+Tabh);
		if(ptinrect(p, r))
			return i;
	}
	return -1;
}

static int
rowat(Point p)
{
	Rectangle lr;
	int y, i, nrows;

	lr = listarea();
	if(!ptinrect(p, lr))
		return -1;
	if(tab == TabPerf)
		return -1;
	y = lr.min.y + Rowh + 4;
	nrows = (lr.max.y - y)/Rowh;
	i = (p.y - y)/Rowh;
	if(i < 0 || i >= nrows)
		return -1;
	if(tab == TabApps)
		return appsscroll + i;
	return procscroll + i;
}

static void
newtask(void)
{
	int fd;

	fd = open("/dev/wctl", OWRITE);
	if(fd >= 0){
		fprint(fd, "new -r 100 100 700 480 rc -i");
		close(fd);
	}
}

static void
switchto(int i)
{
	char path[128];
	int fd;

	if(i < 0 || i >= nwins)
		return;
	snprint(path, sizeof path, "/dev/wsys/%d/wctl", wins[i].id);
	fd = open(path, OWRITE);
	if(fd >= 0){
		fprint(fd, "current");
		close(fd);
	}
}

void
main(int argc, char **argv)
{
	Event e;
	int i, buttons;
	ulong key, timerkey;

	ARGBEGIN{
	default:
		break;
	}ARGEND
	if(initdraw(nil, nil, "q9taskmgr") < 0)
		sysfatal("initdraw: %r");
	face = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xC0C0C0FF);
	light = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xFFFFFFFF);
	shadow = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x808080FF);
	dark = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x000000FF);
	white = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xFFFFFFFF);
	text = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x000000FF);
	hilite = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xE8E8E8FF);
	navy = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x000080FF);
	green = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x00C000FF);
	red = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xE02020FF);
	einit(Emouse|Ekeyboard);
	timerkey = etimer(0, 1000);

	readwins();
	readprocs();
	samplecpu();
	samplemem();
	redraw();
	buttons = 0;
	for(;;){
		key = event(&e);
		if(key == timerkey){
			readwins();
			readprocs();
			samplecpu();
			samplemem();
			redraw();
			continue;
		}
		if(key != Emouse && key != Ekeyboard)
			continue;
		switch(key){
		case Emouse:
			if((e.mouse.buttons & 1) && !(buttons & 1)){
				i = tabat(e.mouse.xy);
				if(i >= 0 && i != tab){
					tab = i;
					redraw();
					break;
				}
				if(tab == TabApps){
					if(ptinrect(e.mouse.xy, Rect(screen->r.max.x-320, screen->r.max.y-40,
						screen->r.max.x-320+Btnw, screen->r.max.y-40+Btnh))){
						endtask(winsel);
						break;
					}
					if(ptinrect(e.mouse.xy, Rect(screen->r.max.x-220, screen->r.max.y-40,
						screen->r.max.x-220+Btnw+10, screen->r.max.y-40+Btnh))){
						newtask();
						break;
					}
					if(ptinrect(e.mouse.xy, Rect(screen->r.max.x-110, screen->r.max.y-40,
						screen->r.max.x-110+Btnw, screen->r.max.y-40+Btnh))){
						switchto(winsel);
						break;
					}
				}else if(tab == TabProc){
					if(ptinrect(e.mouse.xy, Rect(screen->r.max.x-320, screen->r.max.y-40,
						screen->r.max.x-320+Btnw+10, screen->r.max.y-40+Btnh))){
						endprocess(procsel);
						break;
					}
				}
				i = rowat(e.mouse.xy);
				if(i >= 0){
					if(tab == TabApps)
						winsel = i;
					else
						procsel = i;
					redraw();
				}
			}
			if((e.mouse.buttons & 8) && !(buttons & 8)){
				if(tab == TabApps && appsscroll > 0){
					appsscroll--;
					redraw();
				}
				if(tab == TabProc && procscroll > 0){
					procscroll--;
					redraw();
				}
			}
			if((e.mouse.buttons & 16) && !(buttons & 16)){
				if(tab == TabApps && appsscroll+1 < nwins){
					appsscroll++;
					redraw();
				}
				if(tab == TabProc && procscroll+1 < nprocs){
					procscroll++;
					redraw();
				}
			}
			buttons = e.mouse.buttons;
			break;
		case Ekeyboard:
			if(e.kbdc == Kdel || e.kbdc == 'q')
				exits(nil);
			break;
		}
	}
}
