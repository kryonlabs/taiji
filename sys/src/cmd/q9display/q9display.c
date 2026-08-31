#include <u.h>
#include <libc.h>
#include "kryon.h"

/*
 * Kryon-native Display Properties: tabs for Background, Screen Saver
 * and Settings. Background picks a rio wallpaper, Screen Saver sets the
 * idle timeout; both talk to rio through wctl messages. Appearance
 * (theming) lives in the dedicated q9themes applet. Settings is
 * informational.
 */

enum {
	Tabh = 26,
	Btnw = 74,
	Btnh = 26,
	Rowh = 20,
	Listw = 220,
	NTabs = 3,
};

char *tabnames[NTabs] = {
	"Background",
	"Screen Saver",
	"Settings",
};

char *wallpapers[] = {
	"teal",
	"gradient",
	"night",
	"slate",
	nil,
};

char *savers[] = {
	"None",
	"1 minute",
	"5 minutes",
	"10 minutes",
	"30 minutes",
	nil,
};

int savminutes[] = { 0, 1, 5, 10, 30 };

int tab;
int wallsel, savsel;

char curwall[64];
int cursaver;

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

static void
readcurrents(void)
{
	char *home, *path, buf[128];
	int fd, n;

	strecpy(curwall, curwall+sizeof curwall, "teal");
	cursaver = 0;
	home = getenv("home");
	if(home == nil)
		return;
	path = smprint("%s/lib/wallpaper", home);
	if(path != nil){
		fd = open(path, OREAD);
		if(fd >= 0){
			n = read(fd, buf, sizeof(buf)-1);
			if(n > 0){
				buf[n] = 0;
				while(n > 0 && (buf[n-1] == '\n' || buf[n-1] == ' '))
					buf[--n] = 0;
				if(buf[0])
					strecpy(curwall, curwall+sizeof curwall, buf);
			}
			close(fd);
		}
		free(path);
	}
	path = smprint("%s/lib/saver", home);
	if(path != nil){
		fd = open(path, OREAD);
		if(fd >= 0){
			n = read(fd, buf, sizeof(buf)-1);
			if(n > 0){
				buf[n] = 0;
				cursaver = atoi(buf);
			}
			close(fd);
		}
		free(path);
	}
	free(home);
}

static int
indexof(char **list, char *val)
{
	int i;

	for(i = 0; list[i]; i++)
		if(strcmp(list[i], val) == 0)
			return i;
	return 0;
}

static void
apply(void)
{
	int fd;

	fd = open("/dev/wctl", OWRITE);
	if(fd < 0)
		return;
	fprint(fd, "wallpaper %s", wallpapers[wallsel]);
	fprint(fd, "saver %d", savminutes[savsel]);
	close(fd);
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
panelrect(void)
{
	Rectangle r;

	r.x = 10;
	r.y = 8+Tabh-1;
	r.width = GetScreenWidth()-20;
	r.height = GetScreenHeight()-8-Tabh+1-46;
	return r;
}

static Rectangle
rowrect(int i)
{
	Rectangle r;

	r.x = 30;
	r.y = 70+i*(Rowh+4);
	r.width = Listw;
	r.height = Rowh;
	return r;
}

static Rectangle
btnrect(int i)
{
	Rectangle r;

	r.x = GetScreenWidth()-260+i*90;
	r.y = GetScreenHeight()-40;
	r.width = Btnw;
	r.height = Btnh;
	return r;
}

/* draw one tab; returns 1 when it is clicked */
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

/* small button; returns 1 when clicked */
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

/* list row; returns 1 when picked */
static int
drawrow(Rectangle r, char *s, int on)
{
	Vector2 m;
	int hover;

	m = GetMousePosition();
	hover = CheckCollisionPointRec(m, r);
	if(on)
		DrawRectangleRounded(r, 0.12f, 4, oc(GetThemeLink()));
	else if(hover)
		DrawRectangleRounded(r, 0.12f, 4, Fade(GetThemeButtonHover(), 0.5f));
	else
		DrawRectangleRounded(r, 0.12f, 4, oc(GetThemeSurface()));
	DrawText(s, (int)r.x + 8, (int)r.y + ((int)r.height-13)/2, 13,
	    on ? oc(GetThemeBackground()) : oc(GetThemeText()));
	return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

/* little preview monitor for the wallpaper choice */
static void
drawmonitor(void)
{
	Rectangle r, scr, band;
	Color wall;
	int i;

	r.x = GetScreenWidth()-190;
	r.y = 60;
	r.width = 140;
	r.height = 160;
	DrawRectangleRec(r, oc(GetThemeSurface()));
	DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height,
	    muted(0.5f));
	scr = r;
	scr.x += 8;
	scr.y += 8;
	scr.width -= 16;
	scr.height -= 24;
	switch(wallsel){
	default: wall = oc(GetThemeCircle()); break;
	case 1: wall = oc(GetThemeLink()); break;
	case 2: wall = Fade(GetThemeText(), 0.25f); break;
	case 3: wall = oc(GetThemeButtonHover()); break;
	}
	DrawRectangleRec(scr, wall);
	if(wallsel == 1){
		for(i = 0; i < 6; i++){
			band = scr;
			band.y += (float)((int)scr.height*i/6);
			band.height = (float)((int)scr.height/6 + 1);
			DrawRectangleRec(band, i%2 ?
			    Fade(GetThemeButtonHover(), 0.5f) :
			    Fade(GetThemeLink(), 0.8f));
		}
	}
	if(wallsel == 2){
		for(i = 0; i < 12; i++)
			DrawRectangle((int)scr.x+7+i*11,
			    (int)scr.y+5+(i*7)%((int)scr.height-8), 1, 1,
			    oc(GetThemeText()));
	}
}

void
main(int argc, char *argv[])
{
	Rectangle r;
	char buf[128];
	char *vg;
	int i, n, done, applied;

	ARGBEGIN{
	default:
		break;
	}ARGEND

	readcurrents();
	wallsel = indexof(wallpapers, curwall);
	savsel = 0;
	for(i = 0; i < nelem(savminutes); i++)
		if(savminutes[i] == cursaver)
			savsel = i;

	SetSingleInstance(0);
	InitWindow(560, 380, "q9display");
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

	done = 0;
	while(!done && !WindowShouldClose()){
		if(IsKeyPressed(KEY_ESCAPE))
			break;
		applied = 0;
		BeginDrawing();
		ClearBackground(oc(GetThemeBackground()));
		BeginUIFrame(GetScreenWidth(), GetScreenHeight(), 1.0f);
		BeginUI(0x71396473);

		for(i = 0; i < NTabs; i++)
			if(drawtab(i))
				tab = i;

		r = panelrect();
		DrawRectangleRec(r, oc(GetThemeSurface()));
		DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height,
		    muted(0.4f));

		switch(tab){
		case 0:
			DrawText("Wallpaper:", 30, 48, 13, oc(GetThemeText()));
			drawmonitor();
			n = nelem(wallpapers)-1;
			for(i = 0; i < n; i++)
				if(drawrow(rowrect(i), wallpapers[i], i == wallsel))
					wallsel = i;
			break;
		case 1:
			DrawText("Screen saver:", 30, 48, 13, oc(GetThemeText()));
			n = nelem(savers)-1;
			for(i = 0; i < n; i++)
				if(drawrow(rowrect(i), savers[i], i == savsel))
					savsel = i;
			DrawText("After the idle timeout rio blanks the", 280, 60, 13,
			    muted(0.8f));
			DrawText("screen with a bouncing Plan 9 flag.", 280, 78, 13,
			    muted(0.8f));
			break;
		case 2:
			DrawText("Resolution:", 30, 56, 13, oc(GetThemeText()));
			vg = getenv("vgasize");
			snprint(buf, sizeof buf, "%s (fixed by vgasize)",
			    vg ? vg : "unknown");
			free(vg);
			DrawText(buf, 30, 76, 13, oc(GetThemeText()));
			DrawText("Colors: True color (32 bit)", 30, 106, 13,
			    oc(GetThemeText()));
			DrawText("Display: Kryon runtime (libdraw backend)", 30, 126, 13,
			    oc(GetThemeText()));
			break;
		}

		if(drawbtn(btnrect(0), "OK"))
			applied = 1;
		if(drawbtn(btnrect(1), "Cancel"))
			done = 1;
		if(drawbtn(btnrect(2), "Apply"))
			applied = 1;

		EndUI();
		EndUIFrame();
		EndDrawing();
		if(applied)
			apply();
	}
	CloseWindow();
	exits(nil);
}
