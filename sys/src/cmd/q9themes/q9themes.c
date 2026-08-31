/*
 * q9themes - Kryon theme manager for TaijiOS.
 *
 * A Taiji control panel applet for Kryon themes. Runs on the Kryon
 * runtime and reads the palette catalog straight from the library, so
 * the picker always matches the themes the desktop actually ships. It
 * applies the choice through rio's wctl "theme kryon:style:palette:mode"
 * message so the whole desktop re-skins at once.
 */
#include <u.h>
#include <libc.h>
#include "kryon.h"
#include "theme.h"
#include "theme_meta.h"

enum {
	Titleh = 26,
	Rowh = 22,
	Btnw = 84,
	Btnh = 26,
	ListX = 12,
	ListW = 186,
	ListY = 44,
	RightX = 214,
	RightW = 254,
};

char *styles[] = { "Retro", "Material", nil };
char *modes[] = { "Light", "Dark", nil };

int sel;			/* palette index */
int style;			/* 0 retro, 1 material */
int mode;			/* 0 light, 1 dark */
int applied;

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

static char *
kv(char *text, char *key)
{
	char *p, *eol;
	int klen, n;
	static char val[128];

	klen = strlen(key);
	eol = nil;
	for(p = text; p != nil && *p; ){
		eol = strchr(p, '\n');
		if(strncmp(p, key, klen) == 0 && p[klen] == '=')
			break;
		if(eol == nil)
			return nil;
		p = eol+1;
	}
	if(p == nil || *p == 0)
		return nil;
	p += klen+1;
	n = eol == nil ? strlen(p) : eol-p;
	if(n >= sizeof val)
		n = sizeof val-1;
	memmove(val, p, n);
	val[n] = 0;
	return val;
}

static void
readcurrent(void)
{
	char *home, *path, buf[512], *v;
	int fd, n, i;

	sel = THEME_MONO;
	style = 0;
	mode = 0;
	home = getenv("home");
	if(home == nil)
		return;
	path = smprint("%s/lib/kryon/theme", home);
	free(home);
	if(path == nil)
		return;
	fd = open(path, OREAD);
	free(path);
	if(fd < 0)
		return;
	n = read(fd, buf, sizeof buf-1);
	close(fd);
	if(n <= 0)
		return;
	buf[n] = 0;
	v = kv(buf, "name");
	if(v != nil)
		for(i = 0; i < THEME_COUNT; i++)
			if(strcmp(GetThemeMeta((ThemeId)i)->name, v) == 0)
				sel = i;
	v = kv(buf, "style");
	if(v != nil && strcmp(v, "material") == 0)
		style = 1;
	v = kv(buf, "mode");
	if(v != nil && strcmp(v, "dark") == 0)
		mode = 1;
}

static void
apply(void)
{
	int fd;

	fd = open("/dev/wctl", OWRITE);
	if(fd < 0)
		return;
	fprint(fd, "theme kryon:%s:%s:%s",
		style ? "material" : "retro",
		GetThemeMeta((ThemeId)sel)->name,
		mode ? "dark" : "light");
	close(fd);
	applied = 1;
}

static Rectangle
stylerect(int i)
{
	Rectangle r;

	r.x = RightX;
	r.y = 52+i*(Btnh+8);
	r.width = Btnw;
	r.height = Btnh;
	return r;
}

static Rectangle
moderect(int i)
{
	Rectangle r;

	r.x = RightX+Btnw+12;
	r.y = 52+i*(Btnh+8);
	r.width = Btnw;
	r.height = Btnh;
	return r;
}

static Rectangle
previewrect(void)
{
	Rectangle r;

	r.x = RightX;
	r.y = 128;
	r.width = RightW;
	r.height = 190;
	return r;
}

static int
drawbtn(Rectangle r, char *s, int on)
{
	Vector2 m;
	int hover;

	m = GetMousePosition();
	hover = CheckCollisionPointRec(m, r);
	if(on)
		DrawRectangleRounded(r, 0.15f, 4, oc(GetThemeLink()));
	else
		DrawRectangleRounded(r, 0.15f, 4,
		    hover ? oc(GetThemeButtonHover()) : oc(GetThemeButton()));
	DrawText(s, (int)r.x + ((int)r.width - MeasureText(s, 13))/2,
	    (int)r.y + ((int)r.height-13)/2, 13,
	    on ? oc(GetThemeBackground()) : oc(GetThemeText()));
	return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

/* palette row with three catalog swatches; returns 1 when picked */
static int
drawthemrow(int i)
{
	Rectangle row, sw;
	Vector2 m;
	Color c;
	int hover, ok;

	row.x = ListX;
	row.y = ListY + i*Rowh;
	row.width = ListW;
	row.height = Rowh-2;
	m = GetMousePosition();
	hover = CheckCollisionPointRec(m, row);
	if(i == sel)
		DrawRectangleRounded(row, 0.1f, 4, Fade(GetThemeLink(), 0.35f));
	else if(hover)
		DrawRectangleRounded(row, 0.1f, 4, Fade(GetThemeButtonHover(), 0.4f));
	sw.y = row.y + 4;
	sw.width = 14;
	sw.height = Rowh-10;
	sw.x = ListX+4;
	if(GetThemeCatalogColor((ThemeId)i, mode != 0, "background", &c)){
		DrawRectangleRec(sw, oc(c));
		DrawRectangleLines((int)sw.x, (int)sw.y, (int)sw.width,
		    (int)sw.height, muted(0.4f));
	}
	sw.x = ListX+20;
	if(GetThemeCatalogColor((ThemeId)i, mode != 0, "surface", &c)){
		DrawRectangleRec(sw, oc(c));
		DrawRectangleLines((int)sw.x, (int)sw.y, (int)sw.width,
		    (int)sw.height, muted(0.4f));
	}
	sw.x = ListX+36;
	if(GetThemeCatalogColor((ThemeId)i, mode != 0, "link", &c)){
		DrawRectangleRec(sw, oc(c));
		DrawRectangleLines((int)sw.x, (int)sw.y, (int)sw.width,
		    (int)sw.height, muted(0.4f));
	}
	DrawText(GetThemeMeta((ThemeId)i)->name, ListX+56,
	    (int)row.y + (Rowh-2-13)/2, 13, oc(GetThemeText()));
	ok = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
	return ok;
}

/* mini window preview drawn from the live catalog */
static void
drawpreview(Rectangle r)
{
	Color bg, surface, txt, link;
	Rectangle wr, tb;
	int material;

	if(!GetThemeCatalogColor((ThemeId)sel, mode != 0, "background", &bg) ||
	   !GetThemeCatalogColor((ThemeId)sel, mode != 0, "surface", &surface) ||
	   !GetThemeCatalogColor((ThemeId)sel, mode != 0, "text", &txt) ||
	   !GetThemeCatalogColor((ThemeId)sel, mode != 0, "link", &link))
		return;
	material = style == 1;

	DrawRectangleRec(r, oc(bg));
	wr = r;
	wr.x += 14;
	wr.y += 14;
	wr.width -= 28;
	wr.height -= 28;
	wr.height = 108;
	if(material)
		DrawRectangleRounded(wr, 0.06f, 6, oc(surface));
	else{
		DrawRectangleRec(wr, oc(surface));
		DrawRectangleLines((int)wr.x, (int)wr.y, (int)wr.width,
		    (int)wr.height, Fade(txt, 0.5f));
	}
	tb = wr;
	tb.x += 2;
	tb.y += 2;
	tb.width -= 4;
	tb.height = 20;
	DrawRectangleRec(tb, oc(link));
	DrawText(GetThemeMeta((ThemeId)sel)->name, (int)tb.x+8,
	    (int)tb.y + (20-13)/2, 13, oc(bg));
	tb = wr;
	tb.x += 6;
	tb.y += 26;
	tb.width -= 12;
	tb.height -= 32;
	if(material)
		DrawRectangleRounded(tb, 0.05f, 4, Fade(surface, 0.85f));
	else
		DrawRectangleRec(tb, oc(bg));
	DrawText(material ? "material window" : "retro window",
	    (int)tb.x+8, (int)tb.y+6, 13, oc(txt));
	DrawText(mode ? "dark mode" : "light mode",
	    (int)tb.x+8, (int)tb.y+26, 13, oc(txt));
}

void
main(int argc, char *argv[])
{
	Rectangle r;
	char buf[64];
	int i, done, doapply;

	ARGBEGIN{
	default:
		break;
	}ARGEND

	readcurrent();

	SetSingleInstance(0);
	InitWindow(480, 460, "themes");
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
		doapply = 0;
		BeginDrawing();
		ClearBackground(oc(GetThemeBackground()));
		BeginUIFrame(GetScreenWidth(), GetScreenHeight(), 1.0f);
		BeginUI(0x74686d73);

		r.x = 0;
		r.y = 0;
		r.width = GetScreenWidth();
		r.height = Titleh;
		DrawRectangleRec(r, oc(GetThemeSurface()));
		DrawText("Kryon Themes",
		    (GetScreenWidth() - MeasureText("Kryon Themes", 15))/2,
		    (Titleh-15)/2, 15, oc(GetThemeText()));

		DrawText("Palette", ListX, ListY-13-6, 13, muted(0.8f));
		for(i = 0; i < THEME_COUNT; i++)
			if(drawthemrow(i))
				sel = i;

		DrawText("Style", RightX, 36, 13, muted(0.8f));
		for(i = 0; i < 2; i++)
			if(drawbtn(stylerect(i), styles[i], i == style))
				style = i;
		DrawText("Mode", RightX+Btnw+12, 36, 13, muted(0.8f));
		for(i = 0; i < 2; i++)
			if(drawbtn(moderect(i), modes[i], i == mode))
				mode = i;

		DrawText("Preview", RightX, 118, 13, muted(0.8f));
		drawpreview(previewrect());

		r.x = GetScreenWidth()-2*Btnw-24;
		r.y = GetScreenHeight()-Btnh-10;
		r.width = Btnw;
		r.height = Btnh;
		if(drawbtn(r, "Apply", 0))
			doapply = 1;
		r.x = GetScreenWidth()-Btnw-12;
		if(drawbtn(r, "Close", 0))
			done = 1;

		snprint(buf, sizeof buf, "%s %s %s%s",
			styles[style], GetThemeMeta((ThemeId)sel)->name,
			modes[mode], applied ? "  (applied)" : "");
		DrawText(buf, 12, GetScreenHeight()-Btnh-10+(Btnh-13)/2, 13,
		    muted(0.8f));

		EndUI();
		EndUIFrame();
		EndDrawing();
		if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
			doapply = 1;
		if(doapply)
			apply();
	}
	CloseWindow();
	exits(nil);
}
