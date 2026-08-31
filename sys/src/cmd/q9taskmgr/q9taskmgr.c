#include <u.h>
#include <libc.h>
#include "kryon.h"

/*
 * Kryon-native Task Manager: Applications lists rio windows
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

static Color
oc(Color c)
{
	c.a = 255;
	return c;
}

static Color
muted(float a)
{
	Color c;

	c = GetThemeText();
	c.a = (unsigned char)(255.0f * a);
	return c;
}

static Color
mkcol(int r, int g, int b)
{
	Color c;

	c.r = r;
	c.g = g;
	c.b = b;
	c.a = 255;
	return c;
}

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
		if(d[i].name[0] < '0' || d[i].name[0] > '9')
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

static Rectangle
tabrect(int i)
{
	Rectangle r;

	r.x = 10 + i*130;
	r.y = 8;
	r.width = 126;
	r.height = Tabh;
	return r;
}

static Rectangle
listarea(void)
{
	Rectangle r;

	r.x = 12;
	r.y = 8+Tabh+8;
	r.width = GetScreenWidth()-24;
	r.height = GetScreenHeight()-8-Tabh-8-52+8;
	return r;
}

static Rectangle
btnrect(int i)
{
	Rectangle r;

	r.y = GetScreenHeight()-40;
	r.width = Btnw;
	r.height = Btnh;
	switch(i){
	case 0: r.x = GetScreenWidth()-320; break;
	case 1: r.x = GetScreenWidth()-220; break;
	default: r.x = GetScreenWidth()-110; break;
	}
	return r;
}

static int
drawtab(int i)
{
	Rectangle r;
	Vector2 m;
	int hover;

	r = tabrect(i);
	m = GetMousePosition();
	hover = CheckCollisionPointRec(m, r);
	if(i == tab)
		DrawRectangleRec(r, oc(GetThemeSurface()));
	else if(hover)
		DrawRectangleRec(r, oc(GetThemeButtonHover()));
	DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height,
	    muted(0.4f));
	DrawText(tabnames[i],
	    (int)r.x + ((int)r.width - MeasureText(tabnames[i], 13))/2,
	    (int)r.y + (Tabh-13)/2, 13,
	    i == tab ? oc(GetThemeText()) : muted(0.7f));
	return hover && i != tab && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static int
drawbtn(Rectangle r, char *s)
{
	Vector2 m;
	int hover;

	m = GetMousePosition();
	hover = CheckCollisionPointRec(m, r);
	DrawRectangleRounded(r, 0.15f, 4,
	    hover ? oc(GetThemeButtonHover()) : oc(GetThemeButton()));
	DrawText(s, (int)r.x + ((int)r.width - MeasureText(s, 13))/2,
	    (int)r.y + ((int)r.height-13)/2, 13, oc(GetThemeText()));
	return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void
drawgraph(Rectangle r, double *h, int n, Color col)
{
	int i, x1, y1, x2, y2;

	DrawRectangleRec(r, Fade(GetThemeLink(), 0.25f));
	DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height,
	    muted(0.4f));
	for(i = 1; i < 4; i++)
		DrawLine((int)r.x, (int)r.y + (int)r.height*i/4,
		    (int)(r.x + r.width), (int)r.y + (int)r.height*i/4,
		    muted(0.2f));
	for(i = 1; i < 6; i++)
		DrawLine((int)r.x + (int)r.width*i/6, (int)r.y,
		    (int)r.x + (int)r.width*i/6, (int)(r.y + r.height),
		    muted(0.2f));
	for(i = 1; i < n; i++){
		x1 = (int)r.x + (int)r.width*(i-1)/(Nsample-1);
		y1 = (int)(r.y + r.height) - (int)(r.height*h[i-1]);
		x2 = (int)r.x + (int)r.width*i/(Nsample-1);
		y2 = (int)(r.y + r.height) - (int)(r.height*h[i]);
		DrawLine(x1, y1, x2, y2, col);
	}
}

/* one list row; returns the row index when clicked, -1 otherwise */
static int
drawrow(Rectangle lr, int row, char *s, int on)
{
	Rectangle r;
	Vector2 m;
	int hover;

	r.x = lr.x + 2;
	r.y = lr.y + Rowh + 4 + row*Rowh;
	r.width = lr.width - 4;
	r.height = Rowh;
	m = GetMousePosition();
	hover = CheckCollisionPointRec(m, r);
	if(on)
		DrawRectangleRec(r, oc(GetThemeButtonHover()));
	else if(hover)
		DrawRectangleRec(r, Fade(GetThemeButtonHover(), 0.4f));
	DrawText(s, (int)r.x + 6, (int)r.y + 2, 13,
	    on ? oc(GetThemeText()) : muted(0.85f));
	return hover ? (row >= 0 ? row : -1) : -1;
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
main(int argc, char *argv[])
{
	Rectangle lr, gr;
	char buf[128];
	Vector2 m;
	double now, nextrefresh;
	int i, nrows, done, row, wheel;

	ARGBEGIN{
	default:
		break;
	}ARGEND

	SetSingleInstance(0);
	InitWindow(560, 420, "q9taskmgr");
	if(!IsWindowReady())
		sysfatal("kryon window failed: %r");
	EnableEventWaiting();
	SetTargetFPS(30);
	SetUIDefaultFontAutoLoad(1);
	RefreshSystemTheme();
	SetThemeSource(THEME_SOURCE_SYSTEM);
	SetThemeMode(THEME_MODE_SYSTEM);
	SetThemeStyle(THEME_STYLE_SYSTEM);
	SetCurrentTheme(GetDefaultThemeForThemeStyle(THEME_STYLE_SYSTEM),
	    SystemThemePrefersDark());
	ApplyCurrentUITheme();

	readwins();
	readprocs();
	samplecpu();
	samplemem();
	nextrefresh = GetTime() + 1.0;

	done = 0;
	while(!done && !WindowShouldClose()){
		if(IsKeyPressed(KEY_ESCAPE))
			break;
		now = GetTime();
		if(now >= nextrefresh){
			readwins();
			readprocs();
			samplecpu();
			samplemem();
			nextrefresh = now + 1.0;
		}

		BeginDrawing();
		ClearBackground(oc(GetThemeBackground()));
		BeginUIFrame(GetScreenWidth(), GetScreenHeight(), 1.0f);
		BeginUI(0x7461736b);

		for(i = 0; i < NTabs; i++)
			if(drawtab(i))
				tab = i;

		lr = listarea();
		DrawRectangleRec(lr, oc(GetThemeSurface()));
		DrawRectangleLines((int)lr.x, (int)lr.y, (int)lr.width,
		    (int)lr.height, muted(0.4f));

		m = GetMousePosition();
		row = -1;
		wheel = (int)GetMouseWheelMove();
		switch(tab){
		case TabApps:
			DrawText("Task", (int)lr.x+4, (int)lr.y+4, 13,
			    oc(GetThemeText()));
			nrows = ((int)lr.height - Rowh - 4)/Rowh;
			for(i = 0; i < nrows && appsscroll+i < nwins; i++)
				if(drawrow(lr, i, wins[appsscroll+i].label,
				    appsscroll+i == winsel))
					row = appsscroll+i;
			if(wheel > 0 && appsscroll > 0)
				appsscroll -= wheel;
			if(wheel < 0 && appsscroll+nrows < nwins)
				appsscroll += -wheel;
			break;
		case TabProc:
			DrawText("PID", (int)lr.x+4, (int)lr.y+4, 13,
			    oc(GetThemeText()));
			DrawText("Name", (int)lr.x+70, (int)lr.y+4, 13,
			    oc(GetThemeText()));
			DrawText("User", (int)lr.x+250, (int)lr.y+4, 13,
			    oc(GetThemeText()));
			DrawText("State", (int)lr.x+390, (int)lr.y+4, 13,
			    oc(GetThemeText()));
			nrows = ((int)lr.height - Rowh - 4)/Rowh;
			for(i = 0; i < nrows && procscroll+i < nprocs; i++){
				snprint(buf, sizeof buf, "%-8d %-24s %-20s %-14s",
				    procs[procscroll+i].pid,
				    procs[procscroll+i].name,
				    procs[procscroll+i].user,
				    procs[procscroll+i].state);
				if(drawrow(lr, i, buf, procscroll+i == procsel))
					row = procscroll+i;
			}
			if(wheel > 0 && procscroll > 0)
				procscroll -= wheel;
			if(wheel < 0 && procscroll+nrows < nprocs)
				procscroll += -wheel;
			break;
		case TabPerf:
			DrawText("CPU Usage", (int)lr.x+4, (int)lr.y+4, 13,
			    oc(GetThemeText()));
			gr = lr;
			gr.x += 4;
			gr.y += 22;
			gr.width = gr.width*2/3 - 12;
			gr.height = (gr.height - 60)/2 - 4;
			drawgraph(gr, cpuhist, ncpu, mkcol(0x30, 0xC0, 0x50));
			snprint(buf, sizeof buf, "%d%%",
			    (int)(cpuhist[Nsample-1]*100));
			DrawText(buf, (int)(gr.x + gr.width + 12), (int)gr.y+4, 13,
			    oc(GetThemeText()));
			DrawText("Memory Usage", (int)lr.x+4,
			    (int)(gr.y + gr.height + 8), 13, oc(GetThemeText()));
			gr.y += gr.height + 26;
			drawgraph(gr, memhist, Nsample, mkcol(0xE0, 0x40, 0x40));
			snprint(buf, sizeof buf, "%d%%",
			    (int)(memhist[Nsample-1]*100));
			DrawText(buf, (int)(gr.x + gr.width + 12), (int)gr.y+4, 13,
			    oc(GetThemeText()));
			break;
		}

		if(tab == TabApps){
			if(drawbtn(btnrect(0), "End Task"))
				endtask(winsel);
			if(drawbtn(btnrect(1), "New Task"))
				newtask();
			if(drawbtn(btnrect(2), "Switch To"))
				switchto(winsel);
		}else if(tab == TabProc){
			if(drawbtn(btnrect(0), "End Process"))
				endprocess(procsel);
		}else{
			snprint(buf, sizeof buf, "Processes: %d   Tasks: %d   CPU: %d%%",
			    nprocs, nwins, (int)(cpuhist[Nsample-1]*100));
			DrawText(buf, 14, GetScreenHeight()-32, 13, muted(0.8f));
		}

		if(row >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
			if(tab == TabApps)
				winsel = row;
			else if(tab == TabProc)
				procsel = row;
		}

		EndUI();
		EndUIFrame();
		EndDrawing();
	}
	CloseWindow();
	exits(nil);
}
