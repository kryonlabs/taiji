#include "inc.h"

enum {
	PanelBottom,
	PanelOff,
	PanelHeight = 28,
	StartWidth = 86,
	TrayWidth = 96,
	QuickWidth = 58,
	MenuWidth = 176,
	MenuItemHeight = 24,
	IconSize = 16,
	IconGap = 6,
};

typedef struct StartItem StartItem;
struct StartItem {
	char *label;
	char *cmd;
	int icon;
	int submenu;
};

enum {
	Iterm,
	Ifolder,
	Iacme,
	Istats,
	Ikbd,
	Ictl,
	Istart,
};

static StartItem startitems[] = {
	{ "Terminal", "rc -i", Iterm, 0 },
	{ "Explorer", "explorer /", Ifolder, 0 },
	{ "Acme", "acme", Iacme, 0 },
	{ "Stats", "stats -lmisce", Istats, 0 },
	{ "Settings", nil, Ictl, 1 },
};

static StartItem settingsitems[] = {
	{ "Control Panel", "controlpanel", Ictl, 0 },
	{ "Keyboard", "q9kbsetup -reset", Ikbd, 0 },
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
static Image *menubackup;
static Image *submenubackup;
static Rectangle menur;
static Rectangle submenur;
static Rectangle clockr;
static int menuopen;
static int submenuopen;
static int menuhover = -1;
static int submenuhover = -1;

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
	maxx = panelr.max.x - TrayWidth - 8;
	w = 130;
	if(nwindows > 0)
		w = MIN(150, MAX(70, (maxx-x)/nwindows));
	return Rect(x+i*w, panelr.min.y+4, MIN(x+(i+1)*w-3, maxx), panelr.max.y-4);
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
	winborder(screen, clockr, shadow, hilite);
	draw(screen, insetrect(clockr, 1), face, nil, ZP);
	tm = localtime(time(0));
	snprint(clock, sizeof clock, "%02d:%02d", tm->hour, tm->min);
	string(screen, Pt(clockr.min.x+18, clockr.min.y+(Dy(clockr)-font->height)/2),
		display->black, ZP, font, clock);
}

static Rectangle
menuitemrect(int i)
{
	Rectangle r;

	r = insetrect(menur, 3);
	r.min.y += MenuItemHeight*i;
	r.max.y = r.min.y + MenuItemHeight;
	return r;
}

static void
drawmenuitem(int i, int hover)
{
	Rectangle r;

	r = menuitemrect(i);
	draw(screen, r, hover ? active : menuback, nil, ZP);
	drawicon(Rect(r.min.x+3, r.min.y+3, r.min.x+21, r.min.y+21),
		startitems[i].icon);
	string(screen, Pt(r.min.x+26, r.min.y+5), hover ? seltext : display->black,
		ZP, font, startitems[i].label);
	if(startitems[i].submenu)
		string(screen, Pt(r.max.x-14, r.min.y+5), hover ? seltext : display->black,
			ZP, font, ">");
}

static Rectangle
submenuitemrect(int i)
{
	Rectangle r;

	r = insetrect(submenur, 3);
	r.min.y += MenuItemHeight*i;
	r.max.y = r.min.y + MenuItemHeight;
	return r;
}

static void
drawsubmenuitem(int i, int hover)
{
	Rectangle r;

	r = submenuitemrect(i);
	draw(screen, r, hover ? active : menuback, nil, ZP);
	drawicon(Rect(r.min.x+3, r.min.y+3, r.min.x+21, r.min.y+21),
		settingsitems[i].icon);
	string(screen, Pt(r.min.x+26, r.min.y+5), hover ? seltext : display->black,
		ZP, font, settingsitems[i].label);
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
	submenuhover = -1;
	flushimage(display, 1);
}

static void
submenushow(void)
{
	Rectangle r;
	int i, n;

	if(submenuopen)
		return;
	n = nelem(settingsitems);
	r = menuitemrect(menuhover);
	submenur = Rect(menur.max.x-2, r.min.y,
		menur.max.x-2+MenuWidth, r.min.y+n*MenuItemHeight+6);
	submenubackup = allocimage(display, submenur, screen->chan, 0, -1);
	if(submenubackup)
		draw(submenubackup, submenur, screen, nil, submenur.min);
	draw(screen, submenur, menuback, nil, ZP);
	winborder(screen, submenur, hilite, darkshadow);
	for(i = 0; i < n; i++)
		drawsubmenuitem(i, 0);
	submenuopen = TRUE;
	submenuhover = -1;
	flushimage(display, 1);
}

static void
menushow(void)
{
	int i, n;

	n = nelem(startitems);
	menur = Rect(panelr.min.x+3, panelr.min.y-n*MenuItemHeight-8,
		panelr.min.x+3+MenuWidth, panelr.min.y-2);
	freeimage(menubackup);
	menubackup = allocimage(display, menur, screen->chan, 0, -1);
	if(menubackup)
		draw(menubackup, menur, screen, nil, menur.min);
	draw(screen, menur, menuback, nil, ZP);
	winborder(screen, menur, hilite, darkshadow);
	for(i = 0; i < n; i++)
		drawmenuitem(i, 0);
	menuhover = -1;
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
	menuhover = -1;
	flushimage(display, 1);
}

static int
menuitemat(Point p)
{
	Rectangle r;
	int i;

	for(i = 0; i < nelem(startitems); i++){
		r = menuitemrect(i);
		if(ptinrect(p, r))
			return i;
	}
	return -1;
}

static int
submenuitemat(Point p)
{
	Rectangle r;
	int i;

	if(!submenuopen)
		return -1;
	for(i = 0; i < nelem(settingsitems); i++){
		r = submenuitemrect(i);
		if(ptinrect(p, r))
			return i;
	}
	return -1;
}

static int
menumouse(Mousectl *mc)
{
	int hover, subhover, sel;

	hover = menuitemat(mc->xy);
	subhover = submenuitemat(mc->xy);
	if(subhover < 0 && hover != menuhover){
		if(submenuopen && (hover < 0 || !startitems[hover].submenu))
			submenuhide();
		if(menuhover >= 0)
			drawmenuitem(menuhover, 0);
		if(hover >= 0)
			drawmenuitem(hover, 1);
		menuhover = hover;
		flushimage(display, 1);
	}
	if(menuhover >= 0 && startitems[menuhover].submenu)
		submenushow();
	if(subhover != submenuhover){
		if(submenuhover >= 0)
			drawsubmenuitem(submenuhover, 0);
		if(subhover >= 0)
			drawsubmenuitem(subhover, 1);
		submenuhover = subhover;
		flushimage(display, 1);
	}
	if(mc->buttons & 4){
		menuhide();
		drainmouse(mc, nil);
		return 1;
	}
	if(mc->buttons & 1){
		sel = subhover;
		drainmouse(mc, nil);
		menuhide();
		if(sel >= 0)
			launch(settingsitems[sel].cmd);
		else if(hover >= 0 && startitems[hover].cmd != nil)
			launch(startitems[hover].cmd);
		return 1;
	}
	return 1;
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
		menushow();
	drainmouse(mc, nil);
	paneldraw();
	return 1;
}

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
