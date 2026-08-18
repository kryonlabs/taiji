#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>

enum {
	Menuh = 22,
	Toolbarh = 30,
	Addrh = 24,
	Statush = 22,
	Treew = 170,
	Rowh = 18,
	Btnw = 70,
	Iconw = 108,
	Iconh = 72,
	Maxpath = 512,
	Maxhist = 32,
	Mitemh = 20,
};

enum {
	ViewList,
	ViewIcon,
};

typedef struct Entry Entry;
typedef struct TreeItem TreeItem;
struct Entry {
	char name[128];
	char type[16];
	vlong len;
	ulong mtime;
	int isdir;
};
struct TreeItem {
	char *label;
	char *path;
	int indent;
	int y;
	Rectangle r;
};

Image *face, *light, *shadow, *dark, *white, *text, *hilite, *yellow, *blue, *navy, *green, *red;
Entry *ents;
int nents;
char cwd[Maxpath];
int scroll;
int selectedtree = -1;
int viewmode = ViewList;
int sel = -1;			/* selected entry */
int mycomp;			/* My Computer special view */
char clipboard[Maxpath];	/* copied/cut path */
int clipcut;
static char hist[Maxhist][Maxpath];
static int nhist;
static int hpos;
static ulong lastclick;
static int lastsel;
TreeItem tree[] = {
	{ "My Computer", "/mycomputer", 10, 10 },
	{ "Namespace", "/", 22, 32 },
	{ "/", "/", 34, 54 },
	{ "/mnt", "/mnt", 34, 76 },
	{ "/usr/glenda", "/usr/glenda", 34, 98 },
	{ "Recycle Bin", "", 22, 120 },
	{ "Control Panel", "/lib/controlpanel", 22, 142 },
};

Rectangle listrect(void);
void redraw(void);
static void msgbox(char*, char*);
static int mcitemat(Point);
static void menubar(Rectangle);
static void newdir(void);
static void pasteclip(void);
static int menuhitx(Point, char**);

void
settreeforpath(char *path)
{
	int i;

	selectedtree = -1;
	for(i = 0; i < nelem(tree); i++)
		if(strcmp(path, tree[i].path) == 0)
			selectedtree = i;
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

void
drawfileicon(Rectangle r, int isdir, int big)
{
	Point p;

	if(big){
		p = Pt((r.min.x+r.max.x)/2-16, r.min.y+8);
		if(isdir){
			draw(screen, Rect(p.x, p.y+8, p.x+34, p.y+30), yellow, nil, ZP);
			draw(screen, Rect(p.x+4, p.y+4, p.x+18, p.y+10), yellow, nil, ZP);
			border(screen, Rect(p.x, p.y+8, p.x+34, p.y+30), 1, dark, ZP);
		}else{
			draw(screen, Rect(p.x+5, p.y+2, p.x+29, p.y+34), white, nil, ZP);
			border(screen, Rect(p.x+5, p.y+2, p.x+29, p.y+34), 1, dark, ZP);
			line(screen, Pt(p.x+10, p.y+12), Pt(p.x+24, p.y+12), 0, 0, 0, shadow, ZP);
			line(screen, Pt(p.x+10, p.y+18), Pt(p.x+24, p.y+18), 0, 0, 0, shadow, ZP);
		}
		return;
	}
	if(isdir){
		draw(screen, Rect(r.min.x+3, r.min.y+6, r.min.x+19, r.min.y+16), yellow, nil, ZP);
		draw(screen, Rect(r.min.x+5, r.min.y+3, r.min.x+13, r.min.y+8), yellow, nil, ZP);
		border(screen, Rect(r.min.x+3, r.min.y+6, r.min.x+19, r.min.y+16), 1, dark, ZP);
	}else{
		draw(screen, Rect(r.min.x+5, r.min.y+3, r.min.x+17, r.min.y+17), white, nil, ZP);
		border(screen, Rect(r.min.x+5, r.min.y+3, r.min.x+17, r.min.y+17), 1, dark, ZP);
		line(screen, Pt(r.min.x+8, r.min.y+8), Pt(r.min.x+14, r.min.y+8), 0, 0, 0, shadow, ZP);
		line(screen, Pt(r.min.x+8, r.min.y+12), Pt(r.min.x+14, r.min.y+12), 0, 0, 0, shadow, ZP);
	}
}

int
controlpanel(void)
{
	return strcmp(cwd, "/lib/controlpanel") == 0;
}

static char *
trashdir(void)
{
	static char t[Maxpath];
	char *home;

	home = getenv("home");
	if(home == nil)
		strecpy(t, t+sizeof t, "/tmp");
	else{
		snprint(t, sizeof t, "%s/.trash", home);
		free(home);
	}
	return t;
}

int
trashview(void)
{
	return strcmp(cwd, trashdir()) == 0;
}

void
drawcpicon(Rectangle r, char *name)
{
	Point c;

	c = Pt((r.min.x+r.max.x)/2, r.min.y+22);
	if(strcmp(name, "Display") == 0){
		draw(screen, Rect(c.x-15, c.y-10, c.x+15, c.y+8), hilite, nil, ZP);
		border(screen, Rect(c.x-15, c.y-10, c.x+15, c.y+8), 2, dark, ZP);
		draw(screen, Rect(c.x-5, c.y+9, c.x+5, c.y+13), dark, nil, ZP);
	}else if(strcmp(name, "Keyboard") == 0){
		draw(screen, Rect(c.x-16, c.y-8, c.x+16, c.y+11), white, nil, ZP);
		border(screen, Rect(c.x-16, c.y-8, c.x+16, c.y+11), 1, dark, ZP);
		line(screen, Pt(c.x-12, c.y-1), Pt(c.x+12, c.y-1), 0, 0, 0, dark, ZP);
		line(screen, Pt(c.x-12, c.y+5), Pt(c.x+12, c.y+5), 0, 0, 0, dark, ZP);
	}else if(strcmp(name, "Mouse") == 0){
		ellipse(screen, c, 10, 15, 1, white, ZP);
		ellipse(screen, c, 10, 15, 0, dark, ZP);
		line(screen, Pt(c.x, c.y-12), Pt(c.x, c.y-2), 0, 0, 0, dark, ZP);
	}else if(strcmp(name, "Network") == 0){
		ellipse(screen, Pt(c.x-11, c.y-5), 5, 5, 1, hilite, ZP);
		ellipse(screen, Pt(c.x+11, c.y-5), 5, 5, 1, hilite, ZP);
		ellipse(screen, Pt(c.x, c.y+10), 5, 5, 1, hilite, ZP);
		line(screen, Pt(c.x-7, c.y-1), Pt(c.x-2, c.y+6), 0, 0, 0, dark, ZP);
		line(screen, Pt(c.x+7, c.y-1), Pt(c.x+2, c.y+6), 0, 0, 0, dark, ZP);
	}else if(strcmp(name, "Date-Time") == 0){
		ellipse(screen, c, 14, 14, 1, white, ZP);
		ellipse(screen, c, 14, 14, 0, dark, ZP);
		line(screen, c, Pt(c.x, c.y-9), 0, 0, 0, dark, ZP);
		line(screen, c, Pt(c.x+8, c.y+4), 0, 0, 0, dark, ZP);
	}else if(strcmp(name, "Users") == 0){
		ellipse(screen, Pt(c.x, c.y-7), 6, 6, 1, hilite, ZP);
		ellipse(screen, Pt(c.x, c.y-7), 6, 6, 0, dark, ZP);
		draw(screen, Rect(c.x-13, c.y+3, c.x+13, c.y+15), hilite, nil, ZP);
	}else if(strcmp(name, "Fonts") == 0){
		string(screen, Pt(c.x-12, c.y-12), dark, ZP, font, "A");
		string(screen, Pt(c.x, c.y-3), dark, ZP, font, "a");
	}else{
		draw(screen, Rect(c.x-14, c.y-13, c.x+14, c.y+13), white, nil, ZP);
		border(screen, Rect(c.x-14, c.y-13, c.x+14, c.y+13), 1, dark, ZP);
		line(screen, Pt(c.x-9, c.y-5), Pt(c.x+9, c.y-5), 0, 0, 0, dark, ZP);
		line(screen, Pt(c.x-9, c.y+2), Pt(c.x+9, c.y+2), 0, 0, 0, dark, ZP);
		line(screen, Pt(c.x-9, c.y+9), Pt(c.x+9, c.y+9), 0, 0, 0, dark, ZP);
	}
}

void
runentry(char *path)
{
	int fd;

	fd = open("/dev/wctl", OWRITE);
	if(fd < 0)
		return;
	fprint(fd, "new -r 90 100 690 470 rc %s", path);
	close(fd);
}

void
openfile(char *path)
{
	int fd;

	fd = open("/dev/wctl", OWRITE);
	if(fd < 0)
		return;
	fprint(fd, "new -r 90 100 790 570 jot %s", path);
	close(fd);
}

int
iconitemat(Point p)
{
	Rectangle lr, r;
	int i, x, y;

	lr = listrect();
	x = lr.min.x + 20;
	y = lr.min.y + 20;
	for(i = scroll; i < nents; i++){
		r = Rect(x, y, x+Iconw, y+Iconh);
		if(ptinrect(p, r))
			return i;
		x += Iconw + 14;
		if(x+Iconw > lr.max.x-12){
			x = lr.min.x + 20;
			y += Iconh + 12;
		}
		if(y+Iconh > lr.max.y-8)
			break;
	}
	return -1;
}

int
cpitemat(Point p)
{
	Rectangle lr, r;
	int i, x, y;

	lr = listrect();
	x = lr.min.x + 20;
	y = lr.min.y + 20;
	for(i = 0; i < nents; i++){
		if(strcmp(ents[i].name, "..") == 0)
			continue;
		r = Rect(x, y, x+108, y+72);
		if(ptinrect(p, r))
			return i;
		x += 122;
		if(x+108 > lr.max.x-12){
			x = lr.min.x + 20;
			y += 84;
		}
		if(y+72 > lr.max.y-8)
			break;
	}
	return -1;
}

/* unified: which entry is at p, in any view */
int
itemat(Point p)
{
	Rectangle lr;
	int i;

	if(mycomp)
		return mcitemat(p);
	if(controlpanel())
		return cpitemat(p);
	lr = listrect();
	if(!ptinrect(p, lr))
		return -1;
	if(viewmode == ViewIcon)
		return iconitemat(p);
	i = scroll + (p.y - (lr.min.y+27))/Rowh;
	if(i < 0 || i >= nents)
		return -1;
	return i;
}

void
cleanpath(char *dst, int ndst, char *base, char *name)
{
	char buf[Maxpath];

	if(strcmp(name, "..") == 0){
		strecpy(buf, buf+sizeof buf, base);
		cleanname(buf);
		if(strcmp(buf, "/") != 0)
			*strrchr(buf, '/') = 0;
		if(buf[0] == 0)
			strecpy(buf, buf+sizeof buf, "/");
	}else if(strcmp(base, "/") == 0)
		snprint(buf, sizeof buf, "/%s", name);
	else
		snprint(buf, sizeof buf, "%s/%s", base, name);
	cleanname(buf);
	strecpy(dst, dst+ndst, buf);
}

int
loaddir(char *path)
{
	Dir *d;
	int fd, i, n, off;

	fd = open(path, OREAD);
	if(fd < 0)
		return -1;
	n = dirreadall(fd, &d);
	close(fd);
	if(n < 0)
		return -1;
	free(ents);
	ents = nil;
	nents = 0;
	ents = malloc((n+1)*sizeof(Entry));
	if(ents == nil){
		free(d);
		return -1;
	}
	off = 0;
	strecpy(ents[off].name, ents[off].name+sizeof ents[off].name, "..");
	strecpy(ents[off].type, ents[off].type+sizeof ents[off].type, "Folder");
	ents[off].isdir = 1;
	ents[off].len = 0;
	ents[off].mtime = 0;
	off++;
	for(i = 0; i < n; i++){
		strecpy(ents[off].name, ents[off].name+sizeof ents[off].name, d[i].name);
		strecpy(ents[off].type, ents[off].type+sizeof ents[off].type,
			(d[i].mode&DMDIR) ? "Folder" : "File");
		ents[off].isdir = (d[i].mode&DMDIR) != 0;
		ents[off].len = d[i].length;
		ents[off].mtime = d[i].mtime;
		off++;
	}
	nents = off;
	scroll = 0;
	sel = -1;
	strecpy(cwd, cwd+sizeof cwd, path);
	settreeforpath(cwd);
	free(d);
	return 0;
}

/* navigate with Back/Forward history */
void
navigate(char *path)
{
	char clean[Maxpath];

	if(strcmp(path, "/mycomputer") == 0){
		mycomp = 1;
		selectedtree = 0;
		nhist = hpos+1;
		redraw();
		return;
	}
	mycomp = 0;
	strecpy(clean, clean+sizeof clean, path);
	cleanname(clean);
	if(loaddir(clean) != 0)
		return;
	if(hpos >= 0 && hpos < nhist && strcmp(hist[hpos], cwd) == 0)
		goto Out;
	if(hpos+1 >= Maxhist){
		memmove(hist, hist+1, sizeof hist - Maxpath);
		nhist--;
	}
	hpos++;
	strecpy(hist[hpos], hist[hpos]+Maxpath, cwd);
	nhist = hpos+1;
Out:
	redraw();
}

void
goback(void)
{
	if(mycomp && hpos < 0){
		mycomp = 0;
		redraw();
		return;
	}
	if(hpos > 0){
		hpos--;
		mycomp = 0;
		loaddir(hist[hpos]);
		redraw();
	}
}

void
goforward(void)
{
	if(hpos+1 < nhist){
		hpos++;
		loaddir(hist[hpos]);
		redraw();
	}
}

Rectangle
listrect(void)
{
	Rectangle r;

	r = screen->r;
	r.min.y += Menuh + Toolbarh + Addrh;
	r.max.y -= Statush;
	r.min.x += Treew;
	return r;
}

void
drawtree(Rectangle r)
{
	int i;
	Point p;
	char *t;

	draw(screen, r, white, nil, ZP);
	bevel(r, 1);
	for(i = 0; i < nelem(tree); i++){
		p = Pt(r.min.x+tree[i].indent, r.min.y+tree[i].y);
		tree[i].r = Rect(r.min.x+4, p.y-3, r.max.x-4, p.y+font->height+3);
		t = tree[i].label;
		if(tree[i].path[0] == 0){
			/* recycle bin */
			strecpy(tree[i].path, tree[i].path+Maxpath, trashdir());
			t = tree[i].label;
		}
		if(i == selectedtree){
			draw(screen, tree[i].r, hilite, nil, ZP);
			border(screen, tree[i].r, 1, shadow, ZP);
		}
		string(screen, p, text, ZP, font, t);
	}
}

/* ---- My Computer view ---- */

typedef struct McEnt McEnt;
struct McEnt {
	char *label;
	char *path;	/* nil: not accessible */
	int drive;	/* 0 folder-ish, 1 disk, 2 floppy, 3 cd */
};

static McEnt mcents[] = {
	{ "Local Disk (C:)", "/", 1 },
	{ "Floppy (A:)", nil, 2 },
	{ "CD-ROM (D:)", nil, 3 },
	{ "Control Panel", "/lib/controlpanel", 0 },
	{ "Recycle Bin", "", 0 },
	{ "My Documents", "/usr/glenda", 0 },
	{ nil, nil, 0 },
};

static void
static drawmcicon(Rectangle r, McEnt *e)
{
	Point c;

	c = Pt((r.min.x+r.max.x)/2, r.min.y+22);
	switch(e->drive){
	case 1:
	case 2:
		/* disk drive */
		draw(screen, Rect(c.x-17, c.y-9, c.x+17, c.y+9), hilite, nil, ZP);
		border(screen, Rect(c.x-17, c.y-9, c.x+17, c.y+9), 1, dark, ZP);
		draw(screen, Rect(c.x-13, c.y-5, c.x+13, c.y+1), white, nil, ZP);
		draw(screen, Rect(c.x+6, c.y+4, c.x+13, c.y+7), green, nil, ZP);
		if(e->drive == 2)
			draw(screen, Rect(c.x-13, c.y+3, c.x+13, c.y+5), shadow, nil, ZP);
		break;
	case 3:
		/* cd */
		ellipse(screen, c, 14, 14, 1, hilite, ZP);
		ellipse(screen, c, 14, 14, 0, dark, ZP);
		ellipse(screen, c, 4, 4, 1, white, ZP);
		ellipse(screen, c, 4, 4, 0, dark, ZP);
		break;
	default:
		if(strcmp(e->label, "Recycle Bin") == 0){
			line(screen, Pt(c.x-7, c.y-10), Pt(c.x+7, c.y-10), 0, 0, 1, dark, ZP);
			line(screen, Pt(c.x-9, c.y-9), Pt(c.x-5, c.y+11), 0, 0, 1, dark, ZP);
			line(screen, Pt(c.x+9, c.y-9), Pt(c.x+5, c.y+11), 0, 0, 1, dark, ZP);
			line(screen, Pt(c.x-5, c.y+11), Pt(c.x+5, c.y+11), 0, 0, 1, dark, ZP);
			draw(screen, Rect(c.x-4, c.y-8, c.x-2, c.y+9), shadow, nil, ZP);
			draw(screen, Rect(c.x+3, c.y-8, c.x+5, c.y+9), shadow, nil, ZP);
			draw(screen, Rect(c.x-3, c.y-13, c.x+3, c.y-10), dark, nil, ZP);
		}else if(strcmp(e->label, "Control Panel") == 0){
			draw(screen, Rect(c.x-14, c.y-10, c.x+14, c.y+10), white, nil, ZP);
			border(screen, Rect(c.x-14, c.y-10, c.x+14, c.y+10), 1, dark, ZP);
			draw(screen, Rect(c.x-11, c.y-7, c.x+11, c.y-4), navy, nil, ZP);
			line(screen, Pt(c.x-7, c.y-1), Pt(c.x+7, c.y-1), 0, 0, 0, dark, ZP);
			line(screen, Pt(c.x-7, c.y+3), Pt(c.x+7, c.y+3), 0, 0, 0, dark, ZP);
			draw(screen, Rect(c.x-4, c.y-2, c.x-1, c.y+1), green, nil, ZP);
			draw(screen, Rect(c.x+1, c.y+2, c.x+4, c.y+5), red, nil, ZP);
		}else{
			draw(screen, Rect(c.x-16, c.y-6, c.x+16, c.y+11), yellow, nil, ZP);
			draw(screen, Rect(c.x-12, c.y-11, c.x+2, c.y-6), yellow, nil, ZP);
			border(screen, Rect(c.x-16, c.y-6, c.x+16, c.y+11), 1, dark, ZP);
		}
	}
}

static void
static mcopen(int i)
{
	char path[Maxpath];

	if(i < 0 || i >= nelem(mcents)-1)
		return;
	if(mcents[i].path == nil){
		msgbox("Error", "The device is not accessible.");
		return;
	}
	if(mcents[i].path[0] == 0)
		strecpy(path, path+sizeof path, trashdir());
	else
		strecpy(path, path+sizeof path, mcents[i].path);
	navigate(path);
}

static int
static mcitemat(Point p)
{
	Rectangle lr, r;
	int i, x, y;

	lr = listrect();
	x = lr.min.x + 20;
	y = lr.min.y + 20;
	for(i = 0; mcents[i].label != nil; i++){
		r = Rect(x, y, x+108, y+72);
		if(ptinrect(p, r))
			return i;
		x += 122;
		if(x+108 > lr.max.x-12){
			x = lr.min.x + 20;
			y += 84;
		}
		if(y+72 > lr.max.y-8)
			break;
	}
	return -1;
}

/* small message box with a single OK button */
static void
static msgbox(char *title, char *line)
{
	Rectangle dlg, ok;
	Event e;
	int done, buttons;

	dlg = Rect((screen->r.min.x+screen->r.max.x)/2-170,
		(screen->r.min.y+screen->r.max.y)/2-60,
		(screen->r.min.x+screen->r.max.x)/2+170,
		(screen->r.min.y+screen->r.max.y)/2+60);
	ok = Rect(dlg.max.x-100, dlg.max.y-48, dlg.max.x-16, dlg.max.y-24);

	done = 0;
	buttons = 0;
	while(!done){
		draw(screen, dlg, face, nil, ZP);
		border(screen, dlg, 1, dark, ZP);
		draw(screen, Rect(dlg.min.x+3, dlg.min.y+3, dlg.max.x-3, dlg.min.y+24), navy, nil, ZP);
		string(screen, Pt(dlg.min.x+10, dlg.min.y+(24-font->height)/2+3), white, ZP, font, title);
		string(screen, Pt(dlg.min.x+20, dlg.min.y+40), text, ZP, font, line);
		btn(ok, "OK");
		flushimage(display, 1);
		switch(event(&e)){
		case Ekeyboard:
			if(e.kbdc == '\n' || e.kbdc == Kesc)
				done = 1;
			break;
		case Emouse:
			if((e.mouse.buttons & 1) && !(buttons & 1) && ptinrect(e.mouse.xy, ok))
				done = 1;
			buttons = e.mouse.buttons;
			break;
		}
	}
	redraw();
}

void
redraw(void)
{
	Rectangle r, lr, row;
	char buf[128];
	int i, y, x;

	draw(screen, screen->r, face, nil, ZP);
	/* menu bar */
	r = Rect(screen->r.min.x, screen->r.min.y, screen->r.max.x, screen->r.min.y+Menuh);
	draw(screen, r, face, nil, ZP);
	menubar(r);

	r = Rect(screen->r.min.x, screen->r.min.y+Menuh, screen->r.max.x, screen->r.min.y+Menuh+Toolbarh);
	draw(screen, r, face, nil, ZP);
	btn(Rect(r.min.x+6, r.min.y+4, r.min.x+6+Btnw, r.max.y-4), "Back");
	btn(Rect(r.min.x+80, r.min.y+4, r.min.x+80+Btnw, r.max.y-4), "Forward");
	btn(Rect(r.min.x+154, r.min.y+4, r.min.x+154+Btnw, r.max.y-4), "Up");
	btn(Rect(r.min.x+228, r.min.y+4, r.min.x+228+Btnw+12, r.max.y-4), "Refresh");
	btn(Rect(r.min.x+322, r.min.y+4, r.min.x+322+Btnw+18, r.max.y-4), "New Dir");
	btn(Rect(r.min.x+412, r.min.y+4, r.min.x+412+Btnw, r.max.y-4), viewmode == ViewList ? "Icons" : "List");

	r = Rect(screen->r.min.x, screen->r.min.y+Menuh+Toolbarh, screen->r.max.x, screen->r.min.y+Menuh+Toolbarh+Addrh);
	draw(screen, r, face, nil, ZP);
	string(screen, Pt(r.min.x+8, r.min.y+5), text, ZP, font, "Address");
	r.min.x += 70;
	r.max.x -= 8;
	draw(screen, insetrect(r, 3), white, nil, ZP);
	bevel(insetrect(r, 3), 1);
	string(screen, Pt(r.min.x+9, r.min.y+6), text, ZP, font, mycomp ? "My Computer" : cwd);

	r = screen->r;
	r.min.y += Menuh + Toolbarh + Addrh;
	r.max.y -= Statush;
	r.max.x = r.min.x + Treew;
	drawtree(r);

	lr = listrect();
	draw(screen, lr, white, nil, ZP);
	bevel(lr, 1);
	if(mycomp){
		x = lr.min.x + 20;
		y = lr.min.y + 20;
		for(i = 0; mcents[i].label != nil; i++){
			row = Rect(x, y, x+108, y+72);
			if(i == sel){
				draw(screen, row, hilite, nil, ZP);
				border(screen, row, 1, dark, ZP);
			}
			drawmcicon(row, &mcents[i]);
			string(screen, Pt(row.min.x+(Dx(row)-stringwidth(font, mcents[i].label))/2,
				row.min.y+48), text, ZP, font, mcents[i].label);
			x += 122;
			if(x+108 > lr.max.x-12){
				x = lr.min.x + 20;
				y += 84;
			}
			if(y+72 > lr.max.y-8)
				break;
		}
		goto Status;
	}
	if(controlpanel()){
		x = lr.min.x + 20;
		y = lr.min.y + 20;
		for(i = 0; i < nents; i++){
			if(strcmp(ents[i].name, "..") == 0)
				continue;
			row = Rect(x, y, x+108, y+72);
			if(i == sel){
				draw(screen, row, hilite, nil, ZP);
				border(screen, row, 1, dark, ZP);
			}
			drawcpicon(row, ents[i].name);
			string(screen, Pt(row.min.x+(Dx(row)-stringwidth(font, ents[i].name))/2,
				row.min.y+48), text, ZP, font, ents[i].name);
			x += 122;
			if(x+108 > lr.max.x-12){
				x = lr.min.x + 20;
				y += 84;
			}
			if(y+72 > lr.max.y-8)
				break;
		}
		goto Status;
	}
	if(viewmode == ViewIcon){
		x = lr.min.x + 20;
		y = lr.min.y + 20;
		for(i = scroll; i < nents; i++){
			row = Rect(x, y, x+Iconw, y+Iconh);
			drawfileicon(row, ents[i].isdir, 1);
			string(screen, Pt(row.min.x+(Dx(row)-stringwidth(font, ents[i].name))/2,
				row.min.y+48), text, ZP, font, ents[i].name);
			if(i == sel){
				int w = stringwidth(font, ents[i].name);
				draw(screen, Rect(row.min.x+(Dx(row)-w)/2-2, row.min.y+47,
					row.min.x+(Dx(row)-w)/2+w+2, row.min.y+47+font->height+2), navy, nil, ZP);
				string(screen, Pt(row.min.x+(Dx(row)-w)/2, row.min.y+48), white, ZP, font, ents[i].name);
			}
			x += Iconw + 14;
			if(x+Iconw > lr.max.x-12){
				x = lr.min.x + 20;
				y += Iconh + 12;
			}
			if(y+Iconh > lr.max.y-8)
				break;
		}
		goto Status;
	}
	row = Rect(lr.min.x+4, lr.min.y+4, lr.max.x-4, lr.min.y+24);
	draw(screen, row, face, nil, ZP);
	string(screen, Pt(row.min.x+8, row.min.y+4), text, ZP, font, "Name");
	string(screen, Pt(row.min.x+230, row.min.y+4), text, ZP, font, "Size");
	string(screen, Pt(row.min.x+320, row.min.y+4), text, ZP, font, "Type");
	y = row.max.y + 3;
	for(i = scroll; i < nents && y+Rowh < lr.max.y-4; i++){
		row = Rect(lr.min.x+4, y, lr.max.x-4, y+Rowh);
		if(i == sel){
			draw(screen, row, navy, nil, ZP);
			drawfileicon(row, ents[i].isdir, 0);
			string(screen, Pt(row.min.x+34, row.min.y+3), white, ZP, font, ents[i].name);
			if(ents[i].isdir)
				snprint(buf, sizeof buf, "");
			else
				snprint(buf, sizeof buf, "%lld", ents[i].len);
			string(screen, Pt(row.min.x+230, row.min.y+3), white, ZP, font, buf);
			string(screen, Pt(row.min.x+320, row.min.y+3), white, ZP, font, ents[i].type);
		}else{
			drawfileicon(row, ents[i].isdir, 0);
			string(screen, Pt(row.min.x+34, row.min.y+3), text, ZP, font, ents[i].name);
			if(ents[i].isdir)
				snprint(buf, sizeof buf, "");
			else
				snprint(buf, sizeof buf, "%lld", ents[i].len);
			string(screen, Pt(row.min.x+230, row.min.y+3), text, ZP, font, buf);
			string(screen, Pt(row.min.x+320, row.min.y+3), text, ZP, font, ents[i].type);
		}
		y += Rowh;
	}

Status:
	r = Rect(screen->r.min.x, screen->r.max.y-Statush, screen->r.max.x, screen->r.max.y);
	draw(screen, r, face, nil, ZP);
	if(mycomp)
		snprint(buf, sizeof buf, "%d object%s", nelem(mcents)-1, nelem(mcents)==2 ? "" : "s");
	else if(controlpanel())
		snprint(buf, sizeof buf, "%d item%s", nents-1, nents==2 ? "" : "s");
	else if(trashview())
		snprint(buf, sizeof buf, "Recycle Bin: %d object%s", nents-1, nents==2 ? "" : "s");
	else
		snprint(buf, sizeof buf, "%d object%s", nents, nents==1 ? "" : "s");
	string(screen, Pt(r.min.x+8, r.min.y+4), text, ZP, font, buf);
	flushimage(display, 1);
}

/* ---- menu bar ---- */

static char *menutitles[] = { "File", "Edit", "View", "Help", nil };

static void
static menubar(Rectangle r)
{
	int i, x;

	x = r.min.x + 8;
	for(i = 0; menutitles[i]; i++){
		string(screen, Pt(x, r.min.y+(Menuh-font->height)/2), text, ZP, font, menutitles[i]);
		x += stringwidth(font, menutitles[i]) + 18;
	}
}

static int
static menubarhit(Point p)
{
	Rectangle r;
	int i, x;

	r = Rect(screen->r.min.x, screen->r.min.y, screen->r.max.x, screen->r.min.y+Menuh);
	if(!ptinrect(p, r))
		return -1;
	x = r.min.x + 8;
	for(i = 0; menutitles[i]; i++){
		int w = stringwidth(font, menutitles[i]);
		if(p.x >= x-4 && p.x <= x+w+6)
			return i;
		x += w + 18;
	}
	return -1;
}

static void
expnewwin(void)
{
	int fd;

	fd = open("/dev/wctl", OWRITE);
	if(fd >= 0){
		if(mycomp)
			fprint(fd, "new -r 60 80 760 560 explorer /mycomputer");
		else
			fprint(fd, "new -r 60 80 760 560 explorer %s", cwd);
		close(fd);
	}
}

static void
static aboutbox(void)
{
	msgbox("About Explorer", "Plan 9 Explorer - win2k style shell");
}

static void
static menubaractivate(int m, Point p)
{
	char *fitems[] = { "New Window", "New Folder", "-", "Close", nil };
	char *eitems[] = { "Copy", "Cut", "Paste", "-", "Select All", nil };
	char *vitems[] = { "", "-", "Refresh", nil };
	char *hitems[] = { "About Explorer", nil };
	char **items;
	int i, r;

	vitems[0] = viewmode == ViewList ? "Large Icons" : "List";
	switch(m){
	case 0: items = fitems; break;
	case 1: items = eitems; break;
	case 2: items = vitems; break;
	default: items = hitems; break;
	}
	/* anchor the dropdown under its title */
	{
		int x = screen->r.min.x + 8;
		for(i = 0; i < m && menutitles[i]; i++)
			x += stringwidth(font, menutitles[i]) + 18;
		p = Pt(x-4, screen->r.min.y+Menuh);
	}
	r = menuhitx(p, items);
	if(r < 0)
		return;
	switch(m){
	case 0:
		switch(r){
		case 0: expnewwin(); break;
		case 1: newdir(); break;
		case 3: exits(nil); break;
		}
		break;
	case 1:
		switch(r){
		case 0:
		case 1:
			if(sel >= 0 && sel < nents){
				cleanpath(clipboard, sizeof clipboard, cwd, ents[sel].name);
				clipcut = r == 1;
			}
			break;
		case 2:
			pasteclip();
			break;
		case 4:
			sel = nents > 1 ? 1 : -1;
			redraw();
			break;
		}
		break;
	case 2:
		switch(r){
		case 0:
			viewmode = viewmode == ViewList ? ViewIcon : ViewList;
			redraw();
			break;
		case 2:
			if(mycomp){
				mycomp = 0;
				navigate("/mycomputer");
			}else{
				loaddir(cwd);
				redraw();
			}
			break;
		}
		break;
	case 3:
		if(r == 0)
			aboutbox();
		break;
	}
}

/* ---- in-window popup menu ---- */

static int
menuhitx(Point p, char **items)
{
	Rectangle r, ir;
	Event e;
	Point pt;
	int i, n, w, h, hover, cur, buttons;

	for(n = 0; items[n]; n++)
		;
	w = 0;
	for(i = 0; i < n; i++){
		h = stringwidth(font, items[i][0] == '!' ? items[i]+1 : items[i]);
		if(h > w)
			w = h;
	}
	w += 30;
	h = n*Mitemh + 6;
	r = Rect(p.x, p.y, p.x+w, p.y+h);
	if(r.max.x > screen->r.max.x)
		r = rectsubpt(r, Pt(Dx(r)+p.x-screen->r.max.x, 0));
	if(r.max.y > screen->r.max.y)
		r = rectsubpt(r, Pt(0, Dy(r)+p.y-screen->r.max.y));
	if(r.min.x < screen->r.min.x)
		r.min.x = screen->r.min.x;
	if(r.min.y < screen->r.min.y)
		r.min.y = screen->r.min.y;

	draw(screen, r, face, nil, ZP);
	border(screen, r, 1, dark, ZP);
	cur = -1;
	buttons = 0;
	for(;;){
		/* draw items */
		for(i = 0; i < n; i++){
			ir = Rect(r.min.x+3, r.min.y+3+i*Mitemh, r.max.x-3, r.min.y+3+(i+1)*Mitemh);
			if(items[i][0] == '-' && items[i][1] == 0){
				line(screen, Pt(ir.min.x+4, ir.min.y+Mitemh/2),
					Pt(ir.max.x-4, ir.min.y+Mitemh/2), 0, 0, 0, shadow, ZP);
				continue;
			}
			if(i == cur && items[i][0] != '!'){
				draw(screen, ir, navy, nil, ZP);
				pt = Pt(ir.min.x+8, ir.min.y+(Mitemh-font->height)/2);
				string(screen, pt, white, ZP, font, items[i][0]=='!' ? items[i]+1 : items[i]);
			}else{
				draw(screen, ir, face, nil, ZP);
				pt = Pt(ir.min.x+8, ir.min.y+(Mitemh-font->height)/2);
				string(screen, pt, items[i][0]=='!' ? shadow : text, ZP, font,
					items[i][0]=='!' ? items[i]+1 : items[i]);
			}
		}
		flushimage(display, 1);
		switch(event(&e)){
		case Emouse:
			hover = -1;
			if(ptinrect(e.mouse.xy, insetrect(r, 3)))
				hover = (e.mouse.xy.y - (r.min.y+3)) / Mitemh;
			if(hover >= n)
				hover = -1;
			if(hover >= 0 && (items[hover][0] == '!' ||
			    items[hover][0] == '-' && items[hover][1] == 0))
				hover = -1;
			if(hover != cur){
				cur = hover;
			}
			if((e.mouse.buttons & 1) && !(buttons & 1)){
				if(cur >= 0){
					redraw();
					return cur;
				}
				redraw();
				return -1;
			}
			if((e.mouse.buttons & (4|2)) && !(buttons & (4|2))){
				redraw();
				return -1;
			}
			buttons = e.mouse.buttons;
			break;
		case Ekeyboard:
			if(e.kbdc == Kesc){
				redraw();
				return -1;
			}
			break;
		}
	}
}

/* ---- modal text entry inside the window ---- */

static int
textprompt(char *title, char *label, char *buf, int nbuf)
{
	Rectangle dlg, entry, ok, cancel;
	Event e;
	int n, done, rc, buttons;

	n = strlen(buf);
	done = 0;
	rc = 0;
	buttons = 0;

	dlg = Rect((screen->r.min.x+screen->r.max.x)/2-190,
		(screen->r.min.y+screen->r.max.y)/2-80,
		(screen->r.min.x+screen->r.max.x)/2+190,
		(screen->r.min.y+screen->r.max.y)/2+80);
	entry = Rect(dlg.min.x+20, dlg.min.y+64, dlg.max.x-20, dlg.min.y+92);
	ok = Rect(dlg.max.x-180, dlg.max.y-48, dlg.max.x-100, dlg.max.y-24);
	cancel = Rect(dlg.max.x-90, dlg.max.y-48, dlg.max.x-16, dlg.max.y-24);

	for(;;){
		draw(screen, dlg, face, nil, ZP);
		border(screen, dlg, 1, dark, ZP);
		draw(screen, Rect(dlg.min.x+3, dlg.min.y+3, dlg.max.x-3, dlg.min.y+24), navy, nil, ZP);
		string(screen, Pt(dlg.min.x+10, dlg.min.y+(24-font->height)/2+3), white, ZP, font, title);
		string(screen, Pt(dlg.min.x+20, dlg.min.y+38), text, ZP, font, label);
		draw(screen, insetrect(entry, 1), white, nil, ZP);
		bevel(entry, 1);
		stringn(screen, Pt(entry.min.x+4, entry.min.y+(Dy(entry)-font->height)/2),
			text, ZP, font, buf, n);
		btn(ok, "OK");
		btn(cancel, "Cancel");
		flushimage(display, 1);

		if(done)
			break;
		switch(event(&e)){
		case Ekeyboard:
			if(e.kbdc == '\n'){
				done = 1;
				rc = 1;
			}else if(e.kbdc == Kesc){
				done = 1;
				rc = 0;
			}else if(e.kbdc == Kbs || e.kbdc == 0x7f){
				if(n > 0){
					while(n > 0 && (buf[n-1] & 0xC0) == 0x80)
						n--;
					if(n > 0)
						n--;
				}
			}else if(e.kbdc >= ' ' && e.kbdc < 0x7f && n < nbuf-2){
				buf[n++] = e.kbdc;
			}
			break;
		case Emouse:
			if((e.mouse.buttons & 1) && !(buttons & 1)){
				if(ptinrect(e.mouse.xy, ok)){
					done = 1;
					rc = 1;
				}else if(ptinrect(e.mouse.xy, cancel)){
					done = 1;
					rc = 0;
				}
			}
			buttons = e.mouse.buttons;
			break;
		}
	}
	buf[n] = 0;
	redraw();
	return rc;
}

/* ---- properties dialog ---- */

static void
properties(int i)
{
	Rectangle dlg, ok;
	Event e;
	char *cts;
	int done, buttons;

	if(i < 0 || i >= nents)
		return;
	cts = ctime(ents[i].mtime);

	dlg = Rect((screen->r.min.x+screen->r.max.x)/2-170,
		(screen->r.min.y+screen->r.max.y)/2-90,
		(screen->r.min.x+screen->r.max.x)/2+170,
		(screen->r.min.y+screen->r.max.y)/2+90);
	ok = Rect(dlg.max.x-100, dlg.max.y-48, dlg.max.x-16, dlg.max.y-24);

	done = 0;
	buttons = 0;
	while(!done){
		draw(screen, dlg, face, nil, ZP);
		border(screen, dlg, 1, dark, ZP);
		draw(screen, Rect(dlg.min.x+3, dlg.min.y+3, dlg.max.x-3, dlg.min.y+24), navy, nil, ZP);
		string(screen, Pt(dlg.min.x+10, dlg.min.y+(24-font->height)/2+3), white, ZP, font, "Properties");
		string(screen, Pt(dlg.min.x+20, dlg.min.y+40), text, ZP, font, ents[i].name);
		string(screen, Pt(dlg.min.x+20, dlg.min.y+58), text, ZP, font,
			ents[i].isdir ? "Type: File Folder" : "Type: File");
		if(!ents[i].isdir){
			char buf[64];
			snprint(buf, sizeof buf, "Size: %lld bytes", ents[i].len);
			string(screen, Pt(dlg.min.x+20, dlg.min.y+76), text, ZP, font, buf);
		}
		if(ents[i].mtime > 0){
			char buf[64];
			snprint(buf, sizeof buf, "Modified: %.20s", cts);
			string(screen, Pt(dlg.min.x+20, dlg.min.y+94), text, ZP, font, buf);
		}
		string(screen, Pt(dlg.min.x+20, dlg.min.y+112), text, ZP, font, "Location:");
		string(screen, Pt(dlg.min.x+20, dlg.min.y+130), text, ZP, font, cwd);
		btn(ok, "OK");
		flushimage(display, 1);
		switch(event(&e)){
		case Ekeyboard:
			if(e.kbdc == '\n' || e.kbdc == Kesc)
				done = 1;
			break;
		case Emouse:
			if((e.mouse.buttons & 1) && !(buttons & 1) && ptinrect(e.mouse.xy, ok))
				done = 1;
			buttons = e.mouse.buttons;
			break;
		}
	}
	redraw();
}

/* ---- file operations ---- */

static int
dircopy(char *from, char *to)
{
	Dir *d;
	char *sub, *subto;
	uchar *buf;
	int fd, n, i, ok, in, out;

	d = dirstat(from);
	if(d == nil)
		return -1;
	if(!(d->mode & DMDIR)){
		in = open(from, OREAD);
		if(in < 0){
			free(d);
			return -1;
		}
		remove(to);
		out = create(to, OWRITE, d->mode & 0777);
		if(out < 0){
			close(in);
			free(d);
			return -1;
		}
		buf = malloc(65536);
		if(buf != nil){
			while((n = read(in, buf, 65536)) > 0)
				if(write(out, buf, n) < 0)
					break;
			free(buf);
		}
		close(in);
		close(out);
		free(d);
		return 0;
	}
	free(d);
	remove(to);
	if(create(to, OREAD, DMDIR|0777) < 0)
		return -1;
	fd = open(from, OREAD);
	if(fd < 0)
		return -1;
	n = dirreadall(fd, &d);
	close(fd);
	ok = 0;
	for(i = 0; i < n; i++){
		sub = smprint("%s/%s", from, d[i].name);
		subto = smprint("%s/%s", to, d[i].name);
		if(sub && subto && dircopy(sub, subto) < 0)
			ok = -1;
		free(sub);
		free(subto);
	}
	free(d);
	return ok;
}

static int
removeall(char *path)
{
	Dir *d;
	char *sub;
	int fd, n, i, rc;

	fd = open(path, OREAD);
	if(fd < 0)
		return -1;
	n = dirreadall(fd, &d);
	close(fd);
	if(n < 0)
		return -1;
	rc = 0;
	for(i = 0; i < n; i++){
		sub = smprint("%s/%s", path, d[i].name);
		if(sub == nil)
			continue;
		if(d[i].mode & DMDIR)
			removeall(sub);
		if(remove(sub) < 0)
			rc = -1;
		free(sub);
	}
	free(d);
	if(remove(path) < 0 && rc == 0)
		rc = -1;
	return rc;
}

static void
originpath(char *dst, int nd)
{
	char *t;

	t = trashdir();
	snprint(dst, nd, "%s/.origin", t);
}

static void
originrecord(char *base, char *origdir)
{
	char op[Maxpath];
	int fd;

	originpath(op, sizeof op);
	fd = open(op, OWRITE);
	if(fd < 0)
		fd = create(op, OWRITE, 0666);
	if(fd < 0)
		return;
	seek(fd, 0, 2);
	fprint(fd, "%s\t%s\n", base, origdir);
	close(fd);
}

static int
originlookup(char *base, char *origdir, int nd)
{
	char op[Maxpath], buf[8192];
	char *f[3];
	int nf, found, fd, n, i, start;

	originpath(op, sizeof op);
	found = 0;
	fd = open(op, OREAD);
	if(fd < 0)
		return 0;
	n = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if(n <= 0)
		return 0;
	buf[n] = 0;
	start = 0;
	for(i = 0; i <= n; i++){
		if(i == n || buf[i] == '\n'){
			if(i > start){
				buf[i] = 0;
				nf = getfields(buf+start, f, nelem(f), 0, "\t");
				if(nf >= 2 && strcmp(f[0], base) == 0){
					strecpy(origdir, origdir+nd, f[1]);
					found = 1;
					break;
				}
			}
			start = i+1;
		}
	}
	return found;
}

static void
originremove(char *base)
{
	char op[Maxpath], buf[8192], out[8192];
	char *f[3];
	int nf, fd, n, i, start, o, len, save;

	originpath(op, sizeof op);
	fd = open(op, OREAD);
	if(fd < 0)
		return;
	n = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if(n <= 0)
		return;
	buf[n] = 0;
	o = 0;
	start = 0;
	for(i = 0; i <= n; i++){
		if(i != n && buf[i] != '\n')
			continue;
		if(i > start){
			save = buf[i];
			buf[i] = 0;
			nf = getfields(buf+start, f, nelem(f), 0, "\t");
			if(!(nf >= 1 && strcmp(f[0], base) == 0)){
				len = i - start;
				if(o + len + 1 < sizeof out){
					memmove(out+o, buf+start, len);
					o += len;
					out[o++] = '\n';
				}
			}
			buf[i] = save;
		}
		start = i+1;
	}
	fd = create(op, OWRITE, 0666);
	if(fd >= 0){
		write(fd, out, o);
		close(fd);
	}
}

static void
deletetotrash(int i)
{
	char path[Maxpath], dest[Maxpath], base[136], *t;
	int n, fd;

	if(i < 0 || i >= nents || strcmp(ents[i].name, "..") == 0)
		return;
	cleanpath(path, sizeof path, cwd, ents[i].name);
	t = trashdir();
	fd = create(t, OREAD, DMDIR|0777);
	if(fd >= 0)
		close(fd);
	strecpy(base, base+sizeof base, ents[i].name);
	for(n = 0; n < 100; n++){
		if(n == 0)
			snprint(dest, sizeof dest, "%s/%s", t, base);
		else
			snprint(dest, sizeof dest, "%s/%s %d", t, base, n+1);
		if(access(dest, AEXIST) < 0)
			break;
	}
	if(dircopy(path, dest) == 0){
		removeall(path);
		originrecord(strrchr(dest, '/')+1, cwd);
	}
	loaddir(cwd);
	redraw();
}

static void
restorefromtrash(int i)
{
	char path[Maxpath], origdir[Maxpath], dest[Maxpath];

	if(i < 0 || i >= nents || strcmp(ents[i].name, "..") == 0)
		return;
	cleanpath(path, sizeof path, cwd, ents[i].name);
	if(!originlookup(ents[i].name, origdir, sizeof origdir))
		strecpy(origdir, origdir+sizeof origdir, "/usr/glenda");
	if(strcmp(origdir, "/") == 0)
		snprint(dest, sizeof dest, "/%s", ents[i].name);
	else
		snprint(dest, sizeof dest, "%s/%s", origdir, ents[i].name);
	if(dircopy(path, dest) == 0){
		removeall(path);
		originremove(ents[i].name);
	}
	loaddir(cwd);
	redraw();
}

static void
emptybin(void)
{
	Dir *d;
	char *t, *sub;
	int fd, n, i;

	t = trashdir();
	fd = open(t, OREAD);
	if(fd < 0)
		return;
	n = dirreadall(fd, &d);
	close(fd);
	for(i = 0; i < n; i++){
		sub = smprint("%s/%s", t, d[i].name);
		if(sub == nil)
			continue;
		if(d[i].mode & DMDIR)
			removeall(sub);
		remove(sub);
		free(sub);
	}
	free(d);
	loaddir(cwd);
	redraw();
}

static void
newdir(void)
{
	char buf[Maxpath], name[64];

	name[0] = 0;
	if(!textprompt("New Folder", "Name:", name, sizeof name))
		return;
	if(name[0] == 0)
		strecpy(name, name+sizeof name, "New Folder");
	cleanpath(buf, sizeof buf, cwd, name);
	if(create(buf, OREAD, DMDIR|0777) < 0){
		char msg[Maxpath+64];
		snprint(msg, sizeof msg, "Cannot create %s", buf);
		textprompt("Error", msg, name, 0);
	}
	loaddir(cwd);
	redraw();
}

static void
renameentry(int i)
{
	char buf[Maxpath], name[128];
	Dir *d;

	if(i < 0 || i >= nents || strcmp(ents[i].name, "..") == 0)
		return;
	strecpy(name, name+sizeof name, ents[i].name);
	if(!textprompt("Rename", "New name:", name, sizeof name))
		return;
	if(name[0] == 0 || strcmp(name, ents[i].name) == 0)
		return;
	cleanpath(buf, sizeof buf, cwd, ents[i].name);
	d = dirstat(buf);
	if(d == nil)
		return;
	strecpy(d->name, d->name+sizeof d->name, name);
	if(dirwstat(buf, d) < 0){
		free(d);
		return;
	}
	free(d);
	loaddir(cwd);
	redraw();
}

static void
pasteclip(void)
{
	char dest[Maxpath], *slash;

	if(clipboard[0] == 0)
		return;
	slash = strrchr(clipboard, '/');
	cleanpath(dest, sizeof dest, cwd, slash ? slash+1 : clipboard);
	if(dircopy(clipboard, dest) == 0 && clipcut){
		removeall(clipboard);
		clipboard[0] = 0;
	}
	loaddir(cwd);
	redraw();
}

/* ---- context menu ---- */

static void
contextmenu(Point p)
{
	char *bgitems[] = {"New Folder", "Paste", "-", "Refresh", "-", "Properties", nil};
	char *ititems[] = {"Open", "Copy", "Cut", "Rename", "Delete", "-", "Properties", nil};
	char *tritems[] = {"Restore", "-", "Empty Recycle Bin", nil};
	char *cpitems[] = {"Open", nil};
	char **items;
	int i, m;

	i = itemat(p);
	if(mycomp){
		if(i < 0)
			return;
		items = cpitems;	/* just Open */
	}else if(controlpanel()){
		if(i < 0)
			return;
		items = cpitems;
	}else if(trashview()){
		if(i < 0)
			items = bgitems;	/* no paste etc: just refresh props */
		else
			items = tritems;
	}else
		items = i < 0 ? bgitems : ititems;

	if(i >= 0 && !trashview() && !controlpanel()){
		sel = i;
		redraw();
	}
	m = menuhitx(p, items);
	if(m < 0)
		return;

	if(items == cpitems){
		if(m == 0 && i >= 0){
			char path[Maxpath];
			cleanpath(path, sizeof path, cwd, ents[i].name);
			runentry(path);
		}
		return;
	}
	if(items == tritems){
		if(m == 0)
			restorefromtrash(i);
		else if(m == 2)
			emptybin();
		return;
	}
	if(items == bgitems){
		switch(m){
		case 0:
			newdir();
			break;
		case 1:
			pasteclip();
			break;
		case 3:
			loaddir(cwd);
			redraw();
			break;
		case 5:
			properties(i >= 0 ? i : -1);
			break;
		}
		return;
	}
	/* item menu */
	switch(m){
	case 0:{
		char path[Maxpath];
		cleanpath(path, sizeof path, cwd, ents[i].name);
		if(ents[i].isdir && loaddir(path) == 0)
			navigate(path);
		else if(!ents[i].isdir)
			openfile(path);
		break;
	}
	case 1:
		cleanpath(clipboard, sizeof clipboard, cwd, ents[i].name);
		clipcut = 0;
		break;
	case 2:
		cleanpath(clipboard, sizeof clipboard, cwd, ents[i].name);
		clipcut = 1;
		break;
	case 3:
		renameentry(i);
		break;
	case 4:
		deletetotrash(i);
		break;
	case 6:
		properties(i);
		break;
	}
}

int
toolbar(Point p)
{
	Rectangle r;
	int top, bot;

	top = screen->r.min.y+Menuh+4;
	bot = screen->r.min.y+Menuh+Toolbarh-4;
	r = Rect(screen->r.min.x+6, top, screen->r.min.x+6+Btnw, bot);
	if(ptinrect(p, r)){
		goback();
		return 1;
	}
	r = Rect(screen->r.min.x+80, top, screen->r.min.x+80+Btnw, bot);
	if(ptinrect(p, r)){
		goforward();
		return 1;
	}
	r = Rect(screen->r.min.x+154, top, screen->r.min.x+154+Btnw, bot);
	if(ptinrect(p, r)){
		if(mycomp)
			navigate("/");
		else{
			char path[Maxpath];
			cleanpath(path, sizeof path, cwd, "..");
			navigate(path);
		}
		return 1;
	}
	r = Rect(screen->r.min.x+228, top, screen->r.min.x+228+Btnw+12, bot);
	if(ptinrect(p, r)){
		if(!mycomp){
			loaddir(cwd);
			redraw();
		}
		return 1;
	}
	r = Rect(screen->r.min.x+322, top, screen->r.min.x+322+Btnw+18, bot);
	if(ptinrect(p, r)){
		if(!mycomp)
			newdir();
		return 1;
	}
	r = Rect(screen->r.min.x+412, top, screen->r.min.x+412+Btnw, bot);
	if(ptinrect(p, r)){
		viewmode = viewmode == ViewList ? ViewIcon : ViewList;
		redraw();
		return 1;
	}
	return 0;
}

void
openrow(Point p)
{
	char path[Maxpath];
	int i;

	if(mycomp){
		mcopen(mcitemat(p));
		return;
	}
	if(controlpanel()){
		i = cpitemat(p);
		if(i >= 0){
			cleanpath(path, sizeof path, cwd, ents[i].name);
			runentry(path);
		}
		return;
	}
	i = itemat(p);
	if(i < 0 || i >= nents)
		return;
	cleanpath(path, sizeof path, cwd, ents[i].name);
	if(ents[i].isdir)
		navigate(path);
	else
		openfile(path);
}

int
opentree(Point p)
{
	int i;

	for(i = 0; i < nelem(tree); i++)
		if(ptinrect(p, tree[i].r)){
			if(tree[i].path[0] == 0)
				strecpy(tree[i].path, tree[i].path+Maxpath, trashdir());
			navigate(tree[i].path);
			return 1;
		}
	return 0;
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		exits("resize");
	redraw();
}

void
openrowatsel(void)
{
	char path[Maxpath];

	if(sel < 0 || sel >= nents)
		return;
	cleanpath(path, sizeof path, cwd, ents[sel].name);
	if(ents[sel].isdir)
		navigate(path);
	else
		openfile(path);
}

void
main(int argc, char **argv)
{
	Event e;
	char path[Maxpath];
	int buttons, i;
	ulong now;

	ARGBEGIN{
	default:
		break;
	}ARGEND
	if(argc > 0)
		strecpy(path, path+sizeof path, argv[0]);
	else if(getwd(path, sizeof path) == nil)
		strecpy(path, path+sizeof path, "/");
	if(strcmp(path, "/mycomputer") == 0){
		mycomp = 1;
		strecpy(path, path+sizeof path, "/");
	}
	cleanname(path);
	if(initdraw(nil, nil, "explorer") < 0)
		sysfatal("initdraw: %r");
	face = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xC0C0C0FF);
	light = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xFFFFFFFF);
	shadow = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x808080FF);
	dark = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x000000FF);
	white = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xFFFFFFFF);
	text = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x000000FF);
	hilite = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xE8E8E8FF);
	yellow = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xF8D878FF);
	navy = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x000080FF);
	green = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x008000FF);
	red = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xB00000FF);
	einit(Emouse|Ekeyboard);
	if(loaddir(path) < 0)
		sysfatal("open %s: %r", path);
	if(mycomp){
		nhist = 0;
		hpos = -1;
	}else{
		strecpy(hist[0], hist[0]+Maxpath, cwd);
		nhist = 1;
		hpos = 0;
	}
	redraw();
	buttons = 0;
	lastclick = 0;
	lastsel = -1;
	for(;;){
		switch(event(&e)){
		case Emouse:
			if((e.mouse.buttons & 1) && !(buttons & 1)){
				i = menubarhit(e.mouse.xy);
				if(i >= 0){
					menubaractivate(i, e.mouse.xy);
					break;
				}
				if(toolbar(e.mouse.xy))
					break;
				if(opentree(e.mouse.xy))
					break;
				i = itemat(e.mouse.xy);
				now = nsec()/1000000;
				if(i >= 0){
					if(i == lastsel && now - lastclick < 400){
						openrow(e.mouse.xy);
						lastsel = -1;
						lastclick = 0;
					}else{
						sel = i;
						redraw();
						lastsel = i;
						lastclick = now;
					}
				}else{
					if(sel != -1){
						sel = -1;
						redraw();
					}
					lastsel = -1;
				}
			}
			if((e.mouse.buttons & 4) && !(buttons & 4))
				contextmenu(e.mouse.xy);
			if((e.mouse.buttons & 8) && !(buttons & 8) && scroll > 0){
				scroll--;
				redraw();
			}
			if((e.mouse.buttons & 16) && !(buttons & 16) && scroll+1 < nents){
				scroll++;
				redraw();
			}
			buttons = e.mouse.buttons;
			break;
		case Ekeyboard:
			if(e.kbdc == 'v'){
				viewmode = viewmode == ViewList ? ViewIcon : ViewList;
				redraw();
			}
			if(e.kbdc == '\n' && sel >= 0)
				openrowatsel();
			if(e.kbdc == 0x7f && sel >= 0 && !trashview() && !controlpanel())
				deletetotrash(sel);
			if(e.kbdc == 'q' || e.kbdc == Kdel)
				exits(nil);
			break;
		}
	}
}
