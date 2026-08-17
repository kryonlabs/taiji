#include "inc.h"

typedef struct Theme Theme;
struct Theme {
	char *name;
	int border;
	int title;
	int tab;
	int grad;
	void (*decor)(Window*);
	void (*init)(void);
};

int bordersz = 4;
int titlesz = 17;
int tabsz = 18;
int titlegradient;

static Theme themes[] = {
	{ "flat", 4, 18, 18, 0, flatwdecor, flatinittheme },
	{ "simple", 4, 17, 18, 0, simplewdecor, simpleinittheme },
	{ "win3", 4, 19, 23, 0, win3wdecor, win3inittheme },
	{ "win95", 4, 19, 20, 0, win95wdecor, win95inittheme },
	{ "win2k", 4, 19, 20, 1, win95wdecor, win95inittheme },
};

static Theme *theme = &themes[4];

static void
applytheme(Theme *t)
{
	theme = t;
	bordersz = t->border;
	titlesz = t->title;
	tabsz = t->tab;
	titlegradient = t->grad;
}

int
knowntheme(char *name)
{
	int i;

	for(i = 0; i < nelem(themes); i++)
		if(strcmp(name, themes[i].name) == 0)
			return 1;
	return 0;
}

/* switch the running decoration theme (wctl "theme ...") */
void
retheme(char *name)
{
	if(!knowntheme(name))
		return;
	settheme(name);
	inittheme();
	panelinit();
	refresh();
	paneldraw();
}

static ulong
wlerp(ulong c0, ulong c1, int i, int n)
{
	int r, g, b;

	r = ((c0>>24 & 0xFF)*(n-i) + (c1>>24 & 0xFF)*i) / n;
	g = ((c0>>16 & 0xFF)*(n-i) + (c1>>16 & 0xFF)*i) / n;
	b = ((c0>>8 & 0xFF)*(n-i) + (c0>>8 & 0xFF)*i) / n;
	return r<<24 | g<<16 | b<<8 | 0xFF;
}

/* switch the desktop background (wctl "wallpaper ...") */
void
setwallpaper(char *name)
{
	Image *wp;
	Rectangle r;
	ulong seed;
	int i, x, y;
	char *home, *path;
	int fd;

	if(name == nil || name[0] == 0)
		return;
	r = screen->r;
	wp = allocimage(display, r, screen->chan, 0, 0x008080FF);
	if(wp == nil)
		return;
	if(strcmp(name, "gradient") == 0){
		for(i = 0; i < 32; i++)
			draw(wp, Rect(r.min.x, r.min.y + Dy(r)*i/32, r.max.x, r.min.y + Dy(r)*(i+1)/32),
				getcolor(nil, wlerp(0x3A6EA5FF, 0xA6CAF0FF, i, 31)), nil, ZP);
	}else if(strcmp(name, "night") == 0){
		draw(wp, r, display->black, nil, ZP);
		seed = 12345;
		for(i = 0; i < 400; i++){
			seed = seed*1103515245 + 12345;
			x = (seed>>16) & 0xFFFF;
			seed = seed*1103515245 + 12345;
			y = (seed>>16) & 0xFFFF;
			draw(wp, Rect(r.min.x + x%Dx(r), r.min.y + y%Dy(r),
				r.min.x + x%Dx(r) + 1, r.min.y + y%Dy(r) + 1), display->white, nil, ZP);
		}
	}else if(strcmp(name, "slate") == 0){
		draw(wp, r, getcolor(nil, 0x6A7A8AFF), nil, ZP);
	}
	background = wp;

	freescreen(wscreen);
	wscreen = allocscreen(screen, background, 0);
	freeimage(fakebg);
	fakebg = allocwindow(wscreen, screen->r, Refbackup, DNofill);
	draw(fakebg, fakebg->r, background, nil, ZP);
	refresh();

	/* remember the choice */
	home = getenv("home");
	if(home == nil)
		return;
	path = smprint("%s/lib/wallpaper", home);
	free(home);
	if(path == nil)
		return;
	fd = create(path, OWRITE, 0666);
	if(fd >= 0){
		fprint(fd, "%s\n", name);
		close(fd);
	}
	free(path);
}

/* set the screen saver timeout in minutes, 0 disables (wctl "saver n") */
void
setsaver(int minutes)
{
	char *home, *path;
	int fd;

	if(minutes < 0)
		minutes = 0;
	if(minutes > 240)
		minutes = 240;
	savermin = minutes;
	home = getenv("home");
	if(home == nil)
		return;
	path = smprint("%s/lib/saver", home);
	free(home);
	if(path == nil)
		return;
	fd = create(path, OWRITE, 0666);
	if(fd >= 0){
		fprint(fd, "%d\n", minutes);
		close(fd);
	}
	free(path);
}

void
settheme(char *name)
{
	int i;

	for(i = 0; i < nelem(themes); i++)
		if(strcmp(name, themes[i].name) == 0){
			applytheme(&themes[i]);
			return;
		}
	sysfatal("unknown theme %s", name);
}

void
wdecor(Window *w)
{
	theme->decor(w);
}

void
wtitlectl(Window *w)
{
	static Window *lastw;
	static ulong lastt;
	static Point lastp;
	Rectangle r, br;
	int action, d;
	ulong now;

	if((mctl->buttons & 7) == 0)
		return;
	wraise(w);
	wfocus(w);
	if(mctl->buttons & 4){
		/* right click on the title bar: window system menu */
		drainmouse(mctl, nil);
		winsysmenu(w, Rect(w->titlerect.min.x, w->titlerect.max.y,
			w->titlerect.min.x+8, w->titlerect.max.y+1));
		return;
	}
	if(mctl->buttons & 2){
		drainmouse(mctl, nil);
		return;
	}
	if((mctl->buttons & 1) == 0)
		return;

	r = w->titlerect;
	r.max.y -= 1;

	/* app icon at the left of the title bar opens the system menu */
	br = Rect(r.min.x+1, r.min.y+1, r.min.x+17, r.max.y-1);
	if(ptinrect(mctl->xy, br)){
		drainmouse(mctl, nil);
		winsysmenu(w, Rect(br.min.x, w->titlerect.max.y, br.min.x+8, w->titlerect.max.y+1));
		return;
	}
	br = insetrect(r, 2);
	br.min.x = br.max.x - Dy(br) - 2;
	action = 0;
	if(ptinrect(mctl->xy, br))
		action = 1;
	else{
		br = rectaddpt(br, Pt(-Dx(br)-2, 0));
		if(ptinrect(mctl->xy, br))
			action = 2;
		else{
			br = rectaddpt(br, Pt(-Dx(br)-2, 0));
			if(ptinrect(mctl->xy, br))
				action = 3;
		}
	}
	if(action == 1){
		drainmouse(mctl, nil);
		wdelete(w);
		return;
	}
	if(action == 2){
		drainmouse(mctl, nil);
		if(w->maximized)
			wrestore(w);
		else
			wmaximize(w);
		return;
	}
	if(action == 3){
		drainmouse(mctl, nil);
		animminimize(w);
		whide(w);
		paneldraw();
		return;
	}

	now = mctl->msec;
	d = abs(mctl->xy.x-lastp.x) + abs(mctl->xy.y-lastp.y);
	if(lastw == w && now-lastt < 500 && d < 8){
		drainmouse(mctl, nil);
		if(w->maximized)
			wrestore(w);
		else
			wmaximize(w);
		lastw = nil;
		lastt = 0;
		return;
	}
	lastw = w;
	lastt = now;
	lastp = mctl->xy;

	if(!w->maximized)
		grab(w, 1);
}

void
inittheme(void)
{
	theme->init();
}
