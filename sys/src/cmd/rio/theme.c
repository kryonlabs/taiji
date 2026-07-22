#include "inc.h"

typedef struct Theme Theme;
struct Theme {
	char *name;
	int border;
	int title;
	int tab;
	void (*decor)(Window*);
	void (*init)(void);
};

int bordersz = 4;
int titlesz = 17;
int tabsz = 18;

static Theme themes[] = {
	{ "flat", 4, 18, 18, flatwdecor, flatinittheme },
	{ "simple", 4, 17, 18, simplewdecor, simpleinittheme },
	{ "win3", 4, 19, 23, win3wdecor, win3inittheme },
	{ "win95", 4, 19, 20, win95wdecor, win95inittheme },
	{ "win2k", 4, 19, 20, win95wdecor, win95inittheme },
};

static Theme *theme = &themes[4];

static void
applytheme(Theme *t)
{
	theme = t;
	bordersz = t->border;
	titlesz = t->title;
	tabsz = t->tab;
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
		btn3menu();
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
		whide(w);
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
