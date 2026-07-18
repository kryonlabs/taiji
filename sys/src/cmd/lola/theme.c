#include "inc.h"

typedef struct Theme Theme;
struct Theme {
	char *name;
	int border;
	int title;
	int tab;
	void (*decor)(Window*);
	void (*titlectl)(Window*);
	void (*init)(void);
};

int bordersz = 4;
int titlesz = 17;
int tabsz = 18;

static Theme themes[] = {
	{ "flat", 4, 18, 18, flatwdecor, flatwtitlectl, flatinittheme },
	{ "simple", 4, 17, 18, simplewdecor, simplewtitlectl, simpleinittheme },
	{ "win3", 4, 19, 23, win3wdecor, win3wtitlectl, win3inittheme },
	{ "win95", 4, 19, 20, win95wdecor, win95wtitlectl, win95inittheme },
};

static Theme *theme = &themes[1];

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
	theme->titlectl(w);
}

void
inittheme(void)
{
	theme->init();
}
