#include "inc.h"

enum {
	PanelTop,
	PanelBottom,
	PanelLeft,
	PanelRight,
	PanelSize = 26,
	StartWidth = 72,
	ClockWidth = 128,
	TaskMinWidth = 72,
	TaskMaxWidth = 180,
	StartMenuWidth = 132,
	StartMenuItemHeight = 24,
};

typedef struct Task Task;
struct Task {
	Window *w;
	Rectangle r;
};

static int panelon;
static int paneledge = PanelTop;
static Rectangle panelr;
static Rectangle startr;
static Rectangle taskr;
static Rectangle clockr;
static Task tasks[MAXWINDOWS];
static int ntasks;
static char clockbuf[64];
static Image *panelimg;
static Image *panel_face;
static Image *panel_light;
static Image *panel_light2;
static Image *panel_shadow;
static Image *panel_dark;
static Image *panel_text;

static void panelclock(void*);

int
panelenabled(void)
{
	return panelon;
}

void
panelreset(void)
{
	if(panelimg != nil){
		freeimage(panelimg);
		panelimg = nil;
	}
}

void
panelsetedge(char *s)
{
	if(s == nil)
		return;
	if(strcmp(s, "off") == 0 || strcmp(s, "none") == 0){
		panelon = 0;
		panelreset();
		return;
	}
	panelon = 1;
	if(strcmp(s, "top") == 0)
		paneledge = PanelTop;
	else if(strcmp(s, "bottom") == 0)
		paneledge = PanelBottom;
	else if(strcmp(s, "left") == 0)
		paneledge = PanelLeft;
	else if(strcmp(s, "right") == 0)
		paneledge = PanelRight;
}

Rectangle
panelworkrect(void)
{
	Rectangle r;

	r = screen->r;
	if(!panelon)
		return r;
	switch(paneledge){
	case PanelTop:
		r.min.y += PanelSize;
		break;
	case PanelBottom:
		r.max.y -= PanelSize;
		break;
	case PanelLeft:
		r.min.x += StartWidth;
		break;
	case PanelRight:
		r.max.x -= StartWidth;
		break;
	}
	return r;
}

static void
panelcolors(void)
{
	if(panel_face != nil)
		return;
	panel_face = getcolor("3d_face", 0xC0C0C0FF);
	panel_light = getcolor("3d_hilight1", 0xC0C0C0FF);
	panel_light2 = getcolor("3d_hilight2", 0xFFFFFFFF);
	panel_shadow = getcolor("3d_shadow1", 0x808080FF);
	panel_dark = getcolor("3d_shadow2", 0x000000FF);
	panel_text = getcolor("text", 0x000000FF);
}

static void
panelbevel(Image *img, Rectangle r, int down)
{
	winborder(img, r, down ? panel_dark : panel_light2,
		down ? panel_light2 : panel_dark);
	r = insetrect(r, 1);
	winborder(img, r, down ? panel_shadow : panel_light,
		down ? panel_light : panel_shadow);
}

static void
paneltext(Image *img, Rectangle r, char *s, int center)
{
	Point p;

	p = Pt(r.min.x+6, r.min.y+(Dy(r)-font->height)/2);
	if(center)
		p.x = r.min.x + (Dx(r)-stringwidth(font, s))/2;
	if(p.x < r.min.x+4)
		p.x = r.min.x+4;
	string(img, p, panel_text, ZP, font, s);
}

static void
panelclockstr(void)
{
	Tm *tm;

	tm = localtime(time(nil));
	snprint(clockbuf, sizeof clockbuf, "%04d-%02d-%02d %02d:%02d",
		tm->year+1900, tm->mon+1, tm->mday, tm->hour, tm->min);
}

static void
panellayout(void)
{
	int i, n, w, start, end;

	if(!panelon)
		return;
	panelr = screen->r;
	switch(paneledge){
	case PanelTop:
		panelr.max.y = panelr.min.y + PanelSize;
		startr = panelr;
		startr.max.x = startr.min.x + StartWidth;
		clockr = panelr;
		clockr.min.x = clockr.max.x - ClockWidth;
		taskr = panelr;
		taskr.min.x = startr.max.x + 4;
		taskr.max.x = clockr.min.x - 4;
		break;
	case PanelBottom:
		panelr.min.y = panelr.max.y - PanelSize;
		startr = panelr;
		startr.max.x = startr.min.x + StartWidth;
		clockr = panelr;
		clockr.min.x = clockr.max.x - ClockWidth;
		taskr = panelr;
		taskr.min.x = startr.max.x + 4;
		taskr.max.x = clockr.min.x - 4;
		break;
	case PanelLeft:
		panelr.max.x = panelr.min.x + StartWidth;
		startr = panelr;
		startr.max.y = startr.min.y + PanelSize;
		clockr = panelr;
		clockr.min.y = clockr.max.y - 3*PanelSize;
		taskr = panelr;
		taskr.min.y = startr.max.y + 4;
		taskr.max.y = clockr.min.y - 4;
		break;
	case PanelRight:
		panelr.min.x = panelr.max.x - StartWidth;
		startr = panelr;
		startr.max.y = startr.min.y + PanelSize;
		clockr = panelr;
		clockr.min.y = clockr.max.y - 3*PanelSize;
		taskr = panelr;
		taskr.min.y = startr.max.y + 4;
		taskr.max.y = clockr.min.y - 4;
		break;
	}

	ntasks = 0;
	for(i = 0; i < nwindows && ntasks < MAXWINDOWS; i++)
		if(windows[i]->cur != nil)
			tasks[ntasks++].w = windows[i];
	n = ntasks;
	if(n == 0)
		return;
	if(paneledge == PanelTop || paneledge == PanelBottom){
		w = Dx(taskr)/n;
		w = MAX(TaskMinWidth, MIN(TaskMaxWidth, w));
		start = taskr.min.x;
		for(i = 0; i < n; i++){
			tasks[i].r = Rect(start, taskr.min.y+3, start+w-3, taskr.max.y-3);
			if(tasks[i].r.max.x > taskr.max.x)
				tasks[i].r = ZR;
			start += w;
		}
	}else{
		w = PanelSize;
		start = taskr.min.y;
		end = taskr.max.y;
		for(i = 0; i < n; i++){
			tasks[i].r = Rect(taskr.min.x+3, start, taskr.max.x-3, start+w-3);
			if(tasks[i].r.max.y > end)
				tasks[i].r = ZR;
			start += w;
		}
	}
}

void
paneldraw(void)
{
	int i, down;
	Image *img;
	char *label;

	if(!panelon || screen == nil || wscreen == nil || colors[BACK] == nil)
		return;
	panelcolors();
	panelclockstr();
	panellayout();
	if(panelimg == nil || !eqrect(panelimg->r, panelr)){
		panelreset();
		panelimg = allocwindow(wscreen, panelr, Refbackup, DNofill);
		if(panelimg == nil)
			return;
	}
	img = panelimg;
	draw(img, panelr, panel_face, nil, ZP);
	panelbevel(img, panelr, 0);

	draw(img, insetrect(startr, 3), panel_face, nil, ZP);
	panelbevel(img, insetrect(startr, 3), 0);
	paneltext(img, insetrect(startr, 3), "Start", 1);

	for(i = 0; i < ntasks; i++){
		if(Dx(tasks[i].r) <= 0 || Dy(tasks[i].r) <= 0)
			continue;
		down = tasks[i].w == focused;
		draw(img, tasks[i].r, panel_face, nil, ZP);
		panelbevel(img, tasks[i].r, down);
		label = tasks[i].w->cur ? tasks[i].w->cur->label : "<window>";
		paneltext(img, insetrect(tasks[i].r, 2), label, 0);
	}

	draw(img, insetrect(clockr, 3), panel_face, nil, ZP);
	panelbevel(img, insetrect(clockr, 3), 1);
	paneltext(img, insetrect(clockr, 3), clockbuf, 1);
	topwindow(panelimg);
	flushimage(display, 1);
}

static void
panellaunch(char *cmd)
{
	WinTab *t;
	char *argv[] = { "rc", "-c", nil, nil };

	if(strcmp(cmd, "rc") == 0){
		new(newrect());
		return;
	}
	argv[2] = cmd;
	t = wtcreate(newrect(), FALSE, scrolling);
	if(t != nil)
		wincmd(t, 0, nil, argv);
}

static void
panelstartmenu(void)
{
	enum {
		Newrc,
		Acme,
		Stats,
		Kbmap,
		Page,
		Exit,
	};
	static char *str[] = {
		"New rc",
		"Acme",
		"Stats",
		"Kbmap",
		"Page",
		"Exit lola",
		nil,
	};
	Image *m;
	Rectangle r, ir;
	Point p, delta;
	int i, n, sel, lastsel;

	for(n = 0; str[n] != nil; n++)
		;
	r = Rect(0, 0, StartMenuWidth, n*StartMenuItemHeight);
	switch(paneledge){
	case PanelTop:
		r = rectaddpt(r, Pt(startr.min.x, panelr.max.y));
		break;
	case PanelBottom:
		r = rectaddpt(r, Pt(startr.min.x, panelr.min.y-Dy(r)));
		break;
	case PanelLeft:
		r = rectaddpt(r, Pt(panelr.max.x, startr.min.y));
		break;
	case PanelRight:
		r = rectaddpt(r, Pt(panelr.min.x-Dx(r), startr.min.y));
		break;
	}
	delta = ZP;
	if(r.max.x > screen->r.max.x)
		delta.x = screen->r.max.x-r.max.x;
	if(r.max.y > screen->r.max.y)
		delta.y = screen->r.max.y-r.max.y;
	if(r.min.x < screen->r.min.x)
		delta.x = screen->r.min.x-r.min.x;
	if(r.min.y < screen->r.min.y)
		delta.y = screen->r.min.y-r.min.y;
	r = rectaddpt(r, delta);

	while(mctl->buttons)
		readmouse(mctl);

	m = allocwindow(wscreen, r, Refbackup, DWhite);
	if(m == nil)
		return;
	topwindow(m);
	draw(m, r, panel_face, nil, ZP);
	panelbevel(m, r, 0);
	for(i = 0; i < n; i++){
		ir = Rect(r.min.x+3, r.min.y+3+i*StartMenuItemHeight,
			r.max.x-3, r.min.y+3+(i+1)*StartMenuItemHeight);
		paneltext(m, ir, str[i], 0);
	}
	flushimage(display, 1);

	sel = -1;
	lastsel = -1;
	for(;;){
		readmouse(mctl);
		if(mctl->buttons & 6)
			break;
		p = mctl->xy;
		i = -1;
		if(ptinrect(p, r)){
			i = (p.y-r.min.y-3)/StartMenuItemHeight;
			if(i < 0 || i >= n)
				i = -1;
		}
		if(i != lastsel){
			if(lastsel >= 0){
				ir = Rect(r.min.x+3, r.min.y+3+lastsel*StartMenuItemHeight,
					r.max.x-3, r.min.y+3+(lastsel+1)*StartMenuItemHeight);
				draw(m, ir, panel_face, nil, ZP);
				paneltext(m, ir, str[lastsel], 0);
			}
			if(i >= 0){
				ir = Rect(r.min.x+3, r.min.y+3+i*StartMenuItemHeight,
					r.max.x-3, r.min.y+3+(i+1)*StartMenuItemHeight);
				draw(m, ir, panel_shadow, nil, ZP);
				paneltext(m, ir, str[i], 0);
			}
			flushimage(display, 1);
			lastsel = i;
		}
		if(mctl->buttons & 1){
			sel = i;
			while(mctl->buttons)
				readmouse(mctl);
			break;
		}
	}
	freeimage(m);
	paneldraw();
	switch(sel){
	case Newrc:
		panellaunch("rc");
		break;
	case Acme:
		panellaunch("acme");
		break;
	case Stats:
		panellaunch("stats -lmisce");
		break;
	case Kbmap:
		panellaunch("q9kbsetup -reset");
		break;
	case Page:
		panellaunch("page");
		break;
	case Exit:
		killprocs();
		threadexitsall(nil);
	}
	paneldraw();
}

int
panelmouse(Mousectl *mc)
{
	int i;

	if(!panelon || !ptinrect(mc->xy, panelr))
		return 0;
	if((mc->buttons & 1) == 0)
		return 0;
	setcursornormal(nil);
	if(ptinrect(mc->xy, startr)){
		panelstartmenu();
		while(mc->buttons)
			readmouse(mc);
		return 1;
	}
	for(i = 0; i < ntasks; i++)
		if(ptinrect(mc->xy, tasks[i].r)){
			if(tasks[i].w->hidden)
				wunhide(tasks[i].w);
			else{
				wraise(tasks[i].w);
				wfocus(tasks[i].w);
			}
			while(mc->buttons)
				readmouse(mc);
			paneldraw();
			return 1;
		}
	return 0;
}

void
panelinit(void)
{
	if(!panelon)
		return;
	panelclockstr();
	threadcreate(panelclock, nil, mainstacksize);
}

static void
panelclock(void*)
{
	for(;;){
		sleep(60000);
		paneldraw();
	}
}
