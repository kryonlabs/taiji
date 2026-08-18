#include "inc.h"
#include <bio.h>

/*
 * Windows-2000 style desktop icons: loaded from /lib/q9/desktop and
 * $home/lib/desktop (label<TAB>command[<TAB>icon], one per line),
 * drawn on the background between windows, selected with a single
 * click and launched with a double click. Icons can be dragged to
 * any spot (positions persist in $home/lib/deskicons), right click
 * on an icon opens its own menu, right click on the desktop the
 * desktop menu (the classic rio menu stays on the 1-2 chord), and
 * the Recycle Bin icon shows whether the bin holds anything.
 */

enum {
	MaxIcons = 48,
	CellW = 72,
	CellH = 72,
	IconSz = 32,
};

typedef struct DIcon DIcon;
struct DIcon {
	char label[40];
	char cmd[160];
	int icon;
	int selected;
	Point pos;	/* -1,-1: auto layout */
};

static DIcon icons[MaxIcons];
static int nicons;
static ulong lastclick;
static int lasticon;
static int binfull;

static char *
trashdir(void)
{
	static char t[128];
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

static void
subhome(char *dst, int nd, char *s)
{
	char *home, *p;
	int n;

	home = getenv("home");
	if(home == nil){
		strecpy(dst, dst+nd, s);
		return;
	}
	p = strstr(s, "$home");
	if(p == nil){
		strecpy(dst, dst+nd, s);
		free(home);
		return;
	}
	n = p - s;
	if(n >= nd)
		n = nd-1;
	memmove(dst, s, n);
	strecpy(dst+n, dst+nd, p+5);
	free(home);
}

/* suppression list: icons the user deleted from the desktop */
static int
isdeleted(char *label)
{
	char *home, *path, buf[256], *line;
	Biobuf *bp;
	int dead;

	home = getenv("home");
	if(home == nil)
		return 0;
	path = smprint("%s/lib/deskdel", home);
	free(home);
	if(path == nil)
		return 0;
	bp = Bopen(path, OREAD);
	free(path);
	if(bp == nil)
		return 0;
	dead = 0;
	while((line = Brdline(bp, '\n')) != nil){
		line[Blinelen(bp)-1] = 0;
		if(strcmp(line, label) == 0){
			dead = 1;
			break;
		}
	}
	Bterm(bp);
	return dead;
}

static void
addicon(char *label, char *cmd, char *icname)
{
	if(nicons >= MaxIcons)
		return;
	if(label[0] == 0 || cmd[0] == 0)
		return;
	if(isdeleted(label))
		return;
	strecpy(icons[nicons].label, icons[nicons].label+sizeof icons[nicons].label, label);
	subhome(icons[nicons].cmd, sizeof icons[nicons].cmd, cmd);
	icons[nicons].icon = icname[0] ? paneliconbyname(icname) : Ifolder;
	icons[nicons].selected = 0;
	icons[nicons].pos = Pt(-1, -1);
	nicons++;
}

static void
readicons(char *path)
{
	Biobuf *bp;
	char *line, *f[4];
	int nf;

	bp = Bopen(path, OREAD);
	if(bp == nil)
		return;
	while(nicons < MaxIcons && (line = Brdline(bp, '\n')) != nil){
		line[Blinelen(bp)-1] = 0;
		nf = getfields(line, f, nelem(f), 0, "\t");
		if(nf < 2)
			continue;
		addicon(f[0], f[1], nf > 2 ? f[2] : "");
	}
	Bterm(bp);
}

/* remember where the user dragged each icon */
static void
readpositions(void)
{
	Biobuf *bp;
	char *home, *path, *line, *f[4];
	int i, nf, x, y;

	home = getenv("home");
	if(home == nil)
		return;
	path = smprint("%s/lib/deskicons", home);
	free(home);
	if(path == nil)
		return;
	bp = Bopen(path, OREAD);
	if(bp == nil){
		free(path);
		return;
	}
	while((line = Brdline(bp, '\n')) != nil){
		line[Blinelen(bp)-1] = 0;
		nf = getfields(line, f, nelem(f), 0, "\t");
		if(nf < 3)
			continue;
		x = strtol(f[1], nil, 10);
		y = strtol(f[2], nil, 10);
		for(i = 0; i < nicons; i++)
			if(strcmp(icons[i].label, f[0]) == 0)
				icons[i].pos = Pt(x, y);
	}
	Bterm(bp);
	free(path);
}

static void
savepositions(void)
{
	char *home, *path;
	Biobuf *bp;
	int i;

	home = getenv("home");
	if(home == nil)
		return;
	path = smprint("%s/lib/deskicons", home);
	free(home);
	if(path == nil)
		return;
	bp = Bopen(path, OWRITE|OTRUNC);
	if(bp == nil){
		free(path);
		return;
	}
	for(i = 0; i < nicons; i++)
		if(icons[i].pos.x >= 0)
			Bprint(bp, "%s\t%d\t%d\n", icons[i].label, icons[i].pos.x, icons[i].pos.y);
	Bterm(bp);
	free(path);
}

void
deskiconinit(void)
{
	char *home, *path;

	nicons = 0;
	readicons("/lib/q9/desktop");
	home = getenv("home");
	if(home != nil){
		path = smprint("%s/lib/desktop", home);
		if(path != nil){
			readicons(path);
			free(path);
		}
		free(home);
	}
	readpositions();
	lasticon = -1;
	lastclick = 0;
}

static int
iconoccupied(Rectangle wr, Point p)
{
	int i;

	for(i = 0; i < nicons; i++)
		if(icons[i].pos.x >= 0 && abs(icons[i].pos.x-p.x) < CellW &&
		   abs(icons[i].pos.y-p.y) < CellH)
			return 1;
	/* keep off the very edges */
	if(!ptinrect(p, Rect(wr.min.x+6, wr.min.y+4, wr.max.x-CellW-6, wr.max.y-CellH-6)))
		return 1;
	return 0;
}

/* auto place the icons that have no saved position */
static void
layouticons(void)
{
	Rectangle wr;
	int i, x, y;

	wr = panelworkrect();
	x = wr.min.x + 10;
	y = wr.min.y + 8;
	for(i = 0; i < nicons; i++){
		if(icons[i].pos.x >= 0)
			continue;
		for(;;){
			if(y + CellH > wr.max.y){
				x += CellW + 10;
				y = wr.min.y + 8;
			}
			if(!iconoccupied(wr, Pt(x, y)))
				break;
			y += CellH;
		}
		icons[i].pos = Pt(x, y);
		y += CellH;
	}
}

/* split a label into at most two display lines at a space */
static int
splitlabel(char *label, char *l1, char *l2)
{
	char *p, *best;
	int n;

	n = strlen(label);
	l1[0] = l2[0] = 0;
	if(n < 16){
		strcpy(l1, label);
		return 1;
	}
	best = nil;
	for(p = label; *p; p++)
		if(*p == ' ' && p-label >= 4 && p-label < 24)
			best = p;
	if(best == nil){
		if(n < 30){
			strcpy(l1, label);
			return 1;
		}
		memmove(l1, label, 14);
		l1[14] = 0;
		memmove(l2, label+14, n-14 > 15 ? 15 : n-14);
		l2[n-14 > 15 ? 15 : n-14] = 0;
		return 2;
	}
	n = best - label;
	memmove(l1, label, n);
	l1[n] = 0;
	strecpy(l2, l2+20, best+1);
	return 2;
}

static void
drawoneicon(Image *dst, DIcon *ic)
{
	char l1[24], l2[24];
	Rectangle ir, lr;
	Image *sel;
	int nlines, w, i;

	ir = Rect(ic->pos.x + (CellW-IconSz)/2, ic->pos.y + 4,
		ic->pos.x + (CellW+IconSz)/2, ic->pos.y + 4 + IconSz);
	if(ic->selected){
		sel = getcolor(nil, 0x00008080);
		draw(dst, insetrect(ir, -3), sel, nil, ZP);
	}
	paneldrawicon(ir, ic->icon);
	if(ic->icon == Ibin && binfull){
		/* crumpled paper sticking out of the bin */
		draw(dst, Rect(ir.min.x+4, ir.min.y+2, ir.max.x-4, ir.min.y+8),
			getcolor(nil, 0xFFFFFFFF), nil, ZP);
		line(dst, Pt(ir.min.x+6, ir.min.y+2), Pt(ir.min.x+9, ir.min.y-1), 0, 0, 0,
			getcolor(nil, 0xFFFFFFFF), ZP);
		line(dst, Pt(ir.min.x+11, ir.min.y+2), Pt(ir.min.x+13, ir.min.y), 0, 0, 0,
			getcolor(nil, 0xFFFFFFFF), ZP);
	}

	nlines = splitlabel(ic->label, l1, l2);
	for(i = 0; i < nlines; i++){
		w = stringwidth(font, i == 0 ? l1 : l2);
		lr = Rect(ic->pos.x + (CellW-w)/2 - 2,
			ic->pos.y + IconSz + 8 + i*(font->height+1),
			ic->pos.x + (CellW-w)/2 + w + 2,
			ic->pos.y + IconSz + 8 + (i+1)*(font->height+1));
		if(ic->selected){
			draw(dst, lr, getcolor(nil, 0x000080FF), nil, ZP);
			string(dst, Pt(lr.min.x+2, lr.min.y), display->white, ZP, font, i == 0 ? l1 : l2);
		}else{
			/* white halo so labels read on any wallpaper */
			string(dst, Pt(lr.min.x+3, lr.min.y+1), display->white, ZP, font, i == 0 ? l1 : l2);
			string(dst, Pt(lr.min.x+2, lr.min.y), display->black, ZP, font, i == 0 ? l1 : l2);
		}
	}
}

void
deskicondraw(Image *dst)
{
	Dir *d;
	int i, n, fd;

	if(dst == nil || Dx(dst->r) <= 0)
		return;

	/* recycle bin state: anything in the trash besides the origin log? */
	binfull = 0;
	fd = open(trashdir(), OREAD);
	if(fd >= 0){
		n = dirreadall(fd, &d);
		close(fd);
		for(i = 0; i < n; i++)
			if(strcmp(d[i].name, ".origin") != 0){
				binfull = 1;
				break;
			}
		free(d);
	}

	layouticons();
	for(i = 0; i < nicons; i++)
		drawoneicon(dst, &icons[i]);
}

void
deskiconredraw(void)
{
	draw(fakebg, fakebg->r, background, nil, ZP);
	deskicondraw(fakebg);
	flushimage(display, 1);
}

static int
iconat(Point p)
{
	int i;
	Rectangle r;

	for(i = 0; i < nicons; i++){
		r = Rect(icons[i].pos.x, icons[i].pos.y,
			icons[i].pos.x + CellW, icons[i].pos.y + CellH);
		if(ptinrect(p, r))
			return i;
	}
	return -1;
}

static void
selecticon(int sel)
{
	int i;

	for(i = 0; i < nicons; i++)
		icons[i].selected = i == sel;
	deskiconredraw();
}

/*
 * Button 1 on the background: click selects, drag moves the icon to
 * a free spot (remembered in $home/lib/deskicons), double click
 * launches. Returns TRUE when the event was handled here.
 */
int
deskiconmouse(Mousectl *mc)
{
	Rectangle wr;
	Point start, orig, d;
	int hit, dragging;
	ulong now;

	if(nicons == 0)
		return FALSE;
	if(mc->buttons & 1){
		hit = iconat(mc->xy);
		if(hit < 0){
			selecticon(-1);
			return FALSE;
		}
		selecticon(hit);
		start = mc->xy;
		orig = icons[hit].pos;
		dragging = FALSE;
		wr = panelworkrect();
		while(mc->buttons & 1){
			readmouse(mc);
			d = subpt(mc->xy, start);
			if(!dragging && (abs(d.x) > 6 || abs(d.y) > 6))
				dragging = TRUE;
			if(dragging){
				icons[hit].pos = addpt(orig, d);
				if(icons[hit].pos.x < wr.min.x+4)
					icons[hit].pos.x = wr.min.x+4;
				if(icons[hit].pos.y < wr.min.y+4)
					icons[hit].pos.y = wr.min.y+4;
				if(icons[hit].pos.x > wr.max.x-CellW-4)
					icons[hit].pos.x = wr.max.x-CellW-4;
				if(icons[hit].pos.y > wr.max.y-CellH-4)
					icons[hit].pos.y = wr.max.y-CellH-4;
				deskiconredraw();
			}
		}
		if(dragging){
			savepositions();
			lasticon = -1;
			lastclick = 0;
			selecticon(-1);
			return TRUE;
		}
		now = mc->msec;
		if(lasticon == hit && now - lastclick < 900){
			panellaunch(icons[hit].cmd);
			selecticon(-1);
			lasticon = -1;
			lastclick = 0;
		}else{
			lasticon = hit;
			lastclick = now;
		}
		return TRUE;
	}
	return FALSE;
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
}

/* per-icon menu: button 3 on an icon */
static void
iconmenu(Mousectl *mc, int hit)
{
	char *items[5];
	char *home, *path;
	Biobuf *bp;
	int n, sel;

	n = 0;
	items[n++] = "Open";
	items[n++] = "-";
	items[n++] = "Delete Icon";
	items[n] = nil;

	sel = winmenuhit(mc, Rect(mc->xy.x, mc->xy.y, mc->xy.x+1, mc->xy.y+1), items, n, -1);
	switch(sel){
	case 0:
		panellaunch(icons[hit].cmd);
		break;
	case 2:
		/* remember the deletion so it survives the next load */
		home = getenv("home");
		if(home != nil){
			path = smprint("%s/lib/deskdel", home);
			free(home);
			if(path != nil){
				int dfd;
				dfd = open(path, OWRITE);
				if(dfd < 0)
					dfd = create(path, OWRITE, 0666);
				if(dfd >= 0){
					seek(dfd, 0, 2);
					fprint(dfd, "%s\n", icons[hit].label);
					close(dfd);
				}
				free(path);
			}
		}
		memmove(&icons[hit], &icons[hit+1], (nicons-hit-1)*sizeof(DIcon));
		nicons--;
		deskiconredraw();
		break;
	}
}

void
deskmenuactivate(Mousectl *mc)
{
	char *items[11];
	char *home;
	int sel, n, hit;
	char buf[160];

	if((hit = iconat(mc->xy)) >= 0){
		iconmenu(mc, hit);
		return;
	}

	n = 0;
	items[n++] = "New Terminal";
	items[n++] = "-";
	items[n++] = "Arrange Icons";
	items[n++] = "Refresh";
	items[n++] = "-";
	items[n++] = "New Folder";
	items[n++] = "Empty Recycle Bin";
	items[n++] = "-";
	items[n++] = "Properties";
	items[n] = nil;

	sel = winmenuhit(mc, Rect(mc->xy.x, mc->xy.y, mc->xy.x+1, mc->xy.y+1), items, n, -1);
	switch(sel){
	case 0:
		new(newrect());
		break;
	case 2:
		/* re-grid everything and forget saved spots */
		for(n = 0; n < nicons; n++)
			icons[n].pos = Pt(-1, -1);
		savepositions();
		deskiconredraw();
		break;
	case 3:
		refresh();
		break;
	case 5:
		home = getenv("home");
		if(home != nil){
			for(n = 0; n < 100; n++){
				if(n == 0)
					snprint(buf, sizeof buf, "%s/New Folder", home);
				else
					snprint(buf, sizeof buf, "%s/New Folder %d", home, n);
				if(create(buf, OREAD, DMDIR|0777) >= 0)
					break;
				if(n == 99)
					buf[0] = 0;
			}
			free(home);
		}
		break;
	case 6:
		emptybin();
		deskiconredraw();
		break;
	case 8:
		panellaunch("q9display");
		break;
	}
}
