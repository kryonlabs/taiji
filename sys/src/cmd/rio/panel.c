#include "inc.h"
#include <bio.h>

enum {
	PanelBottom,
	PanelOff,
	PanelHeight = 28,
	StartWidth = 86,
	TrayWidth = 96,
	DeskWidth = 56,
	QuickWidth = 58,
	MenuWidth = 200,
	SubMenuWidth = 190,
	MenuItemHeight = 24,
	MenuSepHeight = 9,
	BannerWidth = 21,
	Nbanner = 16,
	MaxMenuItems = 40,
	IconSize = 16,
	IconGap = 6,
};

enum {
	Iterm,
	Ifolder,
	Iacme,
	Istats,
	Ikbd,
	Ictl,
	Istart,
	Iprog,
	Idoc,
	Ijot,
	Irun,
	Ishut,
	Ioff,
	Iclock,
	Igame,
};

enum {
	SubNone,
	SubPrograms,
	SubDocuments,
	SubSettings,
};

enum {
	Msep = 1,
	Mdisable = 2,
};

typedef struct MenuItem MenuItem;
struct MenuItem {
	char *label;
	char *cmd;
	int icon;
	int sub;
	int flags;
};

static MenuItem setitems[] = {
	{ "Control Panel", "controlpanel", Ictl, 0, 0 },
	{ "Keyboard", "q9kbsetup -reset", Ikbd, 0, 0 },
};

static int edge = PanelBottom;
static Rectangle panelr;
static Image *face;
static Image *hilite;
static Image *shadow;
static Image *darkshadow;
static Image *active;
static Image *menuback;
static Image *seltext;
static Image *yellow;
static Image *green;
static Image *red;
static Image *banner[Nbanner];
static Image *menubackup;
static Image *submenubackup;
static Rectangle menur;
static Rectangle submenur;
static Rectangle clockr;
static Rectangle deskr;
static Rectangle mrects[MaxMenuItems];
static Rectangle srects[MaxMenuItems];
static MenuItem mitems[MaxMenuItems];
static MenuItem sitems[MaxMenuItems];
static int nmitems;
static int nsitems;
static int menuopen;
static int submenuopen;
static int cursub;
static int menusel = -1;
static int submenusel = -1;

void
panelinit(void)
{
	face = getcolor("3d_face", 0xC0C0C0FF);
	hilite = getcolor("3d_hilight2", 0xFFFFFFFF);
	shadow = getcolor("3d_shadow1", 0x808080FF);
	darkshadow = getcolor("3d_shadow2", 0x000000FF);
	active = getcolor("titlebar_active", 0x000080FF);
	menuback = getcolor(nil, 0xC0C0C0FF);
	seltext = getcolor(nil, 0xFFFFFFFF);
	yellow = getcolor(nil, 0xFFFF80FF);
	green = getcolor(nil, 0x008000FF);
	red = getcolor(nil, 0x800000FF);
	panelreset();
}

static void
drawicon(Rectangle r, int icon)
{
	int cx, cy;

	cx = (r.min.x+r.max.x)/2;
	cy = (r.min.y+r.max.y)/2;
	r = Rect(cx-IconSize/2, cy-IconSize/2, cx+IconSize/2, cy+IconSize/2);
	switch(icon){
	case Istart:
		draw(screen, Rect(r.min.x+2, r.min.y+2, r.min.x+8, r.min.y+8), red, nil, ZP);
		draw(screen, Rect(r.min.x+9, r.min.y+2, r.max.x-2, r.min.y+8), green, nil, ZP);
		draw(screen, Rect(r.min.x+2, r.min.y+9, r.min.x+8, r.max.y-2), active, nil, ZP);
		draw(screen, Rect(r.min.x+9, r.min.y+9, r.max.x-2, r.max.y-2), yellow, nil, ZP);
		border(screen, insetrect(r, 1), 1, darkshadow, ZP);
		break;
	case Ifolder:
		draw(screen, Rect(r.min.x+1, r.min.y+5, r.max.x-1, r.max.y-2), yellow, nil, ZP);
		draw(screen, Rect(r.min.x+3, r.min.y+3, r.min.x+10, r.min.y+6), yellow, nil, ZP);
		border(screen, Rect(r.min.x+1, r.min.y+5, r.max.x-1, r.max.y-2), 1, darkshadow, ZP);
		break;
	case Iacme:
		draw(screen, r, display->white, nil, ZP);
		border(screen, r, 1, darkshadow, ZP);
		line(screen, Pt(r.min.x+3, r.min.y+5), Pt(r.max.x-4, r.min.y+5), 0, 0, 0, active, ZP);
		line(screen, Pt(r.min.x+3, r.min.y+9), Pt(r.max.x-4, r.min.y+9), 0, 0, 0, active, ZP);
		break;
	case Istats:
		draw(screen, r, display->white, nil, ZP);
		border(screen, r, 1, darkshadow, ZP);
		draw(screen, Rect(r.min.x+3, r.max.y-5, r.min.x+6, r.max.y-2), green, nil, ZP);
		draw(screen, Rect(r.min.x+7, r.max.y-9, r.min.x+10, r.max.y-2), active, nil, ZP);
		draw(screen, Rect(r.min.x+11, r.max.y-12, r.min.x+14, r.max.y-2), red, nil, ZP);
		break;
	case Ikbd:
		draw(screen, r, display->white, nil, ZP);
		border(screen, r, 1, darkshadow, ZP);
		line(screen, Pt(r.min.x+3, r.min.y+6), Pt(r.max.x-4, r.min.y+6), 0, 0, 0, darkshadow, ZP);
		line(screen, Pt(r.min.x+3, r.min.y+10), Pt(r.max.x-4, r.min.y+10), 0, 0, 0, darkshadow, ZP);
		break;
	case Ictl:
		draw(screen, r, display->white, nil, ZP);
		border(screen, r, 1, darkshadow, ZP);
		draw(screen, Rect(r.min.x+1, r.min.y+1, r.max.x-1, r.min.y+4), active, nil, ZP);
		line(screen, Pt(r.min.x+3, r.min.y+7), Pt(r.max.x-4, r.min.y+7), 0, 0, 0, darkshadow, ZP);
		line(screen, Pt(r.min.x+3, r.min.y+11), Pt(r.max.x-4, r.min.y+11), 0, 0, 0, darkshadow, ZP);
		draw(screen, Rect(r.min.x+5, r.min.y+5, r.min.x+8, r.min.y+9), green, nil, ZP);
		draw(screen, Rect(r.min.x+10, r.min.y+9, r.min.x+13, r.min.y+13), red, nil, ZP);
		draw(screen, Rect(r.max.x-5, r.min.y+2, r.max.x-3, r.min.y+4), hilite, nil, ZP);
		break;
	case Iprog:
		draw(screen, Rect(r.min.x+1, r.min.y+6, r.max.x-1, r.max.y-2), yellow, nil, ZP);
		draw(screen, Rect(r.min.x+3, r.min.y+4, r.min.x+10, r.min.y+7), yellow, nil, ZP);
		border(screen, Rect(r.min.x+1, r.min.y+6, r.max.x-1, r.max.y-2), 1, darkshadow, ZP);
		draw(screen, Rect(r.min.x+5, r.min.y+1, r.max.x-1, r.min.y+9), display->white, nil, ZP);
		border(screen, Rect(r.min.x+5, r.min.y+1, r.max.x-1, r.min.y+9), 1, darkshadow, ZP);
		draw(screen, Rect(r.min.x+6, r.min.y+2, r.max.x-2, r.min.y+4), active, nil, ZP);
		break;
	case Idoc:
		draw(screen, Rect(r.min.x+3, r.min.y+1, r.max.x-4, r.max.y-2), display->white, nil, ZP);
		border(screen, Rect(r.min.x+3, r.min.y+1, r.max.x-4, r.max.y-2), 1, darkshadow, ZP);
		line(screen, Pt(r.max.x-7, r.min.y+1), Pt(r.max.x-4, r.min.y+4), 0, 0, 0, shadow, ZP);
		line(screen, Pt(r.min.x+6, r.min.y+6), Pt(r.max.x-6, r.min.y+6), 0, 0, 0, shadow, ZP);
		line(screen, Pt(r.min.x+6, r.min.y+9), Pt(r.max.x-6, r.min.y+9), 0, 0, 0, shadow, ZP);
		line(screen, Pt(r.min.x+6, r.min.y+12), Pt(r.max.x-6, r.min.y+12), 0, 0, 0, shadow, ZP);
		break;
	case Ijot:
		draw(screen, Rect(r.min.x+3, r.min.y+1, r.max.x-4, r.max.y-2), display->white, nil, ZP);
		border(screen, Rect(r.min.x+3, r.min.y+1, r.max.x-4, r.max.y-2), 1, darkshadow, ZP);
		draw(screen, Rect(r.min.x+3, r.min.y+1, r.max.x-4, r.min.y+4), active, nil, ZP);
		line(screen, Pt(r.min.x+5, r.min.y+8), Pt(r.max.x-6, r.min.y+8), 0, 0, 0, shadow, ZP);
		line(screen, Pt(r.min.x+5, r.min.y+11), Pt(r.max.x-6, r.min.y+11), 0, 0, 0, shadow, ZP);
		break;
	case Irun:
		draw(screen, Rect(r.min.x+1, r.min.y+3, r.max.x-6, r.max.y-3), display->white, nil, ZP);
		border(screen, Rect(r.min.x+1, r.min.y+3, r.max.x-6, r.max.y-3), 1, darkshadow, ZP);
		draw(screen, Rect(r.min.x+2, r.min.y+4, r.max.x-7, r.min.y+6), active, nil, ZP);
		draw(screen, Rect(r.max.x-9, cy-1, r.max.x-1, cy+2), green, nil, ZP);
		draw(screen, Rect(r.max.x-11, cy-3, r.max.x-9, cy+4), green, nil, ZP);
		break;
	case Ishut:
		ellipse(screen, Pt(cx, cy+2), 6, 6, 2, red, ZP);
		draw(screen, Rect(cx-1, r.min.y+1, cx+1, cy-3), red, nil, ZP);
		break;
	case Ioff:
		ellipse(screen, Pt(cx, r.min.y+5), 3, 3, 0, darkshadow, ZP);
		ellipse(screen, Pt(cx, r.max.y-1), 5, 4, 0, darkshadow, ZP);
		break;
	case Iclock:
		ellipse(screen, Pt(cx, cy), 7, 7, 0, display->white, ZP);
		ellipse(screen, Pt(cx, cy), 7, 7, 1, darkshadow, ZP);
		line(screen, Pt(cx, cy), Pt(cx, cy-4), 0, 0, 0, darkshadow, ZP);
		line(screen, Pt(cx, cy), Pt(cx+3, cy+2), 0, 0, 0, darkshadow, ZP);
		break;
	case Igame:
		draw(screen, Rect(r.min.x+2, r.min.y+5, r.max.x-2, r.max.y-3), display->black, nil, ZP);
		draw(screen, Rect(r.min.x+3, r.min.y+6, r.min.x+6, r.min.y+7), red, nil, ZP);
		draw(screen, Rect(r.min.x+7, r.min.y+6, r.min.x+10, r.min.y+7), red, nil, ZP);
		draw(screen, Rect(r.min.x+3, r.max.y-4, r.min.x+6, r.max.y-3), green, nil, ZP);
		draw(screen, Rect(cx+2, r.min.y+9, cx+4, r.min.y+11), yellow, nil, ZP);
		break;
	default:
		draw(screen, r, display->black, nil, ZP);
		draw(screen, insetrect(r, 2), display->white, nil, ZP);
		line(screen, Pt(r.min.x+3, r.max.y-4), Pt(r.max.x-4, r.max.y-4), 0, 0, 0, green, ZP);
		break;
	}
}

static void
button(Rectangle r, char *label, int icon, int down)
{
	Rectangle ir;
	int x;

	if(down)
		winborder(screen, r, darkshadow, hilite);
	else
		winborder(screen, r, hilite, darkshadow);
	r = insetrect(r, 1);
	if(down)
		winborder(screen, r, shadow, face);
	else
		winborder(screen, r, face, shadow);
	draw(screen, insetrect(r, 1), face, nil, ZP);
	if(icon >= 0){
		ir = Rect(r.min.x+4+down, r.min.y+3+down, r.min.x+4+IconSize+down, r.min.y+3+IconSize+down);
		drawicon(ir, icon);
	}
	if(label != nil){
		x = r.min.x + 8;
		if(icon >= 0)
			x = r.min.x + 4 + IconSize + IconGap;
		string(screen, Pt(x+down, r.min.y+(Dy(r)-font->height)/2+down),
			display->black, ZP, font, label);
	}
}

static Rectangle
taskrect(int i)
{
	int x, maxx, w;

	x = panelr.min.x + StartWidth + QuickWidth + 8;
	maxx = panelr.max.x - TrayWidth - DeskWidth - 8;
	w = 130;
	if(nwindows > 0)
		w = MIN(150, MAX(70, (maxx-x)/nwindows));
	return Rect(x+i*w, panelr.min.y+4, MIN(x+(i+1)*w-3, maxx), panelr.max.y-4);
}

static Rectangle
deskitemrect(int i)
{
	Rectangle r;
	int col, row, w, h;

	col = i%ndeskx;
	row = i/ndeskx;
	r = insetrect(deskr, 1);
	w = Dx(r)/ndeskx;
	h = Dy(r)/ndesky;
	return Rect(r.min.x+col*w, r.min.y+row*h,
		r.min.x+(col+1)*w-1, r.min.y+(row+1)*h-1);
}

static int
deskitemat(Point p)
{
	int i;

	if(!ptinrect(p, deskr))
		return -1;
	for(i = 0; i < ndeskx*ndesky; i++)
		if(ptinrect(p, deskitemrect(i)))
			return i;
	return -1;
}

static void
deskdraw(void)
{
	Rectangle r;
	int i, cur;

	if(Dx(screen->r) <= 0 || Dy(screen->r) <= 0)
		return;
	cur = screenoff.x/Dx(screen->r) + ndeskx*(screenoff.y/Dy(screen->r));
	winborder(screen, deskr, shadow, hilite);
	draw(screen, insetrect(deskr, 1), face, nil, ZP);
	for(i = 0; i < ndeskx*ndesky; i++){
		r = deskitemrect(i);
		draw(screen, r, i == cur ? active : shadow, nil, ZP);
		border(screen, r, 1, darkshadow, ZP);
	}
}

static void
launch(char *cmd)
{
	WinTab *t;
	char *argv[4];

	t = wtcreate(newrect(), FALSE, scrolling);
	if(t == nil)
		return;
	argv[0] = "rc";
	argv[1] = "-c";
	argv[2] = cmd;
	argv[3] = nil;
	wincmd(t, 0, nil, argv);
}

int
panelenabled(void)
{
	return edge != PanelOff;
}

void
panelreset(void)
{
	panelr = ZR;
}

void
panelsetedge(char *s)
{
	if(s == nil)
		return;
	if(strcmp(s, "off") == 0 || strcmp(s, "none") == 0)
		edge = PanelOff;
	else
		edge = PanelBottom;
}

Rectangle
panelworkrect(void)
{
	Rectangle r;

	r = screen->r;
	if(edge != PanelOff)
		r.max.y -= PanelHeight;
	return r;
}

void
paneldraw(void)
{
	Rectangle r, br;
	Window *w;
	int i;
	Tm *tm;
	char clock[32];

	if(edge == PanelOff || screen == nil)
		return;
	panelr = screen->r;
	panelr.min.y = panelr.max.y - PanelHeight;

	draw(screen, panelr, face, nil, ZP);
	draw(screen, Rect(panelr.min.x, panelr.min.y, panelr.max.x, panelr.min.y+1),
		hilite, nil, ZP);
	draw(screen, Rect(panelr.min.x, panelr.min.y+1, panelr.max.x, panelr.min.y+2),
		shadow, nil, ZP);

	button(Rect(panelr.min.x+3, panelr.min.y+3, panelr.min.x+StartWidth-4, panelr.max.y-3),
		"Start", Istart, 0);

	br = Rect(panelr.min.x+StartWidth+4, panelr.min.y+3,
		panelr.min.x+StartWidth+28, panelr.max.y-3);
	button(br, nil, Ifolder, 0);
	br = rectaddpt(br, Pt(28, 0));
	button(br, nil, Ictl, 0);

	i = 0;
	for(w = bottomwin; w; w = w->higher){
		if(w->cur == nil)
			continue;
		r = taskrect(i++);
		if(r.min.x >= r.max.x)
			break;
		button(r, w->cur->label, -1, w == focused);
	}

	clockr = Rect(panelr.max.x-TrayWidth+4, panelr.min.y+4, panelr.max.x-4, panelr.max.y-4);
	deskr = Rect(clockr.min.x-DeskWidth, panelr.min.y+2, clockr.min.x-4, panelr.max.y-2);
	deskdraw();
	winborder(screen, clockr, shadow, hilite);
	draw(screen, insetrect(clockr, 1), face, nil, ZP);
	tm = localtime(time(0));
	snprint(clock, sizeof clock, "%02d:%02d", tm->hour, tm->min);
	string(screen, Pt(clockr.min.x+18, clockr.min.y+(Dy(clockr)-font->height)/2),
		display->black, ZP, font, clock);
}

/* ---- start menu items ---- */

static int
itemheight(MenuItem *it)
{
	return (it->flags & Msep) ? MenuSepHeight : MenuItemHeight;
}

static void
freeitems(MenuItem *items, int *n)
{
	int i;

	for(i = 0; i < *n; i++){
		free(items[i].label);
		free(items[i].cmd);
	}
	*n = 0;
}

static void
additem(MenuItem *items, int *n, char *label, char *cmd, int icon, int sub, int flags)
{
	if(*n >= MaxMenuItems)
		return;
	if(flags & Msep){
		items[*n].label = nil;
		items[*n].cmd = nil;
		items[*n].icon = -1;
		items[*n].sub = SubNone;
		items[*n].flags = Msep;
	}else{
		items[*n].label = estrdup(label);
		items[*n].cmd = cmd != nil ? estrdup(cmd) : nil;
		items[*n].icon = icon;
		items[*n].sub = sub;
		items[*n].flags = flags;
	}
	(*n)++;
}

static int
iconbyname(char *name)
{
	static struct { char *name; int icon; } tab[] = {
		{ "term", Iterm },
		{ "folder", Ifolder },
		{ "acme", Iacme },
		{ "stats", Istats },
		{ "kbd", Ikbd },
		{ "ctl", Ictl },
		{ "jot", Ijot },
		{ "clock", Iclock },
		{ "doc", Idoc },
		{ "run", Irun },
		{ "shut", Ishut },
		{ "off", Ioff },
		{ "game", Igame },
		{ "prog", Iprog },
	};
	int i;

	for(i = 0; i < nelem(tab); i++)
		if(strcmp(name, tab[i].name) == 0)
			return tab[i].icon;
	return Ifolder;
}

static char*
doccmd(char *path)
{
	Dir *d;
	char *cmd;

	d = dirstat(path);
	if(d != nil && (d->mode & DMDIR))
		cmd = smprint("explorer '%s'", path);
	else
		cmd = smprint("jot '%s'", path);
	free(d);
	return cmd;
}

/* each line: label<TAB>command[<TAB>icon] */
static void
readprogfile(char *path)
{
	Biobuf *bp;
	char *line, *f[4];
	int nf;

	bp = Bopen(path, OREAD);
	if(bp == nil)
		return;
	while(nsitems < MaxMenuItems-1 && (line = Brdline(bp, '\n')) != nil){
		line[Blinelen(bp)-1] = 0;
		nf = getfields(line, f, nelem(f), 0, "\t");
		if(nf < 2 || f[0][0] == 0 || f[1][0] == 0)
			continue;
		additem(sitems, &nsitems, f[0], f[1],
			nf > 2 && f[2][0] != 0 ? iconbyname(f[2]) : Ifolder,
			SubNone, 0);
	}
	Bterm(bp);
}

/* each line: label<TAB>path */
static void
readrecent(void)
{
	Biobuf *bp;
	char *home, *path, *line, *f[3], *cmd;
	int nf;

	home = getenv("home");
	if(home == nil)
		return;
	path = smprint("%s/lib/recent", home);
	free(home);
	if(path == nil)
		return;
	bp = Bopen(path, OREAD);
	free(path);
	if(bp == nil)
		return;
	while(nsitems < MaxMenuItems-1 && (line = Brdline(bp, '\n')) != nil){
		line[Blinelen(bp)-1] = 0;
		nf = getfields(line, f, nelem(f), 0, "\t");
		if(nf < 2 || f[0][0] == 0 || f[1][0] == 0)
			continue;
		cmd = doccmd(f[1]);
		additem(sitems, &nsitems, f[0], cmd, Idoc, SubNone, 0);
		free(cmd);
	}
	Bterm(bp);
}

static void
buildsub(int sub)
{
	char *home, *path;
	int i;

	freeitems(sitems, &nsitems);
	switch(sub){
	case SubSettings:
		for(i = 0; i < nelem(setitems); i++)
			additem(sitems, &nsitems, setitems[i].label, setitems[i].cmd,
				setitems[i].icon, SubNone, 0);
		break;
	case SubPrograms:
		readprogfile("/lib/q9/programs");
		home = getenv("home");
		if(home != nil){
			path = smprint("%s/lib/programs", home);
			free(home);
			if(path != nil){
				readprogfile(path);
				free(path);
			}
		}
		if(nsitems == 0)
			additem(sitems, &nsitems, "(empty)", nil, -1, SubNone, Mdisable);
		break;
	case SubDocuments:
		readrecent();
		if(nsitems == 0)
			additem(sitems, &nsitems, "(empty)", nil, -1, SubNone, Mdisable);
		break;
	}
}

static void
buildmain(void)
{
	freeitems(mitems, &nmitems);
	additem(mitems, &nmitems, "Programs", nil, Iprog, SubPrograms, 0);
	additem(mitems, &nmitems, "Documents", nil, Idoc, SubDocuments, 0);
	additem(mitems, &nmitems, nil, nil, -1, SubNone, Msep);
	additem(mitems, &nmitems, "Settings", nil, Ictl, SubSettings, 0);
	additem(mitems, &nmitems, "Run...", "q9run", Irun, SubNone, 0);
	additem(mitems, &nmitems, nil, nil, -1, SubNone, Msep);
	additem(mitems, &nmitems, "Log Out", "q9logout", Ioff, SubNone, 0);
	additem(mitems, &nmitems, "Shut Down", "q9shut", Ishut, SubNone, 0);
}

/* ---- start menu geometry and drawing ---- */

static void
layoutmenu(void)
{
	Rectangle r;
	int i, y;

	r = insetrect(menur, 3);
	r.min.x += BannerWidth;
	y = r.min.y;
	for(i = 0; i < nmitems; i++){
		mrects[i] = Rect(r.min.x, y, r.max.x, y + itemheight(&mitems[i]));
		y += itemheight(&mitems[i]);
	}
}

static void
layoutsub(void)
{
	Rectangle r;
	int i, y;

	r = insetrect(submenur, 3);
	y = r.min.y;
	for(i = 0; i < nsitems; i++){
		srects[i] = Rect(r.min.x, y, r.max.x, y + itemheight(&sitems[i]));
		y += itemheight(&sitems[i]);
	}
}

static ulong
lerpcolor(ulong c0, ulong c1, int i, int n)
{
	int r, g, b;

	r = ((c0>>24 & 0xFF)*(n-i) + (c1>>24 & 0xFF)*i) / n;
	g = ((c0>>16 & 0xFF)*(n-i) + (c1>>16 & 0xFF)*i) / n;
	b = ((c0>>8 & 0xFF)*(n-i) + (c1>>8 & 0xFF)*i) / n;
	return r<<24 | g<<16 | b<<8 | 0xFF;
}

static void
drawbanner(Rectangle strip)
{
	char *s = "Plan 9";
	char buf[2];
	int i, y, w, h;

	for(i = 0; i < Nbanner; i++)
		if(banner[i] == nil)
			banner[i] = getcolor(nil, lerpcolor(0x0A246AFF, 0xA6CAF0FF, i, Nbanner-1));
	h = Dy(strip);
	for(i = 0; i < Nbanner; i++)
		draw(screen, Rect(strip.min.x, strip.min.y + h*i/Nbanner,
			strip.max.x, strip.min.y + h*(i+1)/Nbanner), banner[i], nil, ZP);
	buf[1] = 0;
	y = strip.max.y - 4;
	for(i = strlen(s)-1; i >= 0; i--){
		buf[0] = s[i];
		if(buf[0] != ' '){
			w = stringwidth(font, buf);
			string(screen, Pt(strip.min.x + (Dx(strip)-w)/2, y - font->height),
				display->white, ZP, font, buf);
		}
		y -= font->height + 1;
	}
}

static int
menuitemat(Point p)
{
	int i;

	for(i = 0; i < nmitems; i++)
		if(ptinrect(p, mrects[i]))
			return i;
	return -1;
}

static int
submenuitemat(Point p)
{
	int i;

	if(!submenuopen)
		return -1;
	for(i = 0; i < nsitems; i++)
		if(ptinrect(p, srects[i]))
			return i;
	return -1;
}

static void
drawmenuitem(int i, int hover)
{
	Rectangle r;
	Point pt;
	Image *txt;

	r = mrects[i];
	if(mitems[i].flags & Msep){
		draw(screen, r, menuback, nil, ZP);
		pt = Pt(r.min.x+4, r.min.y + MenuSepHeight/2);
		line(screen, pt, Pt(r.max.x-4, pt.y), 0, 0, 0, shadow, ZP);
		line(screen, Pt(pt.x, pt.y+1), Pt(r.max.x-4, pt.y+1), 0, 0, 0, hilite, ZP);
		return;
	}
	hover = hover && !(mitems[i].flags & Mdisable);
	draw(screen, r, hover ? active : menuback, nil, ZP);
	drawicon(Rect(r.min.x+4, r.min.y+(MenuItemHeight-IconSize)/2,
		r.min.x+4+IconSize, r.min.y+(MenuItemHeight-IconSize)/2+IconSize),
		mitems[i].icon);
	txt = display->black;
	if(mitems[i].flags & Mdisable)
		txt = shadow;
	else if(hover)
		txt = seltext;
	pt = Pt(r.min.x+28, r.min.y+(MenuItemHeight-font->height)/2);
	string(screen, pt, txt, ZP, font, mitems[i].label);
	if(mitems[i].sub != SubNone)
		string(screen, Pt(r.max.x-14, r.min.y+(MenuItemHeight-font->height)/2),
			txt, ZP, font, ">");
}

static void
drawsubmenuitem(int i, int hover)
{
	Rectangle r;
	Point pt;
	Image *txt;

	r = srects[i];
	if(sitems[i].flags & Msep){
		draw(screen, r, menuback, nil, ZP);
		pt = Pt(r.min.x+4, r.min.y + MenuSepHeight/2);
		line(screen, pt, Pt(r.max.x-4, pt.y), 0, 0, 0, shadow, ZP);
		line(screen, Pt(pt.x, pt.y+1), Pt(r.max.x-4, pt.y+1), 0, 0, 0, hilite, ZP);
		return;
	}
	hover = hover && !(sitems[i].flags & Mdisable);
	draw(screen, r, hover ? active : menuback, nil, ZP);
	drawicon(Rect(r.min.x+4, r.min.y+(MenuItemHeight-IconSize)/2,
		r.min.x+4+IconSize, r.min.y+(MenuItemHeight-IconSize)/2+IconSize),
		sitems[i].icon);
	txt = display->black;
	if(sitems[i].flags & Mdisable)
		txt = shadow;
	else if(hover)
		txt = seltext;
	pt = Pt(r.min.x+28, r.min.y+(MenuItemHeight-font->height)/2);
	string(screen, pt, txt, ZP, font, sitems[i].label);
}

static void
submenuhide(void)
{
	if(submenubackup){
		draw(screen, submenur, submenubackup, nil, submenur.min);
		freeimage(submenubackup);
		submenubackup = nil;
	}
	submenuopen = FALSE;
	submenusel = -1;
	cursub = SubNone;
	flushimage(display, 1);
}

static void
submenushow(int sub)
{
	Rectangle r;
	int i, h;

	if(!menuopen || menusel < 0 || mitems[menusel].sub != sub)
		return;
	if(submenuopen && cursub == sub)
		return;
	if(submenuopen)
		submenuhide();
	buildsub(sub);
	cursub = sub;
	h = 6;
	for(i = 0; i < nsitems; i++)
		h += itemheight(&sitems[i]);
	r = mrects[menusel];
	submenur = Rect(menur.max.x-2, r.min.y, menur.max.x-2+SubMenuWidth, r.min.y+h);
	if(submenur.max.y > screen->r.max.y){
		submenur.min.y -= submenur.max.y - screen->r.max.y;
		submenur.max.y = screen->r.max.y;
	}
	layoutsub();
	submenubackup = allocimage(display, submenur, screen->chan, 0, -1);
	if(submenubackup)
		draw(submenubackup, submenur, screen, nil, submenur.min);
	draw(screen, submenur, menuback, nil, ZP);
	winborder(screen, submenur, hilite, darkshadow);
	for(i = 0; i < nsitems; i++)
		drawsubmenuitem(i, 0);
	submenuopen = TRUE;
	submenusel = -1;
	flushimage(display, 1);
}

static void
menushow(void)
{
	int i, h;

	if(menuopen)
		return;
	submenuhide();
	buildmain();
	h = 6;
	for(i = 0; i < nmitems; i++)
		h += itemheight(&mitems[i]);
	menur = Rect(panelr.min.x+3, panelr.min.y-h-2,
		panelr.min.x+3+MenuWidth, panelr.min.y-2);
	layoutmenu();
	freeimage(menubackup);
	menubackup = allocimage(display, menur, screen->chan, 0, -1);
	if(menubackup)
		draw(menubackup, menur, screen, nil, menur.min);
	draw(screen, menur, menuback, nil, ZP);
	winborder(screen, menur, hilite, darkshadow);
	drawbanner(Rect(menur.min.x+2, menur.min.y+2,
		menur.min.x+2+BannerWidth, menur.max.y-2));
	for(i = 0; i < nmitems; i++)
		drawmenuitem(i, 0);
	menusel = -1;
	menuopen = TRUE;
	flushimage(display, 1);
}

static void
menuhide(void)
{
	submenuhide();
	if(menubackup){
		draw(screen, menur, menubackup, nil, menur.min);
		freeimage(menubackup);
		menubackup = nil;
	}
	menuopen = FALSE;
	menusel = -1;
	flushimage(display, 1);
}

static void
menuactivate(void)
{
	if(submenuopen && submenusel >= 0){
		if(sitems[submenusel].cmd != nil && !(sitems[submenusel].flags & Mdisable)){
			menuhide();
			launch(sitems[submenusel].cmd);
		}
		return;
	}
	if(menusel >= 0 && mitems[menusel].cmd != nil && !(mitems[menusel].flags & Mdisable)){
		menuhide();
		launch(mitems[menusel].cmd);
	}
}

/* step over separators and disabled items */
static int
skipnav(int sel, int dir, int n, MenuItem *items)
{
	for(;;){
		sel += dir;
		if(sel < 0 || sel >= n)
			return -1;
		if(!(items[sel].flags & (Msep|Mdisable)))
			return sel;
	}
}

static void
navmain(int dir)
{
	int sel, start;

	if(nmitems == 0)
		return;
	start = menusel;
	if(start < 0)
		start = dir > 0 ? -1 : nmitems;
	sel = skipnav(start, dir, nmitems, mitems);
	if(sel < 0)
		return;
	if(menusel >= 0)
		drawmenuitem(menusel, 0);
	menusel = sel;
	drawmenuitem(menusel, 1);
	if(mitems[menusel].sub != SubNone)
		submenushow(mitems[menusel].sub);
	else if(submenuopen)
		submenuhide();
	flushimage(display, 1);
}

static void
navsub(int dir)
{
	int sel, start;

	if(!submenuopen || nsitems == 0)
		return;
	start = submenusel;
	if(start < 0)
		start = dir > 0 ? -1 : nsitems;
	sel = skipnav(start, dir, nsitems, sitems);
	if(sel < 0)
		return;
	if(submenusel >= 0)
		drawsubmenuitem(submenusel, 0);
	submenusel = sel;
	drawsubmenuitem(submenusel, 1);
	flushimage(display, 1);
}

static int menumouse(Mousectl *mc);

static void
panelcontextmenu(Mousectl *mc)
{
	enum {
		PanelRefresh,
		Bottom,
		HidePanel,
	};
	static char *str[] = {
		"Refresh Panel",
		"Bottom Edge",
		"Hide Panel",
		nil,
	};
	static Menu menu = { str };

	switch(menuhit(3, mc, &menu, wscreen)){
	case PanelRefresh:
		paneldraw();
		break;
	case Bottom:
		edge = PanelBottom;
		panelreset();
		refresh();
		break;
	case HidePanel:
		edge = PanelOff;
		refresh();
		break;
	}
}

int
panelmouse(Mousectl *mc)
{
	Rectangle r;
	Window *w;
	int i;

	if(edge == PanelOff)
		return 0;
	if(menuopen)
		return menumouse(mc);
	if(!ptinrect(mc->xy, panelr))
		return 0;
	if(mc->buttons == 0)
		return 1;
	if(mc->buttons & 1){
		r = Rect(panelr.min.x+3, panelr.min.y+3, panelr.min.x+StartWidth-4, panelr.max.y-3);
		if(ptinrect(mc->xy, r)){
			paneldraw();
			menushow();
			drainmouse(mc, nil);
			return 1;
		}
		r = Rect(panelr.min.x+StartWidth+4, panelr.min.y+3,
			panelr.min.x+StartWidth+28, panelr.max.y-3);
		if(ptinrect(mc->xy, r)){
			launch("explorer /");
			drainmouse(mc, nil);
			paneldraw();
			return 1;
		}
		r = rectaddpt(r, Pt(28, 0));
		if(ptinrect(mc->xy, r)){
			launch("controlpanel");
			drainmouse(mc, nil);
			paneldraw();
			return 1;
		}
		if(ptinrect(mc->xy, clockr)){
			launch("clock");
			drainmouse(mc, nil);
			paneldraw();
			return 1;
		}
		i = deskitemat(mc->xy);
		if(i >= 0){
			screenoffset((i%ndeskx)*Dx(screen->r), (i/ndeskx)*Dy(screen->r));
			drainmouse(mc, nil);
			paneldraw();
			return 1;
		}
		i = 0;
		for(w = bottomwin; w; w = w->higher){
			if(w->cur == nil)
				continue;
			r = taskrect(i++);
			if(ptinrect(mc->xy, r)){
				if(w->hidden)
					wunhide(w);
				wraise(w);
				wfocus(w);
				drainmouse(mc, nil);
				paneldraw();
				return 1;
			}
		}
	}
	if(mc->buttons & 4)
		panelcontextmenu(mc);
	drainmouse(mc, nil);
	paneldraw();
	return 1;
}

int
panelmenuopen(void)
{
	return menuopen;
}

void
panelkey(ulong kr)
{
	Rune r;

	if(!menuopen){
		if(kr == 0)
			menushow();
		return;
	}
	r = kr;
	if(submenuopen){
		switch(r){
		case Kup:
			navsub(-1);
			break;
		case Kdown:
			navsub(1);
			break;
		case Kleft:
			submenuhide();
			break;
		case '\n':
		case '\r':
			menuactivate();
			break;
		case Kesc:
			menuhide();
			break;
		}
		return;
	}
	switch(r){
	case Kup:
		navmain(-1);
		break;
	case Kdown:
		navmain(1);
		break;
	case Kright:
		if(menusel >= 0 && mitems[menusel].sub != SubNone){
			submenushow(mitems[menusel].sub);
			navsub(1);
		}
		break;
	case '\n':
	case '\r':
		menuactivate();
		break;
	case Kesc:
		menuhide();
		break;
	}
}

static int
menumouse(Mousectl *mc)
{
	int hover, subhover;

	hover = menuitemat(mc->xy);
	subhover = submenuitemat(mc->xy);
	if(subhover < 0 && hover != menusel){
		if(submenuopen && (hover < 0 || mitems[hover].sub == SubNone))
			submenuhide();
		if(menusel >= 0)
			drawmenuitem(menusel, 0);
		if(hover >= 0)
			drawmenuitem(hover, 1);
		menusel = hover;
		flushimage(display, 1);
	}
	if(menusel >= 0 && mitems[menusel].sub != SubNone)
		submenushow(mitems[menusel].sub);
	if(subhover != submenusel){
		if(submenusel >= 0)
			drawsubmenuitem(submenusel, 0);
		if(subhover >= 0)
			drawsubmenuitem(subhover, 1);
		submenusel = subhover;
		flushimage(display, 1);
	}
	if(mc->buttons & 4){
		menuhide();
		drainmouse(mc, nil);
		return 1;
	}
	if(mc->buttons & 1){
		drainmouse(mc, nil);
		if(hover < 0 && subhover < 0)
			menuhide();
		else
			menuactivate();
		return 1;
	}
	return 1;
}
